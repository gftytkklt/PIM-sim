"""Compatibility import; implementation lives in pim_sim.plotting."""
import importlib
import sys

_module = importlib.import_module("pim_sim.plotting")
sys.modules[__name__] = _module
