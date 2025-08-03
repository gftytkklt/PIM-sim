import pickle
import subprocess
import os, re, glob, sys, math
import numpy as np
import pandas as pd
from collections import defaultdict
import configparser as cp
from MNSIM.Interface.interface import *
from MNSIM.Latency_Model.Tile_latency import tile_latency_analysis
from MNSIM.Latency_Model.Pooling_latency import pooling_latency_analysis
from MNSIM.Hardware_Model.Buffer import buffer

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
    #extract submatrix data
    # matrix layout: (row, row) blks, (col, col) elems each blk
    for i in range(row_size):
        i_start = i + row_min
        for j in range(row_size):
            j_start = j + row_min
            sub_blk = matrix[i_start*orig_col:(i_start+1)*orig_col, j_start*orig_col:(j_start+1)*orig_col]
            sub_data = sub_blk[col_min:col_max+1, col_min:col_max+1]
            # assertion check: sub_data must contain all non-zero elements from sub_blk
            if sub_blk.any():
                # print(sub_blk)
                # print(sub_data)
                if not np.array_equal(sub_blk[sub_blk != 0], sub_data[sub_data != 0]):
                    print(i, j, "submatrix data does not match original non-zero elements.")
                    raise ValueError("Submatrix data does not match original non-zero elements.")
            new_matrix[i*square_size:i*square_size+col_size, j*square_size:j*square_size+col_size] = sub_data
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

def create_cfg_file(home_path, mesh_size):
    cfg_file = 'booksim_cfg'
    with open(cfg_file, 'r') as f:
        lines = f.readlines()
    # modify mesh size
    for i, line in enumerate(lines):
        line = line.strip()
        matchobj = re.match(r'^k=', line)
        if matchobj:
            lines[i] = 'k=' + str(mesh_size) + ';' + '\n'
            break
    with open(cfg_file, 'w') as f:
        f.writelines(lines)
    return cfg_file

