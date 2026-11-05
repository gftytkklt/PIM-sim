"""真实ONNX→层次映射→聚合Tile周期模型。时序参数必须显式提供。"""
import argparse
import json
from pathlib import Path
import sys

# Keep PYTHONPATH's selected extension ahead of the repository's historical .so.
sys.path.append(str(Path(__file__).resolve().parents[1] / "src" / "python"))
import pimapping
from pim_sim.onnx_analysis import load_kernel, make_hw_info, make_opt_info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model', type=Path)
    parser.add_argument('--timing', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--xbar', nargs=2, type=int, default=(256, 256))
    parser.add_argument('--xbars-per-tile', type=int, default=1)
    parser.add_argument('--mesh', nargs=2, type=int, default=(2, 2))
    parser.add_argument('--pipeline-depth', type=int, default=1)
    parser.add_argument('--max-cycles', type=int, default=1000000)
    parser.add_argument('--mapping-opt', action='store_true')
    parser.add_argument('--schedule-opt', action='store_true')
    parser.add_argument('--strict-operators', action='store_true')
    args = parser.parse_args()
    kernels = load_kernel(str(args.model), strict_operators=args.strict_operators)
    hardware = make_hw_info(tuple(args.xbar), args.xbars_per_tile, tuple(args.mesh), args.pipeline_depth)
    options = make_opt_info(args.mapping_opt, args.schedule_opt)
    workload = pimapping.export_workload(kernels, hardware, options, args.timing.read_text())
    result = json.loads(pimapping.simulate_workload(workload, args.max_cycles))
    args.output.mkdir(parents=True, exist_ok=True)
    for name, document in [('workload.json',json.loads(workload)),('result.json',result)]:
        with (args.output/name).open('x') as handle:
            json.dump(document,handle,indent=2,allow_nan=False)
            handle.write('\n')
    print(json.dumps({k:result[k] for k in ['completed','stop_reason','completed_tasks','total_tasks','makespan_cycles','sent_bytes','received_bytes']},ensure_ascii=False))
    return 0 if result['completed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
