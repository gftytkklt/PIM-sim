"""PIM-sim performance analysis module."""

import multiprocessing as mp
import os
import sys
import csv
from pathlib import Path
import logging
import pickle
import time
import hashlib
import json
import tempfile
from types import SimpleNamespace
import pimapping
import onnx
import numpy as np
from .mapping_info import latency_est, booksim_eval
from .onnx_analysis import analysis_model, make_opt_info, load_kernel, make_hw_info
from .logger import Logger
from . import paths


def get_opt_str(opt_info, ideal=0):
    if opt_info == (1, 1):
        return "This Paper" if not ideal else "Ideal"
    elif opt_info == (1, 0):
        return "SPATEM"
    elif opt_info == (0, 1):
        return "HITM"
    else:
        return "MNSIM"


class MappingResults(dict):
    """Mapping dictionary plus input provenance used to validate derived caches."""

    def __init__(self, *args, provenance=None, **kwargs):
        super().__init__(*args, **kwargs)
        self.provenance = provenance or {}


def _file_hash(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _model_hash(path):
    """Include external tensor sidecars as well as the ONNX container."""
    path = Path(path)
    model = onnx.load(path, load_external_data=False)
    files = {"model": _file_hash(path)}
    for tensor in model.graph.initializer:
        if tensor.data_location == onnx.TensorProto.EXTERNAL:
            location = {entry.key: entry.value for entry in tensor.external_data}.get("location")
            if not location:
                raise ValueError(f"External tensor {tensor.name!r} has no location")
            files[location] = _file_hash(path.parent / location)
    return files


def perf_analysis(models_dir=None, hwinfo=None, debug=False, *, strict_operators=False):
    """Map every .onnx input or raise; partial results are never reported as success."""
    models_path = paths.DEMO_MODELS_DIR if models_dir is None else Path(models_dir)
    if not models_path.is_dir():
        raise FileNotFoundError(f"Model directory does not exist: {models_dir}")
    model_files = sorted(f for f in models_path.iterdir()
                         if f.is_file() and f.suffix.lower() == ".onnx")
    if not model_files:
        raise ValueError(f"No .onnx models in {models_dir}")
    logger = Logger(paths.artifact_dir("logs"), "perf.log")
    logger.info(f"Mapping {len(model_files)} ONNX model(s) from {models_path}")
    hwinfo = hwinfo or make_hw_info()
    configs = [(1, 1), (1, 0), (0, 1), (0, 0)] if not debug else [(1, 1)]
    results = MappingResults(provenance={
        "models": {p.name: _model_hash(p) for p in model_files},
        "hardware": {name: getattr(hwinfo, name) for name in
                     ("xbar_size", "xbar_num", "tile_size", "pipeline_depth")},
        "strategies": configs,
        "parser_mode": "strict_operators" if strict_operators else "mvm_projection",
        "projections": {},
    })
    communication = {}
    for path in model_files:
        projection = {}
        kernels = load_kernel(path, strict_operators=strict_operators,
                              projection_report=projection)
        results.provenance["projections"][path.name] = projection
        results[path.name], communication[path.name] = {}, {}
        for opt in configs:
            mapped, info = analysis_model(kernels, hwinfo, make_opt_info(*opt))
            results[path.name][opt] = mapped
            communication[path.name][opt] = {
                name: getattr(info, name) for name in
                ("path_num", "datavolume", "total_hops", "total_congestion")
            }
    return results, communication


def _implementation_fingerprint():
    root = paths.REPO_ROOT
    source_files = set()
    for directory in ("src", "include", "third_party/MNSIM"):
        source_files.update(p for p in (root / directory).rglob("*")
                            if p.suffix in (".py", ".cpp", ".h", ".hpp") and p.is_file())
    source_files.update((paths.BOOKSIM_BINARY, paths.BOOKSIM_CONFIG, paths.BOOKSIM_TECHFILE))
    files = {str(p.relative_to(root)): _file_hash(p) for p in sorted(source_files)}
    files["loaded_pimapping"] = _file_hash(pimapping.__file__)
    return files


def _make_cache_key(models, bus_width, xbar_size, *, comm_segs=None, freq=1e9,
                    data_bits=8, traffic_model="custom", config_path=None):
    if comm_segs is None or not getattr(comm_segs, "provenance", None):
        raise ValueError("Verified cache requires MappingResults from perf_analysis")
    payload = {
        "schema": 2, "models": sorted(models), "provenance": comm_segs.provenance,
        "bus_width_bytes_per_cycle": bus_width, "frequency_hz": freq,
        "xbar_size": xbar_size, "data_bits": data_bits, "traffic_model": traffic_model,
        "config": _file_hash(paths.DEFAULT_SIM_CONFIG if config_path is None else config_path),
        "implementation": _implementation_fingerprint(),
        "versions": {"onnx": onnx.__version__, "numpy": np.__version__},
        "segments": [[model, list(opt),
                       [[list(seg.layers), seg.datas] for seg in mapped.comm_segs]]
                      for model, opts in sorted(comm_segs.items())
                      for opt, mapped in sorted(opts.items())],
    }
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, allow_nan=False).encode()).hexdigest()
    return str(paths.artifact_path("cache", f"noc_perf_v2_{digest}.pkl"))


