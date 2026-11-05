# Interconnect estimation of SIAM tool
import os, re, glob, sys, math
import numpy as np
import pandas as pd
from subprocess import call
from pathlib import Path
import math

#ENABLE_XY_AVR = True
ENABLE_XY_OURS = True
ENABLE_XY_AVR = False
#ENABLE_XY_OURS = False

def interconnect_estimation(mapping_matrix,VirtualMappingInfo, Network, xbar_size, xbar_group_num):
    homepath = '/home/zhouyifei/Desktop/MNSIM-2.0'
    homepath = homepath + '/MNSIM/NoC'

    network_type, arch_type = obtain_interconnect_spec(homepath)
    # arch_type = 'serial'
    print(arch_type)
    
    if ENABLE_XY_AVR:
            num_layers, volume_per_tile = create_injection_rate_xy_avrdata(network_type, arch_type, homepath, mapping_matrix)
            latency_array, NoC_latency = interconnect_latency_estimation(homepath)
            interconnect_latency, sum_packs, layer_packs, effective_delay, per_layer_latency = postprocess_latency_array_xy_avrdata(homepath, latency_array, mapping_matrix, num_layers,volume_per_tile, arch_type)
            latency_file = open(homepath + '/Final_Results/Latency.csv', 'a')
            latency_file.write('--------------------------------------------ENABLE_XY_AVR-------------------------------------------' + '--\n'
                                 +'-- Total interconnect using XY-ROUTING v1.0 (xy+average) latency is ' + str(interconnect_latency) + ' ns'+' --\n'
                                 +'--Packs each layer: '+ str(layer_packs)+'--\n'
                                 +'--total packs:'+str(sum_packs)+'--\n'
                                 +'--effective latency(only use inj matrix):'+str(effective_delay)+'--\n'
                                 +'--per layer latency:'+str(per_layer_latency)+'--\n'
                                 +'--------------------------------------------ENABLE_XY_AVR-------------------------------------------' + '--\n'
                               )
            latency_file.close()
    elif ENABLE_XY_OURS:
            num_layers, layer_packs = create_injection_rate_xy_ours(network_type,VirtualMappingInfo,arch_type, homepath,mapping_matrix)
            latency_array, NoC_latency = interconnect_latency_estimation(homepath)
            interconnect_latency, sum_packs, effective_delay, per_layer_latency = postprocess_latency_array_xy_ours(num_layers, latency_array, arch_type, layer_packs)

            latency_file = open(homepath + '/Final_Results/Latency.csv', 'a')
            latency_file.write('--------------------------------------------ENABLE_XY_OURS--'+Network+'-------------------------------------------'+'--\n'
                                 +'xbar_size:'+str(xbar_size)+'--\n'
                                 +'group_num:' + str(xbar_group_num) + '--\n'
                                 +'-- Total interconnect using XY-ROUTING v2.0(xy+ours) latency is ' + str(interconnect_latency) + ' ns'+' --\n'
                                 +'--Packs each layer: '+ str(layer_packs)+'--\n'
                                 +'--total packs:'+str(sum_packs)+'--\n'
                                 +'--effective latency(only use inj matrix):'+str(effective_delay)+'--\n'
                                 +'--per layer latency:'+str(per_layer_latency)+'--\n'
                                 +'--------------------------------------------ENABLE_XY_OURS-------------------------------------------'+'--\n'
                               )
            latency_file.close()
    else:
            num_layers, ip_activation_per_tile, volume_per_tile, layer_packs = create_injection_rate_original(network_type,arch_type, mapping_matrix, homepath)
            total_packs = np.sum(layer_packs)
            latency_array, NoC_latency = interconnect_latency_estimation(homepath)
            interconnect_latency,ori_interconnect_latency,effective_delay,per_layer_latency= postprocess_latency_array_original(num_layers ,ip_activation_per_tile,volume_per_tile, latency_array, mapping_matrix, arch_type,layer_packs)
            latency_file = open(homepath + '/Final_Results/Latency.csv', 'a')
            latency_file.write('--------------------------------------------ORIN & ORIN+PACKS-------------------------------------------' + '--\n'
                               +'-- Total ideal interconnect latency(packs) is ' + str(interconnect_latency) + ' ns'+' --\n'
                               +'--Packs each layer: '+ str(layer_packs)+'--\n'
                               +'--total packs:'+str(total_packs)+'--\n'
                               +'--effective latency(only use inj matrix):'+str(effective_delay)+'--\n'
                               +'--per layer latency:'+str(per_layer_latency)+'--\n'
                               +'--Total ideal interconnect latency(original) is '+str(ori_interconnect_latency)+' ns'+' --\n'
                               +'--------------------------------------------ORIN & ORIN+PACKS-------------------------------------------' + '--\n')
            latency_file.close()
    
    #ori_packs = layer_packs
    #num_layers, num_tiles_per_layer, ip_activation_per_tile, volume_per_tile = create_injection_rate_xy_avrdata(network_type, arch_type, homepath, mapping_matrix)
    #latency_array, NoC_latency = interconnect_latency_estimation(homepath)
    #interconnect_latency = postprocess_latency_array_PxT(num_layers , latency_array, arch_type, ori_packs)
    #latency_file = open(homepath + '/Final_Results/Latency.csv', 'a')
    #latency_file.write('-- Total interconnect latency(xy_avr_latency * sum of original average packs(no xy)) is ' + str(interconnect_latency) + ' ns'+' --\n')

    return NoC_latency


