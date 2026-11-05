"""Check launchers and trace isolation without rebuilding or touching research data."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"


def fake_tool(directory, name, status):
    path = directory / name
    path.write_text(
        f"#!{sys.executable}\n"
        "import json, os, sys\n"
        "with open(os.environ['SCRIPT_LOG'], 'a') as handle:\n"
        "    handle.write(json.dumps({'name': os.path.basename(sys.argv[0]), 'args': sys.argv[1:], "
        "'cwd': os.getcwd(), 'pythonpath': os.environ.get('PYTHONPATH'), "
        "'pycache': os.environ.get('PYTHONPYCACHEPREFIX'), "
        "'artifacts': os.environ.get('PIM_ARTIFACTS_DIR')}) + '\\n')\n"
        f"sys.exit({status})\n")
    path.chmod(0o755)
    return path


class BuildScripts(unittest.TestCase):
    def run_script(self, script, args=(), cmake_exit=0, ctest_exit=0, python_exit=0,
                   relative_build=False):
        with tempfile.TemporaryDirectory(prefix="pim script test ") as tmp:
            tmp = Path(tmp)
            tools = {name: fake_tool(tmp, name, status) for name, status in
                     (("cmake", cmake_exit), ("ctest", ctest_exit), ("python", python_exit))}
            caller = tmp / "unrelated caller"
            caller.mkdir()
            env = dict(os.environ, CMAKE_BIN=str(tools["cmake"]), CTEST_BIN=str(tools["ctest"]),
                       PYTHON=str(tools["python"]), SCRIPT_LOG=str(tmp / "log"),
                       BUILD_DIR="build/with spaces" if relative_build else str(tmp / "build dir"),
                       PYTHONPATH=str(tmp / "untrusted path"), PYTHONDONTWRITEBYTECODE="1")
            env.pop("PYTHONPYCACHEPREFIX", None)
            env.pop("PIM_ARTIFACTS_DIR", None)
            result = subprocess.run([str(SCRIPTS / script), *args], env=env, cwd=caller,
                                    capture_output=True, text=True)
            log = [json.loads(line) for line in (tmp / "log").read_text().splitlines()] if (tmp / "log").exists() else []
            return result.returncode, log, str(caller), result.stderr

    def test_default_executes_and_never_lists_only(self):
        code, log, _, error = self.run_script("test.sh")
        self.assertEqual(code, 0, error)
        self.assertEqual([entry["name"] for entry in log], ["cmake", "cmake", "ctest"])
        self.assertIn("--output-on-failure", log[-1]["args"])
        self.assertIn("--no-tests=error", log[-1]["args"])
        self.assertNotIn("-N", log[-1]["args"])
        self.assertNotIn("rm ", (SCRIPTS / "build.sh").read_text())
        self.assertEqual(log[0]["args"][1], str(ROOT))
        self.assertTrue(all(entry["cwd"] == str(ROOT) for entry in log))

    def test_configure_failure_stops_before_build_and_ctest(self):
        code, log, _, _ = self.run_script("test.sh", cmake_exit=7)
        self.assertEqual(code, 7)
        self.assertEqual(len(log), 1)

    def test_test_failure_propagates_and_name_is_exact(self):
        code, log, _, _ = self.run_script("test.sh", ["process_test"], ctest_exit=8)
        self.assertEqual(code, 8)
        self.assertEqual(log[-1]["args"][-2:], ["-R", "^process_test$"])

    def test_regex_input_is_rejected_before_building(self):
        code, log, _, _ = self.run_script("test.sh", [".*"])
        self.assertEqual(code, 2)
        self.assertEqual(log, [])

    def test_relative_build_directory_is_relative_to_repository(self):
        code, log, _, error = self.run_script("build.sh", relative_build=True)
        self.assertEqual(code, 0, error)
        self.assertEqual(log[0]["args"][3], str(ROOT / "build" / "with spaces"))
        self.assertEqual(log[1]["args"][1], str(ROOT / "build" / "with spaces"))

    def test_python_launcher_keeps_caller_cwd_and_selects_only_declared_sources(self):
        code, log, caller, error = self.run_script("python.sh", ["-c", "pass"], relative_build=True)
        self.assertEqual(code, 0, error)
        self.assertEqual(log[0]["cwd"], caller)
        self.assertEqual(log[0]["args"], ["-P", "-c", "pass"])
        self.assertEqual(log[0]["pythonpath"].split(os.pathsep), [
            str(ROOT / "build" / "with spaces" / "python"),
            str(ROOT / "src" / "python"), str(ROOT / "third_party")])
        self.assertEqual(log[0]["pycache"], str(ROOT / "build" / "with spaces" / "pycache"))
        self.assertEqual(log[0]["artifacts"], str(ROOT / "artifacts"))

    def test_python_failure_propagates(self):
        code, _, _, _ = self.run_script("python.sh", ["-c", "raise RuntimeError()"], python_exit=17)
        self.assertEqual(code, 17)

    def test_default_interpreter_prefers_repository_virtualenv(self):
        with tempfile.TemporaryDirectory() as directory:
            env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
            env.pop("PYTHON", None)
            probe = "import json,sys; print(json.dumps({'executable':sys.executable,'prefix':sys.prefix}))"
            result = subprocess.run([str(SCRIPTS / "python.sh"), "-c", probe], cwd=directory,
                                    env=env, capture_output=True, text=True, check=True)
            data = json.loads(result.stdout)
            if (ROOT / ".venv" / "bin" / "python3").is_file():
                self.assertEqual(Path(data["prefix"]).resolve(), (ROOT / ".venv").resolve())
            else:
                self.assertTrue(Path(data["executable"]).is_file())

    def test_regression_build_failure_stops_before_comparison(self):
        code, log, _, _ = self.run_script("regression/regression_test.sh", ["--ctest-only"], cmake_exit=7)
        self.assertEqual(code, 7)
        self.assertEqual([entry["name"] for entry in log], ["cmake"])

    def test_regression_runs_comparison_once_and_preserves_failure(self):
        code, log, _, _ = self.run_script("regression/regression_test.sh", ["--ctest-only"], python_exit=9)
        self.assertEqual(code, 9)
        self.assertEqual([entry["name"] for entry in log], ["cmake", "cmake", "python"])
        self.assertEqual(log[-1]["args"].count(str(SCRIPTS / "regression" / "compare.py")), 1)
        self.assertIn("--ctest-only", log[-1]["args"])


class TraceIsolation(unittest.TestCase):
    def test_trace_capture_uses_new_executable_layout_and_private_artifact_root(self):
        spec = importlib.util.spec_from_file_location("trace_verification", SCRIPTS / "regression" / "verify_simulator_traces.py")
        traces = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(traces)
        with tempfile.TemporaryDirectory(prefix="pim trace layout ") as directory:
            build = Path(directory) / "build"
            executable = build / "tests" / "cpp" / "oputiletest"
            executable.parent.mkdir(parents=True)
            executable.write_text(
                f"#!{sys.executable}\n"
                "import os\nfrom pathlib import Path\n"
                "root=Path(os.environ['PIM_ARTIFACTS_DIR'])\n"
                "assert root == Path.cwd()\n"
                "(root/'runs').mkdir(parents=True)\n"
                "(root/'runs'/'completed_events.csv').write_text('event0\\nevent1\\n')\n"
                "print('Simulation stopped: Completed')\nprint('Evaluated cycles: 4')\n")
            executable.chmod(0o755)
            before = sorted(str(p.relative_to(build)) for p in build.rglob("*"))
            result = traces.capture(build, "oputiletest")
            self.assertEqual(result["cycles"], 4)
            self.assertEqual(result["events"], 2)
            self.assertEqual(before, sorted(str(p.relative_to(build)) for p in build.rglob("*")))


if __name__ == "__main__":
    unittest.main()
