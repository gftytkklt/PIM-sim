import multiprocessing as mp
import os, sys
import csv
from statistics import geometric_mean, mean
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import PercentFormatter
import numpy as np
from statistics import geometric_mean as gmean
from MNSIM.Latency_Model.Tile_latency import tile_latency_analysis
from collections import defaultdict
import logging
import pickle
import time
from MappingInfo import booksim_eval, tile_latency_cal
from onnx_analysis import analysis_model, make_opt_info, load_kernel, make_hw_info
from logger import Logger
from pimapping import OptType

home_path = os.getcwd()
logger = Logger(f"{home_path}/runs", "perf.log")

def latency_est(
    SimConfig_path="SimConfig.ini",
    inputbit=8,
    outputbit=8,
    mapping_res=None,
    bus_width=8,
    freq=1000000000,
    comm_lat=None,
    ideal=0,
    syn=1,
):
    # get mapping results
    all_tiles_mapping_infos = mapping_res.deploy_info
    all_comm_segs = mapping_res.comm_segs  # comm_seg: layer and data matrix
    # print("tile num is", len(all_tiles_mapping_infos))
    # get inter-tile lat first if not provided
    # if comm_lat is None and ideal == 0:
    latency_map = {}
    # if comm_lat is None:
    #     latency_map = booksim_eval(all_comm_segs, bus_width, freq)
        # modify filename manually after saving
        # pickle.dump(latency_map, open(f"results/noc_perf_dict_bw=1_xbar=256_256.pkl", "wb"))
    # elif ideal == 1:
    #     latency_map = {}
    # else:
    #     latency_map = comm_lat  # lat_layer = latency_map[layer]
    bandwidth = bus_width * freq  # B/s
    # update tile exec info
    exec_info = defaultdict(dict)
    effbw = {}
    seg_cal_lat = []
    seg_trans_lat = []
    seg_trans_dict = {}
    overall_latency = 0.0
    overall_throughput = 0.0
    max_layerseg_lat = 0.0

    # 所有cnode各自的延迟
    cnode_latency_dict = {}
    num_cnode = 0
    for tile_info in all_tiles_mapping_infos:
        for cn in tile_info.cnode:
            cn_latency = tile_latency_cal(SimConfig_path, cn.ifmap_size, inputbit, outputbit) / 100000000
            cnode_latency_dict[cn] = cn_latency
            # print (f"CNode l {cn.layer}, latency: {cn_latency:.6f} s")
            num_cnode += 1
    # print(f"Total number of cnodes: {num_cnode}")

    # update path latency of each segment
    overall_latency = 0.0
    overall_stages = 0
    lat_update_count = 0
    processed_cnodes  = 0
    for idx, comm_seg in enumerate(all_comm_segs):
        # cur layer seg
        layers = comm_seg.layers
        # print layer in layers
        # for layer in layers:
        #     print(f"Segment {idx}, Layer {layer}")
        # equivalent bandwidth computation (B/s)
        # effbw.update({layer: bandwidth / (1 + latency_map.get(layer, 0)) for layer in layers})
        effbw.update(
            {layer: bandwidth * latency_map.get(layer, 1) for layer in layers}
        )  # ideal bandwidth
        # get all paths
        merged_paths = []
        merged_paths.extend(
            (layer, path)
            for layer in layers
            for tile_info in all_tiles_mapping_infos
            if layer in tile_info.layer_paths_map
            for path in tile_info.layer_paths_map[layer]
        )
        # update via delay
        via_delay = {}
        for layer, path in merged_paths:
            cur_effbw = effbw[layer]
            path_delay = path.datavolume / cur_effbw
            for s, d in zip(path.via[:-1], path.via[1:]):
                via_delay.update({(s, d): via_delay.get((s, d), 0) + path_delay})
        # update path delay
        cur_max_path_delay = 0.0
        for layer, path in merged_paths:
            path_delay = 0
            for s, d in zip(path.via[:-1], path.via[1:]):
                path_delay += (
                    via_delay[(s, d)] if not ideal else path.datavolume / cur_effbw
                )
            cur_max_path_delay = max(cur_max_path_delay, path_delay)
        # generate computation pipeline layer-wise
        cur_max_compute_latency = 0.0
        for layer in layers:
            # 1. 收集当前layer的所有cnode，按tile分组
            # print(f"Layer: {layer}")
            tile_cnodes = defaultdict(list)  # tile_idx -> [cnode1, cnode2, ...]
            
            for tile_idx, tile_info in enumerate(all_tiles_mapping_infos):
                for cn in tile_info.cnode:
                    if cn.layer == layer:
                        tile_cnodes[tile_idx].append(cn)
            
            # 2. 构造计算批
            layer_compute_latency = 0.0
            layer_pipeline = 0
            # 继续直到所有tile的所有cnode都被处理
            while any(tile_cnodes.values()):
                current_batch = []
                
                # 从每个tile取出一个cnode（如果该tile还有当前层的cnode）
                for tile_idx in list(tile_cnodes.keys()):
                    if tile_cnodes[tile_idx]:  # 这个tile还有当前层的cnode
                        cn = tile_cnodes[tile_idx].pop(0)
                        current_batch.append(cn)
                        processed_cnodes += 1
                
                # 计算当前批的延迟（取最大值）
                if current_batch:
                    batch_latency = max(cnode_latency_dict[cn] for cn in current_batch)
                    layer_compute_latency += batch_latency
                    lat_update_count += 1
                else:
                    print(f"Warning: No cnodes found for layer {layer} in current batch.")
                
                # 清理已空的tile
                empty_tiles = [tile_idx for tile_idx in tile_cnodes if not tile_cnodes[tile_idx]]
                for tile_idx in empty_tiles:
                    del tile_cnodes[tile_idx]
                layer_pipeline += 1
            # 打印当前层的计算延迟
            # print(f"Layer {layer} compute latency: {layer_compute_latency:.6f} s, pipeline stages: {layer_pipeline}")
            overall_latency += layer_compute_latency
            overall_stages += layer_pipeline
            cur_max_compute_latency = max(cur_max_compute_latency, layer_compute_latency)
        for layer in layers:
            seg_trans_dict[layer] = cur_max_path_delay

    # overall_latency = sum(seg_cal_lat) + sum(seg_trans_lat)
    # print(f"Overall latency: {overall_latency:.6f} s")
    # print(f"Overall pipeline stages: {overall_stages}")
    # print(f"Latency updates: {lat_update_count}")
    # print(f"Processed cnodes: {processed_cnodes}")
    overall_throughput = 1 / cur_max_compute_latency if cur_max_compute_latency > 0 else float("inf")
    return (
        overall_latency,
        overall_throughput,
        overall_stages,
    )

