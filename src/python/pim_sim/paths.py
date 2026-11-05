"""Repository resources and runtime artifacts, independent of the launch directory.

Only defaults are repository-relative. Explicit CLI/file arguments remain relative
 to the caller's working directory. PIM_ARTIFACTS_DIR may override the output root;
 a relative override is also resolved against the caller's working directory.
Importing this module does not create directories or import the native extension.
"""
from pathlib import Path
import os
import sys

REPO_ROOT = Path(__file__).resolve().parents[3]
PYTHON_SOURCE_DIR = REPO_ROOT / "src" / "python"
THIRD_PARTY_DIR = REPO_ROOT / "third_party"
EXAMPLES_DIR = REPO_ROOT / "examples"
INPUTS_DIR = REPO_ROOT / "experiments" / "inputs"
DEMO_MODELS_DIR = INPUTS_DIR / "demo"
MODELS_DIR = INPUTS_DIR / "models"
XBAR_MODELS_DIR = INPUTS_DIR / "xbars"
DEFAULT_SIM_CONFIG = REPO_ROOT / "configs" / "hardware" / "SimConfig.ini"
BOOKSIM_CONFIG = REPO_ROOT / "configs" / "booksim" / "mesh.cfg"
BOOKSIM_TECHFILE = REPO_ROOT / "configs" / "booksim" / "techfile.txt"
BOOKSIM_BINARY = THIRD_PARTY_DIR / "booksim" / "bin" / "booksim"
WORKLOAD_TIMING = REPO_ROOT / "configs" / "workloads" / "tiny.json"


def artifact_dir(kind):
    if kind not in {"cache", "logs", "results", "runs"}:
        raise ValueError(f"Unknown artifact category: {kind}")
    override = os.environ.get("PIM_ARTIFACTS_DIR")
    root = Path(override).expanduser().resolve() if override else REPO_ROOT / "artifacts"
    return root / kind


def artifact_path(kind, *parts):
    return artifact_dir(kind).joinpath(*parts)


def prepare_output(path):
    """Create an output's parent on write, preserving caller-relative paths."""
    path = Path(path).expanduser()
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def ensure_third_party_imports():
    """Expose the vendored MNSIM package without changing its public imports."""
    directory = str(THIRD_PARTY_DIR)
    if directory not in sys.path:
        sys.path.insert(0, directory)


def configure_matplotlib_cache():
    os.environ.setdefault("MPLCONFIGDIR", str(artifact_path("cache", "matplotlib")))