def run_booksim_task(args):
    model, opt, segments, bus_width, freq, data_bits, traffic_model = args
    latency, power = booksim_eval([SimpleNamespace(layers=layers, datas=data)
                                  for layers, data in segments], bus_width, freq,
                                 data_bits=data_bits, traffic_model=traffic_model)
    return model, opt, latency, power, os.getpid()


def get_noc_perf(comm_segs, bus_width=None, xbar_size=None, save=False, *,
                 freq=1e9, data_bits=8, traffic_model="custom", workers=None,
                 config_path=None):
    tasks = [(model, opt, [(list(seg.layers), seg.datas) for seg in mapped.comm_segs],
              bus_width, freq, data_bits, traffic_model)
             for model, opts in sorted(comm_segs.items()) for opt, mapped in sorted(opts.items())]
    filename = None
    if save:
        filename = _make_cache_key(list(comm_segs), bus_width, xbar_size,
                                   comm_segs=comm_segs, freq=freq, data_bits=data_bits,
                                   traffic_model=traffic_model, config_path=config_path)
    if workers is None:
        workers = min(len(tasks), os.cpu_count() or 1, 4)
    if tasks and (not isinstance(workers, int) or workers <= 0):
        raise ValueError("workers must be positive")
    if len(tasks) <= 1 or workers == 1:
        results = [run_booksim_task(task) for task in tasks]
    else:
        with mp.Pool(processes=workers) as pool:
            results = pool.map(run_booksim_task, tasks)
    latency_dict, power_dict = {}, {}
    for model, opt, latency, power, pid in results:
        latency_dict[(model, opt)], power_dict[(model, opt)] = latency, power
    if filename:
        directory = Path(filename).parent
        directory.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=directory, delete=False) as handle:
            pickle.dump({"schema": 2, "result": (latency_dict, power_dict)}, handle)
            temporary = handle.name
        os.replace(temporary, filename)
    return latency_dict, power_dict


def load_noc_perf(bw, xbar_size, models=None, *, comm_segs=None, freq=1e9,
                  data_bits=8, traffic_model="custom", config_path=None):
    """A missing identity is a cache miss; legacy name-only caches are never reused."""
    if comm_segs is None or not getattr(comm_segs, "provenance", None):
        return None
    filename = _make_cache_key(models or list(comm_segs), bw, xbar_size,
                               comm_segs=comm_segs, freq=freq, data_bits=data_bits,
                               traffic_model=traffic_model, config_path=config_path)
    try:
        with open(filename, "rb") as handle:
            payload = pickle.load(handle)
        return payload["result"] if payload.get("schema") == 2 else None
    except (OSError, pickle.UnpicklingError, EOFError, AttributeError, KeyError):
        return None


