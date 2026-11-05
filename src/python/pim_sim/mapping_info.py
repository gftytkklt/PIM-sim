import subprocess
import os, re, math
import tempfile
from pathlib import Path
from .config_validator import validate_config, parse_pair
from . import paths
import numpy as np
import configparser as cp
paths.ensure_third_party_imports()
from MNSIM.Latency_Model.Tile_latency import tile_latency_analysis


def squeeze_matrix(matrix):
    matrix = np.array(matrix)
    elem_num = len(matrix)
    # print(matrix)
    orig_row = int(math.sqrt(elem_num))
    orig_col = int(math.sqrt(elem_num))
    if orig_row * orig_col != elem_num:
        raise ValueError("Matrix is not square or has invalid dimensions.")
    non_zero_srcid = [i for i, val in enumerate(matrix) if any(val)]
    if not non_zero_srcid:
        print("No non-zero elements found in the matrix.")
        return np.zeros((1, 1))  # Return a zero matrix if no non-zero elements found
    # print("Non-zero source IDs:", non_zero_srcid)
    # assert sublists have the same length
    # non zero dest ids
    non_zero_destid = set()
    for id in non_zero_srcid:
        non_zero_id = [i for i, val in enumerate(matrix[id]) if val > 0]
        non_zero_destid.update(non_zero_id)
    # print("Non-zero destination IDs:", non_zero_destid)
    # coordinate transformation
    non_zero_srcid = np.array(non_zero_srcid)
    non_zero_destid = np.array(list(non_zero_destid))
    # print("non_zero_srcid", non_zero_srcid)
    # print("non_zero_destid", non_zero_destid)
    src_row = non_zero_srcid // orig_col
    src_col = non_zero_srcid % orig_col
    dest_row = non_zero_destid // orig_col
    dest_col = non_zero_destid % orig_col
    # print("src_row", src_row)
    # print("src_col", src_col)
    # print("dest_row", dest_row)
    # print("dest_col", dest_col)
    row_min = min(src_row.min(), dest_row.min())
    row_max = max(src_row.max(), dest_row.max())
    col_min = min(src_col.min(), dest_col.min())
    col_max = max(src_col.max(), dest_col.max())
    # print("row_min", row_min, "row_max", row_max)
    # print("col_min", col_min, "col_max", col_max)
    row_size = row_max - row_min + 1
    col_size = col_max - col_min + 1
    # print("row_size", row_size, "col_size", col_size)
    square_size = max(row_size, col_size)
    new_matrix = np.zeros((square_size * square_size, square_size * square_size))
    # extract submatrix data
    # matrix layout: (row, row) blks, (col, col) elems each blk
    for i in range(row_size):
        i_start = i + row_min
        for j in range(row_size):
            j_start = j + row_min
            sub_blk = matrix[
                i_start * orig_col : (i_start + 1) * orig_col,
                j_start * orig_col : (j_start + 1) * orig_col,
            ]
            sub_data = sub_blk[col_min : col_max + 1, col_min : col_max + 1]
            # assertion check: sub_data must contain all non-zero elements from sub_blk
            if sub_blk.any():
                # print(sub_blk)
                # print(sub_data)
                if not np.array_equal(sub_blk[sub_blk != 0], sub_data[sub_data != 0]):
                    print(
                        i,
                        j,
                        "submatrix data does not match original non-zero elements.",
                    )
                    raise ValueError(
                        "Submatrix data does not match original non-zero elements."
                    )
            new_matrix[
                i * square_size : i * square_size + col_size,
                j * square_size : j * square_size + col_size,
            ] = sub_data
            # if sub_blk.any():
            #     test_mat = new_matrix[i*square_size:i*square_size+col_size, j*square_size:j*square_size+col_size]
            #     print(test_mat)
            #     print(sub_data)
            #     if not np.array_equal(sub_data[sub_data != 0], test_mat[test_mat != 0]):
            #         print(i, j,"Submatrix data does not match original non-zero elements.")
            #         raise ValueError("Submatrix data does not match original non-zero elements.")
    # print(matrix.shape, "squeezed to", new_matrix.shape)
    # print(matrix)
    # print(new_matrix)
    # assertion check, non-zero elements should be the same
    # print("Original non-zero elements:")
    # print(matrix[matrix != 0])
    # print(new_matrix[new_matrix != 0])
    # assertion check: squeezed matrix should match original non-zero elements
    if not np.array_equal(matrix[matrix != 0], new_matrix[new_matrix != 0]):
        print("Squeezed matrix does not match original non-zero elements.")
        raise ValueError("Squeezed matrix does not match original non-zero elements.")
    return new_matrix


