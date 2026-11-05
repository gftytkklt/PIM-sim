import pimapping
import onnx
import sys
from onnx import shape_inference
from dataclasses import dataclass
from typing import List, Tuple
from collections import Counter
from math import prod
from hashlib import sha256
import json
from . import paths

# 设置内置的onnx模型for test
model_name = 'resnet18' # 'alexnet'  # 'yolov5m'  # 'resnet18' # 'mobilenetv2'
model_paths = {
    'alexnet': paths.MODELS_DIR / 'alexnet.onnx',
    'yolov5m': paths.MODELS_DIR / 'yolov5m.onnx',
    'resnet18': paths.MODELS_DIR / 'resnet18.onnx',
    'mobilenetv2': paths.MODELS_DIR / 'mobilenetv2.onnx',
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
    dst_chan: Tuple[int, int] = (0, 0)
    elements: int = -1
    tensor_id: str = ""

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

class ModelParsingError(ValueError):
    """The ONNX graph cannot be represented faithfully by the current NNkernel IR."""


def parse_node(node):
    return {
        "name": node.name,
        "op_type": node.op_type,
        "input": list(node.input),
        "output": list(node.output),
        "attribute": {a.name: onnx.helper.get_attribute_value(a) for a in node.attribute},
    }


def get_shape_from_value_info(value_info):
    return [d.dim_value if d.dim_value > 0 else -1
            for d in value_info.type.tensor_type.shape.dim]


def infer_shapes(model):
    inferred = shape_inference.infer_shapes(model, strict_mode=True)
    shapes = {v.name: get_shape_from_value_info(v)
              for v in list(inferred.graph.value_info) + list(inferred.graph.output)
              + list(inferred.graph.input)}
    shapes.update({t.name: list(t.dims) for t in inferred.graph.initializer})
    return shapes


def get_channel_size(shape):
    """Return (channels, spatial shape), for static single-batch NCHW or NC."""
    if shape is None or any(d <= 0 for d in shape):
        raise ModelParsingError(f"Unknown or dynamic tensor shape: {shape}")
    if len(shape) not in (2, 4) or shape[0] != 1:
        raise ModelParsingError(f"Only batch=1 NCHW/NC tensors are supported: {shape}")
    return shape[1], tuple(shape[2:]) if len(shape) == 4 else (1, 1)


def _project_model_strict(model):
    """Project a supported ONNX graph without modifying its protobuf.

    This is a workload/shape projection, not numerical execution. Fused unary
    operations have no independent timing in NNkernel. Multi-input arithmetic,
    channel remapping and grouped convolution require a richer IR and are rejected.
    """
    onnx.checker.check_model(model)
    shapes = infer_shapes(model)
    weights = {t.name: t for t in model.graph.initializer}
    constants = set(weights)
    owners = {v.name: None for v in model.graph.input if v.name not in constants}
    infos, fused_nodes, edges, boundaries = [], [], [], {}
    unary_ops = {"Identity", "Relu", "LeakyRelu", "Sigmoid", "Tanh", "Clip",
                 "BatchNormalization", "Flatten", "Reshape"}

    def fail(node, reason):
        raise ModelParsingError(f"{node.name or node.op_type} ({node.op_type}): {reason}")

    def shape(name, node):
        value = shapes.get(name)
        if value is None or any(d <= 0 for d in value):
            fail(node, f"tensor {name!r} has unknown/dynamic shape {value}")
        return value

    def boundary(owner, name):
        if owner is not None:
            boundaries.setdefault(owner, []).append(name)

    for node in model.graph.node:
        if node.domain not in ("", "ai.onnx"):
            fail(node, f"unsupported operator domain {node.domain!r}")
        attrs = parse_node(node)["attribute"]
        if node.op_type == "Constant":
            constants.update(node.output)
            continue
        if node.op_type in ("Conv", "Gemm"):
            if len(node.input) < 2 or node.input[1] not in weights:
                fail(node, "weights must be a static initializer")
            if node.input[0] not in owners:
                fail(node, "data input has no supported tensor producer")
            if len(node.output) != 1:
                fail(node, "only one data output is supported")
            if len(node.input) > 2 and node.input[2] and node.input[2] not in constants:
                fail(node, "dynamic bias is not representable")
            in_shape, out_shape = shape(node.input[0], node), shape(node.output[0], node)
            cin, ifmap = get_channel_size(in_shape)
            cout, ofmap = get_channel_size(out_shape)
            dims = list(weights[node.input[1]].dims)
            if node.op_type == "Conv":
                if attrs.get("group", 1) != 1:
                    fail(node, "group/depthwise convolution needs explicit group mapping")
                if len(in_shape) != 4 or len(out_shape) != 4 or len(dims) != 4:
                    fail(node, "only 2D NCHW convolution is supported")
                if dims[:2] != [cout, cin]:
                    fail(node, "weight channels disagree with input/output shapes")
                wsize = dims[2:]
                if attrs.get("kernel_shape", wsize) != wsize:
                    fail(node, "kernel_shape disagrees with initializer")
            else:
                if len(in_shape) != 2 or len(out_shape) != 2 or len(dims) != 2:
                    fail(node, "Gemm requires rank-2 data and weights")
                if attrs.get("transA", 0) != 0:
                    fail(node, "transA requires explicit layout mapping")
                if attrs.get("transB", 0) not in (0, 1):
                    fail(node, "transB must be zero or one")
                if attrs.get("alpha", 1.0) != 1.0 or attrs.get("beta", 1.0) != 1.0:
                    fail(node, "non-unit Gemm scaling needs an explicit execution model")
                expected = [cout, cin] if attrs.get("transB", 0) else [cin, cout]
                if dims != expected:
                    fail(node, "Gemm weight dimensions disagree with transB/input/output")
                wsize = [1, 1]
            owner = owners[node.input[0]]
            boundary(owner, node.input[0])
            idx = len(infos)
            if owner is not None:
                if infos[owner].channel[1] != cin:
                    fail(node, "input channel remapping is not represented by DepInfo")
                edges.append((owner, idx))
            infos.append(ConvLayerInfo(idx, wsize, (cin, cout), ifmap, ofmap, []))
            copy = onnx.NodeProto()
            copy.CopyFrom(node)
            fused_nodes.append(copy)
            owners[node.output[0]] = idx
        elif node.op_type in unary_ops:
            if not node.input or node.input[0] not in owners:
                fail(node, "data input has no supported tensor producer")
            if any(name and name not in constants for name in node.input[1:]):
                fail(node, "only constant auxiliary inputs can be fused")
            if len(node.output) != 1:
                fail(node, "auxiliary outputs (e.g. pooling indices) are unsupported")
            before, after = shape(node.input[0], node), shape(node.output[0], node)
            cin, spatial_in = get_channel_size(before)
            cout, spatial_out = get_channel_size(after)
            if cin != cout:
                fail(node, "channel/layout transformation requires a richer dependency IR")
            if spatial_in != spatial_out:
                fail(node, "shape-changing fusion needs separate MVM and fused output shapes")
            owner = owners[node.input[0]]
            if owner is None and node.op_type not in ("Identity", "Flatten", "Reshape"):
                fail(node, "preprocessing before the first MVM has no execution owner")
            owners[node.output[0]] = owner
            if owner is not None:
                fused_nodes[owner].output.extend(node.output)
        else:
            fail(node, "unsupported operator; no implicit fusion is performed")

    if not infos:
        raise ModelParsingError("The model contains no supported Conv/Gemm workload")
    for output in model.graph.output:
        if output.name not in owners:
            raise ModelParsingError(f"Output {output.name!r} has no supported producer")
        boundary(owners[output.name], output.name)
    for idx, info in enumerate(infos):
        # One NNkernel has one output shape. Branches with incompatible post-op
        # shapes cannot be merged into that single description.
        outputs = boundaries.get(idx, [fused_nodes[idx].output[-1]])
        layouts = {get_channel_size(shapes.get(name)) for name in outputs}
        if len(layouts) != 1:
            raise ModelParsingError(f"Layer {idx}: branches need different fused output shapes")
        channels, spatial = layouts.pop()
        if channels != info.channel[1]:
            raise ModelParsingError(f"Layer {idx}: fused output changes channel indexing")
        info.ofmap_size = spatial
    for source, dest in sorted(set(edges)):
        infos[source].depinfo.append(Depinfo(dest, (1, infos[source].channel[1])))
    for info in infos:
        if not info.depinfo:
            info.depinfo.append(Depinfo(-1, (0, 0)))
    return infos, fused_nodes


@dataclass(frozen=True)
class _Origin:
    """One MVM's contribution to a tensor, using inclusive channel intervals."""
    source: int
    src: Tuple[int, int]
    channels: Tuple[int, int]
    elements: int
    tensor_id: str


def _tensor_layout(shape):
    # Non-MVM terminal tensors can have any rank. Their layout is only used to
    # continue dependency projection; MVM boundaries still require NC/NCHW.
    if shape and len(shape) >= 2 and shape[0] == 1 and all(d > 0 for d in shape):
        return shape[1], prod(shape[2:])
    return None


def _project_model(model, projection_report=None):
    """Keep Conv/Gemm workloads and project intervening tensor dependencies.

    Non-MVM operations do not allocate compute nodes or contribute latency.
    Pool/resize volumes, channel concatenation and contiguous flattening are
    represented explicitly. Other valid operators retain their data ancestors
    and are recorded as dependency-only fallbacks rather than being discarded.
    """
    onnx.checker.check_model(model)
    shapes = infer_shapes(model)
    weights = {t.name: t for t in model.graph.initializer}
    origins = {value.name: [] for value in model.graph.input}
    origins.update({name: [] for name in weights})
    infos, nodes = [], []
    skipped, fallback = Counter(), []
    pointwise = {"Identity", "Relu", "LeakyRelu", "Sigmoid", "Tanh", "Clip",
                 "BatchNormalization", "Dropout", "Elu", "Selu", "Softmax",
                 "HardSigmoid", "HardSwish", "PRelu"}
    arithmetic = {"Add", "Sub", "Mul", "Div", "Pow", "Max", "Min", "Sum"}
    spatial = {"MaxPool", "AveragePool", "GlobalAveragePool", "GlobalMaxPool", "Resize", "Upsample"}

    def fail(node, reason):
        raise ModelParsingError(f"{node.name or node.op_type} ({node.op_type}): {reason}")

    def record(node, reason):
        fallback.append({"name": node.name, "op_type": node.op_type,
                         "outputs": list(node.output), "reason": reason})

    def unique(items):
        # Pointwise joins of two paths from the same MVM (e.g. x*sigmoid(x))
        # carry one projected output payload, not two copies of that payload.
        result = {}
        for item in items:
            key = (item.source, item.src, item.channels, item.elements)
            result.setdefault(key, item)
        return list(result.values())

    def reframe(items, before, after, output, mode):
        old, new = _tensor_layout(before), _tensor_layout(after)
        result = []
        for item in items:
            lo, hi = item.channels
            count = item.elements
            if old and new:
                if mode == "flat" and old[0] * old[1] == new[0] * new[1]:
                    # Exact for NCHW->NC flatten and channel-aligned reshape.
                    start, end = (lo - 1) * old[1], hi * old[1]
                    lo, hi = start // new[1] + 1, (end + new[1] - 1) // new[1]
                elif old[0] == new[0]:
                    count = count * new[1] // old[1]
                else:
                    # Unknown layout transform: retain each ancestor and its
                    # relative destination coverage, explicitly reported above.
                    lo = (lo - 1) * new[0] // old[0] + 1
                    hi = max(lo, hi * new[0] // old[0])
                    count = (hi - lo + 1) * new[1]
            # A concat can contain distinct postprocessed values from the same
            # source interval. Preserve that lineage through later unary ops;
            # a fixed-size digest avoids recursively growing DenseNet identities.
            identity = json.dumps([output, item.source, item.src, lo, hi, item.tensor_id],
                                  separators=(",", ":"), ensure_ascii=True)
            view_id = f"{output}:{sha256(identity.encode('utf-8')).hexdigest()}"
            result.append(_Origin(item.source, item.src, (lo, hi), count, view_id))
        return unique(result)

    for node in model.graph.node:
        attrs = parse_node(node)["attribute"]
        standard = node.domain in ("", "ai.onnx")
        if standard and node.op_type == "Constant":
            for output in node.output:
                origins[output] = []
                if "value" in attrs:
                    weights[output] = attrs["value"]
            skipped[node.op_type] += 1
            continue
        if standard and node.op_type in ("Conv", "Gemm"):
            if len(node.input) < 2 or node.input[1] not in weights:
                fail(node, "weights must be a static initializer or Constant tensor")
            if node.input[0] not in origins:
                fail(node, "data input has no tensor producer")
            if len(node.output) != 1:
                fail(node, "only one MVM output is supported")
            if len(node.input) > 2 and node.input[2] and origins.get(node.input[2]):
                fail(node, "dynamic bias requires a separate arithmetic workload")
            in_shape, out_shape = shapes.get(node.input[0]), shapes.get(node.output[0])
            cin, ifmap = get_channel_size(in_shape)
            cout, ofmap = get_channel_size(out_shape)
            dims = list(weights[node.input[1]].dims)
            if node.op_type == "Conv":
                if attrs.get("group", 1) != 1:
                    fail(node, "group/depthwise convolution needs explicit group mapping")
                if len(in_shape) != 4 or len(out_shape) != 4 or len(dims) != 4:
                    fail(node, "only 2D NCHW convolution is supported")
                if dims[:2] != [cout, cin]:
                    fail(node, "weight channels disagree with input/output shapes")
                wsize = dims[2:]
                if list(attrs.get("kernel_shape", wsize)) != wsize:
                    fail(node, "kernel_shape disagrees with initializer")
            else:
                if len(in_shape) != 2 or len(out_shape) != 2 or len(dims) != 2:
                    fail(node, "Gemm requires rank-2 data and weights")
                if attrs.get("transA", 0) != 0:
                    fail(node, "transA requires explicit layout mapping")
                if attrs.get("transB", 0) not in (0, 1):
                    fail(node, "transB must be zero or one")
                expected = [cout, cin] if attrs.get("transB", 0) else [cin, cout]
                if dims != expected:
                    fail(node, "Gemm weight dimensions disagree with transB/input/output")
                if attrs.get("alpha", 1.0) != 1.0 or attrs.get("beta", 1.0) != 1.0:
                    record(node, "Gemm scaling has no separate compute or timing node")
                wsize = [1, 1]
            index = len(infos)
            for item in unique(origins[node.input[0]]):
                if not 1 <= item.channels[0] <= item.channels[1] <= cin:
                    fail(node, f"projected input interval {item.channels} exceeds {cin} channels")
                infos[item.source].depinfo.append(Depinfo(index, item.src, item.channels,
                                                        item.elements, item.tensor_id))
            infos.append(ConvLayerInfo(index, wsize, (cin, cout), ifmap, ofmap, []))
            copy = onnx.NodeProto()
            copy.CopyFrom(node)
            nodes.append(copy)
            origins[node.output[0]] = [_Origin(index, (1, cout), (1, cout),
                                                    cout * prod(ofmap), node.output[0])]
            continue

        skipped[node.op_type] += 1
        inputs = [(name, origins.get(name, [])) for name in node.input if name]
        before = shapes.get(node.input[0]) if node.input else None
        # Shape values control layout, but do not transport activation elements.
        if standard and node.op_type in {"Shape", "Size", "ConstantOfShape"}:
            projected = {name: [] for name in node.output}
        elif standard and node.op_type == "Concat" and attrs.get("axis", 0) == 1:
            entries, offset = [], 0
            valid = True
            for name, items in inputs:
                layout = _tensor_layout(shapes.get(name))
                if not layout:
                    valid = False
                    break
                for item in items:
                    entries.append(_Origin(item.source, item.src,
                                           (item.channels[0] + offset, item.channels[1] + offset),
                                           item.elements, item.tensor_id))
                offset += layout[0]
            if valid:
                projected = {name: entries for name in node.output}
            else:
                record(node, "Concat input layout is dynamic; data ancestors retained")
                projected = {name: reframe(unique([v for _, items in inputs for v in items]),
                                          before, shapes.get(name), name, "fallback") for name in node.output}
        elif standard and node.op_type in pointwise | spatial | {"Flatten", "Reshape", "Squeeze", "Unsqueeze"}:
            items = inputs[0][1] if inputs else []
            mode = "flat" if node.op_type in {"Flatten", "Reshape", "Squeeze", "Unsqueeze"} else "shape"
            projected = {name: reframe(items, before, shapes.get(name), name, mode) for name in node.output}
            if mode == "flat":
                old = _tensor_layout(before)
                for name in node.output:
                    new = _tensor_layout(shapes.get(name))
                    if old and new and any((v.channels[0] - 1) * old[1] % new[1] or
                                           v.channels[1] * old[1] % new[1] for v in items):
                        record(node, "reshape cuts a source channel interval; dependency stream uses channel envelope")
                    elif not old or not new or old[0] * old[1] != new[0] * new[1]:
                        record(node, "layout is not a static contiguous batch-one reshape; data ancestors retained")
        else:
            if not (standard and node.op_type in arithmetic):
                record(node, "dependency-only projection; no independent compute/timing node")
            projected = {}
            for name in node.output:
                items = []
                for source_name, source_items in inputs:
                    items.extend(reframe(source_items, shapes.get(source_name), shapes.get(name), name, "fallback"))
                projected[name] = unique(items)
        for output, items in projected.items():
            origins[output] = items
            for source in sorted({item.source for item in items}):
                if output not in nodes[source].output:
                    nodes[source].output.append(output)

    if not infos:
        raise ModelParsingError("The model contains no Conv/Gemm workload")
    for info in infos:
        if not info.depinfo:
            info.depinfo.append(Depinfo(-1, (0, 0)))
    if projection_report is not None:
        projection_report.update({"mode": "dependency_projection", "mvm_count": len(infos),
                                  "skipped_ops": dict(sorted(skipped.items())), "fallbacks": fallback,
                                  "dependency_count": sum(d.dep_layer >= 0 for i in infos for d in i.depinfo)})
    return infos, nodes


def merge_ops(model, *, strict_operators=False):
    """Return copies of MVM nodes with tensor aliases; never mutate the model."""
    return (_project_model_strict(model) if strict_operators else _project_model(model))[1]


def build_conv_info(model, *, strict_operators=False, projection_report=None):
    if strict_operators:
        result = _project_model_strict(model)[0]
        if projection_report is not None:
            projection_report.update({"mode": "strict_operators", "mvm_count": len(result),
                                      "skipped_ops": {}, "fallbacks": []})
        return result
    return _project_model(model, projection_report)[0]

def convert_to_cpp(conv_info_list):
    # 将conv_info_list转换为C++可以接受的格式
    nnkernel_list = []
    for conv_info in conv_info_list:
        nnkernel = pimapping.NNkernel()
        nnkernel.layer = conv_info.layer
        nnkernel.wsize = (conv_info.wsize[0], conv_info.wsize[1])
        nnkernel.channel = (conv_info.channel[0], conv_info.channel[1])
        nnkernel.ifmap_size = (conv_info.ifmap_size[0], conv_info.ifmap_size[1])
        nnkernel.ofmap_size = (conv_info.ofmap_size[0], conv_info.ofmap_size[1])

        depinfo_list = []
        for dep in conv_info.depinfo:
            depinfo = pimapping.DepInfo()
            depinfo.dep_layer = dep.dep_layer
            depinfo.dep_chan = dep.dep_chan
            depinfo.dst_chan = dep.dst_chan
            depinfo.elements = dep.elements
            depinfo.tensor_id = dep.tensor_id
            depinfo_list.append(depinfo)
        nnkernel.depinfo = depinfo_list
        nnkernel_list.append(nnkernel)
    return nnkernel_list

def make_default_hw_info():
    hw_info = pimapping.HWInfo()
    hw_info.xbar_size = (256, 256) # 256 / (8 / 2)
    hw_info.xbar_num = 16
    hw_info.tile_size = (0, 0)
    hw_info.pipeline_depth = 1
    return hw_info

def make_hw_info(xbar_size=(256, 256), xbar_num=16, tile_size=(0, 0), pipeline_depth=1):
    hw_info = pimapping.HWInfo()
    hw_info.xbar_size = xbar_size
    hw_info.xbar_num = xbar_num
    hw_info.tile_size = tile_size
    hw_info.pipeline_depth = pipeline_depth
    return hw_info

def make_default_opt_info():
    opt_info = pimapping.OptInfo()
    opt_info.mapping_opt = 1
    opt_info.sched_opt = 1
    return opt_info

def make_opt_info(map=1, sched=1):
    opt_info = pimapping.OptInfo()
    opt_info.mapping_opt = map
    opt_info.sched_opt = sched
    return opt_info

# def analysis_model(path=None, hw_info=None, opt_info=None):
    # path = path or set_path()
    # model = load_model(path)
    # print(f'Loading model: {path}')
def load_kernel(path, *, strict_operators=False, projection_report=None):
    path = path or set_path()
    model = load_model(path)
    print(f'Loading model: {path}')
    conv_info_list = build_conv_info(model, strict_operators=strict_operators,
                                     projection_report=projection_report)
    nnkernel_list = convert_to_cpp(conv_info_list)
    return nnkernel_list

def analysis_model(kernel_list=None, hw_info=None, opt_info=None):
    # conv_info_list = build_conv_info(model)
    # nnkernel_list = convert_to_cpp(conv_info_list)
    hw_info = hw_info or make_default_hw_info()
    opt_info = opt_info or make_default_opt_info()
    return pimapping.analyze(kernel_list, hw_info, opt_info)

def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description="Parse and map a supported static ONNX model")
    parser.add_argument("model", help="ONNX model path (relative to the caller's directory)")
    parser.add_argument("--strict-operators", action="store_true", help="reject non-MVM operators outside the strict subset")
    args = parser.parse_args(argv)
    kernels = load_kernel(args.model, strict_operators=args.strict_operators)
    result, communication = analysis_model(kernels)
    print(f"Mapped {len(kernels)} operators to {len(result.deploy_info)} tiles; "
          f"{communication.path_num} communication paths")


if __name__ == "__main__":
    main()
