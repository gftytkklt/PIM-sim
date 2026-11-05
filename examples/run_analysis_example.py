"""Generate a known supported ONNX model and run the real MNSIM/Booksim path.

scripts/python.sh examples/run_analysis_example.py --output-dir /tmp/pim-analysis-example
The output directory must be new. No research reference data is read or written.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.append(str(Path(__file__).resolve().parents[1] / "src" / "python"))

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

from pim_sim.perf import main as analyze


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=False)
    models = args.output_dir / "models"
    models.mkdir()
    graph = helper.make_graph(
        [helper.make_node("Conv", ["input", "w0"], ["hidden"]),
         helper.make_node("Relu", ["hidden"], ["activated"]),
         helper.make_node("Conv", ["activated", "w1"], ["output"])],
        "two_1x1_convolutions",
        [helper.make_tensor_value_info("input", TensorProto.FLOAT, [1, 2, 32, 32])],
        [helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, 2, 32, 32])],
        [numpy_helper.from_array(np.ones((3, 2, 1, 1), dtype=np.float32), "w0"),
         numpy_helper.from_array(np.ones((2, 3, 1, 1), dtype=np.float32), "w1")],
    )
    onnx.save(helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)]), models / "two_convs.onnx")
    result = args.output_dir / "result.json"
    analyze(["--models-dir", str(models), "--xbars-per-tile", "1", "--mesh", "2,2",
             "--output", str(result)])
    report = json.loads(result.read_text())
    assert len(report["results"]) == 4
    for item in report["results"]:
        # Three output channels over 32x32 pixels cross the only logical edge.
        assert item["communication"]["datavolume"] == 3 * 32 * 32
        estimate = item["aggregate_estimate"]
        assert estimate["latency_seconds"] > 0
        assert estimate["throughput_per_second"] > 0
        assert all(0 < value <= 1 for value in estimate["booksim_bandwidth_factors"].values())
    print("Verified four strategies, 3072 communicated elements, finite aggregate estimates.")


if __name__ == "__main__":
    main()
