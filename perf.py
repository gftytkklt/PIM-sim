import os
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import logging
import pickle
from MappingInfo import latency_est
from onnx_analysis import analysis_model, make_opt_info, load_kernel


def perf_test(models_dir='models'):
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

    home_path = os.getcwd()
    SimConfig_path = os.path.join(home_path, "SimConfig.ini")
    inputbit=8
    outputbit=8

    success = 0
    test_results = {}

    for model_path in model_files:
        model_name = model_path.name
        logging.info(f"正在测试模型: {model_name}")
        cur_kernel = load_kernel(model_path)
        for opt1, opt2 in opt_configs:
            logging.info(f"(opt_info=({opt1},{opt2}))")
            try:
                cur_opt_info=make_opt_info(opt1, opt2)
                # plot test
                _,comminfo = analysis_model(
                    kernel_list=cur_kernel,
                    hw_info=None,
                    opt_info=cur_opt_info,
                )
                if model_name not in test_results:
                    test_results[model_name] = {}
                test_results[model_name][(opt1, opt2)] = comminfo.path_num
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
    with open('results/comm_result.pkl', 'wb') as f:
        pickle.dump(test_results, f)
        print("Data saved.")
    return test_results

def plot_perf(comm_result):
    fig, ax = plt.subplots(figsize=(10, 6))
    # 获取所有模型名称和优化选项组合
    models = list(comm_result.keys())
    opt_combinations = sorted(set(opt for opts in comm_result.values() for opt in opts))
    # 设置柱状图的宽度
    bar_width = 0.2
    index = np.arange(len(models))

    # 为每个优化选项组合绘制柱状图
    for i, opt in enumerate(opt_combinations):
        values = [comm_result[model].get(opt, 0) for model in models]  # 获取每个模型对应的值
        ax.bar(index + i * bar_width, values, bar_width, label=f'opt_info={opt}')

    # 设置图形的标签和标题
    ax.set_xlabel('model')
    ax.set_ylabel('opt info')
    ax.set_title('path number under different opt_info')
    ax.set_xticks(index + bar_width * len(opt_combinations) / 2 - bar_width / 2)
    ax.set_xticklabels(models)
    ax.legend()

    

    # 显示图形
    plt.xticks(rotation=45, ha='right')  # 旋转x轴标签以适应
    plt.tight_layout()
    # save fig
    fig.savefig('results/comparison_plot.pdf', bbox_inches='tight')
    print("fig saved.")
    
    plt.show()

def load_and_plot():
    with open('results/comm_result.pkl', 'rb') as f:
        comm_result = pickle.load(f)
    plot_perf(comm_result)

if __name__ == "__main__":
    comm_result = perf_test()
    plot_perf(comm_result)
    # load_and_plot()