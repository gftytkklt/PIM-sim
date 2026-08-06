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
from MappingInfo import latency_est, booksim_eval
from onnx_analysis import analysis_model, make_opt_info, load_kernel, make_hw_info
from logger import Logger

home_path = os.getcwd()
logger = Logger(f"{home_path}/runs", "perf.log")


def get_opt_str(opt_info, ideal=0):
    if opt_info == (1, 1):
        return "This Paper" if not ideal else "Ideal"
    elif opt_info == (1, 0):
        return "SPATEM"
    elif opt_info == (0, 1):
        return "HITM"
    else:
        return "MNSIM"


def perf_analysis(models_dir="demo", hwinfo=None, debug=False):
    models_path = Path(models_dir)

    if not models_path.exists() or not models_path.is_dir():
        logging.error(f"模型目录 '{models_dir}' 不存在或不是一个目录。")
        return

    model_files = [f for f in models_path.iterdir() if f.is_file()]
    if not model_files:
        logging.warning(f"在目录 '{models_dir}' 中未找到任何模型文件。")
        return

    logging.info(f"开始回归测试，总共有 {len(model_files)} 个模型文件。")

    opt_configs = [(1, 1), (1, 0), (0, 1), (0, 0)] if not debug else [(1, 1)]

    success = 0
    comm_results = {}
    comm_segs = {}

    for model_path in model_files:
        model_name = model_path.name
        logging.info(f"正在测试模型: {model_name}")
        cur_kernel = load_kernel(model_path)
        for opt1, opt2 in opt_configs:
            logging.info(f"(opt_info=({opt1},{opt2}))")
            try:
                cur_opt_info = make_opt_info(opt1, opt2)
                comm_seg, comminfo = analysis_model(
                    kernel_list=cur_kernel,
                    hw_info=hwinfo,
                    opt_info=cur_opt_info,
                )
                if model_name not in comm_results:
                    comm_results[model_name] = {}
                if model_name not in comm_segs:
                    comm_segs[model_name] = {}
                comm_results[model_name][(opt1, opt2)] = {
                    "path_num": comminfo.path_num,
                    "datavolume": comminfo.datavolume,
                    "total_hops": comminfo.total_hops,
                    "total_congestion": comminfo.total_congestion,
                }
                comm_segs[model_name][(opt1, opt2)] = comm_seg

                success += 1
                logging.info(f"模型 '{model_name}' opt_info=({opt1},{opt2}) 测试成功。")
                print("\n")
            except FileNotFoundError as e:
                error_msg = str(e)
                logging.error(
                    f"模型 '{model_name}' opt_info=({opt1},{opt2}) 测试失败。错误信息:\n{error_msg}"
                )
                break
            except Exception as e:
                error_msg = str(e)
                logging.exception(
                    f"在测试模型 '{model_name}' opt_info=({opt1},{opt2}) 时发生未预料的错误。"
                )
                break
    print(f"成功测试 {success} 个模型。")
    return comm_segs, comm_results

GLOBAL_COMM_SEGS = None


def _make_cache_key(models, bus_width, xbar_size):
    model_str = "_".join(sorted(models))
    model_hash = hashlib.md5(model_str.encode()).hexdigest()[:8]
    return f"results/noc_perf_dict_bw={bus_width}_xbar={xbar_size[0]}_{xbar_size[1]}_m={model_hash}.pkl"


def run_booksim_task(args):
    model, opt, bus_width = args
    start_time = time.time()
    segs = GLOBAL_COMM_SEGS[model].get(opt, 0).comm_segs
    latency, power = booksim_eval(segs, bus_width)
    print(f"model={model}, opt={opt}, time={time.time() - start_time}")
    return (model, opt, latency, power, os.getpid())


def get_noc_perf(comm_segs, bus_width=None, xbar_size=None, save=False):
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))

    latency_dict, power_dict = {}, {}
    booksim_tasks = [
        (model, opt, bus_width) for model in models for opt in opt_combinations
    ]

    global GLOBAL_COMM_SEGS
    GLOBAL_COMM_SEGS = comm_segs

    with mp.Pool() as pool:
        results = pool.map(run_booksim_task, booksim_tasks)
        for model, opt, latency, power, pid in results:
            latency_dict[(model, opt)] = latency
            power_dict[(model, opt)] = power
            logger.info(f"(model={model}, opt={opt}) booksim worker process PID={pid}")

    if save:
        filename = _make_cache_key(models, bus_width, xbar_size)
        os.makedirs(os.path.dirname(filename), exist_ok=True)
        with open(filename, "wb") as f:
            pickle.dump((latency_dict, power_dict), f)
            print("Data saved.")

    return latency_dict, power_dict


def load_noc_perf(bw, xbar_size, models=None):
    if models:
        filename = _make_cache_key(models, bw, xbar_size)
        if os.path.exists(filename):
            try:
                with open(filename, "rb") as f:
                    return pickle.load(f)
            except Exception:
                pass
    filename = f"results/noc_perf_dict_bw={bw}_xbar={xbar_size[0]}_{xbar_size[1]}.pkl"
    if not os.path.exists(filename):
        return None
    try:
        with open(filename, "rb") as f:
            perf_dict = pickle.load(f)
        return perf_dict
    except Exception as e:
        print(f"Error loading file {filename}: {e}")
        return None


def get_data_percentage(comm_result=None, dict_key=None):
    if comm_result is None:
        with open("results/comm_result.pkl", "rb") as f:
            comm_result = pickle.load(f)
    models = list(comm_result.keys())
    opt_combinations = sorted(set(opt for opts in comm_result.values() for opt in opts))
    base_values = [
        comm_result[model].get(opt_combinations[0], {}).get(dict_key, 0)
        for model in models
    ]
    output_file = f"results/{dict_key}.csv"
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
    pickle.dump(perf_dict, open("results/perf_dict.pkl", "wb"))


def brkdown_analysis():
    stats = pickle.load(open("results/perf_dict.pkl", "rb"))
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