def obtain_interconnect_spec(homepath):
    spec_file = '/interconnect_spec_file.txt'
    arch_type = os.popen('grep "arch_type" ' + homepath + spec_file + ' | tail -1 | awk \'{print $3}\'').read().strip()
    network_type = os.popen('grep "network_type" ' + homepath + spec_file + ' | tail -1 | awk \'{print $3}\'').read().strip()

    return network_type, arch_type

# Latency estimation for interconnect
def interconnect_latency_estimation(homepath):
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

def coord_trans(src_node, dest_node, n):
    inj_src = src_node[0] * n + src_node[1]
    inj_dest = dest_node[0] * n + dest_node[1]
    return inj_src, inj_dest

def create_injection_rate_original(network_type, arch_type, mapping_matrix, homepath):
    injection_directory_name = homepath + '/inj_dir'
    dir_exist = os.path.isdir(injection_directory_name)
    if dir_exist == True:
        os.system('rm -rf ' + injection_directory_name)
    os.mkdir(injection_directory_name)

    quantization_bit = 1
    bus_width = 32
    freq = 1000000000

    num_tiles_per_layer = pd.read_csv(homepath + '/to_interconnect/num_tiles_per_layer.csv', header=None)
    ip_activation = pd.read_csv(homepath + '/to_interconnect/ip_activation.csv', header=None)
    fps = pd.read_csv(homepath + '/to_interconnect/fps.csv', header=None)
    num_layers = num_tiles_per_layer.size
    total_tiles = num_tiles_per_layer.sum()
    volume_per_tile = np.zeros(num_layers - 1)

    #目前这个vol，是平均分的，把下一层的总输入除以上一层和下一层的总连接数，得到每个连接的数据量，但是实际每个连接的数据量是不一样的
    #改这个的话，要把每个连接记录下来，连接的总数据量是不变的，要怎么分配到每个连接呢？不均分的话，其实只要保证总数据量不出错就行了
    for layer_idx in range(num_layers - 1):
        volume_per_tile[layer_idx] = ((ip_activation.loc[layer_idx + 1][0]) * fps.loc[0][0]) / (num_tiles_per_layer.loc[layer_idx][0] * num_tiles_per_layer.loc[layer_idx + 1][0])

    ip_activation_per_tile = np.zeros(num_layers - 1)
    for layer_idx in range(num_layers - 1):
        #ip_activation_per_tile[layer_idx] = ip_activation.loc[layer_idx + 1][0] * fps.loc[0][0] / (num_tiles_per_layer.loc[layer_idx][0] * num_tiles_per_layer.loc[layer_idx + 1][0])
        ip_activation_per_tile[layer_idx] = ip_activation.loc[layer_idx + 1][0] * fps.loc[0][0]

    src_coord = {}
    dest_coord = {}
    n = len(mapping_matrix)  # 假设 mapping_matrix 是方阵
    for layer_idx in range(0, num_layers - 1):
        src_coord[layer_idx] = {"coordinate": []}
        dest_coord[layer_idx + 1] = {"coordinate": []}
        for i in range(len(mapping_matrix)):
            for j in range(len(mapping_matrix[i])):
                if mapping_matrix[i][j] == layer_idx:
                    src_coord[layer_idx]["coordinate"].append((i, j))
                elif mapping_matrix[i][j] == layer_idx + 1:
                    dest_coord[layer_idx + 1]["coordinate"].append((i, j))
    layer_packs = np.zeros(num_layers - 1)
    for layer_idx in range(0, num_layers - 1):
        cur_packsnum = 0
        inj_matrix = np.zeros((n * n, n * n))
        src_coord_list = src_coord[layer_idx]["coordinate"]
        dest_coord_list = dest_coord[layer_idx + 1]["coordinate"]

        for src_node in src_coord_list:
            for dest_node in dest_coord_list:
                inj_src, inj_dest = coord_trans(src_node, dest_node, n)
                inj_matrix[inj_src, inj_dest] += (volume_per_tile[layer_idx] / bus_width) / freq
                cur_packsnum += (volume_per_tile[layer_idx] / bus_width)
                if (inj_matrix[inj_src, inj_dest] < 0.00001):
                    inj_matrix[inj_src, inj_dest] = 0.0001
        layer_packs[layer_idx] = cur_packsnum
        if (arch_type == 'serial'):
            os.chdir(injection_directory_name)
            filename = 'inj_rate_' + str(layer_idx) + '.txt'
            np.savetxt(filename, inj_matrix, fmt='%.12f')
            os.chdir("..")

    if (arch_type == 'parallel'):
        os.chdir(injection_directory_name);
        filename = 'inj_rate_0.txt'
        np.savetxt(filename, inj_matrix, fmt='%.12f')
        os.chdir("..")

    return num_layers, ip_activation_per_tile, volume_per_tile, layer_packs

