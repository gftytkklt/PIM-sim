import os
from pathlib import Path
import logging
from MappingInfo import latency_est
from onnx_analysis import analysis_model, make_opt_info

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
        # (1, 0),
        # (0, 1),
        # (0, 0)
    ]

    home_path = os.getcwd()
    SimConfig_path = os.path.join(home_path, "SimConfig.ini")
    inputbit=8
    outputbit=8

    success = 0

    for model in model_files:
        model_name = model.name
        logging.info(f"正在测试模型: {model_name}")
        for opt1, opt2 in opt_configs:
            logging.info(f"(opt_info=({opt1},{opt2}))")
            try:
                # output = analysis_model(
                #     path=model,
                #     hw_info=None,
                #     opt_info=make_opt_info(opt1, opt2)
                # )
                opt_info=make_opt_info(opt1, opt2)
                latency_est(SimConfig_path, inputbit, outputbit, model, opt_info)
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

if __name__ == "__main__":
    perf_test()