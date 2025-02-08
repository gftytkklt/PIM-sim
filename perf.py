import os
import csv
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import logging
import pickle
import time
from MappingInfo import latency_est, booksim_eval
from onnx_analysis import analysis_model, make_opt_info, load_kernel, make_hw_info

# demo for debug, models for run
def perf_test(models_dir='demo', hwinfo=None):
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
    # with open('results/comm_result.pkl', 'wb') as f:
    #     pickle.dump(comm_results, f)
    #     print("Data saved.")
    # with open('results/analy_res.pkl', 'wb') as f:
    #     pickle.dump(comm_segs, f)
    #     print("Data saved.")
    return comm_segs, comm_results

def get_opt_str(opt_info):
    return f"{'DP' if opt_info[0] else 'ZZ'}-{'CA' if opt_info[1] else 'XY'}"

def plot_comm(comm_result, dict_key=None, norm=1):
    fig, ax = plt.subplots(figsize=(10, 6))
    # 获取所有模型名称和优化选项组合
    models = list(comm_result.keys())
    opt_combinations = sorted(set(opt for opts in comm_result.values() for opt in opts))
    # 设置柱状图的宽度
    bar_width = 0.2
    index = np.arange(len(models))

    # 为每个优化选项组合绘制柱状图
    if norm:
        base_values = [
            comm_result[model].get(opt_combinations[0], {}).get(dict_key, 0) 
            for model in models
        ]
    for i, opt in enumerate(opt_combinations):
        values = [comm_result[model].get(opt, 0).get(dict_key, 0) for model in models]  # 获取每个模型对应的值
        if norm:
            values = [value / base_value for value, base_value in zip(values, base_values)]
        ax.bar(index + i * bar_width, values, bar_width, label=f'{get_opt_str(opt)}')

    # 设置图形的标签和标题
    # ax.set_xlabel('model')
    # ax.set_ylabel('value')
    title = f'Normalized {dict_key} under different opt_info' if norm else f'{dict_key} under different opt_info'
    ax.set_title(title)
    ax.set_xticks(index + bar_width * len(opt_combinations) / 2 - bar_width / 2)
    models_name = [model.split('.')[0] for model in models]
    ax.set_xticklabels(models_name)
    # 设置y轴范围，确保有足够的空间给标签
    ax.set_ylim(0, 1.4)  # 增加y轴的上限
    ax.legend(loc='upper right', bbox_to_anchor=(1, 1))

    # 显示图形
    plt.xticks(rotation=45, ha='right')  # 旋转x轴标签以适应
    # save fig
    file_name = f'results/norm_{dict_key}.pdf' if norm else f'results/{dict_key}.pdf'
    fig.savefig(file_name, bbox_inches='tight')
    print("fig saved.")
    
    # plt.show()

# parse seg elems in this function and plot
def plot_perf(comm_segs, latency_dict=None, bw=4, norm=0):
    fig, ax = plt.subplots(figsize=(10, 6))
    # 获取所有模型名称和优化选项组合
    models = list(comm_segs.keys())
    opt_combinations = sorted(set(opt for opts in comm_segs.values() for opt in opts))
    opt_combinations.append((1, 1))
    # 设置柱状图的宽度
    bar_width = 0.18
    index = np.arange(len(models))

    # 为每个优化选项组合绘制柱状图
    base_values = []
    ideal = 0
    for i, opt in enumerate(opt_combinations):
        if i == 4:
            ideal = 1
        values = [latency_est(mapping_res=comm_segs[model].get(opt, 0), bus_width=bw, comm_lat=latency_dict[(model, opt)] if latency_dict is not None else None, ideal=ideal) for model in models]  # 获取每个模型对应的值
        if norm and i == 0:
            base_values = values
        values = [value / base_value for value, base_value in zip(values, base_values)]
        ax.bar(index + i * bar_width, values, bar_width, label=f'{get_opt_str(opt)}')
    
    dict_key = "latency_"+str(bw)
    title = f'Normalized {dict_key} under different opt_info' if norm else f'{dict_key} under different opt_info'
    ax.set_title(title)
    ax.set_xticks(index + bar_width * len(opt_combinations) / 2 - bar_width / 2)
    models_name = [model.split('.')[0] for model in models]
    ax.set_xticklabels(models_name)
    # 设置y轴范围，确保有足够的空间给标签
    ax.legend(loc='upper right', bbox_to_anchor=(1, 1))

    # 显示图形
    plt.xticks(rotation=45, ha='right')  # 旋转x轴标签以适应
    # save fig
    file_name = f'results/norm_{dict_key}.pdf' if norm else f'results/{dict_key}.pdf'
    fig.savefig(file_name, bbox_inches='tight')
    print("fig saved.")
    
    # plt.show()
# use for save inter layer comm result, deprecated now
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
    bw_list = [1, 2, 4, 8]
    xbar_size = (256, 256)
    hw_info = make_hw_info(xbar_size, 4)
    begin_time = time.time()
    result, _ = perf_test(models_dir='models', hwinfo = hw_info)
    print(f"Total Time: {time.time()-begin_time}")
    for bw in bw_list:
        begin_time = time.time()
        print(f"bw={bw}")
        save_comm_result(result, bw, xbar_size)
        print(f"Total Time: {time.time()-begin_time}")
        # latency_dict = load_lat_result(bw, xbar_size)
        # plot_perf(result, latency_dict, bw, norm=1)
    # opt_info = (0, 0)
    # test model by model
    # model_name = "alexnet.onnx"
    # opt_list = [(1, 1), (1, 0), (0, 1), (0, 0)]
    # for opt_info in opt_list:
    #     comm_seg = comm_segs[model_name][(opt_info)]
    #     latency_map = latency_dict[(model_name, opt_info)]
    #     latency = latency_est(bus_width=bw,mapping_res=comm_seg,comm_lat=latency_map)
    #     print(f"model={model_name}, opt_info={opt_info}, latency={latency}")
    # save comm result
    # xbar = [(256, 256), (512, 256), (1024, 512), (1152, 1024)]
    # hw_infos = [make_hw_info(xbar) for xbar in xbar]
    # for hw_info in hw_infos:
    #     comm_segs, comm_result = perf_test(models_dir='models', hwinfo = hw_info)
    #     begin_time = time.time()
    #     bus_width = [1,2,4,6,8]
    #     for _, bw in enumerate(bus_width):
    #         save_comm_result(comm_segs, bw, hw_info.xbar_size)
    #     print(f"Total Time: {time.time()-begin_time}")
    # with open('results/latency_dict.pkl', 'rb') as f:
    #     load_data = pickle.load(f)
    # print(load_data)
    # latency=load_data[('alexnet.onnx', (1, 1))]
    # print(latency)
    # key_list = ["path_num", "datavolume", "total_hops", "total_congestion"]
    # for key in key_list:
    #     plot_comm(comm_result, key)
        # load_and_plot(key, 1)
        # get_data_percentage(comm_result=None, dict_key=key)
    # plot_comm(comm_result)
    # load_and_plot()