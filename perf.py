import os
import csv
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import numpy as np
import logging
import pickle
import time
from MappingInfo import latency_est, booksim_eval
from onnx_analysis import analysis_model, make_opt_info, load_kernel, make_hw_info

# demo for debug, models for run
def perf_analysis(models_dir='demo', hwinfo=None):
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

def get_opt_str(opt_info):
    return f"{'DP' if opt_info[0] else 'ZZ'}-{'CA' if opt_info[1] else 'XY'}"

# 主调用函数
def plot_all_comm(dict_list, comm_result):
    # 全局样式设置
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman"],
        # "axes.labelsize": 12,
        # "axes.titlesize": 14,
        # "xtick.labelsize": 10,
        # "ytick.labelsize": 10,
        # "legend.fontsize": 10,
        "axes.titlesize": 14,
        "axes.labelsize": 16,
        "xtick.labelsize": 12,
        "ytick.labelsize": 12,
        "legend.fontsize": 12,
        "hatch.linewidth": 0.5
    })
    
    # 创建子图布局
    fig, axs = plt.subplots(2, 2, figsize=(16, 9))  # 适合双栏布局的尺寸
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
              fancybox=False)
    
    # 布局优化
    plt.tight_layout(pad=2.0, w_pad=2.5, h_pad=3.0)
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
                        fontsize=8, rotation=0,
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
                      rotation=0, ha='center', rotation_mode='anchor')
    ax.set_ylabel('Normalized Value' if norm else 'Absolute Value', 
                 labelpad=8)
    
    # 网格和边框
    ax.yaxis.grid(True, linestyle=':', alpha=0.6, zorder=0)
    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(0.5)
    
    # 子图标签
    if subplot_label:
        ax.text(0.5, -0.15, subplot_label,
                transform=ax.transAxes,
                fontsize=14,
                # fontweight='bold',
                ha='center',
                va='center')
    
    return ax

# parse seg elems in this function and plot
def plot_perf(comm_segs, latency_dict=None, bw=4, norm=0, plot_type=None):
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
        "hatch.linewidth": 0.5
    })
    fig, ax = plt.subplots(figsize=(8, 4.5))  # 更适合论文栏宽的尺寸

    # 学术配色方案（ColorBrewer Set1 + 灰度扩展）
    palette = ['#4e79a7', '#f28e2b', '#e15759', '#76b7b2', '#59a14f', '#b07aa1', '#9c755f']
    hatch_patterns = ['//', '\\\\', '||', '--', '++', 'xx', 'oo']
    
    # 获取绘图数据
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))
    bar_width = 0.18  # 调整宽度适应更多分组
    inner_space = 0.2
    index = np.arange(len(models))
    
    # 绘图参数初始化
    base_values = []
    list_id = 0 if plot_type == "latency" else 1
    error_kw = dict(lw=0.8, capsize=3, capthick=0.8)  # 误差线样式
    
    # 绘制柱状图
    for i, opt in enumerate(opt_combinations):
        ideal = 1 if i == 4 else 0
        values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)] if latency_dict is not None else None, ideal=ideal)[list_id] for model in models]  # 保持原有计算逻辑
        
        # 标准化处理
        if norm and i == 0:
            base_values = values
        values = [v/b for v, b in zip(values, base_values)] if norm else values
        
        # 创建柱状图
        pos = index + i * bar_width * (1 + inner_space)
        bars = ax.bar(pos, values, bar_width,
                      color=palette[i%len(palette)],
                      edgecolor='black',
                      linewidth=0.6,
                      hatch=hatch_patterns[i%len(hatch_patterns)],
                      alpha=0.9,
                      label=f'{get_opt_str(opt)}',
                      error_kw=error_kw)
        
        # 特殊标注理想情况
        if opt == (1, 1):
            for bar in bars:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2, height, 
                        f'{height:.2f}', 
                        ha='center', va='bottom',
                        fontsize=8, rotation=0,
                        bbox=dict(facecolor='white', alpha=0.8, 
                                edgecolor='none', pad=0.2))
    
    # 坐标轴和标签优化
    ylabel = f"Normalized {plot_type}" if norm else plot_type
    ax.set_ylabel(ylabel, 
                fontsize=10, labelpad=5)
    ax.set_xlabel('Model Architectures', fontsize=10, labelpad=5)
    # ax.set_xticks(index + bar_width*(len(opt_combinations)/2))
    # ax.set_xticks(index + (len(opt_combinations)-1) * bar_width * (1 + 0.1) / 2)
    # 计算总位移量 (考虑间距系数)
    total_offset = (len(opt_combinations)-1) * bar_width * (1 + inner_space)
    
    # 核心修正：正确定位x轴刻度
    ax.set_xticks(index + total_offset/2)  # 居中定位
    ax.set_xticklabels([m.split('.')[0] for m in models], 
                     rotation=0, ha='center', rotation_mode='anchor')
    
    # 网格和边框优化
    ax.yaxis.grid(True, linestyle='--', alpha=0.6)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['bottom'].set_linewidth(0.5)
    ax.spines['left'].set_linewidth(0.5)

    legend = ax.legend(ncol=2, loc='upper left', 
                     bbox_to_anchor=(0, 1.15),
                     frameon=True,
                     fancybox=False,
                     shadow=False,
                     edgecolor='black')
    legend.get_frame().set_linewidth(0.5)
    
    # 紧凑布局并保存
    plt.tight_layout(pad=1.5)
    file_name = f'results/norm_{plot_type}_{bw}.pdf'
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

