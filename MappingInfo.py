import pickle
import os, re, glob, sys, math
import numpy as np
import pandas as pd
from collections import defaultdict
import configparser as cp
from MNSIM.Interface.interface import *
from MNSIM.Latency_Model.Tile_latency import tile_latency_analysis
from MNSIM.Latency_Model.Pooling_latency import pooling_latency_analysis
from MNSIM.Hardware_Model.Buffer import buffer

def booksim_eval(all_comm_segs, bus_width, freq=1000000000):
    # inter-tile latency estimation by booksim2
    # init sim comfig
    home_path = os.getcwd()
    mesh_size = int(math.sqrt(len(all_comm_segs[0].datas)))
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

    fps = 100
    latency_map = {}
    for idx, comm_seg in enumerate(all_comm_segs):
        layers = comm_seg.layers
        # set injection matrix
        data_matrix = comm_seg.datas
        inj_matrix = divide_list_elements_3(data_matrix, bus_width * freq / fps)
        np.savetxt("inj_rate.txt", inj_matrix, fmt='%.12f')
        log_file = home_path + '/logs/' + str(idx) + '.log'
        booksim_command = home_path + '/booksim ' + cfg_file + ' > ' + log_file
        os.system(booksim_command)
        # additional latency estimation
        packet_latency = os.popen('grep "Packet latency average" ' + log_file + ' | tail -1 | awk \'{print $5}\'').read().strip()
        network_latency = os.popen('grep "Network latency average" ' + log_file + ' | tail -1 | awk \'{print $5}\'').read().strip()
        # print("packet latency is", packet_latency)
        # print("network latency is", network_latency)
        if math.isnan(float(packet_latency)) or math.isnan(float(network_latency)):
            latency = 0 # default latency
        else:
            latency = max(float(packet_latency) - float(network_latency), 0)
        # cur_avglat = trans_time_est(cfg_file)
        for layer in layers:
            # print("layer", layer, "latency is", latency)
            latency_map[layer] = latency
    return latency_map

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
        # pickle.dump(latency_map, open(f"results/latency_dict_bw=1_xbar=256_256.pkl", "wb"))
    elif ideal == 1:
        latency_map = {}
    else:
        latency_map = comm_lat # lat_layer = latency_map[layer]
    bandwidth = bus_width * freq #B/s
    # update tile exec info
    tile_by_layer = defaultdict(list)
    exec_info = defaultdict(dict)
    effbw = {}
    path_delaydict = {}
    layer_latdict = {}
    layer_caldict = {}
    layer_mergedict = {}
    ovarall_latency = 0.0
    overall_throughput = 0.0
    # set tile by layer info
    for idx, tile_info in enumerate(all_tiles_mapping_infos):
        tile_by_layer[tile_info.layer].append(idx)
    # update path latency of each segment
    for idx, comm_seg in enumerate(all_comm_segs):
        # cur layer seg
        layers = comm_seg.layers
        # get all (layer, tile_idx) from tile_by_layer[layers]
        tile_ids = [idx for layer in layers for idx in tile_by_layer[layer]]
        # equivalent bandwidth computation (B/s)
        effbw.update({layer: bandwidth / (1 + latency_map.get(layer, 0)) for layer in layers})
        # get all paths
        merged_paths = []
        for tile_id in tile_ids:
            layer = all_tiles_mapping_infos[tile_id].layer
            merged_paths.extend((layer, path) for path in all_tiles_mapping_infos[tile_id].paths)
        # update via delay
        via_delay = {}
        for layer, path in merged_paths:
            cur_effbw = effbw[layer]
            path_delay = path.datavolume / cur_effbw
            for s, d in zip(path.via[:-1], path.via[1:]):
                # via_delay[(s, d)] = path_delay
                via_delay.update({(s, d): via_delay.get((s, d), 0) + path_delay})
        # update path delay
        for _, path in merged_paths:
            path_delay = 0
            for s, d in zip(path.via[:-1], path.via[1:]):
                path_delay += via_delay[(s, d)]
            path_delaydict[(path.src, path.dst)] = path_delay
            # path_delaydict[(path.src, path.dst)] = path_delay
            # print("path delay from", tile_id, "to", child_id, "is", path_delay)
        
    for idx, tile_info in enumerate(all_tiles_mapping_infos):
        # local info
        tile_id = tile_info.tile_id
        # get ifm size to compute cal latency
        ifm_size = sum(cn.ifmap_size for cn in tile_info.cnode)
        exec_info[tile_id]['cal_lat'] = tile_latency_cal(SimConfig_path, ifm_size, inputbit, outputbit)
        exec_info[tile_id]['cal_lat'] /= 100000000 # ns to s, freq = 100MHz
        # tile-layer map regestration
        cur_layer = tile_info.layer
        max_path_delay = 0.0
        # update child info by path info
        for path in tile_info.paths:
            child_id = path.dst
            # path_delay = (len(path.via)-1) * path.datavolume / cur_effbw
            path_delay = path_delaydict[(tile_id,child_id)]
            # update trans time info(cur tile as parent)
            max_path_delay = max(max_path_delay, path_delay)
            # parent info dict: src layer, path delay, tile id
            if 'parent' not in exec_info[child_id]:
                exec_info[child_id]['parent'] = []
            exec_info[child_id]['parent'].append((cur_layer, path_delay, tile_id))
        # update time dep info(cur tile as child)
        if 'parent' not in exec_info[tile_id]:
            exec_info[tile_id]['begin_time'] = 0.0
            exec_info[tile_id]['merge_time'] = 0.0
        else:
            # merge time: intra-layer data driven
            # # get merge and trans list
            merge_list = [
                element
                for element in exec_info[tile_id]['parent']
                if isinstance(element, (list, tuple)) and len(element) > 0
                and element[0] == cur_layer
            ]
            trans_list = [
                element
                for element in exec_info[tile_id]['parent']
                if isinstance(element, (list, tuple)) and len(element) > 0
                and element[0] != cur_layer
            ]
            # update merge time
            if not merge_list:
                exec_info[tile_id]['merge_time'] = 0 
            else:
                exec_info[tile_id]['merge_time'] = max(merge_lat for _, merge_lat, _ in merge_list)
            # update begin time(consider inter-layer parent only)
            exec_info[tile_id]['begin_time'] = max(
                path_delay + exec_info[parent_id]['begin_time'] + exec_info[parent_id]['cal_lat'] + exec_info[parent_id]['merge_time']
                for _, path_delay, parent_id in trans_list)
        # begin->compute->merge->trans
        exec_info[tile_id]['end_time'] = exec_info[tile_id]['begin_time'] + exec_info[tile_id]['cal_lat'] + exec_info[tile_id]['merge_time'] + max_path_delay
        layer_latdict[cur_layer] = max(layer_latdict.get(cur_layer, 0), exec_info[tile_id]['end_time'] - exec_info[tile_id]['begin_time'])
        layer_caldict[cur_layer] = max(layer_caldict.get(cur_layer, 0), exec_info[tile_id]['cal_lat'])
        layer_mergedict[cur_layer] = max(layer_mergedict.get(cur_layer, 0), exec_info[tile_id]['merge_time'])
        if not syn:
            ovarall_latency = max(ovarall_latency, exec_info[tile_id]['end_time'])
    # throughput get from layer_latdict
    overall_throughput = 1 / max(layer_latdict.values())
    # if sychronous, update latency by layer
    if syn:
        ovarall_latency = sum(layer_latdict.values())
    # get overall cal latency
    overall_cal_latency = sum(layer_caldict.values())
    cal_per = overall_cal_latency / ovarall_latency
    overall_merge_latency = sum(layer_mergedict.values())
    merge_per = sum(layer_mergedict.values()) / ovarall_latency
    return ovarall_latency, overall_throughput, overall_cal_latency, cal_per, overall_merge_latency, merge_per, effbw

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


