"""Reproducible ONNX mapping and optional MNSIM/Booksim aggregate estimates.

Use --help for explicit inputs. This path is distinct from the cycle simulator;
its timing model is an uncalibrated aggregate estimate, not hardware execution.
"""

from .analysis import (
    perf_analysis, get_opt_str, get_noc_perf, load_noc_perf,
    power_analysis, brkdown_stat, brkdown_analysis, get_data_percentage,
)
from .onnx_analysis import make_hw_info
from .plotting import (
    plot_all_comm, plot_comm, plot_perf, plot_bw_perf,
    plot_brkdown, plot_bw, plot_grouped_bars, plot_xbarsize, plot_pipeline,
)
from . import paths


def main(argv=None):
    import argparse
    import configparser
    from datetime import datetime, timezone
    import json
    from pathlib import Path
    from .config_validator import parse_pair, validate_config
    from .mapping_info import latency_est

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models-dir", type=Path, default=paths.DEMO_MODELS_DIR)
    parser.add_argument("--config", type=Path, default=paths.DEFAULT_SIM_CONFIG)
    parser.add_argument("--xbar-size", help="logical array rows,columns; defaults to config Xbar_Size")
    parser.add_argument("--xbars-per-tile", type=int, default=8,
                        help="logical mapping capacity, independently specified (default: 8)")
    parser.add_argument("--mesh", default="0,0", help="rows,columns; 0,0 selects automatic sizing")
    parser.add_argument("--pipeline-depth", type=int, default=1)
    parser.add_argument("--bus-width", type=float, default=1, help="NoC bytes per cycle")
    parser.add_argument("--frequency", type=float, default=1e9, help="NoC frequency in Hz")
    parser.add_argument("--input-bits", type=int, default=8)
    parser.add_argument("--output-bits", type=int, default=8)
    parser.add_argument("--mapping-only", action="store_true")
    parser.add_argument("--strict-operators", action="store_true",
                        help="require the shape-preserving operator subset; default projects non-MVM operators")
    parser.add_argument("--single-strategy", action="store_true", help="only mapping=1,scheduling=1")
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    errors = validate_config(args.config)
    if errors:
        parser.error("; ".join(errors))
    config = configparser.ConfigParser()
    config.read(args.config)
    physical_xbar = parse_pair(config.get("Crossbar level", "Xbar_Size"), "Xbar_Size")
    xbar = parse_pair(args.xbar_size, "xbar-size") if args.xbar_size else physical_xbar
    mesh = parse_pair(args.mesh, "mesh", allow_auto=True)
    if not args.mapping_only and xbar != physical_xbar:
        parser.error("MNSIM Xbar_Size differs from the mapping dimensions; provide a matching --config")
    hardware = make_hw_info(xbar, args.xbars_per_tile, mesh, args.pipeline_depth)
    mapped, communication = perf_analysis(args.models_dir, hardware, args.single_strategy,
                                          strict_operators=args.strict_operators)
    document = {
        "schema": "pim_analysis_v2", "provenance": mapped.provenance,
        "mapping_units": {"data_volume": "scalar_elements", "channel_intervals": "one_based_inclusive"},
        "mnsim_config": {section: dict(config.items(section)) for section in config.sections()},
        "assumptions": [
            "ONNX format and static MVM shapes are checked; Conv/Gemm form compute nodes",
            "Other operators propagate tensor dependencies and projected transfer sizes without standalone compute costs",
            "Mapping arrays represent logical weight elements; physical bit slicing and weight polarity are not represented",
            "Logical crossbars per tile and MNSIM PE/group parameters are configured independently",
            "MNSIM evaluates aggregate tile input once per segment; repeated MVM excitation is not timed individually",
            "Booksim custom endpoint traffic at 100 frames/s; DOR routes do not replay BCE routing",
            "Communication bandwidth is scaled by network_latency/packet_latency; estimates are not calibrated against hardware",
        ],
        "results": [],
    }
    factors = powers = None
    if not args.mapping_only:
        cached = load_noc_perf(args.bus_width, xbar, comm_segs=mapped,
                               freq=args.frequency, data_bits=args.output_bits, config_path=args.config)
        factors, powers = cached or get_noc_perf(
            mapped, args.bus_width, xbar, save=True, freq=args.frequency,
            data_bits=args.output_bits, workers=args.workers, config_path=args.config)
    for model, opts in sorted(mapped.items()):
        for opt, result in sorted(opts.items()):
            item = {"model": model, "strategy": list(opt), "communication": communication[model][opt],
                    "tiles": len(result.deploy_info),
                    "segments": [{"layers": list(seg.layers), "data_elements": seg.datas} for seg in result.comm_segs]}
            if factors is not None:
                estimate = latency_est(args.config, args.input_bits, args.output_bits, result,
                                       args.bus_width, args.frequency, factors[(model, opt)])
                item["aggregate_estimate"] = {
                    "latency_seconds": estimate[0], "throughput_per_second": estimate[1],
                    "segment_transfer_seconds": estimate[2], "segment_compute_seconds": estimate[3],
                    "booksim_bandwidth_factors": factors[(model, opt)],
                    "booksim_reported_network_power": powers[(model, opt)],
                }
            document["results"].append(item)
    output = args.output or paths.artifact_path("runs", "analysis_" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".json")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Explicitly chosen existing outputs must also remain intact.
    with output.open("x") as handle:
        json.dump(document, handle, indent=2, ensure_ascii=False, allow_nan=False)
    print(f"Analysis written to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