def postprocess_latency_array_original(num_layers ,ip_activation_per_tile, volume_per_tile, latency_array,
                              mapping_matrix, arch_type, layer_packs):

    bus_width = 32
    freq = 1000000000

    avg_const_delay = np.zeros(num_layers - 1)
    ori_per_layer_latency = np.zeros(num_layers - 1)
    per_layer_latency = np.zeros(num_layers - 1)
    effective_delay = np.zeros(num_layers - 1)

    src_coord = {}
    dest_coord = {}
    n = len(mapping_matrix)  # 假设 mapping_matrix 是方阵
    for layer_idx in range(0, num_layers - 1):
        src_coord[layer_idx] = {"coordinate": []}
        dest_coord[layer_idx + 1] = {"coordinate": []}
        for i in range(len(mapping_matrix)):
            for j in range(len(mapping_matrix[i])):
                if mapping_matrix[i][j] == layer_idx:
                    src_coord[layer_idx]["coordinate"].append((i, j))
                elif mapping_matrix[i][j] == layer_idx + 1:
                    dest_coord[layer_idx + 1]["coordinate"].append((i, j))

    for layer_idx in range(0, num_layers - 1):
        inj_matrix = np.zeros((n * n, n * n))
        src_coord_list = src_coord[layer_idx]["coordinate"]
        dest_coord_list = dest_coord[layer_idx + 1]["coordinate"]

        if (arch_type == 'serial'):
            avg_latency_layer = latency_array[layer_idx]
        elif (arch_type == 'parallel'):
            avg_latency_layer = latency_array[0]
        else:
            print('Architecture type is not supported')
            assert (0)
        weighted_const_delay = 0
        for src_node in src_coord_list:
            for dest_node in dest_coord_list:
                const_dist = abs(src_node[0] - dest_node[0]) + abs(src_node[1] - dest_node[1])  # number of links
                const_pipeline_delay = 4 * (const_dist + 1)  # number of routers visited is one more than number of links
                source_sink_delay = 3;
                inj_src, inj_dest = coord_trans(src_node, dest_node, n)
                inj_matrix[inj_src, inj_dest] += (volume_per_tile[layer_idx] / bus_width) / freq
                if (inj_matrix[inj_src, inj_dest] < 0.00001):
                    inj_matrix[inj_src, inj_dest] = 0.0001
                total_const_delay = const_dist + const_pipeline_delay + source_sink_delay
                weighted_const_delay += inj_matrix[inj_src, inj_dest]* total_const_delay

        avg_const_delay[layer_idx] = weighted_const_delay / np.sum(inj_matrix)

        if (math.isnan(avg_latency_layer)):
            effective_delay[layer_idx] = 0
        else:
            #effective_delay[layer_idx] = avg_latency_layer - avg_const_delay[layer_idx]
            effective_delay[layer_idx] = avg_latency_layer

        if (effective_delay[layer_idx] < 0):
            effective_delay[layer_idx] = 0

       #per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * (
       #            ip_activation_per_tile[layer_idx] * quantization_bit + 32) / bus_width + avg_const_delay[layer_idx]
        ori_per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * (ip_activation_per_tile[layer_idx]) / bus_width
        per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * layer_packs[layer_idx]
    print('orin_effective_delay', effective_delay)
    ori_total_latency = 0
    if (arch_type == 'serial'):
        ori_total_latency = np.sum(ori_per_layer_latency)
        total_latency = np.sum(per_layer_latency)
    elif (arch_type == 'parallel'):
        total_latency = np.sum(per_layer_latency) / (num_layers - 1)
    else:
        total_latency = -1

    return total_latency, ori_total_latency, effective_delay, per_layer_latency

def postprocess_latency_array_PxT(num_layers , latency_array, arch_type, layer_packs):

    per_layer_latency = np.zeros(num_layers - 1)
    effective_delay = np.zeros(num_layers - 1)

    for layer_idx in range(0, num_layers - 1):

        if (arch_type == 'serial'):
            avg_latency_layer = latency_array[layer_idx]
        elif (arch_type == 'parallel'):
            avg_latency_layer = latency_array[0]
        else:
            print('Architecture type is not supported')
            assert (0)
        
        if (math.isnan(avg_latency_layer)):
            effective_delay[layer_idx] = 0
        else:
            effective_delay[layer_idx] = avg_latency_layer

        if (effective_delay[layer_idx] < 0):
            effective_delay[layer_idx] = 0

        per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * layer_packs[layer_idx]
    sum_packs = 0
    for i in range(len(layer_packs)):
        sum_packs += layer_packs[i]
    print("total packs : ", sum_packs)
    print('PxT_layer_packs', layer_packs)
    print('PxT_effective_delay', effective_delay)
    if (arch_type == 'serial'):
        total_latency = np.sum(per_layer_latency)
    elif (arch_type == 'parallel'):
        total_latency = np.sum(per_layer_latency) / (num_layers - 1)
    else:
        total_latency = -1

    return total_latency