def plot_brkdown(comm_segs, latency_dict, bw=1, threshold=0.1):
    plt.rcParams["font.family"] = "Times New Roman"
    fig, axes = plt.subplots(1, 2, figsize=(12, 6))  # 1x2 子图布局
    models = list(comm_segs.keys())
    opt_combinations = [(0, 0), (1, 1)]
    bar_width = 0.6  # 加宽条形以适应单个子图
    index = np.arange(len(models))

    for i, opt in enumerate(opt_combinations):
        ax = axes[i]
        # 获取数据
        cm_pers = [latency_est(mapping_res=comm_segs[model].get(opt, 0), 
                            bus_width=bw, 
                            comm_lat=latency_dict[(model, opt)])[3:6] 
                 for model in models]
        
        cal_pers = [cm_per[0] for cm_per in cm_pers]
        merge_pers = [cm_per[2] for cm_per in cm_pers]
        lat_pers = [1 - cal_per - merge_per for cal_per, merge_per in zip(cal_pers, merge_pers)]
        
        # 绘制堆叠条形图
        bars1 = ax.barh(index, cal_pers, bar_width, color='#56B4D3', label='Compute', edgecolor='black', linewidth=0.8)
        bars2 = ax.barh(index, merge_pers, bar_width, left=cal_pers, color='#A3BE8C', label='Merge', edgecolor='black', linewidth=0.8)
        bars3 = ax.barh(index, lat_pers, bar_width, 
                       left=[c + m for c, m in zip(cal_pers, merge_pers)], 
                       color='#E69F00', label='Latency', edgecolor='black', linewidth=0.8)
        
        # 添加占比百分比标签
        for j, (cal_per, merge_per, lat_per) in enumerate(zip(cal_pers, merge_pers, lat_pers)):
            if cal_per > threshold:
                ax.text(cal_per / 2, j, f'{cal_per*100:.1f}%', ha='center', va='center', fontsize=16, color='black')
            if merge_per > threshold:
                ax.text(cal_per + merge_per / 2, j, f'{merge_per*100:.1f}%', ha='center', va='center', fontsize=16, color='black')
            if lat_per > threshold:
                ax.text(cal_per + merge_per + lat_per / 2, j, f'{lat_per*100:.1f}%', ha='center', va='center', fontsize=16, color='black')

        # 设置子图属性
        ax.set_title(get_opt_str(opt), fontsize=20)
        ax.set_yticks(index)
        models_name = [model.split('.')[0] for model in models]
        if i == 0:  # 只在左边子图显示模型标签
            ax.set_yticklabels(models_name, fontsize=20)
        else:
            ax.set_yticklabels([])
        ax.tick_params(axis='x', labelsize=20)
        ax.grid(True, axis='x', linestyle='--', alpha=0.6)

        # 设置x轴为百分比格式
        ax.xaxis.set_major_formatter(FuncFormatter(lambda x, _: f'{x*100:.0f}%'))
        sublabel = chr(97+i)  # 97是ASCII码的'a'
        ax.text(0.5, -0.1, f'({sublabel})',  # 调整y坐标控制标签位置
                transform=ax.transAxes,
                ha='center', va='center',
                fontsize=20, fontname='Times New Roman')

    # 统一图例
    handles = [bars1, bars2, bars3]
    labels = ['Compute', 'Merge', 'Trans']
    fig.legend(handles, labels, loc='upper center', 
              ncol=3, bbox_to_anchor=(0.5, 1.1),
              prop={'size': 16})

    plt.tight_layout()
    fig.savefig('results/lat_breakdown.pdf', bbox_inches='tight')

