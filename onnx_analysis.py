import libmain
import onnx
import numpy as np
import sys
from onnx import numpy_helper,shape_inference
from dataclasses import dataclass
from typing import List, Tuple

# 设置内置的onnx模型for test
model_name = 'resnet18' # 'alexnet'  # 'yolov5m'  # 'resnet18' # 'mobilenetv2'
model_paths = {
    'alexnet': 'models/alexnet_Opset17.onnx',  # https://github.com/onnx/models/blob/main/Computer_Vision/alexnet_Opset17_torch_hub/alexnet_Opset17.onnx
    'yolov5m': 'models/yolov5m.onnx',  # https://github.com/ultralytics/yolov5/releases
    'resnet18': 'models/resnet18_Opset18.onnx',  # https://github.com/onnx/models/blob/main/Computer_Vision/resnet18_Opset18_timm/resnet18_Opset18.onnx
    'mobilenetv2': 'models/mobilenetv2-12.onnx', # https://github.com/onnx/models/tree/main/validated/vision/classification/mobilenet/model
}

# 接受命令行自定义的模型路径参数
def set_path():
    if len(sys.argv) > 1:
        path = sys.argv[1]
        return path
    else:
        return model_paths.get(model_name, 'default_model.onnx')

@dataclass
class Depinfo:
    dep_layer: int
    dep_chan: Tuple[int, int]

@dataclass
class ConvLayerInfo:
    layer: int
    wsize: List[int] 
    channel: Tuple[int, int] 
    ifmap_size: Tuple[int, int]  
    ofmap_size: Tuple[int, int]  
    depinfo: List[Depinfo]

def load_model(path):
    """加载模型并检查模型结构"""
    model = onnx.load(path)
    onnx.checker.check_model(model)
    return model

def parse_node(node):
    """解析节点信息"""
    node_info = {
        "name": node.name,
        "op_type": node.op_type,
        "input": [input for input in node.input],
        "output": [output for output in node.output],
        "attribute": {attr.name: attr.ints for attr in node.attribute}
    }
    return node_info
        
def merge_ops(model):
    """获取合并非矩阵向量乘算子到矩阵向量乘算子后的节点"""
    simplified_nodes = []
    last_conv_node = None
    for node in model.graph.node:
        # 如果是矩阵向量乘算子
        if node.op_type in ['Conv','Gemm']:
            # 保存节点信息
            last_conv_node = node
            # 添加矩阵向量乘算子到节点列表中
            simplified_nodes.append(node)
        else:
            # 检查待合并算子输入是否在当前算子输出中
            if last_conv_node:
                # 合并输出,不添加节点
                for output in node.output:
                    if output not in last_conv_node.output and output not in last_conv_node.input:
                        last_conv_node.output.append(output)
                # 卷积节点输出输入不包含待合并算子输入时合并输入
                for input in node.input:
                    if input not in last_conv_node.output and input not in last_conv_node.input:
                        last_conv_node.input.append(input)
        
        # 如果是不影响特征图输出的非矩阵向量乘算子，则直接不添加节点
        # elif node.op_type in []:
        #     pass
        # else:
        #     print(f"Warning: Unrecognized op type: {node.op_type}")
    # 返回合并后的节点列表
    return simplified_nodes

def get_shape_from_value_info(value_info):
    shape = []
    for dim in value_info.type.tensor_type.shape.dim:
        if dim.dim_value > 0:
            shape.append(dim.dim_value)
        else:
            shape.append(-1)  # 未指定的维度
    return shape

def infer_shapes(model):
    inferred_model = shape_inference.infer_shapes(model)
    shapes = {}
    for value_info in list(inferred_model.graph.value_info) + list(inferred_model.graph.output) + list(inferred_model.graph.input):
        shape = get_shape_from_value_info(value_info)
        shapes[value_info.name] = shape
    return shapes
def get_channel_size(shape):
    if len(shape) == 4:
        N, C, H, W = shape
        feature_map_size = (H, W)
        channel = C
    elif len(shape) == 3:
        N, H, W = shape
        channel = 1
        feature_map_size = (H, W)
    elif len(shape) == 2:
        N, W = shape
        channel = 1
        feature_map_size = ( N, W)
    elif len(shape) == 1:
        W = shape[0]
        channel = 1
        feature_map_size = (1, W)
    else:
        channel = -1
        feature_map_size = (-1, -1)
    return channel, feature_map_size