# demo for debug, models for run
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

    # OptType_list = [OptType.PIMAPPING, OptType.PUMA, OptType.TILE2_0_V2, OptType.MNSIM]
    OptType_list = [OptType.TILE2_0]

    success = 0
    comm_results = {}
    comm_segs = {}

    for model_path in model_files:
        model_name = model_path.name
        logging.info(f"正在测试模型: {model_name}")
        cur_kernel = load_kernel(model_path)
        for opt_type in OptType_list:
            logging.info(f"(opt_type={opt_type}) 开始测试模型 '{model_name}'")
            print(f"正在测试模型 '{model_name}' opt_type={opt_type}...")
            try:
                # plot test
                comm_seg, comminfo = analysis_model(
                    kernel_list=cur_kernel,
                    hw_info=hwinfo,
                    opt_info=opt_type,
                )
                if model_name not in comm_results:
                    comm_results[model_name] = {}
                if model_name not in comm_segs:
                    comm_segs[model_name] = {}
                # comm_results[model_name][(opt1, opt2)] = comminfo.path_num
                comm_results[model_name][opt_type] = {
                    "path_num": comminfo.path_num,
                    "datavolume": comminfo.datavolume,
                    "total_hops": comminfo.total_hops,
                    "total_congestion": comminfo.total_congestion,
                }
                comm_segs[model_name][opt_type] = comm_seg

                # perf exec
                # latency_est(SimConfig_path, inputbit, outputbit, cur_kernel, cur_opt_info)

                success += 1
                logging.info(f"模型 '{model_name}' opt_info=(opt_type) 测试成功。")
                print("\n")
            except FileNotFoundError as e:
                error_msg = str(e)
                logging.error(
                    f"模型 '{model_name}' opt_info=测试失败。错误信息:\n{error_msg}"
                )
                break
            except Exception as e:
                error_msg = str(e)
                logging.exception(
                    f"在测试模型 '{model_name}' opt_info=时发生未预料的错误。"
                )
                break
    print(f"成功测试 {success} 个模型。")
    return comm_segs, comm_results

