import os
import csv
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import PercentFormatter
import numpy as np
import logging
import pickle
import time
from MappingInfo import latency_est, booksim_eval
from onnx_analysis import analysis_model, make_opt_info, load_kernel, make_hw_info

# demo for debug, models for run
def perf_analysis(models_dir='demo', hwinfo=None, debug=False):
    models_path = Path(models_dir)

    if not models_path.exists() or not models_path.is_dir():
        logging.error(f"模型目录 '{models_dir}' 不存在或不是一个目录。")
        return
    
    model_files = [f for f in models_path.iterdir() if f.is_file()]
    if not model_files:
        logging.warning(f"在目录 '{models_dir}' 中未找到任何模型文件。")
        return
    
    logging.info(f"开始回归测试，总共有 {len(model_files)} 个模型文件。")

    opt_configs = [
        (1, 1),
        (1, 0),
        (0, 1),
        (0, 0)
    ] if not debug else [
        (1, 1),
    ]

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
                cur_opt_info=make_opt_info(opt1, opt2)
                # plot test
                comm_seg,comminfo = analysis_model(
                    kernel_list=cur_kernel,
                    hw_info=hwinfo,
                    opt_info=cur_opt_info,
                )
                if model_name not in comm_results:
                    comm_results[model_name] = {}
                if model_name not in comm_segs:
                    comm_segs[model_name] = {}
                # comm_results[model_name][(opt1, opt2)] = comminfo.path_num
                comm_results[model_name][(opt1, opt2)] = {
                    "path_num": comminfo.path_num,
                    "datavolume": comminfo.datavolume,
                    "total_hops": comminfo.total_hops,
                    "total_congestion": comminfo.total_congestion,
                }
                comm_segs[model_name][(opt1, opt2)] = comm_seg

                # perf exec
                # latency_est(SimConfig_path, inputbit, outputbit, cur_kernel, cur_opt_info) 

                success += 1
                logging.info(f"模型 '{model_name}' opt_info=({opt1},{opt2}) 测试成功。")
                print("\n")
            except FileNotFoundError as e:
                error_msg = str(e)
                logging.error(f"模型 '{model_name}' opt_info=({opt1},{opt2}) 测试失败。错误信息:\n{error_msg}")
                break
            except Exception as e:
                error_msg = str(e)
                logging.exception(f"在测试模型 '{model_name}' opt_info=({opt1},{opt2}) 时发生未预料的错误。")
                break
    print(f"成功测试 {success} 个模型。")
    return comm_segs, comm_results

def get_opt_str(opt_info, ideal = 0):
    # return f"{'DP' if opt_info[0] else 'ZZ'}-{'CA' if opt_info[1] else 'XY'}"
    if opt_info == (1, 1):
        return "This Paper" if not ideal else "Ideal"
    elif opt_info == (1, 0):
        return "SPATEM"
    elif opt_info == (0, 1):
        return "HITM"
    else:
        return "MNSIM"

# 主调用函数
def plot_all_comm(dict_list, comm_result):
    # 全局样式设置
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        "axes.titlesize": 12,    # 原14
        "axes.labelsize": 10,    # 原16
        "xtick.labelsize": 8,    # 原12
        "ytick.labelsize": 8,    # 原12
        "legend.fontsize": 9,    # 原12
        "hatch.linewidth": 0.3,  # 原0.5
        'font.weight': 'bold'
    })
    
    # 创建子图布局
    fig, axs = plt.subplots(2, 2, figsize=(7.2, 4.8))  # 适合双栏布局的尺寸
    axs = axs.flatten()
    
    # 统一配色方案和阴影模式
    palette = ['#2b83ba', '#abdda4', '#fdae61', '#d7191c']  # ColorBrewer 4-class
    hatches = ['///', '\\\\\\', '|||', '---']
    
    # 遍历数据集
    for idx, dict_key in enumerate(dict_list):
        plot_comm(comm_result, 
                 ax=axs[idx],
                 dict_key=dict_key,
                 norm=1,
                 palette=palette,
                 hatches=hatches,
                 subplot_label=f'({chr(97+idx)})')  # (a), (b) 格式

    # 统一图例
    handles, labels = axs[0].get_legend_handles_labels()
    fig.legend(handles, labels, 
              loc='lower center', 
              ncol=4,
              bbox_to_anchor=(0.5, 0.02),
              frameon=True,
              fancybox=False,
              prop={'weight': 'bold'})
    
    # 布局优化
    plt.tight_layout(pad=2.0, w_pad=2.5, h_pad=2.0)
    fig.subplots_adjust(bottom=0.15, top=0.92)
    
    # 高质量保存
    fig.savefig('results/combined_plot.pdf', dpi=600, bbox_inches='tight')
    plt.close()