def build_conv_info(model):
    shapes = infer_shapes(model)
    # print(shapes)
    conv_layers = merge_ops(model)
    # for layer in conv_layers:
    #     print(f"Layer {layer.name}:")
    #     print(f"type:{layer.op_type}")
    #     print(f"Input:{layer.input}")
    #     print(f"Output:{layer.output}\n")
    # print(shapes)
    conv_info_list = []
    # 为所有节点分配索引
    #conv_name_to_index = {}
    # for idx, conv in enumerate(conv_layers):
    #     conv_name_to_index[conv.name] = idx

    for idx, conv in enumerate(conv_layers):
        # 获取卷积核维度
        W_name = conv.input[1]  # 假设第二个输入是权重
        W = None
        for tensor in model.graph.initializer:
            if tensor.name == W_name:
                W=tensor
                break
        if W is None:
            print(f"警告：未找到权重 {W_name} 对应的初始化数据。")
            wsize = [-1,-1]
        else:
            if len(W.dims) == 2: 
                wsize = [1,1]
            if len(W.dims) == 4:
                wsize = list(W.dims[2:])  # 例如，Conv的权重形状是 [输出通道, 输入通道, 高度, 宽度]
        # print(W.dims)
        # 判断两种卷积核获取方式结果是否相同(Gemm节点无法从节点中读取卷积核大小)    
        if parse_node(conv)["attribute"].get("kernel_shape",None) and wsize != parse_node(conv)["attribute"]["kernel_shape"]:
            print("Warning: kernel shape is not equal to the shape of the weight")
            
        # 获取输入输出特征图大小

        if idx < len(conv_layers)-1:

            input_name = conv_layers[idx].input[0]
            output_name = None
            for name in conv_layers[idx+1].input:
                if name in conv_layers[idx].output:
                    output_name = name
                    break
            if not output_name:
                print(f"Warning: conv_layers {idx+1} has no input from conv_layers {idx}")
                output_name = conv_layers[idx].output[len(conv_layers[idx].output)-1]

            input_shape = shapes.get(input_name, (-1, -1))  # (N, C, H, W)
            output_shape = shapes.get(output_name, (-1, -1))  # (N, C, H, W)

        # 最后一个节点
        else:
            input_name = conv_layers[idx].input[0]
            input_shape = shapes.get(input_name, (-1, -1))
            output_name = model.graph.output[0].name
            output_shape = shapes.get(output_name, (-1, -1))  # (N, C, H, W)
        # print(input_shape)
        # print(output_shape)
        # print("\n")

        # 输入
        input_channel, input_feature_map_size = get_channel_size(input_shape)
        # 输出
        output_channel, output_feature_map_size = get_channel_size(output_shape)
        # 初始化 ConvLayerInfo
        conv_info = ConvLayerInfo(
            layer=idx,
            wsize=wsize,
            channel=[input_channel, output_channel],
            ofmap_size=output_feature_map_size,
            ifmap_size=input_feature_map_size,
            depinfo=[]
        )
        conv_info_list.append(conv_info)
    
    # 查找输入建立依赖关系
    for idx, conv in enumerate(conv_layers):
        for input_name in conv.input:
            # 查找这个输入是来自哪个卷积层
            for src_idx, src_conv in enumerate(conv_layers):
                for src_output_name in src_conv.output:
                    if src_output_name == input_name:
                        # 假设所有输出通道都被传递
                        src_channels = get_channel_size(shapes.get(src_output_name))[0]
                        depinfo = Depinfo(
                            dep_layer=idx,
                            dep_chan=(1, src_channels)  # 假设通道从1开始编号
                        )
                        if depinfo not in conv_info_list[src_idx].depinfo:
                            conv_info_list[src_idx].depinfo.append(depinfo)
        if idx == len(conv_layers)-1:
            depinfo = Depinfo(dep_layer=-1, dep_chan=(0, 0))  # 最后一个节点没有依赖
            conv_info_list[idx].depinfo.append(depinfo)
            break

    # 查找输出建立依赖关系
    # for idx, conv in enumerate(conv_layers):
    #     output_name = None
    #     if idx < len(conv_layers)-1:
    #         for id in range(idx + 1, len(conv_layers)):
    #             for name in conv_layers[id].input:
    #                 if name in conv_layers[idx].output:
    #                     output_name = name
    #                     break
    #         # 查找这个输出流入哪个卷积层
    #         for src_idx, src_conv in enumerate(conv_layers):
    #             for src_input_name in src_conv.input:
    #                 if src_input_name == output_name:
    #                     # 假设所有输出通道都被传递
    #                     src_channels = get_channel_size(shapes.get(src_input_name))[0]
    #                     depinfo = Depinfo(
    #                         dep_layer=src_idx,
    #                         dep_chan=(1, src_channels)  # 假设通道从1开始编号
    #                     )
    #                     if depinfo not in conv_info_list[idx].depinfo:
    #                         conv_info_list[idx].depinfo.append(depinfo)
    #     if idx == len(conv_layers)-1:
    #         depinfo = Depinfo(dep_layer=-1, dep_chan=(0, 0))  # 最后一个节点没有依赖
    #         conv_info_list[idx].depinfo.append(depinfo)
    #         break

    return conv_info_list

