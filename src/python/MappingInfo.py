"""Compatibility import; implementation lives in pim_sim.mapping_info."""
import importlib
import sys

_module = importlib.import_module("pim_sim.mapping_info")
sys.modules[__name__] = _module