def plot_comm(comm_result, ax=None, dict_key=None, norm=1, 
             palette=None, hatches=None, subplot_label=None):
    # 初始化参数
    bar_width = 0.18
    inner_space = 0.2
    index = np.arange(len(comm_result))
    
    # 获取数据
    models = list(comm_result.keys())
    opt_combinations = sorted(set(opt for opts in comm_result.values() for opt in opts))
    
    # 标准化处理
    base_values = [comm_result[model][opt_combinations[0]].get(dict_key, 1e-6) 
                  for model in models] if norm else None
    
    # 绘制柱状图
    for i, opt in enumerate(opt_combinations):
        values = [comm_result[model].get(opt, {}).get(dict_key, 0) 
                 for model in models]
        if norm and base_values:
            values = [v/b for v, b in zip(values, base_values)]
        
        # 图形属性设置
        pos = index + i * bar_width * (1 + inner_space)
        color = palette[i%len(palette)] if palette else None
        hatch = hatches[i%len(hatches)] if hatches else None
        
        bars = ax.bar(pos, values, bar_width,
                      color=color,
                      edgecolor='black',
                      linewidth=0.6,
                      hatch=hatch,
                      alpha=0.9,
                      label=f'{get_opt_str(opt)}',
                      zorder=3)
        
        # 数据标注
        # if opt == (1, 1):
        #     for bar in bars:
        #         height = bar.get_height()
        #         ax.text(bar.get_x() + bar.width/2, height*1.05,
        #                 f'{height:.2f}',
        #                 ha='center', va='bottom',
        #                 fontsize=9,
        #                 rotation=90)
        if opt == (1, 1):
            for bar in bars:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2, height, 
                        f'{height:.2f}', 
                        ha='center', va='bottom',
                        fontsize=8, rotation=0, fontweight='bold',
                        bbox=dict(facecolor='white', alpha=0.8, 
                                edgecolor='none', pad=0.2))
    
    # 坐标轴优化
    # ax.set_xticks(index + bar_width*(len(opt_combinations)/2))
    # ax.set_xticks(index + (len(opt_combinations)-1) * bar_width * (1 + 0.1) / 2)
    # 计算总位移量 (考虑间距系数)
    total_offset = (len(opt_combinations)-1) * bar_width * (1 + inner_space)
    
    # 核心修正：正确定位x轴刻度
    ax.set_xticks(index + total_offset/2)  # 居中定位
    ax.set_xticklabels([m.split('.')[0] for m in models], 
                      rotation=0, ha='center', rotation_mode='anchor', fontweight='bold')
    # ax.set_yticklabels(fontweight='bold')
    ax.set_title(f'Normalized {dict_key}', fontsize=14, fontweight='bold')
    # ax.set_ylabel('Value' if norm else 'Absolute Value', 
    #              labelpad=8)
    
    # 网格和边框
    ax.yaxis.grid(True, linestyle=':', alpha=0.6, zorder=0)
    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(0.5)
    
    # 子图标签
    if subplot_label:
        ax.text(0.5, -0.2, subplot_label,
                transform=ax.transAxes,
                fontsize=10,
                # fontweight='bold',
                ha='center',
                va='center')
    
    return ax

# parse seg elems in this function and plot
def plot_perf(mapping_results, latency_dict=None, bw=4, norm=0, plot_type=None, ideal=0):
    # 设置学术风格参数
    plt.style.use('seaborn-v0_8-paper')
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
        'font.weight': 'bold'
    })
    fig, ax = plt.subplots(figsize=(8, 4.5))  # 更适合论文栏宽的尺寸

    # 学术配色方案（ColorBrewer Set1 + 灰度扩展）
    palette = ['#4e79a7', '#f28e2b', '#e15759', '#76b7b2', '#59a14f', '#b07aa1', '#9c755f']
    hatch_patterns = ['//', '\\\\', '||', '--', '++', 'xx', 'oo']
    
    # 获取绘图数据
    models = list(mapping_results.keys())
    opt_combinations = sorted(set(opt for opts in mapping_results.values() for opt in opts))
    # add ideal option
    if ideal:
        print("Ideal case added")
        opt_combinations.append((1, 1))  # 添加理想情况
        # print(opt_combinations)
    # bar_width = 0.18  # 调整宽度适应更多分组
    # inner_space = 0.2
    n_opts = len(opt_combinations)
    max_bar_width = 0.18  # 最大柱宽
    group_width = 0.9
    bar_width = min(max_bar_width, group_width / (n_opts + (n_opts-1)*0.2))
    inner_space = bar_width * 0.2  # 间距与柱宽比例关联

    index = np.arange(len(models))
    
    # 绘图参数初始化
    base_values = []
    list_id = 0 if plot_type == "latency" else 1
    error_kw = dict(lw=0.8, capsize=3, capthick=0.8)  # 误差线样式
    
    # 绘制柱状图
    for i, opt in enumerate(opt_combinations):
        cur_ideal = 1 if i == 4 else 0
        values = [latency_est(mapping_res=mapping_results[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)] if latency_dict is not None else None, ideal=cur_ideal)[list_id] for model in models]  # 保持原有计算逻辑
        # 标准化处理
        if norm and i == 0:
            base_values = values
        values = [v/b for v, b in zip(values, base_values)] if norm else values
        # print(values)
        
        # 创建柱状图
        # pos = index + i * bar_width * (1 + inner_space)
        pos = index + i * (bar_width + inner_space)
        bars = ax.bar(pos, values, bar_width,
                      color=palette[i%len(palette)],
                      edgecolor='black',
                      linewidth=0.6,
                      hatch=hatch_patterns[i%len(hatch_patterns)],
                      alpha=0.9,
                      label=f'{get_opt_str(opt, cur_ideal)}',
                      error_kw=error_kw)
        
        # 特殊标注理想情况
        if opt == (1, 1):
            for bar in bars:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2, height, 
                        f'{height:.2f}', 
                        ha='center', va='bottom',
                        fontsize=8, rotation=0, fontweight='bold',
                        bbox=dict(facecolor='white', alpha=0.8, 
                                edgecolor='none', pad=0.2))
    
    # 坐标轴和标签优化
    ylabel = f"Normalized {plot_type}" if norm else plot_type
    ax.set_ylabel(ylabel, labelpad=5, fontweight='bold')
    # ax.set_xlabel('Model Architectures', fontsize=10, labelpad=5)
    # ax.set_xticks(index + bar_width*(len(opt_combinations)/2))
    # ax.set_xticks(index + (len(opt_combinations)-1) * bar_width * (1 + 0.1) / 2)
    # 计算总位移量 (考虑间距系数)
    total_offset = (len(opt_combinations)-1) * bar_width * (1 + inner_space)
    
    # 核心修正：正确定位x轴刻度
    ax.set_xticks(index + total_offset/2)  # 居中定位
    ax.set_xticklabels([m.split('.')[0] for m in models], 
                     rotation=0, ha='center', rotation_mode='anchor', fontweight='bold')
    # ax.set_yticklabels(fontweight='bold')
    
    # 网格和边框优化
    ax.yaxis.grid(True, linestyle='--', alpha=0.6)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['bottom'].set_linewidth(0.5)
    ax.spines['left'].set_linewidth(0.5)

    legend = ax.legend(ncol=n_opts, loc='upper left', 
                     bbox_to_anchor=(0, 1.15),
                     frameon=True,
                     fancybox=False,
                     shadow=False,
                     edgecolor='black',
                     prop={'weight': 'bold'})
    legend.get_frame().set_linewidth(0.5)
    
    # 紧凑布局并保存
    plt.tight_layout(pad=1.5)
    file_name = f'results/norm_{plot_type}_{bw}.pdf'
    fig.savefig(file_name, dpi=600, bbox_inches='tight')

