"""Optional PyTorch export tool. Importing this module never exports a model."""
import importlib
from pathlib import Path
import sys
from . import paths


def _model_class(module_name="pytorch.mc_cnn_fast", class_name="FastMcCnn"):
    # The bundled model is an example, separate from production Python sources.
    example_path = str(paths.EXAMPLES_DIR)
    if example_path not in sys.path:
        sys.path.append(example_path)
    return getattr(importlib.import_module(module_name), class_name)


def Convert_ONNX(torch_model=None, dummy_input=None, onnx_path=None):
    """Export and check a model; keep the historical callable name available."""
    import torch
    import onnx
    torch_model = _model_class()() if torch_model is None else torch_model
    dummy_input = torch.randn(64, 64) if dummy_input is None else dummy_input
    output = paths.artifact_path("runs", "FastMcCnn.onnx") if onnx_path is None else Path(onnx_path)
    output = paths.prepare_output(output)
    torch_model.eval()
    with output.open("xb") as handle:
        torch.onnx.export(torch_model, dummy_input, handle, opset_version=18,
                          input_names=["Input"], output_names=["Output"])
    onnx.checker.check_model(onnx.load(output))
    print(f"Validated ONNX export: {output}")
    return output


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="new ONNX file; default: artifacts/runs/FastMcCnn.onnx")
    parser.add_argument("--module", default="pytorch.mc_cnn_fast")
    parser.add_argument("--class-name", default="FastMcCnn")
    parser.add_argument("--shape", type=int, nargs="+", default=[64, 64])
    args = parser.parse_args(argv)
    if not args.shape or min(args.shape) <= 0:
        parser.error("all input dimensions must be positive")
    import torch
    Convert_ONNX(_model_class(args.module, args.class_name)(), torch.randn(*args.shape), args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