def create_injection_rate_xy_avrdata(network_type, arch_type, homepath, mapping_matrix):
    print(homepath)
    injection_directory_name = homepath + '/inj_dir'
    dir_exist = os.path.isdir(injection_directory_name)
    if dir_exist == True:
        os.system('rm -rf ' + injection_directory_name)
    os.mkdir(injection_directory_name)

    quantization_bit = 1
    bus_width = 32
    freq = 1000000000

    num_tiles_per_layer = pd.read_csv(homepath + '/to_interconnect/num_tiles_per_layer.csv', header=None)
    print(num_tiles_per_layer)
    ip_activation = pd.read_csv(homepath + '/to_interconnect/ip_activation.csv', header=None)
    fps = pd.read_csv(homepath + '/to_interconnect/fps.csv', header=None)
    num_layers = num_tiles_per_layer.size
    print("rgt num_layers--------------------")
    print(num_layers)
    total_tiles = num_tiles_per_layer.sum()
    volume_per_tile = np.zeros(num_layers - 1)

    for layer_idx in range(num_layers - 1):
        volume_per_tile[layer_idx] = ((ip_activation.loc[layer_idx + 1][0] * quantization_bit + bus_width) * fps.loc[0][
            0]) / \
                                     (num_tiles_per_layer.loc[layer_idx][0] * num_tiles_per_layer.loc[layer_idx + 1][
                                         0])

    ip_activation_per_tile = np.zeros(num_layers - 1)
    for layer_idx in range(num_layers - 1):
        ip_activation_per_tile[layer_idx] = ip_activation.loc[layer_idx + 1][0] / (
                num_tiles_per_layer.loc[layer_idx][0] * num_tiles_per_layer.loc[layer_idx + 1][0])

    src_coord = {}
    dest_coord = {}
    n = len(mapping_matrix)  # 假设 mapping_matrix 是方阵
    for layer_idx in range(0, num_layers - 1):
        src_coord[layer_idx] = {"coordinate": []}
        dest_coord[layer_idx + 1] = {"coordinate": []}
        for i in range(len(mapping_matrix)):
            for j in range(len(mapping_matrix[i])):
                if mapping_matrix[i][j] == layer_idx:
                    src_coord[layer_idx]["coordinate"].append((i, j))
                elif mapping_matrix[i][j] == layer_idx + 1:
                    dest_coord[layer_idx + 1]["coordinate"].append((i, j))
    # print(src_coord)
    # print(dest_coord)

    # assert(num_tiles_per_layer.size()) == num_layers);
    for layer_idx in range(0, num_layers - 1):
        inj_matrix = np.zeros((n * n, n * n))
        src_coord_list = src_coord[layer_idx]["coordinate"]
        dest_coord_list = dest_coord[layer_idx + 1]["coordinate"]

        for src_node in src_coord_list:
            for dest_node in dest_coord_list:
                current_node = list(src_node)
                mid_node = []
                deltax = -1 if src_node[0] > dest_node[0] else 1
                deltay = -1 if src_node[1] > dest_node[1] else 1

                while current_node[0] != dest_node[0]:
                    cur_src_node = list(current_node)
                    current_node[0] += deltax
                    cur_dest_node = list(current_node)
                    mid_node.append((current_node[0], current_node[1]))
                    inj_src, inj_dest = coord_trans(cur_src_node, cur_dest_node, n)
                    inj_matrix[inj_src, inj_dest] += (volume_per_tile[layer_idx] / bus_width) / freq
                    if (inj_matrix[inj_src, inj_dest] < 0.00001):
                        inj_matrix[inj_src, inj_dest] = 0.0001

                while current_node[1] != dest_node[1]:
                    cur_src_node = list(current_node)
                    current_node[1] += deltay
                    cur_dest_node = list(current_node)
                    mid_node.append((current_node[0], current_node[1]))
                    inj_src, inj_dest = coord_trans(cur_src_node, cur_dest_node, n)
                    inj_matrix[inj_src, inj_dest] += (volume_per_tile[layer_idx] / bus_width) / freq
                    if (inj_matrix[inj_src, inj_dest] < 0.00001):
                        inj_matrix[inj_src, inj_dest] = 0.0001

        if (arch_type == 'serial'):
            os.chdir(injection_directory_name)
            filename = 'inj_rate_' + str(layer_idx) + '.txt'
            np.savetxt(filename, inj_matrix, fmt='%.12f')
            os.chdir("..")

    if (arch_type == 'parallel'):
        os.chdir(injection_directory_name)
        filename = 'inj_rate_0.txt'
        np.savetxt(filename, inj_matrix, fmt='%.12f')
        os.chdir("..")

    return num_layers, volume_per_tile