class BooksimError(RuntimeError):
    """No valid Booksim measurement was produced."""


def create_cfg_file(home_path, mesh_size, statistic_mode=False, inj_rate=0.01,
                    run_dir=None, bus_width=None):
    """Create a private configuration; custom traffic reads cwd/inj_rate.txt."""
    # home_path remains the optional repository root for compatibility; resource
    # locations inside it follow the standard layout. Explicit run_dir is caller-relative.
    root = paths.REPO_ROOT if home_path is None else Path(home_path).resolve()
    directory = Path(run_dir) if run_dir else paths.artifact_path("runs", "booksim", str(os.getpid()))
    directory.mkdir(parents=True, exist_ok=True)
    template = root / "configs" / "booksim" / "mesh.cfg"
    text = template.read_text()
    values = {"k": mesh_size, "injection_rate": inj_rate,
              "traffic": "uniform" if statistic_mode else "custom",
              "injection_process": "on_off" if statistic_mode else "custom",
              "tech_file": str(root / "configs" / "booksim" / "techfile.txt")}
    if bus_width is not None:
        values["channel_width"] = int(bus_width * 8)
    for key, value in values.items():
        text, count = re.subn(r"(?m)^\s*" + re.escape(key) + r"\s*=\s*[^;]*;",
                              f"{key} = {value};", text)
        if count != 1:
            raise ValueError(f"Booksim template must define {key} exactly once")
    path = directory / "booksim.cfg"
    path.write_text(text)
    return str(path)


def _positive(value, name):
    if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
        raise ValueError(f"{name} must be finite and positive")


def booksim_eval(all_comm_segs, bus_width, freq=1000000000, *, data_bits=8,
                 traffic_model="custom", timeout=120):
    """Return per-layer (bandwidth factors, network power).

    Data volumes in the graph count scalar elements; data_bits converts them to
    bytes. bus_width is bytes/cycle and freq is Hz. Injection assumes 100 frames/s.
    The historical network_latency / packet_latency bandwidth factor is retained
    as an *uncalibrated heuristic*, not latency in cycles or seconds. Custom traffic
    preserves endpoint traffic, but Booksim's DOR routing does not replay BCE paths.
    Uniform traffic is available only as an explicitly requested approximation.
    """
    _positive(bus_width, "bus_width")
    _positive(freq, "freq")
    _positive(data_bits, "data_bits")
    if bus_width * 8 != int(bus_width * 8):
        raise ValueError("bus_width must represent a whole number of bits")
    if traffic_model not in ("custom", "uniform"):
        raise ValueError("traffic_model must be custom or uniform")
    factors, powers = {}, {}
    root = paths.REPO_ROOT
    fps = 100
    for comm_seg in all_comm_segs:
        layers = list(comm_seg.layers)
        if any(layer in factors for layer in layers) or len(set(layers)) != len(layers):
            raise ValueError("Each layer must occur in exactly one communication segment")
        matrix = np.asarray(comm_seg.datas, dtype=float)
        if matrix.ndim != 2 or matrix.shape[0] != matrix.shape[1]:
            raise ValueError("Communication matrix must be square")
        if not np.isfinite(matrix).all() or (matrix < 0).any():
            raise ValueError("Communication matrix must contain finite nonnegative volumes")
        # Local transfers do not enter the inter-tile network.
        matrix = matrix.copy()
        np.fill_diagonal(matrix, 0)
        if not matrix.any():
            factors.update({layer: 1.0 for layer in layers})
            powers.update({layer: 0.0 for layer in layers})
            continue
        mesh_size = math.isqrt(matrix.shape[0])
        if mesh_size * mesh_size != matrix.shape[0]:
            raise ValueError("Booksim adapter currently requires a square 2D physical mesh")
        # Preserve physical IDs and the original mesh; do not squeeze the topology.
        injection = matrix * (data_bits / 8) / (bus_width * freq / fps)
        source_rates = injection.sum(axis=1)
        if (source_rates >= 1).any():
            raise ValueError("Per-source injection rate must be less than one packet/cycle")
        scratch = paths.artifact_path("runs", "booksim")
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="pim-booksim-", dir=scratch) as directory:
            np.savetxt(Path(directory) / "inj_rate.txt", injection, fmt="%.17g")
            cfg = create_cfg_file(root, mesh_size,
                                  statistic_mode=traffic_model == "uniform",
                                  inj_rate=float(source_rates.mean()),
                                  run_dir=directory, bus_width=bus_width)
            try:
                result = subprocess.run([str(paths.BOOKSIM_BINARY), cfg], cwd=directory,
                                        capture_output=True, text=True, timeout=timeout)
            except (subprocess.TimeoutExpired, OSError) as error:
                raise BooksimError(f"Booksim failed for layers {layers}: {error}") from error
            if result.returncode != 0:
                raise BooksimError(f"Booksim exited {result.returncode} for layers {layers}: "
                                   f"{result.stderr[-1000:]} {result.stdout[-1000:]}")
            number = r"([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)"
            metrics = []
            for label in ("Packet latency average", "Network latency average", "- Total Power:"):
                separator = r"\s*=\s*" if not label.endswith(":") else r"\s*"
                match = re.search(re.escape(label) + separator + number, result.stdout)
                if not match:
                    raise BooksimError(f"Booksim missing {label!r} for layers {layers}")
                metrics.append(float(match.group(1)))
            packet, network, power = metrics
            if (not all(math.isfinite(v) for v in metrics) or packet <= 0
                    or network <= 0 or network > packet or power < 0):
                raise BooksimError(f"Invalid Booksim metrics for layers {layers}: {metrics}")
            factors.update({layer: network / packet for layer in layers})
            powers.update({layer: power for layer in layers})
    return factors, powers


