"""Compatibility import; implementation lives in pim_sim.logger."""
import importlib
import sys

_module = importlib.import_module("pim_sim.logger")
sys.modules[__name__] = _module