def get_data_percentage(comm_result=None, dict_key=None):
    if comm_result is None:
        with open(paths.artifact_path("results", "comm_result.pkl"), "rb") as f:
            comm_result = pickle.load(f)
    models = list(comm_result.keys())
    opt_combinations = sorted(set(opt for opts in comm_result.values() for opt in opts))
    base_values = [
        comm_result[model].get(opt_combinations[0], {}).get(dict_key, 0)
        for model in models
    ]
    output_file = paths.prepare_output(paths.artifact_path("results", f"{dict_key}.csv"))
    with open(output_file, "w") as f:
        writer = csv.writer(f)
        writer.writerow(["opt_info", "values", "range", "average"])
        for _, opt in enumerate(opt_combinations):
            values = [
                comm_result[model].get(opt, 0).get(dict_key, 0) for model in models
            ]
            values = [
                value / base_value for value, base_value in zip(values, base_values)
            ]
            value_range = (min(values), max(values))
            average = sum(values) / len(values)
            writer.writerow([get_opt_str(opt), values, value_range, average])


def power_analysis(mapping_results, latency_dict, power_dict, bw, comm_results=None):
    models = list(mapping_results.keys())
    opt_combinations = sorted(
        set(opt for opts in mapping_results.values() for opt in opts)
    )
    all_powers = {opt: [] for opt in opt_combinations}
    for i, opt in enumerate(opt_combinations):
        print(f"Optimization: {get_opt_str(opt)}")
        ideal = 1 if i == 4 else 0
        time_data = [
            latency_est(
                mapping_res=mapping_results[model].get(opt, 0),
                bus_width=bw,
                comm_lat=(
                    latency_dict[(model, opt)] if latency_dict is not None else None
                ),
                ideal=ideal,
            )[-1]
            for model in models
        ]
        cur_pwr_dict = [
            value
            for (model, i), value in power_dict.items()
            if model in models and i == opt
        ]
        power = [0] * len(models)
        for i in range(len(models)):
            for key in cur_pwr_dict[i].keys():
                avg_pwr = cur_pwr_dict[i][key]
                total_time = time_data[i][key]
                power[i] += avg_pwr * total_time
        if comm_results is not None:
            data_volume = [
                comm_results[model].get(opt, {}).get("datavolume", 0)
                for model in models
            ]
            efficiency = [1 / p for p, dv in zip(power, data_volume)]
        all_powers[opt] = power
        if all_powers[(0, 0)]:
            normalized_power = [p / all_powers[(0, 0)][i] for i, p in enumerate(power)]
        print(normalized_power)
    return power, data_volume, efficiency


def brkdown_stat(comm_segs, latency_dict, bw=1):
    models = list(comm_segs.keys())
    print(models)
    opt_combinations = [(0, 0), (0, 1), (1, 0), (1, 1)]
    perf_dict = {}
    for opt in opt_combinations:
        print(get_opt_str(opt))
        perf_dict[opt] = {}
        stats = [
            latency_est(
                mapping_res=comm_segs[model].get(opt, 0),
                bus_width=bw,
                comm_lat=latency_dict[(model, opt)],
            )
            for model in models
        ]
        overall_stats = [stat[0] for stat in stats]
        cal_stats = [sum(stat[3]) for stat in stats]
        trans_stats = [sum(stat[2]) for stat in stats]
        print(overall_stats)
        print(cal_stats)
        print(trans_stats)
        perf_dict[opt]["overall"] = overall_stats
        perf_dict[opt]["cal"] = cal_stats
        perf_dict[opt]["trans"] = trans_stats
    with paths.prepare_output(paths.artifact_path("results", "perf_dict.pkl")).open("wb") as handle:
        pickle.dump(perf_dict, handle)


