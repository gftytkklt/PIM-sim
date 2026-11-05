"""Hand-checkable ONNX, Booksim/MNSIM and regression contract tests.

Run with scripts/python.sh tests/python/test_pipeline.py.
No external network, reference updates, or hardware calibration are performed.
"""
import importlib.util
import math
import os
from pathlib import Path
import pickle
import subprocess
import tempfile
from types import SimpleNamespace as NS
import unittest
from unittest.mock import patch

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper
from pim_sim import onnx_analysis as oa, mapping_info as mi, analysis, config_validator, paths

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("regression_compare", ROOT / "scripts/regression/compare.py")
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


def tensor(name, shape):
    return helper.make_tensor_value_info(name, TensorProto.FLOAT, shape)


def weight(name, shape):
    return numpy_helper.from_array(np.ones(shape, dtype=np.float32), name)


def model(nodes, weights, outputs, inputs=None):
    return helper.make_model(helper.make_graph(nodes, "hand_checked", inputs or [tensor("x", [1, 2, 2, 2])],
                                               outputs, weights), opset_imports=[helper.make_opsetid("", 13)])


def two_convs():
    return model([helper.make_node("Conv", ["x", "w0"], ["a"]),
                  helper.make_node("Relu", ["a"], ["b"]),
                  helper.make_node("Conv", ["b", "w1"], ["y"])],
                 [weight("w0", (3, 2, 1, 1)), weight("w1", (2, 3, 1, 1))],
                 [tensor("y", [1, 2, 2, 2])])


class ParserContracts(unittest.TestCase):
    def test_gemm_reduction_and_output_channels(self):
        for trans, dims in ((0, (3, 4)), (1, (4, 3))):
            with self.subTest(trans=trans):
                graph = model([helper.make_node("Gemm", ["x", "w"], ["y"], transB=trans)],
                              [weight("w", dims)], [tensor("y", [1, 4])], [tensor("x", [1, 3])])
                info = oa.build_conv_info(graph)[0]
                self.assertEqual(tuple(info.channel), (3, 4))
                self.assertEqual(info.ifmap_size, (1, 1))
                self.assertEqual(info.ofmap_size, (1, 1))

    def test_fusion_follows_tensor_not_last_node_and_never_mutates(self):
        graph = model([helper.make_node("Conv", ["x", "wa"], ["a"]),
                       helper.make_node("Conv", ["x", "wb"], ["b"]),
                       helper.make_node("Relu", ["a"], ["y"])],
                      [weight("wa", (2, 2, 1, 1)), weight("wb", (2, 2, 1, 1))],
                      [tensor("y", [1, 2, 2, 2]), tensor("b", [1, 2, 2, 2])])
        before = graph.SerializeToString()
        infos = oa.build_conv_info(graph)
        self.assertEqual(before, graph.SerializeToString())
        self.assertEqual([[d.dep_layer for d in i.depinfo] for i in infos], [[-1], [-1]])
        fused = oa.merge_ops(graph)
        self.assertIn("y", fused[0].output)
        self.assertNotIn("y", fused[1].output)
        self.assertEqual(infos, oa.build_conv_info(graph))

    def test_chain_dependencies_and_shapes(self):
        infos = oa.build_conv_info(two_convs())
        self.assertEqual(tuple(infos[0].channel), (2, 3))
        self.assertEqual(tuple(infos[1].channel), (3, 2))
        dep = infos[0].depinfo[0]
        self.assertEqual((dep.dep_layer, dep.dep_chan, dep.dst_chan, dep.elements),
                         (1, (1, 3), (1, 3), 12))
        self.assertTrue(dep.tensor_id.startswith("b:"))
        self.assertEqual(infos[1].depinfo, [oa.Depinfo(-1, (0, 0))])

    def test_fork_retains_both_dependencies(self):
        graph = two_convs()
        graph.graph.initializer.append(weight("w2", (1, 3, 1, 1)))
        graph.graph.node.append(helper.make_node("Conv", ["b", "w2"], ["z"]))
        graph.graph.output.append(tensor("z", [1, 1, 2, 2]))
        infos = oa.build_conv_info(graph)
        self.assertEqual([(d.dep_layer, d.dep_chan, d.dst_chan, d.elements) for d in infos[0].depinfo],
                         [(1, (1, 3), (1, 3), 12), (2, (1, 3), (1, 3), 12)])
        self.assertEqual(infos[0].depinfo[0].tensor_id, infos[0].depinfo[1].tensor_id)

    def test_unknown_operator_and_multi_input_arithmetic_are_rejected(self):
        for op, inputs in (("Sin", ["a"]), ("Add", ["a", "a"])):
            graph = model([helper.make_node("Conv", ["x", "w"], ["a"]),
                           helper.make_node(op, inputs, ["y"])], [weight("w", (2, 2, 1, 1))],
                          [tensor("y", [1, 2, 2, 2])])
            with self.subTest(op=op), self.assertRaises(ValueError):
                oa.build_conv_info(graph, strict_operators=True)

    def test_pooling_does_not_overwrite_partial_sum_shape(self):
        graph = model([helper.make_node("Conv", ["x", "w"], ["a"]),
                       helper.make_node("MaxPool", ["a"], ["y"], kernel_shape=[2, 2])],
                      [weight("w", (2, 2, 1, 1))], [tensor("y", [1, 2, 1, 1])])
        with self.assertRaises(ValueError):
            oa.build_conv_info(graph, strict_operators=True)

    def test_batch_dynamic_group_and_flatten_are_rejected(self):
        graph = two_convs()
        graph.graph.input[0].type.tensor_type.shape.dim[0].dim_value = 2
        with self.assertRaises((ValueError, onnx.shape_inference.InferenceError)):
            oa.build_conv_info(graph)
        grouped = model([helper.make_node("Conv", ["x", "w"], ["y"], group=2)],
                        [weight("w", (2, 1, 1, 1))], [tensor("y", [1, 2, 2, 2])])
        with self.assertRaises(ValueError):
            oa.build_conv_info(grouped)
        flat = model([helper.make_node("Conv", ["x", "w"], ["a"]),
                      helper.make_node("Flatten", ["a"], ["y"])],
                     [weight("w", (2, 2, 1, 1))], [tensor("y", [1, 8])])
        with self.assertRaises(ValueError):
            oa.build_conv_info(flat, strict_operators=True)


