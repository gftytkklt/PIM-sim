"""Compatibility import; implementation lives in pim_sim.perf."""
import importlib
import sys

_module = importlib.import_module("pim_sim.perf")
if __name__ == "__main__":
    raise SystemExit(_module.main())
else:
    sys.modules[__name__] = _module
