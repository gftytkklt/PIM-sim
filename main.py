#!/usr/bin/python
# -*-coding:utf-8-*-
import torch
import sys
import os
import math
import argparse
import numpy as np
import torch
import collections
import configparser
import time
from importlib import import_module
from MNSIM.Interface.interface import *
from MNSIM.Interface.network import *
from MNSIM.Accuracy_Model.Weight_update import weight_update
from MNSIM.Mapping_Model.Behavior_mapping import behavior_mapping
from MNSIM.Mapping_Model.Tile_connection_graph import TCG
from MNSIM.Latency_Model.Model_latency import Model_latency
from MNSIM.Area_Model.Model_Area import Model_area
from MNSIM.Power_Model.Model_inference_power import Model_inference_power
from MNSIM.Energy_Model.Model_energy import Model_energy
from MNSIM.NoC.to_interconnect.CsvTransformer import CsvTransformer



def main():
    home_path = os.getcwd()
    print(home_path)
    SimConfig_path = os.path.join(home_path, "SimConfig.ini")
    #weights_file_path = os.path.join(home_path, "pth/cifar10_vgg8_params.pth")
    weights_dir = os.path.join(home_path, "pth")
    weights_files = [f for f in os.listdir(weights_dir) if f.endswith('.pth')]

    for weights_file in weights_files:
        NN = extract_network_name(weights_file)
        parser = argparse.ArgumentParser(description='MNSIM example')
        parser.add_argument("-AutoDelete", "--file_auto_delete", default=True,
            help="Whether delete the unnecessary files automatically")
        parser.add_argument("-HWdes", "--hardware_description", default=SimConfig_path,
            help="Hardware description file location & name, default:/MNSIM-2.0/SimConfig.ini")
        parser.add_argument("-Weights", "--weights", default=home_path+'/pth/'+weights_file,
            help="Directory containing NN model weights files, default:/MNSIM_Python/pth")
        parser.add_argument("-NN", "--NN", default=str(NN),
            help="NN model description (name), default: vgg8")
        parser.add_argument("-DisHW", "--disable_hardware_modeling", action='store_true', default=False,
            help="Disable hardware modeling, default: false")
        parser.add_argument("-DisAccu", "--disable_accuracy_simulation", action='store_true', default=False,
            help="Disable accuracy simulation, default: false")
        parser.add_argument("-SAF", "--enable_SAF", action='store_true', default=False,
            help="Enable simulate SAF, default: false")
        parser.add_argument("-Var", "--enable_variation", action='store_true', default=False,
            help="Enable simulate variation, default: false")
        parser.add_argument("-Rratio", "--enable_R_ratio", action='store_true', default=False,
            help="Enable simulate the effect of R ratio, default: false")
        parser.add_argument("-FixRange", "--enable_fixed_Qrange", action='store_true', default=False,
            help="Enable fixed quantization range (max value), default: false")
        parser.add_argument("-DisPipe", "--disable_inner_pipeline", action='store_true', default=False,
            help="Disable inner layer pipeline in latency modeling, default: false")
        parser.add_argument("-D", "--device", default=0,
            help="Determine hardware device (CPU or GPU-id) for simulation, default: CPU")
        parser.add_argument("-DisModOut", "--disable_module_output", action='store_true', default=False,
            help="Disable module simulation results output, default: false")
        parser.add_argument("-DisLayOut", "--disable_layer_output", action='store_true', default=False,
            help="Disable layer-wise simulation results output, default: false")
        args = parser.parse_args()

        # 此处可以调用你的仿真或处理函数，假设为process_weights_file()
        __TestInterface = TrainTestInterface(network_module=args.NN, dataset_module='MNSIM.Interface.cifar10',  
        SimConfig_path=args.hardware_description, weights_file=args.weights, device=args.device)
        structure_file = __TestInterface.get_structure()
        print("========================mapping Results=================================")
        TCG_mapping = TCG(structure_file, args.hardware_description)
        mappingMatrix = TCG_mapping.mapping_net()
        xbarsize = []
        xbarsize = TCG_mapping.xbar_size
        VM_TileInfo = TCG_mapping.layerDataPerTile
        print("======================== mapping matrix here ========================")
        print(mappingMatrix)
        params = TCG_mapping.ip_acti
        layer_id = TCG_mapping.layerid
        __csvtransfomer = CsvTransformer(home_path)
        layer_tile_info = TCG_mapping.layer_tileinfo
        __csvtransfomer.Param2CSV(params)
        __csvtransfomer.mappingArray2csv(mappingMatrix, TCG_mapping.layer_num)
        if not (args.disable_hardware_modeling):
            __latency = Model_latency(NetStruct=structure_file,VirtualMappingInfo=VM_TileInfo, mapping_matrix=mappingMatrix, SimConfig_path=args.hardware_description, TCG_mapping=TCG_mapping, Network=str(NN))
        if not (args.disable_inner_pipeline):
            print("-------------pipe----------------")
            __latency.calculate_model_latency(mode=1)
        else:
            __latency.calculate_model_latency_nopipe()
            print("-------------nopipe----------------")
        print("========================Latency Results=================================")
        __latency.model_latency_output(not (args.disable_module_output), not (args.disable_layer_output))
        print("========================Latency Results end=================================")

def process_weights_file(args, weights_file_path):
    print("Hardware description file location:", args.hardware_description)
    print("Software model file location:", weights_file_path)
    print("Whether perform hardware simulation:", not (args.disable_hardware_modeling))
    print("Whether perform accuracy simulation:", not (args.disable_accuracy_simulation))
    print("Whether consider SAFs:", args.enable_SAF)
    print("Whether consider variations:", args.enable_variation)
    if args.enable_fixed_Qrange:
        print("Quantization range: fixed range (depends on the maximum value)")
    else:
        print("Quantization range: dynamic range (depends on the data distribution)")
def extract_network_name(weights_file_path):
    # 假设网络名称在文件名中，如 'cifar10_vgg8_params.pth'
    base_name = os.path.basename(weights_file_path)
    parts = base_name.split('_')
    if len(parts) > 1:
        return parts[1]  # 提取第二部分作为网络名称
    return "unknown"  # 如果文件名格式不正确，则返回 'unknown'

if __name__ == '__main__':
    # Data_clean()
    main()