def brkdown_analysis():
    with paths.artifact_path("results", "perf_dict.pkl").open("rb") as handle:
        stats = pickle.load(handle)
    base_stats = stats[(0, 0)]
    HITM_stats = stats[(0, 1)]
    SPATEM_stats = stats[(1, 0)]
    opt_stats = stats[(1, 1)]

    base_cal = base_stats["cal"]
    base_trans = base_stats["trans"]
    base_overall = [c + t for c, t in zip(base_cal, base_trans)]

    HITM_cal = HITM_stats["cal"]
    HITM_trans = HITM_stats["trans"]
    HITM_overall = [c + t for c, t in zip(HITM_cal, HITM_trans)]

    SPATEM_cal = SPATEM_stats["cal"]
    SPATEM_trans = SPATEM_stats["trans"]
    SPATEM_overall = [c + t for c, t in zip(SPATEM_cal, SPATEM_trans)]

    opt_cal = opt_stats["cal"]
    opt_trans = opt_stats["trans"]
    opt_overall = [c + t for c, t in zip(opt_cal, opt_trans)]

    HITM_cal_improve = [(opt - base) / base for base, opt in zip(base_cal, HITM_cal)]
    SPATEM_cal_improve = [
        (opt - base) / base for base, opt in zip(base_cal, SPATEM_cal)
    ]
    cal_improve = [(opt - base) / base for base, opt in zip(base_cal, opt_cal)]

    HITM_trans_improve = [
        (opt - base) / base for base, opt in zip(base_trans, HITM_trans)
    ]
    SPATEM_trans_improve = [
        (opt - base) / base for base, opt in zip(base_trans, SPATEM_trans)
    ]
    trans_improve = [(opt - base) / base for base, opt in zip(base_trans, opt_trans)]

    HITM_overall_improve = [
        (opt - base) / base for base, opt in zip(base_overall, HITM_overall)
    ]
    SPATEM_overall_improve = [
        (opt - base) / base for base, opt in zip(base_overall, SPATEM_overall)
    ]
    overall_improve = [
        (opt - base) / base for base, opt in zip(base_overall, opt_overall)
    ]
    base_cal = [f"{stat*10}" for stat in base_cal]
    base_trans = [f"{stat*10}" for stat in base_trans]
    base_overall = [f"{stat*10}" for stat in base_overall]

    HITM_cal = [f"{stat*10}" for stat in HITM_cal]
    HITM_trans = [f"{stat*10}" for stat in HITM_trans]
    HITM_overall = [f"{stat*10}" for stat in HITM_overall]

    SPATEM_cal = [f"{stat*10}" for stat in SPATEM_cal]
    SPATEM_trans = [f"{stat*10}" for stat in SPATEM_trans]
    SPATEM_overall = [f"{stat*10}" for stat in SPATEM_overall]

    opt_cal = [f"{stat*10}" for stat in opt_cal]
    opt_trans = [f"{stat*10}" for stat in opt_trans]
    opt_overall = [f"{stat*10}" for stat in opt_overall]
    print("base_cal:", base_cal)
    print("base_trans:", base_trans)
    print("base_overall:", base_overall)
    print("HITM_cal:", HITM_cal)
    print("HITM_trans:", HITM_trans)
    print("HITM_overall:", HITM_overall)
    print("SPATEM_cal:", SPATEM_cal)
    print("SPATEM_trans:", SPATEM_trans)
    print("SPATEM_overall:", SPATEM_overall)
    print("opt_cal:", opt_cal)
    print("opt_trans:", opt_trans)
    print("opt_overall:", opt_overall)

    HITM_cal_improve = [f"{improve:.2%}" for improve in HITM_cal_improve]
    SPATEM_cal_improve = [f"{improve:.2%}" for improve in SPATEM_cal_improve]
    cal_improve = [f"{improve:.2%}" for improve in cal_improve]

    HITM_trans_improve = [f"{improve:.2%}" for improve in HITM_trans_improve]
    SPATEM_trans_improve = [f"{improve:.2%}" for improve in SPATEM_trans_improve]
    trans_improve = [f"{improve:.2%}" for improve in trans_improve]

    HITM_overall_improve = [f"{improve:.2%}" for improve in HITM_overall_improve]
    SPATEM_overall_improve = [f"{improve:.2%}" for improve in SPATEM_overall_improve]
    overall_improve = [f"{improve:.2%}" for improve in overall_improve]

    print("HITM_cal_improve:", HITM_cal_improve)
    print("HITM_trans_improve:", HITM_trans_improve)
    print("HITM_overall_improve:", HITM_overall_improve)
    print("SPATEM_cal_improve:", SPATEM_cal_improve)
    print("SPATEM_trans_improve:", SPATEM_trans_improve)
    print("SPATEM_overall_improve:", SPATEM_overall_improve)
    print("cal_improve:", cal_improve)
    print("trans_improve:", trans_improve)
    print("overall_improve:", overall_improve)