# for different bw performance plot
def plot_bw_perf(mapping_results, bw_list=[1,2,4,8,16], xbar_size=(256, 256), norm=1, plot_type=None):
        # 设置学术风格参数
    plt.style.use('seaborn-v0_8-paper')
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
        'font.weight': 'bold'
    })
    fig, ax = plt.subplots(figsize=(8, 4.5))  # 更适合论文栏宽的尺寸

    # 学术配色方案（ColorBrewer Set1 + 灰度扩展）
    palette = ['#4e79a7', '#f28e2b', '#e15759', '#76b7b2', '#59a14f', '#b07aa1', '#9c755f']
    hatch_patterns = ['//', '\\\\', '||', '--', '++', 'xx', 'oo']
    
    # 获取绘图数据
    models = list(mapping_results.keys())
    opt_combinations = sorted(set(opt for opts in mapping_results.values() for opt in opts))
    index = np.arange(len(models))
    
    # 绘图参数初始化
    lat_dict = {}
    thr_dict = {}
    # list_id = 0 if plot_type == "latency" else 1

    for bw in bw_list:
        print(f"bw: {bw}")
        latency_dict, _ = load_noc_perf(bw, xbar_size)
        avg_lat = []
        avg_thr = []
        base_values = []
        for opt in opt_combinations:
            # six absolute value for different models
            values = [latency_est(mapping_res=mapping_results[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)] if latency_dict is not None else None)[0:2] for model in models]
            # latency = [v[0] for v in values]
            # throughput = [v[1] for v in values]
            # rely on baseline is the first opt option
            if norm and not base_values:
                base_values = values
            # values = [v/b for v, b in zip(values, base_values)]
            latency = [v[0]/b[0] for v, b in zip(values, base_values)] if base_values else latency
            throughput = [v[1]/b[1] for v, b in zip(values, base_values)] if base_values else throughput
            print(latency)
            print(throughput)
            avg_lat.append(np.mean(latency))
            avg_thr.append(np.mean(throughput))
        # print(bw_avgs)
        lat_dict[bw] = avg_lat
        thr_dict[bw] = avg_thr
        print(lat_dict[bw], thr_dict[bw])

    n_opts = len(opt_combinations)
    n_bw = len(bw_list)
    max_bar_width = 0.18  # 最大柱宽
    group_width = 0.8
    bar_width = min(max_bar_width, group_width / n_opts)
    inner_space = bar_width * 0.2  # 间距与柱宽比例关联
    
    index = np.arange(n_bw)
    
    # 绘制柱状图
    for i, opt in enumerate(opt_combinations):
        lat_values = [lat_dict[bw][i] for bw in bw_list]
        thr_values = [thr_dict[bw][i] for bw in bw_list]

        # 计算柱状图位置
        pos = index + i * (bar_width + inner_space)
        
        bars = ax.bar(pos, lat_values, bar_width,
                      color=palette[i % len(palette)],
                      edgecolor='black',
                      linewidth=0.6,
                      hatch=hatch_patterns[i % len(hatch_patterns)],
                      alpha=0.9,
                      label=f'{get_opt_str(opt)}')
        
        # 特殊标注理想情况
        if opt == (1, 1):
            for bar, value in zip(bars, lat_values):
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2, height, 
                        f'{value:.2f}', 
                        ha='center', va='bottom',
                        fontsize=8, rotation=0, fontweight='bold',
                        bbox=dict(facecolor='white', alpha=0.8, 
                                edgecolor='none', pad=0.2))
    
    # 坐标轴和标签优化
    ylabel = f"Normalized {plot_type}" if norm else plot_type
    ax.set_ylabel(ylabel, labelpad=5, fontweight='bold')
    ax.set_xlabel('Bus Width', fontweight='bold')
    
    # 设置横坐标标签为带宽值
    ax.set_xticks(index + (n_opts - 1) * (bar_width + inner_space) / 2)
    ax.set_xticklabels([str(bw) for bw in bw_list], 
                     rotation=0, ha='center', rotation_mode='anchor', fontweight='bold')
    
    # 网格和边框优化
    ax.yaxis.grid(True, linestyle='--', alpha=0.6)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['bottom'].set_linewidth(0.5)
    ax.spines['left'].set_linewidth(0.5)

    # 图例
    legend = ax.legend(ncol=n_opts, loc='upper left', 
                     bbox_to_anchor=(0, 1.15),
                     frameon=True,
                     fancybox=False,
                     shadow=False,
                     edgecolor='black',
                     prop={'weight': 'bold'})
    legend.get_frame().set_linewidth(0.5)
    
    # 紧凑布局并保存
    plt.tight_layout(pad=1.5)
    file_name = f'results/avg_by_bw_{plot_type}.pdf'
    fig.savefig(file_name, dpi=600, bbox_inches='tight')

def brkdown_stat(comm_segs, latency_dict, bw=1):
    models = list(comm_segs.keys())
    opt_combinations = [(0, 0), (1, 1)]
    perf_dict = {}
    for opt in opt_combinations:
        perf_dict[opt] = {}
        stats = [latency_est(mapping_res=comm_segs[model].get(opt, 0), 
                            bus_width=bw, 
                            comm_lat=latency_dict[(model, opt)])
                 for model in models]
        overall_stats = [stat[0] for stat in stats]
        cal_stats = [stat[2] for stat in stats]
        merge_stats = [stat[4] for stat in stats]
        trans_stats = [overall_stat - cal_stat - merge_stat for overall_stat, cal_stat, merge_stat in zip(overall_stats, cal_stats, merge_stats)]
        perf_dict[opt]["overall"] = overall_stats
        perf_dict[opt]["cal"] = cal_stats
        perf_dict[opt]["merge"] = merge_stats
        perf_dict[opt]["trans"] = trans_stats
    pickle.dump(perf_dict, open("results/perf_dict.pkl", "wb"))