def postprocess_latency_array_xy_avrdata(homepath, latency_array, mapping_matrix, num_layers, volume_per_tile, arch_type):
    quantization_bit = 1
    bus_width = 32
    freq = 1000000000
    print("this is num_layers----------------------------")
    print(num_layers)
    avg_const_delay = np.zeros(num_layers - 1)
    per_layer_latency = np.zeros(num_layers - 1)
    effective_delay = np.zeros(num_layers - 1)

    fps = pd.read_csv(homepath + '/to_interconnect/fps.csv', header=None)

    src_coord = {}
    dest_coord = {}
    n = len(mapping_matrix)  # 假设 mapping_matrix 是方阵
    for layer_idx in range(0, num_layers - 1):
        src_coord[layer_idx] = {"coordinate": []}
        dest_coord[layer_idx + 1] = {"coordinate": []}
        for i in range(len(mapping_matrix)):
            for j in range(len(mapping_matrix[i])):
                if mapping_matrix[i][j] == layer_idx:
                    src_coord[layer_idx]["coordinate"].append((i, j))
                elif mapping_matrix[i][j] == layer_idx + 1:
                    dest_coord[layer_idx + 1]["coordinate"].append((i, j))

    # weighted_const_delay = 0
    # assert(num_tiles_per_layer.size()) == num_layers);
    layer_packs = np.zeros(num_layers - 1)
    for layer_idx in range(0, num_layers - 1):
        inj_matrix = np.zeros((n * n, n * n))
        src_coord_list = src_coord[layer_idx]["coordinate"]
        dest_coord_list = dest_coord[layer_idx + 1]["coordinate"]
        weighted_const_delay = 0
        const_dist = 0
        curlayer_packs = 0
        const_pipeline_delay = 0
        total_const_delay = 0

        if (arch_type == 'serial'):
            avg_latency_layer = latency_array[layer_idx]
        elif (arch_type == 'parallel'):
            avg_latency_layer = latency_array[0]
        else:
            print('Architecture type is not supported')
            assert (0)

        for src_node in src_coord_list:
            for dest_node in dest_coord_list:
                current_node = list(src_node)
                mid_node = []
                deltax = -1 if src_node[0] > dest_node[0] else 1
                deltay = -1 if src_node[1] > dest_node[1] else 1

                while current_node[0] != dest_node[0]:
                    cur_src_node = list(current_node)
                    current_node[0] += deltax
                    cur_dest_node = list(current_node)
                    mid_node.append((current_node[0], current_node[1]))
                    inj_src, inj_dest = coord_trans(cur_src_node, cur_dest_node, n)
                    inj_matrix[inj_src, inj_dest] += (volume_per_tile[layer_idx] / bus_width) / freq
                    curlayer_packs += (volume_per_tile[layer_idx] / bus_width)
                    const_dist += abs(cur_src_node[0] - cur_dest_node[0]) + abs(cur_src_node[1] - cur_dest_node[1])
                    const_pipeline_delay = 4 * (const_dist + 1)
                    source_sink_delay = 3
                    total_const_delay = const_dist + const_pipeline_delay + source_sink_delay
                    if (inj_matrix[inj_src, inj_dest] < 0.00001):
                        inj_matrix[inj_src, inj_dest] = 0.0001
                    weighted_const_delay += inj_matrix[inj_src, inj_dest] * total_const_delay

                while current_node[1] != dest_node[1]:
                    cur_src_node = list(current_node)
                    current_node[1] += deltay
                    cur_dest_node = list(current_node)
                    mid_node.append((current_node[0], current_node[1]))
                    inj_src, inj_dest = coord_trans(cur_src_node, cur_dest_node, n)
                    inj_matrix[inj_src, inj_dest] += (volume_per_tile[layer_idx] / bus_width) / freq
                    curlayer_packs += (volume_per_tile[layer_idx] / bus_width)
                    const_dist += abs(cur_src_node[0] - cur_dest_node[0]) + abs(
                        cur_src_node[1] - cur_dest_node[1])  # jia duo le
                    const_pipeline_delay = 4 * (const_dist + 1)
                    source_sink_delay = 3
                    total_const_delay = const_dist + const_pipeline_delay + source_sink_delay
                    if (inj_matrix[inj_src, inj_dest] < 0.00001):
                        inj_matrix[inj_src, inj_dest] = 0.0001
                    weighted_const_delay += inj_matrix[inj_src, inj_dest] * total_const_delay
        layer_packs[layer_idx] = curlayer_packs
        print("packs_each layer 1.0:", layer_packs)
        avg_const_delay[layer_idx] = weighted_const_delay / np.sum(inj_matrix)

        if (math.isnan(avg_latency_layer)):
            effective_delay[layer_idx] = 0
        else:
            #effective_delay[layer_idx] = avg_latency_layer - avg_const_delay[layer_idx]
            effective_delay[layer_idx] = avg_latency_layer

        if (effective_delay[layer_idx] < 0):
            effective_delay[layer_idx] = 0

        #per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * (
        #        ip_activation_per_tile[layer_idx] * quantization_bit + 32) / bus_width + avg_const_delay[layer_idx]
        sum_packs = 0
        for i in range(len(layer_packs)):
            sum_packs += layer_packs[i]
        print("total packs : ", sum_packs)
        print("effective_delay:", effective_delay)
        per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * layer_packs[layer_idx]
        print("per_layer_latency:", per_layer_latency)

    if (arch_type == 'serial'):
        total_latency = np.sum(per_layer_latency)
    elif (arch_type == 'parallel'):
        total_latency = np.sum(per_layer_latency) / (num_layers - 1)
    else:
        total_latency = -1

    return total_latency, sum_packs, layer_packs, effective_delay, per_layer_latency