def plot_bw(comm_segs, latency_dict, bw=1):
    fig, ax = plt.subplots(figsize=(10, 6))
    plt.rcParams["font.family"] = "Times New Roman"

    # 获取所有模型名称和优化选项组合
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))
    for i, model in enumerate(models):
        values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)]) for opt in opt_combinations]
        ideal_values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)], ideal=1) for opt in opt_combinations]
        comm_lat = [value[0] - value[2] for value in values]
        ideal_lat = [value[0] - value[2] for value in values]
        ax.plot(opt_combinations, comm_lat, label=f"{model.split('.')[0]}")
    # for i, opt in enumerate(opt_combinations):
    #     values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)]) for model in models]
    #     ideal_values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)], ideal=1) for model in models]
    #     comm_lat = [value[0] - value[2] for value in values]
    #     ideal_lat = [value[0] - value[2] for value in ideal_values]
    #     util_per = [100 * ideal / lat for lat, ideal in zip(comm_lat, ideal_lat)]
        # print(f"opt={opt}, util_per={util_per}")

# use for save inter layer comm result
def save_comm_result(comm_segs, bus_width = None, xbar_size = None):
    # 获取所有模型名称和优化选项组合
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))
    latency_dict = {}
    # save latency dict
    for i, opt in enumerate(opt_combinations):
        for model in models:
            start_time = time.time()
            # latency = latency_est(SimConfig_path, inputbit, outputbit, comm_segs[model].get(opt, 0), bus_width)  # 获取每个模型对应的值
            segs = comm_segs[model].get(opt, 0).comm_segs
            latency = booksim_eval(segs, bus_width)
            latency_dict[(model, opt)] = latency  # 将latency值按模型和优化方法存储到字典
            print(f"model={model}, opt={opt}, time={time.time()-start_time}")

    filename = f"results/latency_dict_bw={bus_width}_xbar={xbar_size[0]}_{xbar_size[1]}.pkl"
    with open(filename, 'wb') as f:
        pickle.dump(latency_dict, f)
        print("Data saved.")
    return latency_dict

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
        
def load_lat_result(bw, xbar_size):
    filename = f"results/latency_dict_bw={bw}_xbar={xbar_size[0]}_{xbar_size[1]}.pkl"
    if not os.path.exists(filename):  # 检查文件是否存在
        return None
    try:
        with open(filename, 'rb') as f:
            latency_dict = pickle.load(f)
        return latency_dict
    except Exception as e:  # 捕获其他异常
        print(f"Error loading file {filename}: {e}")
        return None

if __name__ == "__main__":
    bw = 1
    xbar_size = (256, 256)
    hw_info = make_hw_info(xbar_size, 4)
    begin_time = time.time()
    mapping_result, comm_result = perf_analysis(models_dir='models', hwinfo = hw_info)
    print(f"Total Time: {time.time()-begin_time}")
    latency_dict = load_lat_result(bw, xbar_size)
    if latency_dict is None:
        print("latency dict not found. generate by mapping result...")
        latency_dict = save_comm_result(mapping_result, bw, xbar_size)
    # plot_perf(mapping_result, latency_dict, bw, norm=1, plot_type="latency")
    # plot_perf(mapping_result, latency_dict, bw, norm=1, plot_type="throughput")
    key_list = ["path_num", "datavolume", "total_hops", "total_congestion"]
    plot_all_comm(key_list, comm_result)
    for key in key_list:
        get_data_percentage(comm_result, dict_key=key)
    brkdown_stat(mapping_result, latency_dict, bw)
    plot_brkdown(mapping_result, latency_dict, bw)
    brkdown_analysis()
    # plot_bw(mapping_result, latency_dict, bw)