def brkdown_analysis():
    stats = pickle.load(open("results/perf_dict.pkl", "rb"))
    base_stats = stats[(0, 0)]
    opt_stats = stats[(1, 1)]
    base_cal = base_stats["cal"]
    base_merge = base_stats["merge"]
    base_trans = base_stats["trans"]
    opt_cal = opt_stats["cal"]
    opt_merge = opt_stats["merge"]
    opt_trans = opt_stats["trans"]
    cal_improve = [(opt - base) / base for base, opt in zip(base_cal, opt_cal)]
    merge_improve = [(opt - base) / base for base, opt in zip(base_merge, opt_merge)]
    trans_improve = [(opt - base) / base for base, opt in zip(base_trans, opt_trans)]
    # print base stat, transfer to ms
    # base_cal = [f"{stat*10:.2f}ms" for stat in base_cal]
    # base_merge = [f"{stat*10:.2f}ms" for stat in base_merge]
    # base_trans = [f"{stat*10:.2f}ms" for stat in base_trans]
    # multiple by 10 to transfer to ms
    base_cal = [f"{stat*10}" for stat in base_cal]
    base_merge = [f"{stat*10}" for stat in base_merge]
    base_trans = [f"{stat*10}" for stat in base_trans]
    opt_cal = [f"{stat*10}" for stat in opt_cal]
    opt_merge = [f"{stat*10}" for stat in opt_merge]
    opt_trans = [f"{stat*10}" for stat in opt_trans]
    print("base_cal:", base_cal)
    print("base_merge:", base_merge)
    print("base_trans:", base_trans)
    # print improve stat, transfer to ms
    # opt_cal = [f"{stat*10:.2f}ms" for stat in opt_cal]
    # opt_merge = [f"{stat*10:.2f}ms" for stat in opt_merge]
    # opt_trans = [f"{stat*10:.2f}ms" for stat in opt_trans]
    print("opt_cal:", opt_cal)
    print("opt_merge:", opt_merge)
    print("opt_trans:", opt_trans)
    # transfer to percentage, reserve 2 decimal
    cal_improve = [f"{improve:.2%}" for improve in cal_improve]  
    merge_improve = [f"{improve:.2%}" for improve in merge_improve]
    trans_improve = [f"{improve:.2%}" for improve in trans_improve]
    print("cal_improve:", cal_improve)
    print("merge_improve:", merge_improve)
    print("trans_improve:", trans_improve)

def plot_brkdown(comm_segs, latency_dict, bw=1, threshold=0.1, ideal = 0):
    plt.rcParams["font.family"] = "Times New Roman"
    plt.rcParams.update({
        'font.size': 10,          # 基础字号
        'axes.titlesize': 12,     # 子图标题
        'axes.labelsize': 10,    # 坐标轴标签
        'xtick.labelsize': 9,     # x轴刻度
        'ytick.labelsize': 10,     # y轴刻度
        'font.weight': 'bold'
    })
    fig, axes = plt.subplots(1, 2, figsize=(8, 3.5))  # 1x2 子图布局
    models = list(comm_segs.keys())
    opt_combinations = [(0, 0), (1, 1)]
    bar_width = 0.6  # 加宽条形以适应单个子图
    index = np.arange(len(models))
    color_palette = ['#4C72B0', '#55A868', '#C44E52']  # 优化颜色对比度

    plt.subplots_adjust(wspace=0.25, left=0.15, right=0.95, top=0.85)

    for i, opt in enumerate(opt_combinations):
        ax = axes[i]
        # 获取数据
        cm_pers = [latency_est(mapping_res=comm_segs[model].get(opt, 0), 
                            bus_width=bw, 
                            comm_lat=latency_dict[(model, opt)], ideal=ideal)[3:6] 
                 for model in models]
        
        cal_pers = [cm_per[0] for cm_per in cm_pers]
        merge_pers = [cm_per[2] for cm_per in cm_pers]
        lat_pers = [1 - cal_per - merge_per for cal_per, merge_per in zip(cal_pers, merge_pers)]
        
        # 绘制堆叠条形图
        bars1 = ax.barh(index, cal_pers, bar_width, color=color_palette[0], label='Compute', edgecolor='black', linewidth=0.8)
        bars2 = ax.barh(index, merge_pers, bar_width, left=cal_pers, color=color_palette[1], label='Merge', edgecolor='black', linewidth=0.8)
        bars3 = ax.barh(index, lat_pers, bar_width, 
                       left=[c + m for c, m in zip(cal_pers, merge_pers)], 
                       color=color_palette[2], label='Latency', edgecolor='black', linewidth=0.8)
        
        # 优化百分比标签
        label_params = {
            'ha': 'center', 
            'va': 'center',
            'fontsize': 8,  # 缩小标签字号
            'color': 'black',
            'fontweight': 'bold'
        }
        for j, (cal_per, merge_per, lat_per) in enumerate(zip(cal_pers, merge_pers, lat_pers)):
            if cal_per > threshold:
                ax.text(cal_per/2, j, f'{cal_per*100:.0f}%', **label_params)
            if merge_per > threshold:
                ax.text(cal_per + merge_per/2, j, f'{merge_per*100:.0f}%', **label_params)
            if lat_per > threshold:
                ax.text(cal_per + merge_per + lat_per/2-0.02, j,  # 微调位置
                       f'{lat_per*100:.0f}%', **label_params)
        # 优化坐标轴设置
        ax.set_title(get_opt_str(opt), pad=10, fontsize=12, fontweight='bold')
        ax.set_xlim(0, 1.05)  # 统一x轴范围
        models_name = [model.split('.')[0] for model in models]
        ax.set_yticks(index)
        # ax.set_xticklabels(fontweight='bold')
        ax.set_yticklabels(models_name if i==0 else [], 
                          fontsize=10, 
                          fontstyle='italic', fontweight='bold')  # 斜体突出模型名称
        
        # 优化网格线
        ax.grid(True, axis='x', linestyle=':', alpha=0.4)
        ax.spines[['top', 'right']].set_visible(False)

        # 子图标签
        sublabel = chr(97+i)
        ax.text(-0.15, 1.05, f'({sublabel})',  # 调整标签位置
               transform=ax.transAxes,
               ha='left', va='bottom',
               fontsize=12, fontweight='bold')

    # 优化图例
    handles = [bars1, bars2, bars3]
    labels = ['Compute', 'Merge', 'Trans']
    fig.legend(handles, labels,
              loc='upper center',
              bbox_to_anchor=(0.5, 1.02),  # 提升图例位置
              ncol=3,
              frameon=False,
              fontsize=10,
              handletextpad=0.5,
              columnspacing=1.5,
              prop={'weight': 'bold'})

    # 最终布局调整
    plt.tight_layout(rect=[0, 0, 1, 0.95])  # 保留顶部空间
    fig.savefig('results/lat_breakdown.pdf', bbox_inches='tight')