def create_injection_rate_xy_ours(network_type, VirtualMappingInfo, arch_type, homepath,mapping_matrix):
    print("rgt homepath--------------------")
    print(homepath)
    injection_directory_name = homepath + '/inj_dir'
    dir_exist = os.path.isdir(injection_directory_name)
    if dir_exist == True:
        os.system('rm -rf ' + injection_directory_name)
    os.mkdir(injection_directory_name)

    quantization_bit = 1
    bus_width = 32
    freq = 1000000000

    num_tiles_per_layer = pd.read_csv(homepath + '/to_interconnect/num_tiles_per_layer.csv', header=None)
    print(num_tiles_per_layer)
    ip_activation = pd.read_csv(homepath + '/to_interconnect/ip_activation.csv', header=None)
    fps = pd.read_csv(homepath + '/to_interconnect/fps.csv', header=None)
    num_layers = num_tiles_per_layer.size
    print("rgt num_layers--------------------")
    print(num_layers)

    avg_const_delay = np.zeros(num_layers - 1)
    per_layer_latency = np.zeros(num_layers - 1)
    effective_delay = np.zeros(num_layers - 1)

    list_tile_per_layer=[]
    for i in range(num_layers):
        list_tile_per_layer.append(num_tiles_per_layer.loc[i][0])

    print("list_tile_per_layer = ",list_tile_per_layer)

    total_tiles = num_tiles_per_layer.sum()
    volume_per_tile = np.zeros(num_layers - 1)

    for layer_idx in range(num_layers - 1):
        volume_per_tile[layer_idx] = ((ip_activation.loc[layer_idx + 1][0] * quantization_bit + bus_width) * fps.loc[0][
            0]) / \
                                     (num_tiles_per_layer.loc[layer_idx][0] * num_tiles_per_layer.loc[layer_idx + 1][
                                         0])
        
    #physical mapping
    #读入每层的virtual mapping tile info

    #需要把vmapping 的序号跟这边phyMapping的坐标联系起来
    for layer_idx in range(num_layers):#0-10
        tileN =0
        if VirtualMappingInfo[layer_idx][0]["layer_type"] != 'element_sum':
            for i in range(len(mapping_matrix)):
                for j in range(len(mapping_matrix[i])):
                        if mapping_matrix[i][j] == layer_idx:
                            VirtualMappingInfo[layer_idx][tileN]["coord"] = (i, j) #这一层的所有node都是src node，还有src node对应的数据量，要对应存下来
                            tileN += 1
        else:
            print('hehehheheheheheehe')
            print(VirtualMappingInfo[layer_idx+1][0]["coord"])
            VirtualMappingInfo[layer_idx][0]["coord"] = VirtualMappingInfo[layer_idx+1][0]["coord"]

    for layer_idx in range(num_layers):#0-10
        tileN =0
        if VirtualMappingInfo[layer_idx][0]["layer_type"] == 'element_sum':
            print(VirtualMappingInfo[layer_idx+1][0]["coord"])
            VirtualMappingInfo[layer_idx][0]["coord"] = VirtualMappingInfo[layer_idx+1][0]["coord"]

    print("-------------------------VM_layerPerTile new--------------------------")
    print(VirtualMappingInfo)

    src_coord = []
    for layer_idx in range(num_layers):
            src_coord.append([{} for _ in range(list_tile_per_layer[layer_idx])])

    for layer_idx in range(num_layers - 1):  # 0-10,第11层需要输出
        layerTileNum = list_tile_per_layer[layer_idx]
        if VirtualMappingInfo[layer_idx][0]["layer_type"] != 'element_sum':
            for tileN in range(layerTileNum):
                src_coord[layer_idx][tileN] = {
                    "layerNum": layer_idx,
                    "tileNum": layerTileNum,
                    "src_node": (0, 0),
                    "odata":0
                }
                src_coord[layer_idx][tileN]["src_node"] = VirtualMappingInfo[layer_idx][tileN]["coord"]
                src_coord[layer_idx][tileN]["odata"] = VirtualMappingInfo[layer_idx][tileN]["Odata"]
    print("-------------------------src_coord new--------------------------")
    print(src_coord)

    dest_coord = []
    for layer_idx in range(num_layers):
            dest_coord.append([{} for _ in range(list_tile_per_layer[layer_idx])])

    for layer_idx in range(num_layers-1):  # 0-10,第11层需要输出
        layerTileNum = list_tile_per_layer[layer_idx]
        if VirtualMappingInfo[layer_idx][0]["layer_type"] != 'element_sum':
            for tileN in range(layerTileNum):
                dest_coord[layer_idx][tileN] = {
                    "layerNum": layer_idx,
                    "tileNum":  layerTileNum,
                    "dest_node": (0, 0)
                }
                if "AccEn" in VirtualMappingInfo[layer_idx][tileN] and VirtualMappingInfo[layer_idx][tileN]["AccEn"] == 1:
                    # 检查当前层的下一个点是否存在
                    if tileN + 1 < len(VirtualMappingInfo[layer_idx]):
                        dest_coord[layer_idx][tileN]["dest_node"] = VirtualMappingInfo[layer_idx][tileN + 1]["coord"]
                        dest_coord[layer_idx][tileN]["tileNum"] = layerTileNum
                        dest_coord[layer_idx][tileN]["layerNum"] = layer_idx
                    else:
                        print(f"错误: 当前层的下一个点 (tileN + 1 = {tileN + 1}) 超出范围，layer_idx = {layer_idx}。")
                elif "concatEn" in VirtualMappingInfo[layer_idx][tileN] and \
                        VirtualMappingInfo[layer_idx][tileN]["concatEn"] == 1:
                    # 检查下一层是否存在并且至少有一个点
                    if layer_idx + 1 < num_layers and len(VirtualMappingInfo[layer_idx + 1]) > 0:
                        dest_coord[layer_idx][tileN]["dest_node"] = VirtualMappingInfo[layer_idx + 1][0]["coord"]
                        dest_coord[layer_idx][tileN]["tileNum"] = layerTileNum
                        dest_coord[layer_idx][tileN]["layerNum"] = layer_idx
                    else:
                        print(f"错误: 下一层或下一层的第一个点不存在，layer_idx = {layer_idx}。")
                elif (VirtualMappingInfo[layer_idx][tileN]['AccEn'] == 0) and (
                        VirtualMappingInfo[layer_idx][tileN]['concatEn'] == 0):
                    # 直接输出到下一层的第一个点
                    if layer_idx + 1 < num_layers and len(VirtualMappingInfo[layer_idx + 1]) > 0:
                        dest_coord[layer_idx][tileN]["dest_node"] = VirtualMappingInfo[layer_idx + 1][0]["coord"]
                        dest_coord[layer_idx][tileN]["tileNum"] = layerTileNum
                        dest_coord[layer_idx][tileN]["layerNum"] = layer_idx
                    else:
                        print(f"错误: 下一层或下一层的第一个点不存在，layer_idx = {layer_idx}。")
    print("-------------------------dest_coord new--------------------------")
    print(dest_coord)

   # ip_activation_per_tile = np.zeros(num_layers - 1)
   # for layer_idx in range(num_layers - 1):
   #     if VirtualMappingInfo[layer_idx][0]["layer_type"] != 'element_sum':
   #         ip_activation_per_tile[layer_idx] = ip_activation.loc[layer_idx + 1][0] / (
   #                     num_tiles_per_layer.loc[layer_idx][0] * num_tiles_per_layer.loc[layer_idx + 1][0])
   # print('ip_activation_per_tile', ip_activation_per_tile)

    n = len(mapping_matrix)
    layer_packs = np.zeros(num_layers - 1)
    for layer_idx in range(0, num_layers - 1):
        if VirtualMappingInfo[layer_idx][0]["layer_type"] != 'element_sum':
            print('layer_idx valid:', layer_idx)
            inj_matrix = np.zeros((n * n, n * n))
            layerTileNum = list_tile_per_layer[layer_idx]
            weighted_const_delay = 0
            const_dist = 0
            curlayer_packs= 0
            for tilenum in range(layerTileNum):
                mid_node = []
                src_node = src_coord[layer_idx][tilenum]["src_node"]
                src_node_odata = int(src_coord[layer_idx][tilenum]["odata"]) * fps.loc[0][0]
                print(fps.loc[0][0])
                dest_node = dest_coord[layer_idx][tilenum]["dest_node"]
                current_node = list(src_node)
                deltax = -1 if src_node[0] > dest_node[0] else 1
                deltay = -1 if src_node[1] > dest_node[1] else 1

                while current_node[0] != dest_node[0]:
                    cur_src_node = list(current_node)
                    current_node[0] += deltax
                    cur_dest_node = list(current_node)
                    mid_node.append((current_node[0], current_node[1]))
                    inj_src, inj_dest = coord_trans(cur_src_node, cur_dest_node, n)
                    inj_matrix[inj_src, inj_dest] += ((src_node_odata / bus_width ) / freq)
                    curlayer_packs += src_node_odata / bus_width
                    const_dist += abs(cur_src_node[0] - cur_dest_node[0]) + abs(cur_src_node[1] - cur_dest_node[1])# rst in while
                    const_pipeline_delay = 4 * (const_dist + 1)
                    source_sink_delay = 3
                    total_const_delay = const_dist + const_pipeline_delay + source_sink_delay
                    if (inj_matrix[inj_src, inj_dest] < 0.00001):
                        inj_matrix[inj_src, inj_dest] = 0.0001
                    weighted_const_delay += inj_matrix[inj_src, inj_dest] * total_const_delay

                while current_node[1] != dest_node[1]:
                    cur_src_node = list(current_node)
                    cur_src_node = list(current_node)
                    current_node[1] += deltay
                    cur_dest_node = list(current_node)
                    mid_node.append((current_node[0], current_node[1]))
                    inj_src, inj_dest = coord_trans(cur_src_node, cur_dest_node, n)
                    inj_matrix[inj_src, inj_dest] += ((src_node_odata / bus_width) / freq)
                    curlayer_packs += src_node_odata / bus_width
                    const_dist += abs(cur_src_node[0] - cur_dest_node[0]) + abs(cur_src_node[1] - cur_dest_node[1])
                    const_pipeline_delay = 4 * (const_dist + 1)
                    source_sink_delay = 3
                    total_const_delay = const_dist + const_pipeline_delay + source_sink_delay
                    if (inj_matrix[inj_src, inj_dest] < 0.00001):
                        inj_matrix[inj_src, inj_dest] = 0.0001
                    weighted_const_delay += inj_matrix[inj_src, inj_dest] * total_const_delay
        layer_packs[layer_idx] = curlayer_packs
        print("packs_each layer 2.0:", layer_packs)
        avg_const_delay[layer_idx] = weighted_const_delay / np.sum(inj_matrix)

        if (arch_type == 'serial'):
            os.chdir(injection_directory_name)
            filename = 'inj_rate_' + str(layer_idx) + '.txt'
            np.savetxt(filename, inj_matrix, fmt='%.12f')
            os.chdir("..")

    if (arch_type == 'parallel'):
        os.chdir(injection_directory_name)
        filename = 'inj_rate_0.txt'
        np.savetxt(filename, inj_matrix, fmt='%.12f')
        os.chdir("..")


    return num_layers, layer_packs

