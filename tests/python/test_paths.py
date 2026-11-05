"""Repository defaults, legacy imports and outputs work from arbitrary directories."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper
from pim_sim import paths


class LayoutPaths(unittest.TestCase):
    def environment(self, artifacts=None):
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
        env.pop("PIM_ARTIFACTS_DIR", None)
        env.pop("MPLCONFIGDIR", None)
        if artifacts is not None:
            env["PIM_ARTIFACTS_DIR"] = str(artifacts)
        return env

    def command(self, arguments, cwd, env):
        completed = subprocess.run([sys.executable, "-P", *arguments], cwd=cwd, env=env,
                                   capture_output=True, text=True, timeout=60)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        return completed

    def create_model(self, directory):
        directory.mkdir()
        graph = helper.make_graph(
            [helper.make_node("Conv", ["x", "w0"], ["mid"]),
             helper.make_node("Conv", ["mid", "w1"], ["y"])], "path-test",
            [helper.make_tensor_value_info("x", TensorProto.FLOAT, [1, 1, 1, 1])],
            [helper.make_tensor_value_info("y", TensorProto.FLOAT, [1, 1, 1, 1])],
            [numpy_helper.from_array(np.ones((1, 1, 1, 1), dtype=np.float32), name)
             for name in ("w0", "w1")])
        onnx.save(helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)]), directory / "tiny.onnx")

    def test_defaults_are_absolute_and_import_has_no_output_side_effects(self):
        code = """
import json
from pim_sim import paths
from pim_sim.config_validator import validate_config
assert not validate_config()
print(json.dumps({'repo': str(paths.REPO_ROOT), 'config': str(paths.DEFAULT_SIM_CONFIG),
                  'models': str(paths.DEMO_MODELS_DIR),
                  'cache': str(paths.artifact_dir('cache'))}))
"""
        with tempfile.TemporaryDirectory() as temporary:
            result = self.command(["-c", code], temporary, self.environment())
            self.assertEqual(list(Path(temporary).iterdir()), [])
        values = json.loads(result.stdout)
        self.assertEqual(Path(values["repo"]), paths.REPO_ROOT)
        self.assertEqual(Path(values["config"]), paths.REPO_ROOT / "configs/hardware/SimConfig.ini")
        self.assertEqual(Path(values["models"]), paths.REPO_ROOT / "experiments/inputs/demo")
        self.assertEqual(Path(values["cache"]), paths.REPO_ROOT / "artifacts/cache")

    def test_relative_artifact_override_is_relative_to_caller(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = self.command(["-c", "from pim_sim.paths import artifact_dir; print(artifact_dir('logs'))"],
                                  temporary, self.environment("relative-artifacts"))
            self.assertEqual(Path(result.stdout.strip()), Path(temporary) / "relative-artifacts/logs")
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_legacy_imports_share_the_package_module(self):
        code = """
import analysis, MappingInfo, onnx_analysis, config_validator, perf, torch2onnx
from pim_sim import analysis as a, mapping_info as m, onnx_analysis as o
from pim_sim import config_validator as c, perf as p, torch2onnx as t
assert (analysis, MappingInfo, onnx_analysis, config_validator, perf, torch2onnx) == (a,m,o,c,p,t)
assert 'torch' not in __import__('sys').modules
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cwd = root / "launch"
            cwd.mkdir()
            self.command(["-c", code], cwd, self.environment(root / "artifacts"))
            self.assertEqual(list(cwd.iterdir()), [])
            self.assertFalse(any(root.rglob("*.onnx")))

    def test_real_mapping_defaults_write_only_to_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cwd = root / "launch"
            cwd.mkdir()
            self.create_model(root / "inputs")
            artifacts = root / "artifacts"
            self.command(["-m", "pim_sim", "--models-dir", "../inputs", "--mapping-only",
                          "--single-strategy", "--xbars-per-tile", "1", "--mesh", "1,2"],
                         cwd, self.environment(artifacts))
            reports = list((artifacts / "runs").glob("analysis_*.json"))
            self.assertEqual(len(reports), 1)
            report = json.loads(reports[0].read_text())
            self.assertEqual(report["results"][0]["communication"]["datavolume"], 1)
            self.assertTrue((artifacts / "logs/perf.log").is_file())
            self.assertTrue((artifacts / "logs/cpp_analysis.log").is_file())
            self.assertEqual(list(cwd.iterdir()), [])

    def test_explicit_relative_config_and_output_keep_caller_semantics(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cwd = root / "launch"
            cwd.mkdir()
            self.create_model(root / "inputs")
            (cwd / "local.ini").write_text(paths.DEFAULT_SIM_CONFIG.read_text())
            self.command(["-m", "pim_sim.perf", "--models-dir", "../inputs", "--config", "local.ini",
                          "--output", "report.json", "--mapping-only", "--single-strategy",
                          "--xbars-per-tile", "1", "--mesh", "1,2"],
                         cwd, self.environment(root / "artifacts"))
            self.assertEqual({p.name for p in cwd.iterdir()}, {"local.ini", "report.json"})
            self.assertEqual(json.loads((cwd / "report.json").read_text())["results"][0]["model"], "tiny.onnx")


if __name__ == "__main__":
    unittest.main(verbosity=2)
