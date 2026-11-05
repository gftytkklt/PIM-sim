"""Compatibility import; implementation lives in pim_sim.analysis."""
import importlib
import sys

_module = importlib.import_module("pim_sim.analysis")
sys.modules[__name__] = _module
