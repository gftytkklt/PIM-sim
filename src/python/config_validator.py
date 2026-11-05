"""Compatibility import; implementation lives in pim_sim.config_validator."""
import importlib
import sys

_module = importlib.import_module("pim_sim.config_validator")
if __name__ == "__main__":
    raise SystemExit(_module.main())
else:
    sys.modules[__name__] = _module