def plot_bw(comm_segs, latency_dict, bw=1):
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams["font.family"] = "Times New Roman"
    plt.rcParams.update({
        'font.size': 10,          # 缩小基础字号
        'axes.titlesize': 11,     # 子图标题字号
        'axes.labelsize': 10,     # 坐标轴标签字号
        'xtick.labelsize': 9,     # x轴刻度字号
        'ytick.labelsize': 9,      # y轴刻度字号
        'font.weight': 'bold'
    })
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8, 5))
    # 获取所有模型名称和优化选项组合
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))
    x_ticks = range(len(opt_combinations))
    colors = plt.cm.tab10.colors  # 使用tab10调色板
    line_styles = ['-', '--', '-.', ':']
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p']
    highlight_opt = (1, 1)  # 需要标注的优化组合
    try:
        highlight_idx = opt_combinations.index(highlight_opt)
    except ValueError:
        highlight_idx = -1
    max_util = 0
    max_ideal = 0
    for idx, model in enumerate(models):
        model_name = model.split('.')[0]
        opt_name = [get_opt_str(opt) for opt in opt_combinations]
        values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)]) for opt in opt_combinations]
        ideal_values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)], ideal=1) for opt in opt_combinations]
        comm_lats = [value[0] - value[2] for value in values]
        ideal_lats = [value[0] - value[2] for value in ideal_values]
        # print(f"model={model}, comm_lats={comm_lats}, ideal_lats={ideal_lats}")
        util_per = [ideal_lat / comm_lat for ideal_lat, comm_lat in zip(ideal_lats, comm_lats)]
        # print(f"model={model}, util_per={util_per}")
        # get per in (0,0) for basevalue
        base_value = util_per[0]
        util_per = [value / base_value for value in util_per]
        # print(f"model={model.split('.')[0]}, util_per={util_per}")
        base_ideal_value = ideal_lats[0]
        ideal_per = [value / base_ideal_value for value in ideal_lats]

        # 更新最大值
        current_max_util = max(util_per)
        current_max_ideal = max(ideal_per)
        max_util = max(max_util, current_max_util)
        max_ideal = max(max_ideal, current_max_ideal)

        line1, = ax1.plot(x_ticks, util_per,
                 color=colors[idx%10],
                 linestyle=line_styles[idx//10],
                 marker=markers[idx%8],
                 markersize=8,
                 linewidth=2,
                 alpha=0.8,
                 label=model_name)
        line2, = ax2.plot(x_ticks, ideal_per,
                 color=colors[idx%10],
                 linestyle=line_styles[idx//10],
                 marker=markers[idx%8],
                 markersize=8,
                 linewidth=2,
                 alpha=0.8)
        # 标注特殊点
        if highlight_idx != -1 and highlight_idx < len(util_per):
            # 左图标注
            y_val = util_per[highlight_idx]
            ax1.text(highlight_idx+0.2, y_val, 
                     f'{y_val:.2f}x',
                    #  color=line1.get_color(),
                     color = 'black',
                     fontsize=8,
                     fontweight='bold',
                     ha='center',
                     va='bottom')
            
            # 右图标注
            y_val = ideal_per[highlight_idx]
            ax2.text(highlight_idx+0.2, y_val,
                     f'{y_val:.2f}x',
                    #  color=line2.get_color(),
                     color = 'black',
                     fontsize=8,
                    fontweight='bold',
                     ha='center',
                     va='bottom')
    for ax, title, y_max in zip([ax1, ax2], ['Normalized Bandwidth Utilization', 'Ideal Comm Latency Improvement'],[max_util*1.2, max_ideal*1.2]):
        ax.set_xticks(x_ticks)
        ax.set_xticklabels(opt_name, rotation=0, ha='center', fontweight='bold')
        # ax.set_yticklabels(fontweight='bold')
        # ax.set_xlabel('Optimization Combinations', fontsize=12)
        ax.set_ylabel('Percentage', labelpad=5, fontweight='bold')
        ax.set_ylim(top=y_max)
        ax.yaxis.set_major_formatter(PercentFormatter(1.0))  # 转换为百分比格式
        ax.grid(True, linestyle='--', alpha=0.6)
        ax.set_title(title, pad=10, fontweight='bold')
        ax.spines[['top', 'right']].set_visible(False)
    # 统一图例
    ax1.set_xlabel('(a)', fontsize=12)
    ax2.set_xlabel('(b)', fontsize=12)
    handles, labels = ax1.get_legend_handles_labels()
    fig.legend(handles, labels,
              loc='upper center',
              bbox_to_anchor=(0.5, 1.15),
              ncol=len(models),
              frameon=True,
              shadow=True,
              title="Models",
              title_fontsize=10,
              fontsize=9,
              prop = {'weight': 'bold'},
              columnspacing=1)
    plt.tight_layout()
    plt.subplots_adjust(right=0.88, wspace=0.35)
    fig.savefig('results/lat_bw.pdf', bbox_inches='tight')

def plot_grouped_bars(data1, data2, 
                     group_labels=('256x256', '128x128'),
                     bar_labels=['A', 'B', 'C', 'D'],
                     ylabel='Performance Metric',
                     save_path='grouped_bars.pdf'):
    """
    学术论文分组柱状图模板
    
    参数：
    data1 : list[4] - 第一子图的两组数据 [list1, list2]
    data2 : list[4] - 第二子图的两组数据 [list1, list2]
    group_labels : tuple - 每组数据的标签（长度2）
    bar_labels : list - 单个柱状图标签（A-D）
    ylabel : str - Y轴标签
    save_path : str - 保存路径
    """
    # 样式设置
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams.update({
        'font.family': 'Times New Roman',
        'font.size': 9,
        'axes.titlesize': 10,
        'axes.labelsize': 9,
        'xtick.labelsize': 8,
        'ytick.labelsize': 8,
        'font.weight': 'bold'
    })

    # 创建画布
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7, 3.5))  # 单栏适配尺寸
    plt.subplots_adjust(wspace=0.35)

    # 通用参数
    x = np.arange(len(bar_labels))  # 柱状图位置
    width = 0.35  # 柱宽
    colors = ['#4C72B0', '#DD8452']  # 学术蓝橙配色

    # 绘制子图1
    for idx, (d, label) in enumerate(zip(data1, group_labels)):
        bars = ax1.bar(x - width/2 + idx*width, d, width, 
               color=colors[idx], 
               edgecolor='white',
               linewidth=0.5,
               label=label)
        for bar in bars:
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height,
                     f'{height:.2f}',
                     ha='center', va='bottom',fontweight='bold',
                     fontsize=8)

    # 绘制子图2
    for idx, (d, label) in enumerate(zip(data2, group_labels)):
        bars = ax2.bar(x - width/2 + idx*width, d, width, 
               color=colors[idx], 
               edgecolor='white',
               linewidth=0.5,
               label=label)
        for bar in bars:
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height,
                     f'{height:.2f}',
                     ha='center', va='bottom',fontweight='bold',
                     fontsize=8)

    # 统一设置子图格式
    for ax, title in zip([ax1, ax2], ['Normalized latency', 'Normalized throughput']):
        ax.set_xticks(x)
        ax.set_xticklabels(bar_labels, fontweight='bold')
        # ax.set_yticklabels(fontweight='bold')
        # ax.set_ylabel(ylabel)
        ax.grid(axis='y', linestyle=':', alpha=0.4)
        ax.spines[['top', 'right']].set_visible(False)
        ax.set_title(title, pad=10, fontweight='semibold')
        
        # 添加子图标签
        ax.text(0.5, -0.1, f'({"a" if ax==ax1 else "b"})', 
               transform=ax.transAxes,
               va='top', ha='center',
               fontsize=10, fontweight='bold')
        
    # ax1.set_ylabel('Normalized latency')

    # 统一图例
    handles = [plt.Rectangle((0,0),1,1, fc=colors[i], ec='white') 
              for i in range(2)]
    fig.legend(handles, group_labels,
              loc='upper center', 
              bbox_to_anchor=(0.5, 1.05),
              ncol=2,
              frameon=False,
              fontsize=9,
              prop={'weight': 'bold'})

    # 优化布局并保存
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(save_path, dpi=300, bbox_inches='tight')
    plt.close()