# parse seg elems in this function and plot
def plot_perf(
    mapping_results, latency_dict=None, bw=4, norm=0, plot_type=None, ideal=0
):
    # 设置学术风格参数
    plt.style.use("seaborn-v0_8-paper")
    plt.rcParams.update(
        {
            "font.family": "Times New Roman",
            "mathtext.fontset": "stix",
            "axes.titlesize": 20,
            "axes.labelsize": 16,
            "xtick.labelsize": 12,
            "ytick.labelsize": 12,
            "legend.fontsize": 12,
            "grid.linewidth": 0.5,
            "lines.linewidth": 1,
            "hatch.linewidth": 0.5,
            "font.weight": "bold",
        }
    )
    fig, ax = plt.subplots(figsize=(8, 4.5))  # 更适合论文栏宽的尺寸

    # 学术配色方案（ColorBrewer Set1 + 灰度扩展）
    palette = [
        "#4e79a7",
        "#f28e2b",
        "#e15759",
        "#76b7b2",
        "#59a14f",
        "#b07aa1",
        "#9c755f",
    ]
    # hatch_patterns = ['//', '\\\\', '||', '--', '++', 'xx', 'oo']

    # 获取绘图数据
    models = list(mapping_results.keys())
    opt_combinations = set(opt for opts in mapping_results.values() for opt in opts)

        # print(opt_combinations)
    # bar_width = 0.18  # 调整宽度适应更多分组
    # inner_space = 0.2
    n_opts = len(opt_combinations)
    max_bar_width = 0.18  # 最大柱宽
    group_width = 0.9
    bar_width = min(max_bar_width, group_width / (n_opts + (n_opts - 1) * 0.2))
    inner_space = bar_width * 0.2  # 间距与柱宽比例关联

    index = np.arange(len(models))

    # 绘图参数初始化
    base_values = []
    list_id = 0 if plot_type == "latency" else 1
    error_kw = dict(lw=0.8, capsize=3, capthick=0.8)  # 误差线样式

    # 绘制柱状图
    for i, opt in enumerate(opt_combinations):
        cur_ideal = 1 if i == 4 else 0
        values = [
            latency_est(
                mapping_res=mapping_results[model].get(opt, 0),
                bus_width=bw,
                comm_lat=(
                    latency_dict[(model, opt)] if latency_dict is not None else None
                ),
                ideal=cur_ideal,
            )[list_id]
            for model in models
        ]  # 保持原有计算逻辑
        # 标准化处理
        if norm and i == 0:
            base_values = values
        values = [v / b for v, b in zip(values, base_values)] if norm else values
        # print(values)

        # 创建柱状图
        # pos = index + i * bar_width * (1 + inner_space)
        pos = index + i * (bar_width + inner_space)
        bars = ax.bar(
            pos,
            values,
            bar_width,
            color=palette[i % len(palette)],
            edgecolor="none",
            linewidth=0.6,
            #   hatch=hatch_patterns[i%len(hatch_patterns)],
            alpha=0.9,
            # label=f"{get_opt_str(opt, cur_ideal)}",
            label=f"{opt.name}",
            error_kw=error_kw,
        )

        # 特殊标注理想情况
        if True:
            for bar in bars:
                height = bar.get_height()
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    height,
                    f"{height:.2f}",
                    ha="center",
                    va="bottom",
                    fontsize=8,
                    rotation=0,
                    fontweight="bold",
                    bbox=dict(facecolor="white", alpha=0.8, edgecolor="none", pad=0.2),
                )

    # 坐标轴和标签优化
    ylabel = f"Normalized {plot_type}" if norm else plot_type
    ax.set_ylabel(ylabel, labelpad=5, fontweight="bold")
    # ax.set_xlabel('Model Architectures', fontsize=10, labelpad=5)
    # ax.set_xticks(index + bar_width*(len(opt_combinations)/2))
    # ax.set_xticks(index + (len(opt_combinations)-1) * bar_width * (1 + 0.1) / 2)
    # 计算总位移量 (考虑间距系数)
    total_offset = (len(opt_combinations) - 1) * bar_width * (1 + inner_space)

    # 核心修正：正确定位x轴刻度
    ax.set_xticks(index + total_offset / 2)  # 居中定位
    ax.set_xticklabels(
        [m.split(".")[0] for m in models],
        rotation=0,
        ha="center",
        rotation_mode="anchor",
        fontweight="bold",
    )
    # ax.set_yticklabels(fontweight='bold')

    # 网格和边框优化
    ax.yaxis.grid(True, linestyle="--", alpha=0.6)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["bottom"].set_linewidth(0.5)
    ax.spines["left"].set_linewidth(0.5)

    legend = ax.legend(
        ncol=n_opts,
        loc="upper left",
        bbox_to_anchor=(0, 1.15),
        frameon=True,
        fancybox=False,
        shadow=False,
        edgecolor="black",
        prop={"weight": "bold"},
    )
    legend.get_frame().set_linewidth(0.5)

    # 紧凑布局并保存
    plt.tight_layout(pad=1.5)
    file_name = f"results4/norm_{plot_type}_{bw}.pdf"
    fig.savefig(file_name, dpi=600, bbox_inches="tight")