# mapping_res: deploy_info, comm_segs
def latency_est(
    SimConfig_path=None,
    inputbit=8,
    outputbit=8,
    mapping_res=None,
    bus_width=8,
    freq=1000000000,
    comm_lat=None,
    ideal=0,
    syn=1,
):
    """Return the legacy aggregate estimate in seconds, not calibrated cycles.

    Each tile/segment receives one MNSIM evaluation with aggregate input bytes.
    Repeated MVM excitation counts and occupied PE/group assignments are not
    represented here. The historical syn parameter is reserved and has no effect.
    An empty workload has zero latency and zero reported throughput.
    """
    _positive(bus_width, "bus_width")
    _positive(freq, "freq")
    _positive(inputbit, "inputbit")
    _positive(outputbit, "outputbit")
    if mapping_res is None:
        raise ValueError("mapping_res is required")
    SimConfig_path = paths.DEFAULT_SIM_CONFIG if SimConfig_path is None else SimConfig_path
    # Latencies are seconds; graph data volumes are scalar elements.
    all_tiles_mapping_infos = mapping_res.deploy_info
    all_comm_segs = mapping_res.comm_segs  # comm_seg: layer and data matrix
    if ideal:
        latency_map = {layer: 1.0 for seg in all_comm_segs for layer in seg.layers}
    elif comm_lat is None:
        latency_map, _ = booksim_eval(all_comm_segs, bus_width, freq, data_bits=outputbit)
    else:
        latency_map = comm_lat  # lat_layer = latency_map[layer]
    bandwidth = bus_width * freq  # B/s
    effbw = {}
    seg_cal_lat = []
    seg_trans_lat = []
    seg_trans_dict = {}
    overall_latency = 0.0
    overall_throughput = 0.0
    max_layerseg_lat = 0.0
    # update path latency of each segment
    for idx, comm_seg in enumerate(all_comm_segs):
        # cur layer seg
        layers = comm_seg.layers
        for layer in layers:
            if layer not in latency_map:
                raise ValueError(f"Missing Booksim bandwidth factor for layer {layer}")
            factor = latency_map[layer]
            _positive(factor, "bandwidth factor")
            if factor > 1:
                raise ValueError("bandwidth factor must not exceed one")
            effbw[layer] = bandwidth * factor
        # get all paths
        merged_paths = []
        merged_paths.extend(
            (layer, path)
            for layer in layers
            for tile_info in all_tiles_mapping_infos
            if layer in tile_info.layer_paths_map
            for path in tile_info.layer_paths_map[layer]
        )
        # update via delay
        via_delay = {}
        for layer, path in merged_paths:
            if not math.isfinite(path.datavolume) or path.datavolume < 0:
                raise ValueError("Path data volume must be finite and nonnegative")
            if path.datavolume > 0 and not path.via:
                raise ValueError("Nonzero path data volume needs a route")
            cur_effbw = effbw[layer]
            path_delay = path.datavolume * outputbit / 8 / cur_effbw
            for s, d in zip(path.via[:-1], path.via[1:]):
                via_delay.update({(s, d): via_delay.get((s, d), 0) + path_delay})
        # update path delay
        cur_max_path_delay = 0.0
        for layer, path in merged_paths:
            path_delay = 0
            for s, d in zip(path.via[:-1], path.via[1:]):
                path_delay += (
                    via_delay[(s, d)] if not ideal else path.datavolume * outputbit / 8 / effbw[layer]
                )
            cur_max_path_delay = max(cur_max_path_delay, path_delay)
        # generate computation pipeline layer-wise
        cur_max_compute_latency = 0.0
        for idx, tile_info in enumerate(all_tiles_mapping_infos):
            cur_cnode = [node for node in tile_info.cnode if node.layer in layers]
            if not cur_cnode:
                continue
            # local info
            ifm_size = sum(cn.ifmap_size for cn in cur_cnode)
            tile_compute_latency = (
                tile_latency_cal(SimConfig_path, ifm_size * inputbit / 8, inputbit, outputbit)
                / 1e9
            )
            cur_max_compute_latency = max(cur_max_compute_latency, tile_compute_latency)
        max_layerseg_lat = max(
            max_layerseg_lat, cur_max_compute_latency + cur_max_path_delay
        )
        seg_cal_lat.append(cur_max_compute_latency)
        seg_trans_lat.append(cur_max_path_delay)
        for layer in layers:
            seg_trans_dict[layer] = cur_max_path_delay

    overall_latency = sum(seg_cal_lat) + sum(seg_trans_lat)
    overall_throughput = 1 / max_layerseg_lat if max_layerseg_lat > 0 else 0.0
    return (
        overall_latency,
        overall_throughput,
        seg_trans_lat,
        seg_cal_lat,
        seg_trans_dict,
    )


