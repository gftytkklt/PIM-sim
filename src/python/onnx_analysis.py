"""Compatibility import; implementation lives in pim_sim.onnx_analysis."""
import importlib
import sys

_module = importlib.import_module("pim_sim.onnx_analysis")
if __name__ == "__main__":
    raise SystemExit(_module.main())
else:
    sys.modules[__name__] = _module