def plot_stages(
    mapping_results, latency_dict=None, bw=1, norm=0
):
    # 设置学术风格参数
    plt.style.use("seaborn-v0_8-paper")
    plt.rcParams.update(
        {
            "font.family": "Times New Roman",
            "mathtext.fontset": "stix",
            "axes.titlesize": 20,
            "axes.labelsize": 16,
            "xtick.labelsize": 12,
            "ytick.labelsize": 12,
            "legend.fontsize": 12,
            "grid.linewidth": 0.5,
            "lines.linewidth": 1,
            "hatch.linewidth": 0.5,
            "font.weight": "bold",
        }
    )
    fig, ax = plt.subplots(figsize=(8, 4.5))  # 更适合论文栏宽的尺寸

    # 学术配色方案（ColorBrewer Set1 + 灰度扩展）
    palette = [
        "#4e79a7",  # 蓝色
        "#f28e2b",  # 橙色
        "#e15759",  # 红色
        "#76b7b2",  # 青绿色
        "#59a14f",  # 绿色
        "#b07aa1",  # 紫色
        "#9c755f",  # 棕色
    ]
    
    # 获取绘图数据
    models = list(mapping_results.keys())
    opt_combinations = set(opt for opts in mapping_results.values() for opt in opts)
    
    # 计算柱状图宽度
    n_opts = len(opt_combinations)
    max_bar_width = 0.18
    group_width = 0.9
    bar_width = min(max_bar_width, group_width / (n_opts + (n_opts - 1) * 0.2))
    inner_space = bar_width * 0.2
    
    index = np.arange(len(models))
    error_kw = dict(lw=0.8, capsize=3, capthick=0.8)
    
    # 绘制柱状图
    for i, opt in enumerate(opt_combinations):
        cur_ideal = 1 if i == 4 else 0
        values = [
            latency_est(
                mapping_res=mapping_results[model].get(opt, 0),
                bus_width=bw,
                comm_lat=(
                    latency_dict[(model, opt)] if latency_dict is not None else None
                ),
                ideal=cur_ideal,
            )[2]  # 第2个索引对应overall_stages
            for model in models
        ]
        
        # 标准化处理
        if norm and i == 0:
            base_values = values
        if norm:
            values = [v / b for v, b in zip(values, base_values)]
        
        # 创建柱状图
        pos = index + i * (bar_width + inner_space)
        bars = ax.bar(
            pos,
            values,
            bar_width,
            color=palette[i % len(palette)],
            edgecolor="none",
            linewidth=0.6,
            alpha=0.9,
            label=f"{opt.name}",
            error_kw=error_kw,
        )
        
        # 在柱状图上添加数值标签
        for bar in bars:
            height = bar.get_height()
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                height,
                f"{height:.1f}" if norm else f"{height:.0f}",
                ha="center",
                va="bottom",
                fontsize=8,
                rotation=0,
                fontweight="bold",
                bbox=dict(facecolor="white", alpha=0.8, edgecolor="none", pad=0.2),
            )
    
    # 坐标轴和标签
    ylabel = "Normalized Pipeline Stages" if norm else "Pipeline Stages"
    ax.set_ylabel(ylabel, labelpad=5, fontweight="bold")
    
    # 设置x轴刻度
    total_offset = (len(opt_combinations) - 1) * (bar_width + inner_space)
    ax.set_xticks(index + total_offset / 2)
    ax.set_xticklabels(
        [m.split(".")[0] for m in models],
        rotation=0,
        ha="center",
        rotation_mode="anchor",
        fontweight="bold",
    )
    
    # 网格和边框优化
    ax.yaxis.grid(True, linestyle="--", alpha=0.6)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["bottom"].set_linewidth(0.5)
    ax.spines["left"].set_linewidth(0.5)
    
    # 设置y轴为整数刻度（如果是归一化则不需要）
    if not norm:
        from matplotlib.ticker import MaxNLocator
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))
    
    # 图例
    legend = ax.legend(
        ncol=n_opts,
        loc="upper left",
        bbox_to_anchor=(0, 1.15),
        frameon=True,
        fancybox=False,
        shadow=False,
        edgecolor="black",
        prop={"weight": "bold"},
    )
    legend.get_frame().set_linewidth(0.5)
    
    # 紧凑布局并保存
    plt.tight_layout(pad=1.5)
    file_name = f"results4/norm_{'stages' if norm else 'stages_abs'}_{bw}.pdf"
    fig.savefig(file_name, dpi=600, bbox_inches="tight")
    print(f"Pipeline stages plot saved to: {file_name}")