def booksim_eval(all_comm_segs, bus_width, freq=1000000000):
    # inter-tile latency estimation by booksim2
    # init sim comfig
    home_path = os.getcwd()

    fps = 100
    latency_map = {}
    power_map = {}
    for idx, comm_seg in enumerate(all_comm_segs):
        layers = comm_seg.layers
        # set injection matrix
        data_matrix = comm_seg.datas
        # print("original data_matrix shape is", np.array(data_matrix).shape)
        data_matrix = squeeze_matrix(data_matrix)
        mesh_size = int(data_matrix.shape[0] ** 0.5)
        # print("squeezed mesh size is", mesh_size)
        # print("squeezed data_matrix shape is", data_matrix.shape)
        inj_matrix = divide_list_elements_3(data_matrix, bus_width * freq / fps)
        np.savetxt("inj_rate.txt", inj_matrix, fmt='%.12f')
        # create cfg file
        cfg_file = create_cfg_file(home_path, mesh_size)
        # pipe based implementation
        booksim_command = [home_path + '/booksim', cfg_file]
        result = subprocess.run(booksim_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        output_lines = result.stdout.strip()
        # print("output_line", output_lines)
        packet_line = re.search(r'Packet latency average = ([\d.]+)', output_lines)
        network_line = re.search(r'Network latency average = ([\d.]+)', output_lines)
        power_line = re.search(r'- Total Power:\s+([\d.]+)', output_lines)
        if power_line:
            power = float(power_line.group(1))
            # print("Total Power is", power)
        else:
            print("Total Power not found in output.")
        if packet_line and network_line:
            packet_latency = float(packet_line.group(1))
            network_latency = float(network_line.group(1))
            # print("packet latency is", packet_latency)
            # print("network latency is", network_latency)
            latency = max(packet_latency - network_latency, 0)
        else:
            latency = 0  # default latency if not found
        for layer in layers:
            latency_map[layer] = latency
            power_map[layer] = power
    return latency_map, power_map

# mapping_res: deploy_info, comm_segs
def latency_est(SimConfig_path='SimConfig.ini',inputbit=8, outputbit=8, mapping_res=None, bus_width=8, freq = 1000000000, comm_lat=None, ideal = 0, syn = 1):
    # get mapping results
    all_tiles_mapping_infos = mapping_res.deploy_info
    all_comm_segs = mapping_res.comm_segs # comm_seg: layer and data matrix
    # print("tile num is", len(all_tiles_mapping_infos))
    # get inter-tile lat first if not provided
    if comm_lat is None and ideal == 0:
        latency_map = booksim_eval(all_comm_segs, bus_width, freq)
        # modify filename manually after saving
        # pickle.dump(latency_map, open(f"results/noc_perf_dict_bw=1_xbar=256_256.pkl", "wb"))
    elif ideal == 1:
        latency_map = {}
    else:
        latency_map = comm_lat # lat_layer = latency_map[layer]
    bandwidth = bus_width * freq #B/s
    # update tile exec info
    # tile_by_layer = defaultdict(list)
    exec_info = defaultdict(dict)
    effbw = {}
    seg_cal_lat = []
    seg_trans_lat = []
    seg_trans_dict = {}
    overall_latency = 0.0
    overall_throughput = 0.0
    max_layerseg_lat = 0.0
    # set tile by layer info
    # for idx, tile_info in enumerate(all_tiles_mapping_infos):
    #     tile_by_layer[tile_info.layer].append(idx)
    # update path latency of each segment
    for idx, comm_seg in enumerate(all_comm_segs):
        # cur layer seg
        layers = comm_seg.layers
        # get all (layer, tile_idx) from tile_by_layer[layers]
        # tile_ids = [idx for layer in layers for idx in tile_by_layer[layer]]
        # equivalent bandwidth computation (B/s)
        effbw.update({layer: bandwidth / (1 + latency_map.get(layer, 0)) for layer in layers})
        # get all paths
        merged_paths = []
        merged_paths.extend((layer, path) 
                            for layer in layers 
                            for tile_info in all_tiles_mapping_infos 
                            if layer in tile_info.layer_paths_map
                            for path in tile_info.layer_paths_map[layer])
        # print(merged_paths)
        # for tile_id in tile_ids:
        #     layer = all_tiles_mapping_infos[tile_id].layer
        #     merged_paths.extend((layer, path) for path in all_tiles_mapping_infos[tile_id].paths)
        # update via delay
        via_delay = {}
        for layer, path in merged_paths:
            cur_effbw = effbw[layer]
            path_delay = path.datavolume / cur_effbw
            for s, d in zip(path.via[:-1], path.via[1:]):
                # via_delay[(s, d)] = path_delay
                via_delay.update({(s, d): via_delay.get((s, d), 0) + path_delay})
        # update path delay
        cur_max_path_delay = 0.0
        for _, path in merged_paths:
            path_delay = 0
            for s, d in zip(path.via[:-1], path.via[1:]):
                path_delay += via_delay[(s, d)]
            cur_max_path_delay = max(cur_max_path_delay, path_delay)
        # generate computation pipeline layer-wise
        cur_max_compute_latency = 0.0
        for idx, tile_info in enumerate(all_tiles_mapping_infos):
            cur_cnode = [node for node in tile_info.cnode if node.layer in layers]
            if not cur_cnode:
                continue
            # local info
            ifm_size = sum(cn.ifmap_size for cn in cur_cnode)
            tile_compute_latency = tile_latency_cal(SimConfig_path, ifm_size, inputbit, outputbit) / 100000000
            cur_max_compute_latency = max(cur_max_compute_latency, tile_compute_latency)
        max_layerseg_lat = max(max_layerseg_lat, cur_max_compute_latency + cur_max_path_delay)
        seg_cal_lat.append(cur_max_compute_latency)
        seg_trans_lat.append(cur_max_path_delay)
        for layer in layers:
            seg_trans_dict[layer] = cur_max_path_delay

    overall_latency = sum(seg_cal_lat) + sum(seg_trans_lat)
    overall_throughput = 1 / max_layerseg_lat if overall_latency > 0 else float('inf')
    return overall_latency, overall_throughput, seg_trans_lat, seg_trans_dict


    # for idx, tile_info in enumerate(all_tiles_mapping_infos):
    #     # local info
    #     tile_id = tile_info.tile_id
    #     # get ifm size to compute cal latency
    #     ifm_size = sum(cn.ifmap_size for cn in tile_info.cnode)
    #     # exec_info[tile_id]['cal_lat'] = 0
    #     exec_info[tile_id]['cal_lat'] = tile_latency_cal(SimConfig_path, ifm_size, inputbit, outputbit)
    #     exec_info[tile_id]['cal_lat'] /= 100000000 # ns to s, freq = 100MHz
    #     # tile-layer map regestration
    #     cur_layer = tile_info.layer
    #     max_path_delay = 0.0
    #     # update child info by path info
    #     for path in tile_info.paths:
    #         child_id = path.dst
    #         # path_delay = (len(path.via)-1) * path.datavolume / cur_effbw
    #         path_delay = path_delaydict[(tile_id,child_id)]
    #         # update trans time info(cur tile as parent)
    #         max_path_delay = max(max_path_delay, path_delay)
    #         # parent info dict: src layer, path delay, tile id
    #         if 'parent' not in exec_info[child_id]:
    #             exec_info[child_id]['parent'] = []
    #         exec_info[child_id]['parent'].append((cur_layer, path_delay, tile_id))
    #     # update time dep info(cur tile as child)
    #     if 'parent' not in exec_info[tile_id]:
    #         exec_info[tile_id]['begin_time'] = 0.0
    #         exec_info[tile_id]['merge_time'] = 0.0
    #     else:
    #         # merge time: intra-layer data driven
    #         # # get merge and trans list
    #         merge_list = [
    #             element
    #             for element in exec_info[tile_id]['parent']
    #             if isinstance(element, (list, tuple)) and len(element) > 0
    #             and element[0] == cur_layer
    #         ]
    #         trans_list = [
    #             element
    #             for element in exec_info[tile_id]['parent']
    #             if isinstance(element, (list, tuple)) and len(element) > 0
    #             and element[0] != cur_layer
    #         ]
    #         # update merge time
    #         if not merge_list:
    #             exec_info[tile_id]['merge_time'] = 0 
    #         else:
    #             exec_info[tile_id]['merge_time'] = max(merge_lat for _, merge_lat, _ in merge_list)
    #         # update begin time(consider inter-layer parent only)
    #         exec_info[tile_id]['begin_time'] = max(
    #             path_delay + exec_info[parent_id]['begin_time'] + exec_info[parent_id]['cal_lat'] + exec_info[parent_id]['merge_time']
    #             for _, path_delay, parent_id in trans_list)
    #     # begin->compute->merge->trans
    #     exec_info[tile_id]['end_time'] = exec_info[tile_id]['begin_time'] + exec_info[tile_id]['cal_lat'] + exec_info[tile_id]['merge_time'] + max_path_delay
    #     layer_latdict[cur_layer] = max(layer_latdict.get(cur_layer, 0), exec_info[tile_id]['end_time'] - exec_info[tile_id]['begin_time'])
    #     layer_caldict[cur_layer] = max(layer_caldict.get(cur_layer, 0), exec_info[tile_id]['cal_lat'])
    #     layer_mergedict[cur_layer] = max(layer_mergedict.get(cur_layer, 0), exec_info[tile_id]['merge_time'])
    #     layer_transdict[cur_layer] = max(layer_transdict.get(cur_layer, 0), max_path_delay)
    #     if not syn:
    #         ovarall_latency = max(ovarall_latency, exec_info[tile_id]['end_time'])
    # # throughput get from layer_latdict
    # overall_throughput = 1 / max(layer_latdict.values())
    # # if sychronous, update latency by layer
    # if syn:
    #     for layer in layer_latdict.keys():
    #         layer_latdict[layer] = layer_caldict[layer] + layer_mergedict[layer] + layer_transdict[layer]
    #     ovarall_latency = sum(layer_latdict.values())
    #     overall_throughput = 1 / max(layer_latdict.values())
    # # get overall cal latency
    # overall_cal_latency = sum(layer_caldict.values())
    # cal_per = overall_cal_latency / ovarall_latency
    # overall_merge_latency = sum(layer_mergedict.values())
    # merge_per = sum(layer_mergedict.values()) / ovarall_latency
    # return ovarall_latency, overall_throughput, overall_cal_latency, cal_per, overall_merge_latency, merge_per, effbw, layer_mergedict, layer_transdict

def tile_latency_cal(SimConfig_path,tile_indata,inputbit,outputbit):
    modelL_config = cp.ConfigParser()
    modelL_config.read(SimConfig_path, encoding='UTF-8')
    xbar_size = (modelL_config.get('Crossbar level','Xbar_Size'))
    pe_size = list(map(int, modelL_config.get('Tile level', 'PE_Num').split(',')))
    temp_tile_latency = tile_latency_analysis(SimConfig_path=SimConfig_path,
                                              read_row=int(xbar_size[0]),
                                              read_column=int(xbar_size[1]),
                                              indata=0, rdata=0, inprecision=inputbit,
                                              PE_num=int(pe_size[0])*int(pe_size[1]),
                                              default_inbuf_size=0,
                                              default_outbuf_size=0
                                              )
    temp_tile_latency.outbuf.calculate_buf_read_latency(rdata=(int(xbar_size[1])*
                                                               outputbit * int(pe_size[0])*int(pe_size[1]) / 8))
    temp_tile_latency.tile_buf_rlatency = temp_tile_latency.outbuf.buf_rlatency

    temp_tile_latency.update_tile_latency(indata=tile_indata, rdata=tile_indata)

    tile_latency = temp_tile_latency.tile_latency
    return tile_latency

def divide_list_elements_3(input_list, divisor):
    # 使用 numpy 数组进行元素的除法操作
    input_array = np.array(input_list)
    result = input_array / divisor
    # result_array = input_array / divisor
    # result = np.maximum(result, 1e-4)
    return result.tolist()

if __name__ == '__main__':
    home_path = os.getcwd()
    SimConfig_path = os.path.join(home_path, "SimConfig.ini")
    latency_est(SimConfig_path,8,8)