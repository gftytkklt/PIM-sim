"""生成两层1×1 Conv的可手算ONNX模型，无需PyTorch或下载。"""
import argparse
from pathlib import Path
import numpy as np
import onnx
from onnx import helper, numpy_helper, TensorProto


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    graph = helper.make_graph([
        helper.make_node('Conv',['x','w0'],['mid'],name='conv0'),
        helper.make_node('Conv',['mid','w1'],['y'],name='conv1')], 'two-conv',
        [helper.make_tensor_value_info('x',TensorProto.FLOAT,[1,1,1,1])],
        [helper.make_tensor_value_info('y',TensorProto.FLOAT,[1,1,1,1])],
        [numpy_helper.from_array(np.ones((1,1,1,1),dtype=np.float32),name=n) for n in ['w0','w1']])
    model = helper.make_model(graph,opset_imports=[helper.make_opsetid('',13)])
    onnx.checker.check_model(model)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('xb') as handle:
        handle.write(model.SerializeToString())


if __name__ == '__main__':
    main()
