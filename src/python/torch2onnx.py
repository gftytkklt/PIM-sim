"""Compatibility import; implementation lives in pim_sim.torch2onnx."""
import importlib
import sys

_module = importlib.import_module("pim_sim.torch2onnx")
if __name__ == "__main__":
    raise SystemExit(_module.main())
else:
    sys.modules[__name__] = _module