def plot_xbarsize():
    mapping_result_256, _ = perf_analysis(models_dir='demo', hwinfo = make_hw_info((256, 256), 4, (0,0), 1))
    mapping_result_128, _ = perf_analysis(models_dir='demo', hwinfo = make_hw_info((128, 128), 4, (0,0), 1))
    lat_dict_256 = pickle.load(open("results/noc_perf_dict_bw=1_xbar=256_256.pkl", "rb"))
    lat_dict_128 = pickle.load(open("results/noc_perf_dict_bw=1_xbar=128_128.pkl", "rb"))
    model = 'vgg16.onnx'
    opt_combinations = sorted(set(opt for opts in mapping_result_256.values() for opt in opts))
    res_256 = [latency_est(mapping_res=mapping_result_256[model].get(opt, 0), bus_width=1, comm_lat=lat_dict_256[(model, opt)]) for opt in opt_combinations]
    res_128 = [latency_est(mapping_res=mapping_result_128[model].get(opt, 0), bus_width=1, comm_lat=lat_dict_128[(model, opt)]) for opt in opt_combinations]
    lat_256 = [res[0] for res in res_256]
    throughput_256 = [res[1] for res in res_256]
    lat_128 = [res[0] for res in res_128]
    throughput_128 = [res[1] for res in res_128]
    lat_base = lat_256[0]
    lat_256 = [lat / lat_base for lat in lat_256]
    lat_128 = [lat / lat_base for lat in lat_128]
    throughput_base = throughput_256[0]
    throughput_256 = [throughput / throughput_base for throughput in throughput_256]
    throughput_128 = [throughput / throughput_base for throughput in throughput_128]
    bar_labels = [get_opt_str(opt) for opt in opt_combinations]
    plot_grouped_bars([lat_256, lat_128], [throughput_256, throughput_128], bar_labels=bar_labels, save_path='results/xbarsize.pdf')
    # print(f"lat_256={lat_256}, throughput_256={throughput_256}")
    # print(f"lat_128={lat_128}, throughput_128={throughput_128}")