# 添加新的绘图函数
def plot_hw_configs_performance(
    hw_configs_results,  # 硬件配置结果: {config_name: {model_name: {opt_type: latency_value}}}
    xbar_num_list=None,  # 用于xbar_num变动的x轴标签
    xbar_size_list=None,  # 用于xbar_size变动的x轴标签
    config_type="xbar_num",  # "xbar_num" 或 "xbar_size"
    y_label="Normalized Latency",
    save_path="results4"
):
    """
    绘制不同硬件配置下各种优化方法的平均性能对比图
    
    Args:
        hw_configs_results: 硬件配置结果字典
        config_type: 配置类型 ("xbar_num" 或 "xbar_size")
        xbar_num_list: xbar_num参数列表
        xbar_size_list: xbar_size参数列表
    """
    # 设置学术风格参数
    plt.style.use("seaborn-v0_8-paper")
    plt.rcParams.update({
        "font.family": "Times New Roman",
        "mathtext.fontset": "stix",
        "axes.titlesize": 20,
        "axes.labelsize": 16,
        "xtick.labelsize": 12,
        "ytick.labelsize": 12,
        "legend.fontsize": 12,
        "grid.linewidth": 0.5,
        "lines.linewidth": 1,
        "hatch.linewidth": 0.5,
        "font.weight": "bold",
    })
    
    fig, ax = plt.subplots(figsize=(8, 4.5))
    
    # 学术配色方案
    palette = [
        "#4e79a7", "#f28e2b", "#e15759", "#76b7b2", 
        "#59a14f", "#b07aa1", "#9c755f", "#edc948"
    ]
    
    # 获取所有优化方法
    all_opt_types = set()
    for config_result in hw_configs_results.values():
        for model_result in config_result.values():
            all_opt_types.update(model_result.keys())
    
    # 过滤掉None值
    all_opt_types = {opt for opt in all_opt_types if opt is not None}
    all_opt_types = sorted(list(all_opt_types), key=lambda x: x.name)
    
    # 收集每个硬件配置下各优化方法的归一化平均值
    config_names = sorted(list(hw_configs_results.keys()))
    
    # 计算每个配置下各优化方法的平均值
    avg_results = {}  # {config_name: {opt_type: avg_latency}}
    
    for config_name, config_result in hw_configs_results.items():
        config_avgs = {}
        
        # 对每种优化方法
        for opt_type in all_opt_types:
            latencies = []
            
            # 对每个模型
            for model_name, model_result in config_result.items():
                if opt_type in model_result:
                    # 计算延迟
                    try:
                        # 使用latency_est计算延迟
                        mapping_res = model_result[opt_type]
                        lat, throughput, stages = latency_est(
                            mapping_res=mapping_res,
                            bus_width=1,  # 可以调整
                            comm_lat=None,
                            ideal=1
                        )
                        latencies.append(lat)
                    except Exception as e:
                        print(f"Error calculating latency for {config_name}, {model_name}, {opt_type}: {e}")
                        continue
            
            if latencies:
                # 计算几何平均值
                config_avgs[opt_type] = geometric_mean(latencies)
            else:
                config_avgs[opt_type] = 0
        
        avg_results[config_name] = config_avgs
    
    # 归一化到MNSIM
    normalized_results = {}
    for config_name, config_avgs in avg_results.items():
        normalized = {}
        
        # 找到MNSIM的基准值
        baseline_value = None
        for opt_type, value in config_avgs.items():
            if opt_type.name == "MNSIM":
                baseline_value = value
                break
        
        if baseline_value and baseline_value > 0:
            for opt_type, value in config_avgs.items():
                normalized[opt_type] = value / baseline_value
        else:
            normalized = config_avgs
        
        normalized_results[config_name] = normalized
    
    # 准备绘图数据
    n_configs = len(config_names)
    n_opts = len(all_opt_types)
    
    # 计算柱状图宽度
    max_bar_width = 0.18
    group_width = 0.9
    bar_width = min(max_bar_width, group_width / (n_opts + (n_opts - 1) * 0.2))
    inner_space = bar_width * 0.2
    
    index = np.arange(n_configs)
    
    # 绘制柱状图
    for i, opt_type in enumerate(all_opt_types):
        values = [normalized_results[config_name].get(opt_type, 0) 
                 for config_name in config_names]
        
        pos = index + i * (bar_width + inner_space)
        bars = ax.bar(
            pos,
            values,
            bar_width,
            color=palette[i % len(palette)],
            edgecolor="none",
            linewidth=0.6,
            alpha=0.9,
            label=f"{opt_type.name}",
        )
        
        # 添加数值标签
        for bar, value in zip(bars, values):
            height = bar.get_height()
            if height > 0:  # 只显示有效值
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    height * 1.02,  # 稍微抬高一点
                    f"{height:.2f}" if height >= 0.1 else f"{height:.3f}",
                    ha="center",
                    va="bottom",
                    fontsize=8,
                    rotation=0,
                    fontweight="bold",
                    bbox=dict(facecolor="white", alpha=0.8, edgecolor="none", pad=0.5),
                )
    
    # 设置坐标轴
    ax.set_ylabel(y_label, labelpad=5, fontweight="bold")
    
    # 设置x轴标签
    if config_type == "xbar_num" and xbar_num_list:
        x_labels = [f"Xbar Num={num}" for num in xbar_num_list]
    elif config_type == "xbar_size" and xbar_size_list:
        x_labels = [f"Xbar Size={size[0]}×{size[1]}" for size in xbar_size_list]
    else:
        x_labels = config_names
    
    ax.set_xticks(index + (n_opts - 1) * (bar_width + inner_space) / 2)
    ax.set_xticklabels(
        x_labels,
        rotation=0,
        ha="center",
        rotation_mode="anchor",
        fontweight="bold",
    )
    
    # 网格和边框优化
    ax.yaxis.grid(True, linestyle="--", alpha=0.6)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["bottom"].set_linewidth(0.5)
    ax.spines["left"].set_linewidth(0.5)
    
    # 添加水平线y=1表示MNSIM基准
    ax.axhline(y=1.0, color='black', linestyle='--', linewidth=0.8, alpha=0.5)
    
    # 图例
    legend = ax.legend(
        ncol=min(n_opts, 4),
        loc="upper left",
        bbox_to_anchor=(0, 1.15),
        frameon=True,
        fancybox=False,
        shadow=False,
        edgecolor="black",
        prop={"weight": "bold"},
    )
    legend.get_frame().set_linewidth(0.5)
    
    plt.tight_layout(pad=1.5)
    file_name = f"{save_path}/avg_perf_{config_type}.pdf"
    fig.savefig(file_name, dpi=600, bbox_inches="tight")
    print(f"Average performance plot saved to: {file_name}")

