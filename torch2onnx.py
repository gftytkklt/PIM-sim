import torch
import onnx
from onnx_analysis import main
import importlib
# 定义待转换的模块名称和模型类名
module_name = "pytorch.mc_cnn_fast"
model_name = "FastMcCnn"
# 动态导入模块
_module = importlib.import_module(module_name)
# 从模块中获取类
_model = getattr(_module, model_name)

# 初始化一个模型，可能需要根据实际情况调整参数
torch_model = _model()
# 定义数据
dummy_input = torch.randn(64,64)
# 定义生成的onnx模型路径，可以根据需要修改
onnx_path = "models/"+model_name+".onnx"

# unstable 
# onnx_program = torch.onnx.dynamo_export(torch_model, dummy_input)
# onnx_program.save(onnx_path)
# print('Model has been converted to ONNX') 


#Function to Convert to ONNX 
def Convert_ONNX(): 

    # set the model to inference mode 
    torch_model.eval() 

    # 定义初始化参数   
    torch.onnx.export(torch_model,         # model being run 
         dummy_input,       # model input (or a tuple for multiple inputs) 
         onnx_path,       # where to save the model
         opset_version = 18,
         input_names = ['Input'],   # the model's input names 
         output_names = ['Output'], # the model's output names 
) 
    print(" ") 
    print('Model has been converted to ONNX') 

# Convert the model to ONNX
Convert_ONNX() 

# Load the ONNX model
onnx_model = onnx.load(onnx_path)

# 验证模型是否符合 ONNX 规范
onnx.checker.check_model(onnx_model)

print("ONNX 模型验证通过!")
# 调用 main 函数进行后续解析
main(onnx_path)