def plot_pipeline():
    mapping_result_LS, _ = perf_analysis(models_dir='models', hwinfo = make_hw_info((256, 256), 4, (0,0), 1))
    mapping_result_LP, _ = perf_analysis(models_dir='models', hwinfo = make_hw_info((256, 256), 4, (0,0), 10000))
    lat_dict_LS = pickle.load(open("results/noc_perf_dict_bw=1_xbar=256_256.pkl", "rb"))
    lat_dict_LP = pickle.load(open("results/noc_perf_dict_bw=1_xbar=256_256_LP.pkl", "rb"))
    models = list(mapping_result_LS.keys())
    index = np.arange(len(models))
    opts = [(0, 0), (1, 1)]
    base_LS = [latency_est(mapping_res=mapping_result_LS[model].get(opts[0], 0), bus_width=1, comm_lat=lat_dict_LS[(model, opts[0])]) for model in models]
    base_LP = [latency_est(mapping_res=mapping_result_LP[model].get(opts[0], 0), bus_width=1, comm_lat=lat_dict_LP[(model, opts[0])], syn=0) for model in models]
    opt_LS = [latency_est(mapping_res=mapping_result_LS[model].get(opts[1], 0), bus_width=1, comm_lat=lat_dict_LS[(model, opts[1])]) for model in models]
    opt_LP = [latency_est(mapping_res=mapping_result_LP[model].get(opts[1], 0), bus_width=1, comm_lat=lat_dict_LP[(model, opts[1])], syn=0) for model in models]
    lat_base_LS = [res[0] for res in base_LS]
    lat_base_LP = [res[0] for res in base_LP]
    lat_opt_LS = [res[0] for res in opt_LS]
    lat_opt_LP = [res[0] for res in opt_LP]
    lat_base = lat_base_LS
    lat_base_LS = [lat / base for lat, base in zip(lat_base_LS, lat_base)]
    lat_base_LP = [lat / base for lat, base in zip(lat_base_LP, lat_base)]
    lat_opt_LS = [lat / base for lat, base in zip(lat_opt_LS, lat_base)]
    lat_opt_LP = [lat / base for lat, base in zip(lat_opt_LP, lat_base)]
    throughput_base_LS = [res[1] for res in base_LS]
    throughput_base_LP = [res[1] for res in base_LP]
    throughput_opt_LS = [res[1] for res in opt_LS]
    throughput_opt_LP = [res[1] for res in opt_LP]
    throughput_base = throughput_base_LS
    throughput_base_LS = [throughput / base for throughput, base in zip(throughput_base_LS, throughput_base)]
    throughput_base_LP = [throughput / base for throughput, base in zip(throughput_base_LP, throughput_base)]
    throughput_opt_LS = [throughput / base for throughput, base in zip(throughput_opt_LS, throughput_base)]
    throughput_opt_LP = [throughput / base for throughput, base in zip(throughput_opt_LP, throughput_base)]
    lat_data = [lat_base_LS, lat_base_LP, lat_opt_LS, lat_opt_LP]
    throughput_data = [throughput_base_LS, throughput_base_LP, throughput_opt_LS, throughput_opt_LP]
    # print(f"lat_base_LS={lat_base_LS}, throughput_base_LS={throughput_base_LS}")
    # print(f"lat_base_LP={lat_base_LP}, throughput_base_LP={throughput_base_LP}")
    # print(f"lat_opt_LS={lat_opt_LS}, throughput_opt_LS={throughput_opt_LS}")
    # print(f"lat_opt_LP={lat_opt_LP}, throughput_opt_LP={throughput_opt_LP}")
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams.update({
        'font.family': 'Times New Roman',
        'font.size': 10,
        'axes.titlesize': 14,
        'axes.labelsize': 12,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'font.weight': 'bold'
    })

    # 创建画布
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 8))  # 单栏适配尺寸
    plt.subplots_adjust(wspace=0.35)

    # 绘图参数
    bar_width = 0.18  # 柱宽
    x = np.arange(len(models))  # 模型位置
    colors = ['#4C72B0', '#55A868', '#C44E52', '#8172B2']  # 学术配色方案
    patterns = ['//', 'xx', '..', '**']  # 纹理样式
    labels = ['Base-LS', 'Base-LP', 'Opt-LS', 'Opt-LP']

    # 设置对数刻度
    for ax in [ax1, ax2]:
        ax.set_yscale('log')
        ax.minorticks_off()  # 关闭次要刻度
        ax.yaxis.set_major_formatter(plt.ScalarFormatter())  # 禁用科学计数法

    # 绘制延迟子图
    for i in range(4):
        bars = ax1.bar(x + i*bar_width, lat_data[i], bar_width,
               color=colors[i],
               edgecolor='k',
               hatch=patterns[i],
               label=labels[i])
        for bar in bars:
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height,
                     f'{height:.2f}',
                     ha='center', va='bottom',fontweight='bold',
                     fontsize=8)

    # 绘制吞吐率子图
    for i in range(4):
        bars = ax2.bar(x + i*bar_width, throughput_data[i], bar_width,
               color=colors[i],
               edgecolor='k',
               hatch=patterns[i],
               label=labels[i])
        for bar in bars:
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height,
                     f'{height:.2f}',
                     ha='center', va='bottom',fontweight='bold',
                     fontsize=8)

    # 统一设置子图格式
    model_name = [model.split('.')[0] for model in models]
    for ax, title, ylabel in zip([ax1, ax2], 
                                ['Latency Comparison', 'Throughput Comparison'],
                                ['Normalized Latency', 'Normalized Throughput']):
        ax.set_xticks(x + 1.5*bar_width)
        ax.set_xticklabels(model_name, fontweight='semibold')
        # ax.set_yticklabels(fontweight='bold')
        ax.set_ylabel(ylabel,fontweight='bold')
        ax.grid(axis='y', linestyle=':', alpha=0.4)
        ax.spines[['top', 'right']].set_visible(False)
        ax.set_title(title, pad=12, fontweight='semibold')
        
        # 添加子图标签（下方居中）
        ax.text(0.5, -0.2, f'({"a" if ax==ax1 else "b"})', 
               transform=ax.transAxes,
               va='top', ha='center',
               fontsize=10, fontweight='bold')

    # 统一图例
    handles = [plt.Rectangle((0,0),1,1, fc=colors[i], ec='k', hatch=patterns[i]) 
             for i in range(4)]
    fig.legend(handles, labels,
              loc='center',
              bbox_to_anchor=(0.5, 1),
              ncol=4,
              frameon=False,
              fontsize=9,
              prop={'weight': 'bold'})

    # 优化布局并保存
    plt.tight_layout(rect=[0, 0, 1, 0.92])
    fig.savefig("results/pipeline.pdf", dpi=300, bbox_inches='tight')
    plt.close()