def convert_to_cpp(conv_info_list):
    # 将conv_info_list转换为C++可以接受的格式
    nnkernel_list = []
    for conv_info in conv_info_list:
        nnkernel = libmain.NNkernel()
        nnkernel.layer = conv_info.layer
        nnkernel.wsize = (conv_info.wsize[0], conv_info.wsize[1])
        nnkernel.channel = (conv_info.channel[0], conv_info.channel[1])
        nnkernel.ifmap_size = (conv_info.ifmap_size[0], conv_info.ifmap_size[1])
        nnkernel.ofmap_size = (conv_info.ofmap_size[0], conv_info.ofmap_size[1])
        
        depinfo_list = []
        for dep in conv_info.depinfo:
            depinfo = libmain.Depinfo()
            depinfo.dep_layer = dep.dep_layer
            depinfo.dep_chan = dep.dep_chan
            depinfo_list.append(depinfo)
        nnkernel.depinfo = depinfo_list
        nnkernel_list.append(nnkernel)
    return nnkernel_list

def make_default_hw_info():
    hw_info = libmain.HWInfo()
    hw_info.xbar_size = (64, 256) # 256 / (8 / 2)
    hw_info.xbar_num = 16
    hw_info.tile_size = (0, 0)
    hw_info.pipeline_depth = 1
    return hw_info

def make_default_opt_info():
    opt_info = libmain.OptInfo()
    opt_info.mapping_opt = 1
    opt_info.sched_opt = 1
    return opt_info

def make_opt_info(map=1, sched=1):
    opt_info = libmain.OptInfo()
    opt_info.mapping_opt = map
    opt_info.sched_opt = sched
    return opt_info

def analysis_model(path=None, hw_info=None, opt_info=None):
    path = path or set_path()
    model = load_model(path)
    print(f'Loading model: {path}\n')
    conv_info_list = build_conv_info(model)
    nnkernel_list = convert_to_cpp(conv_info_list)
    hw_info = hw_info or make_default_hw_info()
    opt_info = opt_info or make_default_opt_info()
    return libmain.analyze(nnkernel_list, hw_info, opt_info)

def main():
    """主函数"""
    res = analysis_model()
    # print(res.deploy_info[-1].tile_id)
    # 加载模型
    # model = load_model(path)
    # print(f'Loading model: {path}\n')
    
    # 打印模型信息
    # print(onnx.helper.printable_graph(model.graph))

    # 获取模型中的卷积层
    # conv_info_list = build_conv_info(model)
    
    #将提取的卷积节点信息打印
    # for conv_info in conv_info_list:
    #     print(f"Conv Layer {conv_info.layer}:")
    #     print(f"  Kernel Dims: {conv_info.wsize}")
    #     print(f"  Input Channel: {conv_info.channel[0]}, Output Channel: {conv_info.channel[1]}")
    #     print(f"  Input Feature Map Size: {conv_info.ifmap_size}")
    #     print(f"  Output Feature Map Size: {conv_info.ofmap_size}")
    #     print(f"  Dependencies:")
    #     for dep in conv_info.depinfo:
    #         print(f"    - Destination Index: {dep.dep_layer}, Channel Indices: {dep.dep_chan}")

    # 将conv_info_list转换为C++可以接受的格式
    # nnkernel_list = convert_to_cpp(conv_info_list)

    # Call the C++ function
    # if libmain.test(nnkernel_list) == 114514: # 114514 is a placeholder for success
    #     print("HOMO!")
    #     print("Program finished.")
    # else:
    #     print("Program failed.")

    # test analysis result getter
    # res = libmain.analyze(nnkernel_list)
    # print(res.deploy_info[-1].tile_id)

# run main
if __name__ == "__main__":
    main()