def trans_time_booksim(homepath):
    # trace_file_dir = "trace_dir"

    inj_rate_dir = homepath + "/inj_dir"
    NoC_latency = []

    # Initialize dictionary to hold latency values
    latency_dict = dict()

    # Get a list of all files in directory
    files = glob.glob(inj_rate_dir + '/*txt')
    # print(len(files))

    # Initialize file counter
    file_counter = 0

    # Create directory to store config files
    os.system('mkdir -p ' + homepath + '/logs/configs')

    # Iterate over all files
    for file in files:

        # Increment file counter
        file_counter += 1

        print('[ INFO] Processing file ' + file + ' ...')

        # Extract file name without extension and absolute path from filename
        run_name = os.path.splitext(os.path.basename(file))[0]

        # Extract first line of file
        line1 = os.popen('head -1 ' + file).read()

        # Extract size of mesh
        line1 = line1.strip()
        values = line1.split()
        mesh_size = int(math.sqrt(len(values)))

        if (mesh_size < 30):
            # Open read file handle of config file
            fp = open(homepath + '/mesh_config_inj_rate', 'r')

            # Set path to config file
            config_file = homepath + '/logs/configs/' + run_name + '_mesh_config'

            # Open write file handle for config file
            outfile = open(config_file, 'w')

            # Iterate over file and set size of mesh in config file
            for line in fp:

                line = line.strip()

                # Search for pattern
                matchobj = re.match(r'^k=', line)

                # Set size of mesh if line in file corresponds to mesh size
                if matchobj:
                    line = 'k=' + str(mesh_size) + ';'

                # Write config to file
                outfile.write(line + '\n')

            # Close file handles
            fp.close()
            outfile.close()

            # Set path to log file
            log_file = homepath + '/logs/' + run_name + '.log'

            # Copy injection rate matrix file
            os.system('cp ' + file + ' ' + homepath + '/inj_rate.txt')

            # Run Booksim with config file and save log
            booksim_command = homepath + '/booksim ' + config_file + ' > ' + log_file
            os.system(booksim_command)

            # Grep for packet latency average from log file
            latency = os.popen(
                'grep "Packet latency average" ' + log_file + ' | tail -1 | awk \'{print $5}\'').read().strip()
            print(latency)
        # latency = os.popen('grep "Trace is finished in" ' + log_file + ' | tail -1 | awk \'{print $5}\'').read().strip()
        else:
            latency = str(mesh_size)
        print('[ INFO] Latency: ' + latency + '\n')

        # Add key, value to dictionary
        latency_dict[run_name] = latency

    # Open output file handle
    # print(homepath + '/logs/latency_mesh.csv')
    outfile = open(homepath + '/logs/latency_mesh.csv', 'w')

    latency_array = np.zeros(file_counter)

    print(range(file_counter))

    # Write latencies to CSV
    for index in range(file_counter):
        print(index)
        run_name = 'inj_rate_' + str(index)
        outfile.write(latency_dict[run_name] + '\n')
        print("latency_dictlatency_dictlatency_dictlatency_dictlatency_dictlatency_dict")
        print(latency_dict[run_name])
        print("latency_dictlatency_dictlatency_dictlatency_dictlatency_dictlatency_dict")
        NoC_latency.append(float(latency_dict[run_name]))
        # total_latency = total_latency + int(latency_dict[run_name])
        latency_array[index] = latency_dict[run_name]

    outfile.close()

    return latency_array, NoC_latency

def create_injection_rate_files(homepath, layer_num, inj_matrix_by_layer):
    print("rgt homepath--------------------")
    print(homepath)
    injection_directory_name = homepath + '/inj_dir'
    dir_exist = os.path.isdir(injection_directory_name)
    if dir_exist == True:
        os.system('rm -rf ' + injection_directory_name)
    os.mkdir(injection_directory_name)

    fac = 10000

    for layer_idx in range(0, layer_num - 1):
        try:
            inj_matrix = inj_matrix_by_layer[layer_idx]
        except IndexError as e:
            print(f"Index {layer_idx} is out of range. Error message: {e}")
        os.chdir(injection_directory_name)
        filename = 'inj_rate_' + str(layer_idx) + '.txt'
        np.savetxt(filename, inj_matrix, fmt='%.12f')
        os.chdir("..")

    return 0

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