def postprocess_latency_array_xy_ours(num_layers, latency_array, arch_type, layer_packs):
    quantization_bit = 1
    bus_width = 32
    freq = 1000000000
    per_layer_latency = np.zeros(num_layers - 1)
    effective_delay = np.zeros(num_layers - 1)
    print(latency_array)

    for layer_idx in range(0, num_layers - 1):
        if (arch_type == 'serial'):
            avg_latency_layer = latency_array[layer_idx]
        elif (arch_type == 'parallel'):
            avg_latency_layer = latency_array[0]
        else:
            print('Architecture type is not supported')
            assert (0)

        if (math.isnan(avg_latency_layer)):
            effective_delay[layer_idx] = 0
        else:
            #effective_delay[layer_idx] = avg_latency_layer - avg_const_delay[layer_idx]
            effective_delay[layer_idx] = avg_latency_layer

        if (effective_delay[layer_idx] < 0):
            effective_delay[layer_idx] = 0

        #per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * (
        #        ip_activation_per_tile[layer_idx] * quantization_bit + 32) / bus_width + avg_const_delay[layer_idx]

       #per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * (
       #        ip_activation_per_tile[layer_idx] * quantization_bit + 32) / bus_width
        sum_packs = 0
        for i in range(len(layer_packs)):
            sum_packs += layer_packs[i]
        print("total packs : ", sum_packs)
        print("effective_delay:", effective_delay)
        per_layer_latency[layer_idx] = (effective_delay[layer_idx] + 1) * layer_packs[layer_idx]
        print("per_layer_latency:", per_layer_latency)

    if (arch_type == 'serial'):
        total_latency = np.sum(per_layer_latency)
    elif (arch_type == 'parallel'):
        total_latency = np.sum(per_layer_latency) / (num_layers - 1)
    else:
        total_latency = -1

    return total_latency, sum_packs, effective_delay, per_layer_latency