class IntegrationContracts(unittest.TestCase):
    def test_mnsim_passes_dimensions_not_characters(self):
        with patch.object(mi, "tile_latency_analysis") as factory:
            factory.return_value.tile_latency = 123
            self.assertEqual(mi.tile_latency_cal(str(paths.DEFAULT_SIM_CONFIG), 16, 8, 8), 123)
            self.assertEqual(factory.call_args.kwargs["read_row"], 256)
            self.assertEqual(factory.call_args.kwargs["read_column"], 256)
            self.assertEqual(factory.call_args.kwargs["PE_num"], 16)

    def test_latency_seconds_bytes_and_default_booksim_pair(self):
        mapped = NS(deploy_info=[NS(cnode=[NS(layer=0, ifmap_size=10)],
                                   layer_paths_map={0: [NS(via=[0, 1], datavolume=8)]})],
                    comm_segs=[NS(layers=[0])])
        with patch.object(mi, "booksim_eval", return_value=({0: 0.5}, {0: 2})), \
                patch.object(mi, "tile_latency_cal", return_value=20) as tile:
            latency, throughput, transfer, compute, _ = mi.latency_est(
                mapping_res=mapped, bus_width=2, freq=1e9, inputbit=16, outputbit=16)
        # 8 elements x 2 bytes / (2 bytes/cycle x 1GHz x 0.5) = 16ns.
        self.assertAlmostEqual(transfer[0], 16e-9)
        self.assertAlmostEqual(compute[0], 20e-9)
        self.assertAlmostEqual(latency, 36e-9)
        self.assertAlmostEqual(throughput, 1 / 36e-9)
        self.assertEqual(tile.call_args.args[1], 20)
        empty = mi.latency_est(mapping_res=NS(deploy_info=[], comm_segs=[]))
        self.assertEqual(empty[:2], (0.0, 0.0))

    def test_booksim_zero_data_skips_process(self):
        with patch.object(mi.subprocess, "run") as run:
            result = mi.booksim_eval([NS(layers=[4], datas=[[0]])], 1)
        self.assertEqual(result, ({4: 1.0}, {4: 0.0}))
        run.assert_not_called()

    def test_booksim_failures_never_become_zero_or_ideal(self):
        segment = NS(layers=[0], datas=[[0, 100, 0, 0], [0] * 4, [0] * 4, [0] * 4])
        for outcome in (subprocess.TimeoutExpired("booksim", 1),
                        NS(returncode=1, stdout="", stderr="failure"),
                        NS(returncode=0, stdout="missing metrics", stderr="")):
            kwargs = {"side_effect": outcome} if isinstance(outcome, Exception) else {"return_value": outcome}
            with self.subTest(outcome=outcome), patch.object(mi.subprocess, "run", **kwargs):
                with self.assertRaises(RuntimeError):
                    mi.booksim_eval([segment], 1)

    def test_booksim_preserves_topology_private_input_and_bus_width(self):
        matrix = [[0] * 9 for _ in range(9)]
        matrix[0][1] = 2000  # A 3x3 physical mesh must not shrink to 2x2.
        def execute(command, **kwargs):
            directory = Path(kwargs["cwd"])
            data = np.loadtxt(directory / "inj_rate.txt")
            self.assertEqual(data.shape, (9, 9))
            self.assertAlmostEqual(data[0, 1], 2000 / (2 * 1e9 / 100))
            config = Path(command[1]).read_text()
            self.assertIn("k = 3;", config)
            self.assertIn("channel_width = 16;", config)
            self.assertIn("traffic = custom;", config)
            return NS(returncode=0, stdout="Packet latency average = 2e1\nNetwork latency average = 1e1\n- Total Power: 2.5e-2", stderr="")
        with patch.object(mi.subprocess, "run", side_effect=execute):
            self.assertEqual(mi.booksim_eval([NS(layers=[0], datas=matrix)], 2), ({0: 0.5}, {0: 0.025}))

    def test_invalid_config_dimensions_and_nonfinite_values(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.ini"
            text = paths.DEFAULT_SIM_CONFIG.read_text()
            path.write_text(text.replace("Xbar_Size = 256,256", "Xbar_Size = 256").replace("Device_Area = 0.67", "Device_Area = nan"))
            errors = config_validator.validate_config(path)
            self.assertTrue(any("Xbar_Size" in error for error in errors))
        self.assertIsNotNone(config_validator._validate_value("nan", {"type": float, "range": (0, 10)}, "x", "s"))


class CacheAndRegressionContracts(unittest.TestCase):
    def test_external_weight_content_invalidates_model_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "toy.onnx"
            onnx.save_model(two_convs(), path, save_as_external_data=True,
                            all_tensors_to_one_file=True, location="weights.bin", size_threshold=0)
            before = analysis._model_hash(path)
            sidecar = path.parent / "weights.bin"
            content = bytearray(sidecar.read_bytes())
            content[0] ^= 1
            sidecar.write_bytes(content)
            self.assertNotEqual(before, analysis._model_hash(path))

    def test_noc_tasks_only_include_existing_model_strategy_pairs(self):
        mapped = {"a": {(0, 0): NS(comm_segs=[NS(layers=[0], datas=[[0]])])},
                  "b": {(1, 1): NS(comm_segs=[NS(layers=[0], datas=[[0]])])}}
        with patch.object(analysis, "booksim_eval", return_value=({0: 1.0}, {0: 0.0})) as run:
            factors, _ = analysis.get_noc_perf(mapped, 1, (256, 256), workers=1)
        self.assertEqual(set(factors), {("a", (0, 0)), ("b", (1, 1))})
        self.assertEqual(run.call_count, 2)

    def test_cache_identifies_input_config_strategy_and_payload(self):
        mapped = analysis.MappingResults({"toy.onnx": {(1, 1): NS(comm_segs=[NS(layers=[0], datas=[[0]])])}},
                                         provenance={"models": {"toy.onnx": "original"}, "hardware": {"xbar_num": 4}})
        with tempfile.TemporaryDirectory() as directory, patch.object(analysis, "_implementation_fingerprint", return_value={"impl": "one"}):
            config = Path(directory) / "config.ini"
            config.write_text("old")
            def key():
                return analysis._make_cache_key(list(mapped), 1, (256, 256), comm_segs=mapped, config_path=config)
            keys = [key()]
            mapped.provenance["models"]["toy.onnx"] = "changed weights"
            keys.append(key())
            mapped.provenance["hardware"]["xbar_num"] = 8
            keys.append(key())
            config.write_text("changed hardware")
            keys.append(key())
            mapped["toy.onnx"][(1, 1)].comm_segs[0].datas = [[2]]
            keys.append(key())
            with patch.object(analysis, "_implementation_fingerprint", return_value={"impl": "two"}):
                keys.append(key())
            self.assertEqual(len(keys), len(set(keys)))
        self.assertIsNone(analysis.load_noc_perf(1, (256, 256), models=["toy.onnx"]))

    def test_missing_reference_is_error_and_never_created(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.pkl"
            with patch.object(compare, "perf_analysis") as perf:
                result = compare.compare_perf("ignored", None, missing)
            self.assertEqual(result["status"], "MISSING_REFERENCE")
            perf.assert_not_called()
            self.assertFalse(missing.exists())
            result = compare.compare_ctest(Path(directory), missing)
            self.assertEqual(result["status"], "MISSING_REFERENCE")
            self.assertFalse(missing.exists())

    def test_two_failed_ctests_are_not_a_passing_regression(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "ctest.pkl"
            reference.write_bytes(pickle.dumps({"passed": False, "returncode": 1,
                                                "stdout": "0% tests passed, 1 tests failed out of 1"}))
            with patch.object(compare.subprocess, "run", return_value=NS(returncode=1, stdout="tests failed", stderr="")):
                result = compare.compare_ctest(Path(directory), reference)
            self.assertEqual(result["status"], "FAIL")

    def test_performance_reference_rejects_empty_or_invalid_payloads(self):
        invalid = ({}, [], {"toy": {}}, {"toy": {(1, 1): {}}},
                   {"toy": {(2, 0): {"path_num": 0, "datavolume": 0,
                                       "total_hops": 0, "total_congestion": 0}}})
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "perf.pkl"
            for payload in invalid:
                reference.write_bytes(pickle.dumps(payload))
                with self.subTest(payload=payload), patch.object(compare, "perf_analysis", return_value=({}, {})) as run:
                    result = compare.compare_perf("ignored", None, reference)
                    self.assertEqual(result["status"], "INVALID_REFERENCE")
                    run.assert_not_called()

    def test_performance_current_cannot_be_empty(self):
        valid = {"toy": {(1, 1): {"path_num": 0, "datavolume": 0,
                                  "total_hops": 0, "total_congestion": 0}}}
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "perf.pkl"
            reference.write_bytes(pickle.dumps(valid))
            with patch.object(compare, "perf_analysis", return_value=({}, {})):
                result = compare.compare_perf("ignored", None, reference)
            self.assertEqual(result["status"], "INVALID_CURRENT")

    def test_valid_performance_reference_shape_still_passes(self):
        valid = {"toy": {(1, 1): {"path_num": 0, "datavolume": 0,
                                  "total_hops": 0, "total_congestion": 0}}}
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "perf.pkl"
            reference.write_bytes(pickle.dumps(valid))
            with patch.object(compare, "perf_analysis", return_value=({}, valid)):
                result = compare.compare_perf("ignored", None, reference)
            self.assertEqual(result["status"], "PASS")

    def test_corrupt_reference_pickle_never_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "corrupt.pkl"
            reference.write_bytes(b"not a pickle")
            with patch.object(compare, "perf_analysis") as perf, patch.object(compare.subprocess, "run") as ctest:
                self.assertEqual(compare.compare_perf("ignored", None, reference)["status"], "INVALID_REFERENCE")
                self.assertEqual(compare.compare_ctest(Path(directory), reference)["status"], "INVALID_REFERENCE")
            perf.assert_not_called()
            ctest.assert_not_called()

    def test_ctest_requires_valid_nonempty_run_evidence(self):
        zero = "100% tests passed, 0 tests failed out of 0"
        valid = {"passed": True, "returncode": 0,
                 "stdout": "100% tests passed, 0 tests failed out of 5"}
        invalid = ([], {"passed": True}, dict(valid, stdout=zero), dict(valid, stdout=""))
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / "ctest.pkl"
            for payload in invalid:
                reference.write_bytes(pickle.dumps(payload))
                with self.subTest(payload=payload), patch.object(compare.subprocess, "run") as run:
                    self.assertEqual(compare.compare_ctest(Path(directory), reference)["status"], "INVALID_REFERENCE")
                    run.assert_not_called()
            reference.write_bytes(pickle.dumps(valid))
            with patch.object(compare.subprocess, "run", return_value=NS(returncode=0, stdout=zero, stderr="")):
                self.assertEqual(compare.compare_ctest(Path(directory), reference)["status"], "FAIL")
            with patch.object(compare.subprocess, "run", return_value=NS(returncode=0, stdout=valid["stdout"], stderr="")):
                result = compare.compare_ctest(Path(directory), reference)
            self.assertEqual(result["status"], "PASS")
            self.assertEqual(result["tests"], 5)

    def test_nan_is_not_equal_to_finite_reference(self):
        self.assertTrue(compare.deep_compare(float("nan"), 10.0))

    def test_model_selection_ignores_sidecars_and_propagates_mapping_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            onnx.save(two_convs(), Path(directory) / "toy.onnx")
            (Path(directory) / "README.txt").write_text("not a model")
            with patch.object(analysis, "analysis_model", side_effect=RuntimeError("mapping failed")) as run:
                with self.assertRaisesRegex(RuntimeError, "mapping failed"):
                    analysis.perf_analysis(directory)
            self.assertEqual(run.call_count, 1)


class RealMappingContracts(unittest.TestCase):
    def test_tiny_model_parses_and_maps_all_strategies(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "toy.onnx"
            onnx.save(two_convs(), path)
            mapped, communication = analysis.perf_analysis(directory, oa.make_hw_info((4, 4), 1, (2, 2), 1))
        self.assertEqual(set(mapped["toy.onnx"]), {(0, 0), (0, 1), (1, 0), (1, 1)})
        for opt, result in mapped["toy.onnx"].items():
            nodes = [n for tile in result.deploy_info for n in tile.cnode]
            self.assertEqual(set(n.layer for n in nodes), {0, 1})
            self.assertEqual(sum(n.ofmap_size for n in nodes if n.layer == 0), 12)
            self.assertEqual(sum(n.ofmap_size for n in nodes if n.layer == 1), 8)
            self.assertEqual(communication["toy.onnx"][opt]["datavolume"], 12)
            self.assertGreaterEqual(communication["toy.onnx"][opt]["path_num"], 1)
            self.assertEqual(sorted(layer for seg in result.comm_segs for layer in seg.layers), [0, 1])


class RegressionScriptContracts(unittest.TestCase):
    def run_script(self, cmake_code=0, compare_code=0):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            for name, code in (("cmake", cmake_code), ("python", compare_code)):
                script = directory / name
                script.write_text(f'#!/bin/sh\nprintf "%s\\n" "$*" >> "$REVIEW_SCRIPT_LOG"\nexit {code}\n')
                script.chmod(0o755)
            env = dict(os.environ, CMAKE_BIN=str(directory / "cmake"),
                       PYTHON=str(directory / "python"), BUILD_DIR=str(directory / "build"),
                       REVIEW_SCRIPT_LOG=str(directory / "log"))
            result = subprocess.run(["bash", str(ROOT / "scripts/regression/regression_test.sh"),
                                     "--ctest-only"], env=env, cwd=ROOT, capture_output=True)
            return result.returncode, (directory / "log").read_text()

    def test_failed_comparison_propagates_and_ctest_is_not_duplicated(self):
        code, log = self.run_script(compare_code=9)
        self.assertEqual(code, 9)
        self.assertEqual(sum("compare.py" in line for line in log.splitlines()), 1)
        self.assertIn("--ctest-only", log)
        self.assertNotIn("/ctest ", log)

    def test_failed_build_stops_comparison(self):
        code, log = self.run_script(cmake_code=7)
        self.assertEqual(code, 7)
        self.assertNotIn("compare.py", log)


if __name__ == "__main__":
    unittest.main(verbosity=2)