def tile_latency_cal(SimConfig_path, tile_indata, inputbit, outputbit):
    SimConfig_path = paths.DEFAULT_SIM_CONFIG if SimConfig_path is None else SimConfig_path
    errors = validate_config(SimConfig_path)
    if errors:
        raise ValueError("Invalid MNSIM configuration: " + "; ".join(errors))
    _positive(inputbit, "inputbit")
    _positive(outputbit, "outputbit")
    if not math.isfinite(tile_indata) or tile_indata < 0:
        raise ValueError("tile_indata must be a finite nonnegative byte count")
    modelL_config = cp.ConfigParser()
    modelL_config.read(SimConfig_path, encoding="UTF-8")
    xbar_size = parse_pair(modelL_config.get("Crossbar level", "Xbar_Size"), "Xbar_Size")
    pe_size = parse_pair(modelL_config.get("Tile level", "PE_Num"), "PE_Num", allow_auto=True)
    if pe_size == (0, 0):
        pe_size = (4, 4)  # MNSIM's documented automatic PE layout
    temp_tile_latency = tile_latency_analysis(
        SimConfig_path=SimConfig_path,
        read_row=int(xbar_size[0]),
        read_column=int(xbar_size[1]),
        indata=0,
        rdata=0,
        inprecision=inputbit,
        PE_num=int(pe_size[0]) * int(pe_size[1]),
        default_inbuf_size=16,
        default_outbuf_size=4,
    )
    temp_tile_latency.outbuf.calculate_buf_read_latency(
        rdata=(int(xbar_size[1]) * outputbit * int(pe_size[0]) * int(pe_size[1]) / 8)
    )
    temp_tile_latency.tile_buf_rlatency = temp_tile_latency.outbuf.buf_rlatency

    temp_tile_latency.update_tile_latency(indata=tile_indata, rdata=tile_indata)

    tile_latency = temp_tile_latency.tile_latency
    if not math.isfinite(tile_latency) or tile_latency < 0:
        raise ValueError("MNSIM returned invalid latency")
    return tile_latency


def divide_list_elements_3(input_list, divisor):
    # 使用 numpy 数组进行元素的除法操作
    input_array = np.array(input_list)
    result = input_array / divisor
    # result_array = input_array / divisor
    # result = np.maximum(result, 1e-4)
    return result.tolist()


if __name__ == "__main__":
    raise SystemExit("Use python -m pim_sim with an explicit model input; this module requires a mapping result.")
