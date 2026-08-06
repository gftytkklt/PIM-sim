"""SimConfig.ini schema validator.

Provides validation and friendly error messages for the hardware
configuration file used by PIM-sim.

Usage:
    from config_validator import validate_config
    errors = validate_config("SimConfig.ini")
    if errors:
        for e in errors:
            print(f"ERROR: {e}")
"""

import configparser
import os
from typing import List, Dict, Any, Optional, Union


# Schema: section -> { key: (type, required, default, validator) }
# validator is None or a callable(value) -> Optional[str] (error message)
SCHEMA: Dict[str, Dict[str, Dict[str, Any]]] = {
    "Device level": {
        "Device_Tech":       {"type": int,   "required": True,  "range": (1, 1000)},
        "Device_Type":       {"type": str,   "required": True,  "options": ["NVM", "SRAM"]},
        "Device_Area":       {"type": float, "required": True,  "range": (0.01, 1000)},
        "Read_Level":        {"type": int,   "required": True,  "range": (1, 32)},
        "Read_Voltage":      {"type": str,   "required": True},
        "Write_Level":       {"type": int,   "required": True,  "range": (1, 32)},
        "Write_Voltage":     {"type": str,   "required": True},
        "Read_Latency":      {"type": float, "required": True,  "range": (0.01, 1e6)},
        "Write_Latency":     {"type": float, "required": True,  "range": (0.01, 1e6)},
        "Device_Level":      {"type": int,   "required": True},
        "Device_Resistance": {"type": str,   "required": True},
        "Read_Energy":       {"type": float, "required": False},
        "Write_Energy":      {"type": float, "required": False},
    },
    "Crossbar level": {
        "Xbar_Size":         {"type": str,   "required": True},
        "Subarray_Size":     {"type": int,   "required": True,  "range": (1, 10000)},
        "Cell_Type":         {"type": str,   "required": True},
    },
    "Interface level": {
        "ADC_Precision":     {"type": int,   "required": True,  "range": (1, 32)},
        "DAC_Precision":     {"type": int,   "required": True,  "range": (1, 32)},
    },
    "Process element level": {
        "PIM_Type":          {"type": int,   "required": True,  "range": (0, 1)},
        "Xbar_Polarity":     {"type": int,   "required": True,  "range": (1, 2)},
        "Group_Num":         {"type": int,   "required": True,  "range": (1, 256)},
        "DAC_Num":           {"type": int,   "required": True,  "range": (0, 4096)},
        "ADC_Num":           {"type": int,   "required": True,  "range": (0, 4096)},
    },
    "Tile level": {
        "PE_Num":            {"type": str,   "required": True},
        "Inter_Tile_Bandwidth":  {"type": int, "required": True, "range": (0, 10000)},
        "Intra_Tile_Bandwidth":  {"type": int, "required": True, "range": (0, 100000)},
    },
    "Architecture level": {
        "Tile_Num":          {"type": str,   "required": True},
        "NoC_enable":        {"type": int,   "required": False, "range": (0, 1)},
    },
    "Algorithm Configuration": {
        "Weight_Polarity":   {"type": int,   "required": False, "range": (1, 2)},
        "Simulation_Level":  {"type": int,   "required": False, "range": (0, 1)},
        "NoC_enable":        {"type": int,   "required": False, "range": (0, 1)},
    },
}


def _validate_value(value_str: str, spec: Dict[str, Any], key: str, section: str) -> Optional[str]:
    """Validate a single config value against its schema."""
    expected_type = spec["type"]

    if expected_type == int:
        try:
            val = int(value_str)
        except ValueError:
            return f"[{section}] {key}: expected integer, got '{value_str}'"
        if "range" in spec:
            lo, hi = spec["range"]
            if val < lo or val > hi:
                return f"[{section}] {key}: value {val} out of range [{lo}, {hi}]"
        return None

    elif expected_type == float:
        try:
            val = float(value_str)
        except ValueError:
            return f"[{section}] {key}: expected float, got '{value_str}'"
        if "range" in spec:
            lo, hi = spec["range"]
            if val < lo or val > hi:
                return f"[{section}] {key}: value {val} out of range [{lo}, {hi}]"
        return None

    elif expected_type == str:
        if "options" in spec and value_str not in spec["options"]:
            opts = ", ".join(spec["options"])
            return f"[{section}] {key}: '{value_str}' not in allowed options: [{opts}]"
        return None

    return None


def validate_config(config_path: str = "SimConfig.ini") -> List[str]:
    """Validate a SimConfig.ini file against the schema.

    Args:
        config_path: Path to the SimConfig.ini file.

    Returns:
        List of error messages. Empty list means valid.
    """
    if not os.path.exists(config_path):
        return [f"Config file not found: {config_path}"]

    config = configparser.ConfigParser()
    try:
        config.read(config_path)
    except configparser.Error as e:
        return [f"Failed to parse {config_path}: {e}"]

    errors = []

    for section, keys in SCHEMA.items():
        if not config.has_section(section):
            errors.append(f"Missing section: [{section}]")
            continue

        for key, spec in keys.items():
            if spec.get("required", False) and not config.has_option(section, key):
                errors.append(f"[{section}] Missing required key: {key}")
                continue

            if config.has_option(section, key):
                value = config.get(section, key)
                err = _validate_value(value, spec, key, section)
                if err:
                    errors.append(err)

    return errors


if __name__ == "__main__":
    import sys
    path = sys.argv[1] if len(sys.argv) > 1 else "SimConfig.ini"
    errs = validate_config(path)
    if errs:
        print(f"Found {len(errs)} error(s):")
        for e in errs:
            print(f"  {e}")
        sys.exit(1)
    else:
        print("SimConfig.ini is valid.")
        sys.exit(0)