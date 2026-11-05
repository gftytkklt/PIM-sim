"""Hand-computable checks for non-MVM dependency projection."""
import json
import tempfile
from pathlib import Path
import unittest

import numpy as np
import onnx
import pimapping
from onnx import TensorProto, helper, numpy_helper

from pim_sim import onnx_analysis as oa
from pim_sim.paths import WORKLOAD_TIMING


def tensor(name, shape):
    return helper.make_tensor_value_info(name, TensorProto.FLOAT, shape)


def weight(name, shape):
    return numpy_helper.from_array(np.ones(shape, dtype=np.float32), name)


def model(nodes, weights, output_shape, input_shape=(1, 2, 2, 2)):
    return helper.make_model(helper.make_graph(nodes, "projection", [tensor("x", input_shape)],
                                               [tensor("y", output_shape)], weights),
                             opset_imports=[helper.make_opsetid("", 13)])


def conv(source, weight_name, target):
    return helper.make_node("Conv", [source, weight_name], [target])


class ProjectionContracts(unittest.TestCase):
    def test_pool_keeps_raw_mvm_shape_and_transports_pooled_elements(self):
        graph = model([conv("x", "wa", "a"),
                       helper.make_node("MaxPool", ["a"], ["p"], kernel_shape=[2, 2]),
                       conv("p", "wb", "y")],
                      [weight("wa", (2, 2, 1, 1)), weight("wb", (1, 2, 1, 1))], [1, 1, 1, 1])
        info = oa.build_conv_info(graph)
        self.assertEqual(info[0].ofmap_size, (2, 2))
        self.assertEqual(info[1].ifmap_size, (1, 1))
        self.assertEqual(info[0].depinfo[0].elements, 2)
        self.assertEqual(info[0].depinfo[0].dst_chan, (1, 2))

    def test_flatten_maps_source_channels_to_all_gemm_features(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Flatten", ["a"], ["f"]),
                       helper.make_node("Gemm", ["f", "wb"], ["y"], transB=1)],
                      [weight("wa", (2, 2, 1, 1)), weight("wb", (3, 8))], [1, 3])
        info = oa.build_conv_info(graph)
        dep = info[0].depinfo[0]
        self.assertEqual((dep.dep_chan, dep.dst_chan, dep.elements), ((1, 2), (1, 8), 8))
        self.assertEqual(info[0].ofmap_size, (2, 2))
        self.assertEqual(info[1].channel, (8, 3))

    def test_concat_offsets_and_flatten_preserve_all_sources(self):
        graph = model([conv("x", "wa", "a"), conv("x", "wb", "b"),
                       helper.make_node("Concat", ["a", "b"], ["c"], axis=1),
                       helper.make_node("Flatten", ["c"], ["f"]),
                       helper.make_node("Gemm", ["f", "wc"], ["y"], transB=1)],
                      [weight("wa", (2, 2, 1, 1)), weight("wb", (3, 2, 1, 1)),
                       weight("wc", (1, 20))], [1, 1])
        info = oa.build_conv_info(graph)
        self.assertEqual((info[0].depinfo[0].dst_chan, info[0].depinfo[0].elements), ((1, 8), 8))
        self.assertEqual((info[1].depinfo[0].dst_chan, info[1].depinfo[0].elements), ((9, 20), 12))
        self.assertEqual(info[2].channel, (20, 1))

    def test_residual_add_keeps_both_producers(self):
        graph = model([conv("x", "wa", "a"), conv("x", "wb", "b"),
                       helper.make_node("Add", ["a", "b"], ["sum"]), conv("sum", "wc", "y")],
                      [weight(n, (2, 2, 1, 1)) for n in ("wa", "wb", "wc")], [1, 2, 2, 2])
        info = oa.build_conv_info(graph)
        self.assertEqual([(i.depinfo[0].dep_layer, i.depinfo[0].elements) for i in info[:2]], [(2, 8), (2, 8)])

    def test_silu_does_not_duplicate_the_same_mvm_ancestor(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Sigmoid", ["a"], ["s"]),
                       helper.make_node("Mul", ["a", "s"], ["m"]), conv("m", "wb", "y")],
                      [weight(n, (2, 2, 1, 1)) for n in ("wa", "wb")], [1, 2, 2, 2])
        dep = oa.build_conv_info(graph)[0].depinfo
        self.assertEqual(len(dep), 1)
        self.assertEqual(dep[0].elements, 8)

    def test_resize_counts_output_elements(self):
        scales = numpy_helper.from_array(np.array([1, 1, 2, 2], dtype=np.float32), "scales")
        graph = model([conv("x", "wa", "a"),
                       helper.make_node("Resize", ["a", "", "scales"], ["r"], mode="nearest"),
                       conv("r", "wb", "y")],
                      [weight(n, (2, 2, 1, 1)) for n in ("wa", "wb")] + [scales], [1, 2, 4, 4])
        info = oa.build_conv_info(graph)
        self.assertEqual(info[0].ofmap_size, (2, 2))
        self.assertEqual(info[0].depinfo[0].elements, 32)
        self.assertEqual(info[1].ifmap_size, (4, 4))

    def test_views_are_distinct_but_shared_payload_reuses_identity(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Relu", ["a"], ["r"]),
                       conv("a", "wb", "b"), conv("r", "wc", "c"), conv("r", "wd", "y")],
                      [weight(n, (2, 2, 1, 1)) for n in ("wa", "wb", "wc", "wd")], [1, 2, 2, 2])
        deps = oa.build_conv_info(graph)[0].depinfo
        self.assertNotEqual(deps[0].tensor_id, deps[1].tensor_id)
        self.assertEqual(deps[1].tensor_id, deps[2].tensor_id)

    def test_unary_after_concat_keeps_different_value_views_distinct(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Sigmoid", ["a"], ["s"]),
                       helper.make_node("Concat", ["a", "s"], ["c"], axis=1),
                       helper.make_node("Relu", ["c"], ["post"]), conv("post", "wb", "y")],
                      [weight("wa", (2, 1, 1, 1)), weight("wb", (1, 4, 1, 1))],
                      [1, 1, 1, 1], input_shape=(1, 1, 1, 1))
        info = oa.build_conv_info(graph)
        deps = info[0].depinfo
        self.assertEqual([dep.dst_chan for dep in deps], [(1, 2), (3, 4)])
        self.assertNotEqual(deps[0].tensor_id, deps[1].tensor_id)
        _, communication = oa.analysis_model(oa.convert_to_cpp(info),
                                              oa.make_hw_info((2, 2), 2, (1, 2), 1),
                                              oa.make_opt_info(False, False))
        # Both destination input blocks share a tile. Their values differ, so
        # TGraph's multicast union must retain 2 + 2 rather than collapse to 2.
        self.assertEqual(communication.datavolume, 4)

    def test_legal_unknown_operator_keeps_dependency_and_reports_fallback(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Sin", ["a"], ["s"]),
                       conv("s", "wb", "y")],
                      [weight(n, (2, 2, 1, 1)) for n in ("wa", "wb")], [1, 2, 2, 2])
        report = {}
        info = oa.build_conv_info(graph, projection_report=report)
        self.assertEqual((info[0].depinfo[0].dep_layer, info[0].depinfo[0].elements), (1, 8))
        self.assertEqual(report["skipped_ops"], {"Sin": 1})
        self.assertEqual(report["fallbacks"][0]["op_type"], "Sin")
        json.dumps(report)
        with self.assertRaises(oa.ModelParsingError):
            oa.build_conv_info(graph, strict_operators=True)

    def test_terminal_decoding_does_not_replace_raw_conv_shape(self):
        dims = numpy_helper.from_array(np.array([1, 4, 6], dtype=np.int64), "dims")
        graph = model([conv("x", "wa", "a"),
                       helper.make_node("Transpose", ["a"], ["t"], perm=[0, 2, 3, 1]),
                       helper.make_node("Reshape", ["t", "dims"], ["y"])],
                      [weight("wa", (6, 2, 1, 1)), dims], [1, 4, 6])
        report = {}
        info = oa.build_conv_info(graph, projection_report=report)
        self.assertEqual((info[0].channel, info[0].ofmap_size), ((2, 6), (2, 2)))
        self.assertEqual(info[0].depinfo, [oa.Depinfo(-1, (0, 0))])
        self.assertTrue(any(item["op_type"] == "Transpose" for item in report["fallbacks"]))

    def test_invalid_onnx_attributes_are_still_rejected(self):
        graph = model([helper.make_node("Conv", ["x", "wa"], ["y"], made_up_attribute=1)],
                      [weight("wa", (2, 2, 1, 1))], [1, 2, 2, 2])
        with self.assertRaises(onnx.checker.ValidationError):
            oa.build_conv_info(graph)

    def test_public_load_kernel_preserves_extended_dependency_fields(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Flatten", ["a"], ["f"]),
                       helper.make_node("Gemm", ["f", "wb"], ["y"], transB=1)],
                      [weight("wa", (2, 2, 1, 1)), weight("wb", (3, 8))], [1, 3])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "flatten.onnx"
            onnx.save(graph, path)
            report = {}
            kernels = oa.load_kernel(path, projection_report=report)
        dep = kernels[0].depinfo[0]
        self.assertEqual((dep.dep_chan, dep.dst_chan, dep.elements), ((1, 2), (1, 8), 8))
        self.assertEqual(report["mvm_count"], 2)
        json.dumps(report)

    def test_flatten_split_stream_conserves_elements_through_cycle_engine(self):
        graph = model([conv("x", "wa", "a"), helper.make_node("Flatten", ["a"], ["f"]),
                       helper.make_node("Gemm", ["f", "wb"], ["y"], transB=1)],
                      [weight("wa", (2, 2, 1, 1)), weight("wb", (3, 8))], [1, 3])
        kernels = oa.convert_to_cpp(oa.build_conv_info(graph))
        workload = pimapping.export_workload(kernels, oa.make_hw_info((3, 2), 1, (4, 4), 1),
                                              oa.make_opt_info(False, False), WORKLOAD_TIMING.read_text())
        data = json.loads(workload)
        propagation = [dep for dep in data["dependencies"] if dep["kind"] == "propagation"]
        # Eight flattened elements are distributed 3+3+2 over input blocks,
        # once for each of the two output-channel blocks (channels 1-2 and 3).
        self.assertEqual(sorted(dep["elements"] for dep in propagation), [2, 2, 3, 3, 3, 3])
        self.assertEqual(sorted(tuple(dep["projection"]["stream_range"]) for dep in propagation),
                         [(0, 3), (0, 3), (3, 6), (3, 6), (6, 8), (6, 8)])
        self.assertEqual(sum(dep["elements"] for dep in data["dependencies"]), 16 + 6)
        result = json.loads(pimapping.simulate_workload(workload, 1000))
        self.assertTrue(result["completed"])
        self.assertEqual(result["stop_reason"], "Completed")
        self.assertEqual(result["completed_tasks"], 7)
        self.assertEqual(result["sent_bytes"], 22)
        self.assertEqual(result["received_bytes"], 22)
        self.assertEqual(result, json.loads(pimapping.simulate_workload(workload, 1000)))


if __name__ == "__main__":
    unittest.main()
