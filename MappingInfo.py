import os, re, glob, sys, math
import numpy as np
import pandas as pd
from onnx_analysis import analysis_model
import configparser as cp
from MNSIM.Interface.interface import *
from MNSIM.Latency_Model.Tile_latency import tile_latency_analysis
from MNSIM.Latency_Model.Pooling_latency import pooling_latency_analysis
from MNSIM.Hardware_Model.Buffer import buffer

def latency_est(SimConfig_path,inputbit=8,outputbit=8,model=None,opt_info=None):
    home_path = os.getcwd()
    mapping_res = analysis_model(path=model, hw_info=None, opt_info=opt_info)

    all_tiles_mapping_infos = mapping_res.deploy_info
    data_matrix_by_layer = mapping_res.datas # Byte

    tiles_by_layer = {}
    layer=[]
    bus_width = 8 #byte  booksim
    freq = 1000000 #Hz
    #search deploy_info
    #all_tiles_mapping_infos[i] represents a specifc tile

    #print(len(all_tiles_mapping_infos));

    tile_num = len(all_tiles_mapping_infos)

    for i in range(0,tile_num):
        # print(all_tiles_mapping_infos[i].layer)# layer of i_th tile
        layer.append(all_tiles_mapping_infos[i].layer)

    layer_num = max(layer)+1
    print('layer num is ',layer_num)

    inj_matrix=divide_list_elements_3(data_matrix_by_layer,bus_width*freq)
    create_injection_rate_files(home_path, layer_num, inj_matrix)

    for tile_info in all_tiles_mapping_infos:
        layer = tile_info.layer
        if layer in tiles_by_layer:
            tiles_by_layer[layer].append(tile_info)
        else :
            tiles_by_layer[layer] = [tile_info]

    # print(tiles_by_layer)

    cur_tile_info = {}
    cur_tile_path = {}
    avg_delay_pack=[]
    beginTime_by_layer=[]
    finishTime_by_layer = []
    PathDelay_cur_layer = []

    latency_array, NoC_latency = trans_time_booksim(home_path)
    print('latency_array is', latency_array)


    for i in range(0,layer_num-1):
        print('########################')
        print('#########layer',i,'######')
        print('########################')

        j=0
        avgdelay_perpack = latency_array[i] / freq
        print('avgdelay_perpack is', avgdelay_perpack)

        if (i == 0):
            begin_time = 0
        else:
            begin_time = max(PathDelay_cur_layer) # actually ,this  is pathdelay of last layer
        beginTime_by_layer.append(begin_time)
        print('layer',i,'begin time is ',begin_time)

        #reset the path delay list for each layer
        PathDelay_cur_layer = []

        for tile in  tiles_by_layer[i]:

            tile_indata = 0

            print('************')
            print('tile',j,tile.tile_id)
            print('************')
            cur_tile_info['tile_id']=  tile.tile_id
            cur_tile_info['child_tile'] = tile.child_tile
            cur_tile_info['paths'] = tile.paths
            cur_tile_info['cnode'] = tile.cnode
            j=j+1

            pth = 0
            for cnode in cur_tile_info['cnode']:#specifc tile specifc path
                tile_indata += cnode.ifmap_size

            for path in cur_tile_info['paths']:#specifc tile specifc path
                print('-----------')
                print('path', pth)
                print('-----------')
                cur_tile_path['src'] = path.src
                cur_tile_path['dst'] = path.dst
                cur_tile_path['via'] = path.via # jumps
                cur_tile_path['data_vol'] = path.datavolume #num of packs
                print('src is',path.src)
                print('dst is',path.dst)
                print('datavolume is', path.datavolume)
                print('vias are', path.via) # in this version , via is the whole path
                print('-----------')
                pth=pth+1
                tile_delay = tile_latency_cal(SimConfig_path,tile_indata,inputbit,outputbit)
                transdelay = int(begin_time) + int(len(path.via)-1) * int(avgdelay_perpack) * int(path.datavolume)
                Stile_Spath_delay = tile_delay+transdelay+int(begin_time)
                print('Stile_Spath_delay is',Stile_Spath_delay)
                PathDelay_cur_layer.append(int(Stile_Spath_delay))
        print(PathDelay_cur_layer)


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

    tile_latency = temp_tile_latency.tile_latency;
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
        # if layer_idx not in inj_matrix_by_layer:
        #     continue
        inj_matrix = inj_matrix_by_layer[layer_idx]
        os.chdir(injection_directory_name)
        filename = 'inj_rate_' + str(layer_idx) + '.txt'
        np.savetxt(filename, inj_matrix, fmt='%.12f')
        os.chdir("..")

    return 0

def divide_list_elements_3(input_list, divisor):
    # 使用 numpy 数组进行元素的除法操作
    input_array = np.array(input_list)
    # result = input_array / divisor
    result_array = input_array / divisor
    result = np.maximum(result_array, 1e-10)
    return result.tolist()

if __name__ == '__main__':
    home_path = os.getcwd()
    SimConfig_path = os.path.join(home_path, "SimConfig.ini")
    latency_est(SimConfig_path,8,8)