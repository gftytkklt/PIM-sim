import os
from pathlib import Path
import logging
import csv
import concurrent.futures
from onnx_analysis import analysis_model, make_opt_info

def setup_logging(log_file='regression_test.log'):
    """设置日志记录格式和级别。"""
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file, mode='w', encoding='utf-8'),
            logging.StreamHandler()
        ]
    )

def save_results_to_csv(results, output_file='test_results.csv'):
    """将测试结果保存到CSV文件。"""
    with open(output_file, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Model Name', 'opt1', 'opt2', 'Status', 'Message'])
        for result in results:
            writer.writerow(result)

def run_regression_test(models_dir='models', max_workers=4):
    """
    遍历models目录下的每个模型文件，并执行analysis_model进行测试。
    对每个模型，使用四种不同的opt_info参数配置。
    
    参数:
    models_dir (str): 模型文件所在的目录。
    max_workers (int): 并行执行的最大工作线程数。
    """
    models_path = Path(models_dir)
    
    # 检查models目录是否存在
    if not models_path.exists() or not models_path.is_dir():
        logging.error(f"模型目录 '{models_dir}' 不存在或不是一个目录。")
        return
    
    # 获取所有模型文件（可根据需要过滤文件类型）
    model_files = [f for f in models_path.iterdir() if f.is_file()]
    if not model_files:
        logging.warning(f"在目录 '{models_dir}' 中未找到任何模型文件。")
        return
    
    logging.info(f"开始回归测试，总共有 {len(model_files)} 个模型文件。")
    
    # 定义opt_info的四种配置
    opt_configs = [
        (1, 1),
        (1, 0),
        (0, 1),
        (0, 0)
    ]
    
    results = []

    def test_model(model_path, opt1, opt2):
        model_name = model_path.name
        logging.info(f"正在测试模型: {model_name} (opt_info=({opt1},{opt2}))")
        try:
            # 调用analysis_model函数
            output = analysis_model(
                path=model_path,
                hw_info=None,
                opt_info=make_opt_info(opt1, opt2)
            )
            logging.info(f"模型 '{model_name}' opt_info=({opt1},{opt2}) 测试成功。")
            return (model_name, opt1, opt2, 'Success', output)
        except FileNotFoundError as e:
            error_msg = str(e)
            logging.error(f"模型 '{model_name}' opt_info=({opt1},{opt2}) 测试失败。错误信息:\n{error_msg}")
            return (model_name, opt1, opt2, 'Failed', error_msg)
        except Exception as e:
            error_msg = str(e)
            logging.exception(f"在测试模型 '{model_name}' opt_info=({opt1},{opt2}) 时发生未预料的错误。")
            return (model_name, opt1, opt2, 'Error', error_msg)
    
    # 使用ThreadPoolExecutor进行并行测试
    with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
        # 提交所有测试任务
        future_to_test = {
            executor.submit(test_model, model, opt1, opt2): (model.name, opt1, opt2)
            for model in model_files
            for opt1, opt2 in opt_configs
        }
        
        # 处理完成的测试任务
        for future in concurrent.futures.as_completed(future_to_test):
            model_name, opt1, opt2 = future_to_test[future]
            try:
                result = future.result()
                results.append(result)
            except Exception as e:
                logging.exception(f"在测试模型 '{model_name}' opt_info=({opt1},{opt2}) 时发生未预料的错误。")
                results.append((model_name, opt1, opt2, 'Error', str(e)))
    
    # 汇总测试结果
    success_count = sum(1 for r in results if r[3] == 'Success')
    failed_count = sum(1 for r in results if r[3] == 'Failed')
    error_count = sum(1 for r in results if r[3] == 'Error')
    
    logging.info("\n回归测试总结:")
    logging.info(f"总测试次数: {len(results)}")
    logging.info(f"成功: {success_count}")
    logging.info(f"失败: {failed_count}")
    logging.info(f"错误: {error_count}")
    
    # 保存结果到CSV
    save_results_to_csv(results)
    logging.info(f"测试结果已保存到 'test_results.csv'。")

if __name__ == "__main__":
    setup_logging()
    run_regression_test(models_dir='models', max_workers=8)  # 根据系统资源调整max_workers