# 修改主函数
if __name__ == "__main__":
    # mp.set_start_method("fork", force=True)
    
    # # 实验1: 固定xbar_size=(256, 256)，变动xbar_num
    # xbar_size_fixed = (256, 256)
    # xbar_num_list = [16, 32, 64]
    
    # hw_configs_results_xbar_num = {}
    
    # for xbar_num in xbar_num_list:
    #     print(f"\n{'='*60}")
    #     print(f"Testing with xbar_size={xbar_size_fixed}, xbar_num={xbar_num}")
    #     print('='*60)
        
    #     hw_info = make_hw_info(xbar_size_fixed, xbar_num, (0, 0), 1)
    #     mapping_result, comm_results = perf_analysis(
    #         models_dir="models", 
    #         hwinfo=hw_info
    #     )
        
    #     config_name = f"xbar_num_{xbar_num}"
    #     hw_configs_results_xbar_num[config_name] = mapping_result
    
    # # 绘制xbar_num变动的平均性能图
    # plot_hw_configs_performance(
    #     hw_configs_results_xbar_num,
    #     xbar_num_list=xbar_num_list,
    #     config_type="xbar_num",
    #     y_label="Normalized Latency (vs MNSIM)",
    #     save_path="results4"
    # )
    
    # 实验2: 固定xbar_num=16，变动xbar_size
    # xbar_num_fixed = 16
    # xbar_size_list = [(256, 256), (256, 512), (512, 512), (1024, 512)]
    
    # hw_configs_results_xbar_size = {}
    
    # for xbar_size in xbar_size_list:
    #     print(f"\n{'='*60}")
    #     print(f"Testing with xbar_num={xbar_num_fixed}, xbar_size={xbar_size}")
    #     print('='*60)
        
    #     hw_info = make_hw_info(xbar_size, xbar_num_fixed, (0, 0), 1)
    #     mapping_result, comm_results = perf_analysis(
    #         models_dir="models", 
    #         hwinfo=hw_info
    #     )
        
    #     config_name = f"xbar_size_{xbar_size[0]}x{xbar_size[1]}"
    #     hw_configs_results_xbar_size[config_name] = mapping_result
    
    # # 绘制xbar_size变动的平均性能图
    # plot_hw_configs_performance(
    #     hw_configs_results_xbar_size,
    #     xbar_size_list=xbar_size_list,
    #     config_type="xbar_size",
    #     y_label="Normalized Latency (vs MNSIM)",
    #     save_path="results4"
    # )

    # tile2.0专属映射

    hw_info = make_hw_info((1152,256), 4, (2, 3), 1)
    mapping_result, comm_results = perf_analysis(
            models_dir="demo", 
            hwinfo=hw_info
        )
    print("\nAll experiments completed!")


# if __name__ == "__main__":
#     xbar_size = (512, 512)
#     hw_info = make_hw_info(xbar_size, 16, (0, 0), 1)
#     mapping_result, comm_results = perf_analysis(models_dir="models", hwinfo=hw_info)
#     mp.set_start_method("fork", force=True)
#     # for bw data gen
#     bw=1
#     # perf_dict = load_noc_perf(1, xbar_size)
#     # if perf_dict is None:
#     #     logger.warning("Latency dict not found, generate from mapping result...")
#     #     latency_dict, power_dict = get_noc_perf(
#     #         mapping_result, bw, xbar_size, save=True
#     #     )
#     # else:
#     #     logger.info("Latency dict found, use it.")
#     #     latency_dict, power_dict = perf_dict

#     # power_analysis(mapping_result, latency_dict, power_dict, bw, comm_result)

#     plot_perf(
#         mapping_result, None, bw, norm=1, plot_type="latency", ideal=1
#     )
#     plot_stages(
#         mapping_result, None, bw, norm=0
#     )
#     # plot_perf(
#     #     mapping_result, None, bw, norm=1, plot_type="throughput", ideal=1
#     # )