def extract_row_and_column_from_id(ID, NO_OF_ROWS, NO_OF_COLS):
    remainder = ID % NO_OF_COLS
    quotient = ID // NO_OF_COLS

    if remainder == 0:
        column = NO_OF_COLS
        row = quotient
    else:
        column = remainder
        row = quotient + 1

    return row, column

# Area and power estimation for interconnect
def interconnect_area_power_estimation(num_tiles_per_layer, homepath):
    num_tile_total = np.sum(num_tiles_per_layer)
    # mesh_size = 10
    mesh_size = int(math.sqrt(num_tile_total))

    # Open read file handle of config file
    fp = open(homepath + '/mesh_config_dummy', 'r')

    # Set path to config file
    config_file = homepath + '/mesh_config'

    # Open write file handle for config file
    outfile = open(config_file, 'w')

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

    # Run Booksim with config file and save log
    log_file = homepath + '/dummy_output.log'
    booksim_command = homepath + '/booksim ' + config_file + ' > ' + log_file
    os.system(booksim_command)

    # Grep for area
    # latency = os.popen('grep "Packet latency average" ' + log_file + ' | tail -1 | awk \'{print $5}\'').read().strip()
    area = os.popen('grep "Total Area" ' + log_file + ' | tail -1 | awk \'{print $4}\'').read().strip()

    print('[ INFO] Area: ' + area + '\n')

    power = os.popen('grep "Total Power" ' + log_file + ' | tail -1 | awk \'{print $4}\'').read().strip()

    print('[ INFO] Power: ' + power + '\n')

    return area, power




if __name__ == '__main__':
    matrix = [
        [0, 0, 4, 4],
        [2, 1, 4, 5],
        [2, 3, 4, 5],
        [-1,-1, 6, 6],
    ]
    interconnect_estimation(matrix)