def power_analysis(mapping_results, latency_dict, power_dict, bw, comm_results=None):
    models = list(mapping_results.keys())
    opt_combinations = sorted(set(opt for opts in mapping_results.values() for opt in opts))
    for i, opt in enumerate(opt_combinations):
        print(f"Optimization: {get_opt_str(opt)}")
        ideal = 1 if i == 4 else 0
        time_data = [latency_est(mapping_res=mapping_results[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)] if latency_dict is not None else None, ideal=ideal)[-1] for model in models]  # 保持原有计算逻辑
        # merge_dict, trans_dict = zip(*datas)  # 解包数据
        # print(time_data)
        cur_pwr_dict = [value for (model, i), value in power_dict.items() if model in models and i == opt]  # 获取当前功耗数据
        power = [0] * len(models)
        for i in range(len(models)):
            for key in cur_pwr_dict[i].keys():
                avg_pwr = cur_pwr_dict[i][key]
                # total_time = merge_dict[i][key] + trans_dict[i][key]
                total_time = time_data[i][key]  # 使用计算得到的时间
                power[i] += avg_pwr * total_time
        # get data volume from comm_results
        if comm_results is not None:
            data_volume = [comm_results[model].get(opt, {}).get('datavolume', 0) for model in models]
            efficiency = [ 1 / p for p, dv in zip(power, data_volume)]
        print(f"Power Consumption for {get_opt_str(opt)}: {power}")
        print(f"data_volume for {get_opt_str(opt)}: {data_volume}")
        print(f"Efficiency for {get_opt_str(opt)}: {efficiency}")
    return power, data_volume, efficiency

def get_noc_perf(comm_segs, bus_width = None, xbar_size = None, save=False):
    # 获取所有模型名称和优化选项组合
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))
    latency_dict = {}
    power_dict = {}
    # save latency dict
    for i, opt in enumerate(opt_combinations):
        for model in models:
            start_time = time.time()
            # latency = latency_est(SimConfig_path, inputbit, outputbit, comm_segs[model].get(opt, 0), bus_width)  # 获取每个模型对应的值
            segs = comm_segs[model].get(opt, 0).comm_segs
            latency, power = booksim_eval(segs, bus_width)
            latency_dict[(model, opt)] = latency  # 将latency值按模型和优化方法存储到字典
            power_dict[(model, opt)] = power  # 将power值按模型和优化方法存储到字典
            print(f"model={model}, opt={opt}, time={time.time()-start_time}")
    
    if save:
        filename = f"results/noc_perf_dict_bw={bus_width}_xbar={xbar_size[0]}_{xbar_size[1]}.pkl"
        with open(filename, 'wb') as f:
            # pickle.dump(latency_dict, f)
            pickle.dump((latency_dict, power_dict), f)
            print("Data saved.")
    return latency_dict, power_dict

def load_and_plot(dict_key=None, norm=0):
    with open('results/comm_result.pkl', 'rb') as f:
        comm_result = pickle.load(f)
    plot_comm(comm_result, dict_key, norm)

def get_data_percentage(comm_result=None, dict_key=None):
    if comm_result is None:
        with open('results/comm_result.pkl', 'rb') as f:
            comm_result = pickle.load(f)
    # 获取所有模型名称和优化选项组合
    models = list(comm_result.keys())
    opt_combinations = sorted(set(opt for opts in comm_result.values() for opt in opts))
    base_values = [
        comm_result[model].get(opt_combinations[0], {}).get(dict_key, 0) 
        for model in models
    ]
    output_file = f"results/{dict_key}.csv"
    with open(output_file, 'w') as f:
        writer = csv.writer(f)
        writer.writerow(["opt_info", "values", "range", "average"])
        for _, opt in enumerate(opt_combinations):
            values = [comm_result[model].get(opt, 0).get(dict_key, 0) for model in models]  # 获取每个模型对应的值
            values = [value / base_value for value, base_value in zip(values, base_values)]
            value_range = (min(values), max(values))
            average = sum(values) / len(values)
            # 将结果写入文件
            writer.writerow([get_opt_str(opt), values, value_range, average])
            # f.write(f"opt_info={opt}, values={values}, range={value_range}\n")
            # print(f"opt_info={opt}, values={values}, range={value_range}")  # 打印到控制台（可选）
        
def load_noc_perf(bw, xbar_size):
    filename = f"results/noc_perf_dict_bw={bw}_xbar={xbar_size[0]}_{xbar_size[1]}.pkl"
    if not os.path.exists(filename):  # 检查文件是否存在
        return None
    try:
        with open(filename, 'rb') as f:
            perf_dict = pickle.load(f)
        return perf_dict
    except Exception as e:  # 捕获其他异常
        print(f"Error loading file {filename}: {e}")
        return None

if __name__ == "__main__":
    bw_list = [1, 2, 4, 8, 16]
    xbar_size = (256, 256)
    hw_info = make_hw_info(xbar_size, 8, (0,0), 1)
    begin_time = time.time()
    mapping_result, comm_result = perf_analysis(models_dir='demo', hwinfo = hw_info)
    print(f"Total Time: {time.time()-begin_time}")
    # for bw in bw_list:
    #     perf_dict = load_noc_perf(bw, xbar_size)
    #     if perf_dict is None:
    #         print("latency dict not found. generate by mapping result...")
    #         latency_dict, power_dict = get_noc_perf(mapping_result, bw, xbar_size, save=True)
    #     else:
    #         print("latency dict found. use it.")
    #         latency_dict, power_dict = perf_dict
    #     power_analysis(mapping_result, latency_dict, power_dict, bw, comm_result)
    #     plot_perf(mapping_result, latency_dict, bw, norm=1, plot_type="latency", ideal=1)
    #     plot_perf(mapping_result, latency_dict, bw, norm=1, plot_type="throughput", ideal=1)
    plot_bw_perf(mapping_result, bw_list, xbar_size)
    # key_list = ["path_num", "datavolume", "total_hops", "total_congestion"]
    # plot_all_comm(key_list, comm_result)
    # for key in key_list:
    #     get_data_percentage(comm_result, dict_key=key)
    # # brkdown_stat(mapping_result, latency_dict, bw)
    # plot_brkdown(mapping_result, latency_dict, bw, ideal=0)
    # # brkdown_analysis()
    # plot_bw(mapping_result, latency_dict, bw)
    # plot_xbarsize()
    # plot_pipeline()