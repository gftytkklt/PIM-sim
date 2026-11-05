#!/usr/bin/python
# -*-coding:utf-8-*-
import torch
import sys
import os
import math
import configparser as cp

work_path = os.path.dirname(os.getcwd())
sys.path.append(work_path)
from MNSIM.Hardware_Model import *
from MNSIM.Hardware_Model.Crossbar import crossbar
from MNSIM.Hardware_Model.Tile import tile
from MNSIM.Hardware_Model.PE import ProcessElement
from MNSIM.Interface.interface import *
import collections
import pandas as pd


class PE_node():
    def __init__(self, PE_id=0, ltype='conv', lnum=0):
        # PE_id: the id of PE node, ltype: layer type of this PE, lnum: layer number of this PE
        self.id = PE_id
        self.type = ltype
        self.lnum = lnum
        self.inMerge_list = []
        self.outMerge = 0

    def set_inMerge(self, Merge_id):
        if Merge_id not in self.inMerge_list:
            self.inMerge_list.append(Merge_id)
            self.inMerge_list.sort()

    def set_outMerge(self, Merge_id):
        self.outMerge = Merge_id


class Merge_node():
    def __init__(self, Merge_id=0, mtype=0, lnum=0):
        # Merge_id: the id of Merge node, mtype: merge type (0: add, 1: concat, 2: pooling)
        self.id = Merge_id
        self.type = mtype
        self.lnum = lnum
        self.inPE_list = []
        self.outPE_list = []
        self.inMerge_list = []
        self.outMerge_list = []

    def set_inPE(self, PE_id):
        if PE_id not in self.inPE_list:
            self.inPE_list.append(PE_id)
            self.inPE_list.sort()

    def set_outPE(self, PE_id):
        if PE_id not in self.outPE_list:
            self.outPE_list.append(PE_id)
            self.outPE_list.sort()

    def set_inMerge(self, Merge_id):
        if Merge_id not in self.inMerge_list:
            self.inMerge_list.append(Merge_id)
            self.inMerge_list.sort()

    def set_outMerge(self, Merge_id):
        if Merge_id not in self.outMerge_list:
            self.outMerge_list.append(Merge_id)
            self.outMerge_list.sort()

# The following matrix generations aim to conduct weights mapping on tiles

def generate_normal_matrix(row, column):
    matrix = np.zeros([row, column])
    start = 0
    for i in range(row):
        for j in range(column):
            matrix[i][j] = start
            start += 1
    return matrix

def generate_snake_matrix(row, column):
    matrix = np.zeros([row, column])
    start = 0
    for i in range(row):
        for j in range(column):
            if i % 2:
                matrix[i][column - j - 1] = start
            else:
                matrix[i][j] = start
            start += 1
    return matrix

def generate_hui_matrix(row, column):
    matrix = np.zeros([row, column])
    state = 0
    stride = 1
    step = 0
    start = 0
    dl = 0
    ru = 0
    i = 0
    j = 0
    for x in range(row * column):
        if x == 0:
            matrix[i][j] = start
        else:
            if state == 0:
                j += 1
                matrix[i][j] = start
                state = 1
            elif state == 1:
                if dl == 0:
                    i += 1
                    matrix[i][j] = start
                    step += 1
                    if step == stride:
                        dl = 1
                        step = 0
                elif dl == 1:
                    j -= 1
                    matrix[i][j] = start
                    step += 1
                    if step == stride:
                        dl = 0
                        step = 0
                        stride += 1
                        state = 2
            elif state == 2:
                i += 1
                matrix[i][j] = start
                state = 3
            elif state == 3:
                if ru == 0:
                    j += 1
                    matrix[i][j] = start
                    step += 1
                    if step == stride:
                        ru = 1
                        step = 0
                elif ru == 1:
                    i -= 1
                    matrix[i][j] = start
                    step += 1
                    if step == stride:
                        ru = 0
                        step = 0
                        stride += 1
                        state = 0
        start += 1
    return matrix

def generate_zigzag_matrix(row, column):
    matrix = np.zeros([row, column])
    state = 0
    stride = 1
    step = 0
    i = 0
    j = 0
    start = 0
    for x in range(row * column):
        if x == 0:
            matrix[i][j] = start
        else:
            if state == 0:
                if j < column - 1:
                    j += 1
                    matrix[i][j] = start
                else:
                    i += 1
                    matrix[i][j] = start
                state = 1
            elif state == 1:
                i += 1
                j -= 1
                matrix[i][j] = start
                step += 1
                if i == row - 1:
                    state = 2
                    stride -= 1
                    step = 0
                elif step == stride:
                    state = 2
                    stride += 1
                    step = 0
            elif state == 2:
                if i < row - 1:
                    i += 1
                    matrix[i][j] = start
                else:
                    j += 1
                    matrix[i][j] = start
                state = 3
            elif state == 3:
                j += 1
                i -= 1
                matrix[i][j] = start
                step += 1
                if j == column - 1:
                    state = 0
                    stride -= 1
                    step = 0
                elif step == stride:
                    state = 0
                    stride += 1
                    step = 0
        start += 1
    return matrix

class Mapped_Tile(tile):
    def __init(self,SimConfig_path,cur_idx,dst_idx,mapped_pe_num):
        tile.__init__(self,SimConfig_path)
        self.cur_idx = cur_idx
        self.dst_idx = dst_idx
        self.mapped_pe = mapped_pe_num
        

#virtual grouping 
class Virtual_grouping():
    def __init__(self,SimConfig_path,NetStruct):
        PE_info = ProcessElement(SimConfig_path)
        TCG_config = cp.ConfigParser()
        TCG_config.read(SimConfig_path, encoding='UTF-8')
        self.tile = tile(SimConfig_path)
        self.net = NetStruct
        print("***********this is self.net************")
        self.layer_num = len(self.net)
        #here we consider group num is 8,and each device can only represent 1 bit
        Params_perPE = PE_info.xbar_column * PE_info.xbar_row

        self.tile_num = list(map(int, TCG_config.get('Architecture level', 'Tile_Num').split(',')))
        if self.tile_num[0] == 0:
            self.tile_num[0] = 8
            self.tile_num[1] = 8
        assert self.tile_num[0] > 0, "Tile number < 0"
        assert self.tile_num[1] > 0, "Tile number < 0"
        # 长*宽
        self.tile_total_num = self.tile_num[0] * self.tile_num[1]

        self.pe_num_perTile = list(map(int, TCG_config.get('Tile level', 'PE_Num').split(',')))
        self.tile_total_pe = self.pe_num_perTile[0] * self.pe_num_perTile[1] 

        #read weights of each layer and conduct grouping   
        self.tiles_perlayer_need = []
        self.PEs_ptpl_alct= [] # pes per tile per layer allocated

        n = self.tile_num[0] 
        for i in len(self.tiles_perlayer_need):
            total_tiles_need += self.tiles_perlayer_need[i]

        #init a matrix the same size as tile array
        tile_matrix = np.zeros((n, n))
        mapping_matrix = np.zeros((n, n))
        start_id = 0
        for layer_id in range(self.layer_num):
            #这边只获取net字典中的逐层信息，例如本层的卷积大小、kernelsize等，array不要
            layer_dict = self.net[layer_id][0][0]
            next_layer_dict = self.net[layer_id+1][0][0]
            layer_type = layer_dict['type']
            next_layer_type = next_layer_dict['type']
            #we should know if the objective network is resnet or not  
            #duplication or not , this canbe houxu work
            isNotResnet = 1
            if(isNotResnet):  
                if layer_type == 'conv':

                    pe_perlayer_need = accurate_pe_num((int(layer_dict['Kernelsize']) ** 2),int((layer_dict['Inputchannel'])),int((layer_dict['Outputchannel'])),PE_info.xbar_row,PE_info.xbar_column)
                    print("pe_perlayer_need:",pe_perlayer_need)


                    #Put PEs on tile ,for each layer
                    tmp_tpln = math.ceil(pe_perlayer_need / self.tile_total_pe)
                    self.tiles_perlayer_need.append(tmp_tpln)
                    self.PEs_ptpl_alct.append(math.ceil(pe_perlayer_need / tmp_tpln)) # pes average each tile each layer

                    # get the intra-layer dependency of tiles in a layers
                    acc_times,concat_times = Getting_intra_layer_dependency()
                    
                    # get the inter-layer dependency of tiles in different layers


                    # mapping current layer , update the mapping result matrix
                    for i in concat_times:
                        for idx in acc_times:
                            x = idx + i*concat_times + base_idx


                    if(self.tile_total_num >= total_tiles_need):
                        start_coord = (0,0)
                    else:
                        print('resource not enough')
                        assert(0)



                elif  layer_type == 'fc':
                    weights_per_layer  = int((layer_dict['Infeature'])) * int((layer_dict['Outfeature'])) + int((layer_dict['Outfeature']))
                    pe_perlayer_need   = math.ceil(weights_per_layer / Params_perPE)

                    #Put PEs on tile ,for each layer
                    tmp_tpln = math.ceil(pe_perlayer_need / self.tile_total_pe)
                    self.tiles_perlayer_need.append(tmp_tpln)
                    self.PEs_ptpl_alct.append(math.ceil(pe_perlayer_need / tmp_tpln)) # pes average each tile each layer

                elif  layer_type == 'pooling':
                    pe_perlayer_need = accurate_pe_num((int(layer_dict['Kernelsize']) ** 2),int((layer_dict['Inputchannel'])),int((layer_dict['Outputchannel'])),PE_info.xbar_row,PE_info.xbar_column)

                    #Put PEs on tile ,for each layer
                    tmp_tpln = math.ceil(pe_perlayer_need / self.tile_total_pe)
                    self.tiles_perlayer_need.append(tmp_tpln)
                    self.PEs_ptpl_alct.append(math.ceil(pe_perlayer_need / tmp_tpln)) # pes average each tile each layer

                elif  layer_type == 'element_sum':
                    weights_per_layer  = 0
                    pe_perlayer_need   = 0

                    #Put PEs on tile ,for each layer
                    tmp_tpln = math.ceil(pe_perlayer_need / self.tile_total_pe)
                    self.tiles_perlayer_need.append(tmp_tpln)
                    self.PEs_ptpl_alct.append(math.ceil(pe_perlayer_need / tmp_tpln)) # pes average each tile each layer

        
    def mapping_result_gen(self):
        n = self.tile_num[0] 
        for i in len(self.tiles_perlayer_need):
            total_tiles_need += self.tiles_perlayer_need[i]

        #init a matrix the same size as tile array
        tile_matrix = np.zeros((n, n))
        mapping_matrix = np.zeros((n, n))
        #here we should detect the resource 
        if(self.tile_total_num >= total_tiles_need):
            start_coord = (0,0)
 

        else:
            print('resource not enough')
            assert(0)
    #mapping 
    #mapping usually needs mapping order,and the mapping order can be a mapping matrix or some relationship like dependency 






def accurate_pe_num(ker_size,ker_Ichs,ker_Ochs,pe_row,pe_col):
    #this method consider each col of pe mapping one output channel of kernel
    
    #step1: unroll the kernel to 2-D ,calculate the size of the 2-d kernel box
    ker_row = ker_Ichs * (ker_size **2)
    ker_col = ker_Ochs
    #step2: put the pe box into kernel box 
    if(ker_col < pe_col):#in this case,the pe num depends on the ker rows
        mapped_pe_num = math.ceil(ker_row / pe_row)
    elif(ker_col > pe_col):
        if(ker_row > pe_row):#in this case,the pe num depends on the ker rows and cols
            mapped_pe_num = math.ceil(ker_row / pe_row) * math.ceil(ker_col / pe_col)
        elif(ker_row < pe_row):#in this case,the pe num depends on the ker rows
            mapped_pe_num = math.ceil(ker_col / pe_col)
    return mapped_pe_num
    #this outdata is cpt one time! tile level ,we use data packs to express
    #outdata_eachPE = pe_col * 8
    #note that different pe has different connections with others, this should be recorded

def Getting_intra_layer_dependency(ker_size,ker_Ichs,ker_Ochs,tiles_perlayer_need,tiles_row,colnum_perTile,rownum_pertile):
    #this method consider each col of pe mapping one output channel of kernel
    
    #intra layer dependency is reduction , as we usually known ,"lei jia"

    #step1: unroll the kernel to 2-D ,calculate the size of the 2-d kernel box
    ker_row = ker_Ichs * (ker_size **2)
    ker_col = ker_Ochs
    acc_times = 0
    concat_times = 0
    tile_idx = 0
    #step2: put the pe box into kernel box 
    if(tiles_perlayer_need > 1):
        if(ker_col <= colnum_perTile):#in this case,the pe num depends on the ker rows
            acc_times = tiles_perlayer_need -1
            concat_times = 0

        elif(ker_col > colnum_perTile):
            if(ker_row > rownum_pertile):#in this case,the pe num depends on the ker rows and cols
                acc_times = (tiles_perlayer_need // tiles_row) * (tiles_row-1) +(tiles_perlayer_need % tiles_row) 
                concat_times = math.ceil(tiles_perlayer_need / tiles_row) - 1

            elif(ker_row < rownum_pertile):#in this case,the pe num depends on the ker col
                acc_times = 0
                concat_times = tiles_perlayer_need -1
    else:
            acc_times = 0
            concat_times = 0
                
    return acc_times, concat_times
    #this outdata is cpt one time! tile level ,we use data packs to express
    #outdata_eachPE = pe_col * 8
    #note that different pe has different connections with others, this should be recorded



class TCG():
    def __init__(self, NetStruct, SimConfig_path, multiple=None):#TCG类的构造函数，在创建对象时会被自动执行
        # NetStruct: layer structure, SimConfig_path: Hardware config path, multiple: allocate more resources for some layers (i.e., duplicate)
        TCG_config = cp.ConfigParser()
        TCG_config.read(SimConfig_path, encoding='UTF-8')
        if multiple is None:
            multiple = [1] * len(NetStruct)
        self.tile = tile(SimConfig_path)
        self.net = NetStruct
        print("***********this is self.net************")
        self.layer_num = len(self.net)
        self.layer_tileinfo = []
        self.params_this_layer = []
        self.ip_acti = []
        self.layerid = []
        self.layerDataPerTile = {}
        self.xbar_size = list(map(int, TCG_config.get('Crossbar level', 'Xbar_Size').split(',')))
        self.xbar_group_num = int(TCG_config.get('Process element level', 'Group_Num'))
        self.xbar_polarity = int(TCG_config.get('Process element level', 'Xbar_Polarity'))
        self.tile_connection = int(TCG_config.get('Architecture level', 'Tile_Connection'))
        self.tile_num = list(map(int, TCG_config.get('Architecture level', 'Tile_Num').split(',')))
        self.tile_pe_num = list(map(int, TCG_config.get('Tile level', 'PE_Num').split(',')))
        if self.tile_num[0] == 0:
            self.tile_num[0] = 8
            self.tile_num[1] = 8
        assert self.tile_num[0] > 0, "Tile number < 0"
        assert self.tile_num[1] > 0, "Tile number < 0"
        # 长*宽
        self.tile_total_num = self.tile_num[0] * self.tile_num[1]
        #初始化mapping order和mapping result矩阵
        self.mapping_order = -1 * np.ones(self.tile_num)
        self.mapping_result = -1 * np.ones(self.tile_num)
        start_tileid = 0
            # the start Tile id
        self.max_inbuf_size = 0
            # the maximum input buffer size of each PE, unit: KB
        self.max_outbuf_size = 0
            # the maximum output buffer size of each tile, unit: KB
        self.global_buf_size = 0
            # the global buffer size for accumulator
        self.global_data_size = 0
        self.global_adder_num = 0
            # the global adder number in accumulator
        self.global_adder_bitwidth = 8
        num = []

            # track PE number of each layer 
        total_xbar_num = 0
        #超大for循环，逐层做映射
        for layer_id in range(self.layer_num):
            layer_dict = self.net[layer_id][0][0]#这边只获取net字典中的逐层信息，例如本层的卷积大小、kernelsize等，array不要
            print("***********this is layer_dict************")
            print(layer_dict)
            #用tmp_tileinfo存储每层的信息，字典存储dict，key->value
            tmp_tileinfo = collections.OrderedDict()
            print("***********this is tmp_tileinfo************")
            print(tmp_tileinfo)
            layer_type = layer_dict['type']
            if self.xbar_polarity == 1:
                weight_precision = int(layer_dict['Weightbit'])
            else:
                assert self.xbar_polarity == 2, "Crossbar polarity must be 1 or 2"
                weight_precision = int(layer_dict['Weightbit']) - 1
            tmp_tileinfo['startid'] = start_tileid
            input_size = 0
            inputchannel = 0
            outputchannel = 0
            data_inbuf = 0
            data_outbuf = 0

            if layer_type == 'conv':
                tmp_tileinfo['type'] = 'conv'
                tmp_tileinfo['ip_acti'] = int(layer_dict['Inputbit'])*(int(layer_dict['Inputsize'][0])**2)*int(layer_dict['Inputchannel'])
                tmp_tileinfo['outputdata'] = int(layer_dict['outputbit'])*(int(layer_dict['Outputsize'][0])**2)*int(layer_dict['Outputchannel'])
                print("conv layer output data-----",tmp_tileinfo['outputdata'])
                #看整个输出通道需要多少个xbar去部署，我这边每个pe设置成了一个xbar
                #这边计算mx和my的时候，能不能得到已部署的权重，从而得到当前tile需要输出多少数据。但是这边算出来的是总共需要多少pe，我们得分开看，哦，那就是看每个tile上哪些pe是work的，哪些是不work的就行。
                tmp_tileinfo['mx'] = math.ceil(weight_precision / self.tile.group_num) * math.ceil(int(layer_dict['Outputchannel']) / self.tile.xbar_column)
                if (math.ceil(int(layer_dict['Outputchannel']) / self.tile.xbar_column)) > 1 :
                    mapped_outputchannels = (int(layer_dict['Outputchannel']) // self.tile.xbar_column) * self.tile.xbar_column
                    rest_outputchannels = int(layer_dict['Outputchannel']) % self.tile.xbar_column
                else :
                    real_outputchannels =  int(layer_dict['Outputchannel'])
                


                #向上取整是为了得到所需pe的数量，根据所有的pe，去划分tile，需要多少个tile，每个tile需要多少个通道
                #取余，是为了得到实际多少通道数
                #一个pe可以涵盖256个输出通道，假设mx方向上有N个pe，则这一层的输出通道≤N*256

                #一个tile可以涵盖多少个输出通道
                #这边应该得到实际每个tile部署了多少行多少列，取余取整，余数是剩余的列数，整数是横向上的pe个数


                #判断ceil(int(layer_dict['Outputchannel']) / self.tile.xbar_column)的值，若这个等于1，则输出通道方向上不需要分块,但需要知道实际有多少数据。
                #若输出通道分块、输入通道不分块，则部署这一层的每个tile都要把数据输给下一层的tile（但是这样就涉及到下一层的输入通道是否分块）中
                #若输出通道不分块，输入通道分块，则部署这一层的tile之间需要进行数据的流动，进行累加，最后再送至下一层的输入通道。同样涉及到下一层输入通道是否分块的问题
                #若输出通道、输入通道都不分块，则直接把数据送给下一层的tile中

                print("mx here")
                print(tmp_tileinfo['mx'])
                    # mx: PE number in x-axis 这边是考虑行能不能放下所有输入通道，总输入通道数/单个pe放几个输入通道
                tmp_tileinfo['my'] = math.ceil((int(layer_dict['Inputchannel']) * (int(layer_dict['Kernelsize']) ** 2)) / self.tile.xbar_row)
                    #计算总共需要多少行
                cur_layer_totalRows = int(layer_dict['Inputchannel']) * (int(layer_dict['Kernelsize']) ** 2)
                if (math.ceil((int(layer_dict['Inputchannel']) * (int(layer_dict['Kernelsize']) ** 2)) / self.tile.xbar_row)) > 1 :
                    mapped_inputrows = ((int(layer_dict['Inputchannel']) * (int(layer_dict['Kernelsize']) ** 2)) // self.tile.xbar_row) * self.tile.xbar_row
                    rest_inputrows = (int(layer_dict['Inputchannel']) * (int(layer_dict['Kernelsize']) ** 2)) % self.tile.xbar_row
                else :
                    real_inputrows =  (int(layer_dict['Inputchannel']) * (int(layer_dict['Kernelsize']) ** 2))


                
                    #计算实际一个tile放了多少输入通道，一共总行数需要 Inputchannel * k*k行
                    #所以行方向的pe个数应该是，ceil（Inputchannel * k*k / xbar row）
                    #然后取整，得到所需pe的数量-1，取余，得到剩余的行数

                print("my here")
                print(tmp_tileinfo['my'])
                    # my: PE number in y-axis
                tmp_tileinfo['max_group'] = min(weight_precision, self.tile.group_num)
                    # max_group: maximum used groups in one PE of this layer
                tmp_tileinfo['max_row'] = min((self.tile.xbar_row // (int(layer_dict['Kernelsize']) ** 2)),# //为地板除，只取整数部分，**为幂运算
                    int(layer_dict['Inputchannel'])) * (int(layer_dict['Kernelsize']) ** 2)
                    # max_row: maximum used row in one crossbar of this layer
                tmp_tileinfo['max_column'] = min(int(layer_dict['Outputchannel']), self.tile.xbar_column)
                #计算参数量
                tmp_tileinfo['params_this_layer'] = int((layer_dict['Inputchannel'])) * int((layer_dict['Outputchannel'])) * (int(layer_dict['Kernelsize']) ** 2) + int((layer_dict['Outputchannel']))
                    # max_column: maximum used column in one crossbar of this layer
                if 'Inputindex' not in layer_dict.keys():
                    tmp_tileinfo['Inputindex'] = [-1]
                else:
                    tmp_tileinfo['Inputindex'] = list(map(int, layer_dict['Inputindex']))
                    # Inputindex: the relative index of the input layers of this layer
                if 'Outputindex' not in layer_dict.keys():
                    tmp_tileinfo['Outputindex'] = [1]
                else:
                    tmp_tileinfo['Outputindex'] = list(map(int, layer_dict['Outputindex']))
                    # Outputindex: the relative index of the output layers of this layer
                if len(tmp_tileinfo['Outputindex']) == 1:
                    tmp_tileinfo['is_branchin'] = -1
                else:
                    tmp_tileinfo['is_branchin'] = 1
                    # is_branchin: if this layer is the input layer of a branch
                tmp_tileinfo['is_branchout'] = 1
                    # is_branchout: if this layer is the output layer of a branch (the next layer is element_sum)
                for i in tmp_tileinfo['Outputindex']:
                    tmp_layer = self.net[i+layer_id][0][0]
                    if tmp_layer['type'] != 'element_sum':
                        tmp_tileinfo['is_branchout'] = -1
                input_size_list = list(map(int, layer_dict['Inputsize']))
                input_size = input_size_list[0] * input_size_list[1]
                inputchannel = int(layer_dict['Inputchannel'])
                data_inbuf = input_size_list[1] * int(layer_dict['Kernelsize']) * inputchannel * int(layer_dict['Inputbit'])/8
                    # assume using the line buffer structure
                outputchannel = int(layer_dict['Outputchannel'])
                data_outbuf = outputchannel*int(layer_dict['outputbit'])/8
                # buffer_size: unit Byte
            elif layer_type == 'fc':
                tmp_tileinfo['type'] = 'fc'
                tmp_tileinfo['mx'] = math.ceil(weight_precision / self.tile.group_num) * math.ceil(int(layer_dict['Outfeature']) / self.tile.xbar_column)
                    # mx: PE number in x-axis
                tmp_tileinfo['my'] = math.ceil(int(layer_dict['Infeature']) / self.tile.xbar_row)
                    # my: PE number in y-axis
                tmp_tileinfo['max_group'] = min(weight_precision, self.tile.group_num)
                    # max_group: maximum used groups in one PE of this layer
                tmp_tileinfo['max_row'] = min(int(layer_dict['Infeature']), self.tile.xbar_row)
                    # max_row: maximum used row in one crossbar of this layer
                tmp_tileinfo['max_column'] = min(int(layer_dict['Outfeature']), self.tile.xbar_column)
                    # max_row: maximum used column in one crossbar of this layer
                tmp_tileinfo['params_this_layer'] = int((layer_dict['Infeature'])) * int((layer_dict['Outfeature'])) + int((layer_dict['Outfeature']))
                tmp_tileinfo['ip_acti'] = int(layer_dict['Inputbit']) * (int(layer_dict['Infeature']))
                tmp_tileinfo['outputdata'] = int(layer_dict['outputbit']) * int(layer_dict['Outfeature'])
                if 'Inputindex' not in layer_dict.keys():
                    tmp_tileinfo['Inputindex'] = [-1]
                else:
                    tmp_tileinfo['Inputindex'] = list(map(int, layer_dict['Inputindex']))
                    # Inputindex: the relative index of the input layers of this layer
                if 'Outputindex' not in layer_dict.keys():
                    tmp_tileinfo['Outputindex'] = [1]
                else:
                    tmp_tileinfo['Outputindex'] = list(map(int, layer_dict['Outputindex']))
                    # Outputindex: the relative index of the output layers of this layer
                if len(tmp_tileinfo['Outputindex']) == 1:
                    tmp_tileinfo['is_branchin'] = -1
                else:
                    tmp_tileinfo['is_branchin'] = 1
                tmp_tileinfo['is_branchout'] = 1
                # is_branchout: if this layer is the output layer of a branch (the next layer is element_sum)
                for i in tmp_tileinfo['Outputindex']:
                    if (i+layer_id) < self.layer_num:
                        tmp_layer = self.net[i + layer_id][0][0]
                        if tmp_layer['type'] != 'element_sum':
                            tmp_tileinfo['is_branchout'] = -1
                # is_branchin: if this layer is the input layer of a branch
                input_size = int(layer_dict['Infeature'])
                inputchannel = 1
                data_inbuf = input_size * inputchannel * int(layer_dict['Inputbit'])/8
                data_outbuf = int(layer_dict['Outfeature']) * int(layer_dict['outputbit'])/8
                # buffer_size: unit Byte
            elif layer_type == 'pooling':
                tmp_tileinfo['type'] = 'pooling'
                tmp_tileinfo['ip_acti'] = int(layer_dict['Inputbit']) * (int(layer_dict['Inputsize'][0]) ** 2) * int(layer_dict['Inputchannel'])
                tmp_tileinfo['outputdata'] = int(layer_dict['outputbit'])*(int(layer_dict['Outputsize'][0])**2)*int(layer_dict['Outputchannel'])
                tmp_tileinfo['mx'] = 1
                tmp_tileinfo['my'] = 1
                tmp_tileinfo['max_row'] = 0
                tmp_tileinfo['max_column'] = 0
                tmp_tileinfo['max_group'] = 0
                tmp_tileinfo['params_this_layer'] = int((layer_dict['Inputchannel'])) * int(
                    (layer_dict['Outputchannel'])) * (int(layer_dict['Kernelsize']) ** 2) + int(
                    (layer_dict['Outputchannel']))
                tmp_tileinfo['outputdata'] = int(layer_dict['outputbit'])*(int(layer_dict['Outputsize'][0])**2)*int(layer_dict['Outputchannel'])
                if 'Inputindex' not in layer_dict.keys():
                    tmp_tileinfo['Inputindex'] = [-1]
                else:
                    tmp_tileinfo['Inputindex'] = list(map(int, layer_dict['Inputindex']))
                # Inputindex: the relative index of the input layers of this layer
                if 'Outputindex' not in layer_dict.keys():
                    tmp_tileinfo['Outputindex'] = [1]
                else:
                    tmp_tileinfo['Outputindex'] = list(map(int, layer_dict['Outputindex']))
                # Outputindex: the relative index of the output layers of this layer
                if len(tmp_tileinfo['Outputindex']) == 1:
                    tmp_tileinfo['is_branchin'] = -1
                else:
                    tmp_tileinfo['is_branchin'] = 1
                # is_branchin: if this layer is the input layer of a branch
                tmp_tileinfo['is_branchout'] = 1
                # is_branchout: if this layer is the output layer of a branch (the next layer is element_sum)
                for i in tmp_tileinfo['Outputindex']:
                    tmp_layer = self.net[i + layer_id][0][0]
                    if tmp_layer['type'] != 'element_sum':
                        tmp_tileinfo['is_branchout'] = -1
                input_size_list = list(map(int, layer_dict['Inputsize']))
                input_size = input_size_list[0] * input_size_list[1]
                inputchannel = int(layer_dict['Inputchannel'])
                data_inbuf = 0 # assume the pooling module shares the same buffer with xbar PEs
                data_outbuf = 0
                    # assume the buffer size depends on the conv/fc layers
            elif layer_type == 'element_sum':# kua ceng lian jie
                tmp_tileinfo['type'] = 'element_sum'
                tmp_tileinfo['mx'] = 0
                tmp_tileinfo['my'] = 0
                tmp_tileinfo['max_row'] = 0
                tmp_tileinfo['max_column'] = 0
                tmp_tileinfo['max_group'] = 0
                #这个地方不知道指的是什么
                tmp_tileinfo['params_this_layer'] = 0
                if 'Outputindex' not in layer_dict.keys():
                    tmp_tileinfo['Outputindex'] = [1]
                else:
                    tmp_tileinfo['Outputindex'] = list(map(int, layer_dict['Outputindex']))
                # Outputindex: the relative index of the output layers of this layer
                if len(tmp_tileinfo['Outputindex']) > 1:
                    tmp_tileinfo['is_branchout'] = 1
                else:
                    tmp_tileinfo['is_branchout'] = -1 #gaosu xia yiceng zheshi fenzhi

                # is_branchin: if this layer is the input layer of a branch
                Inputindex_list = list(map(int, layer_dict['Inputindex']))
                tmp_tileinfo['Inputindex'] = Inputindex_list
                assert len(Inputindex_list)>1, "the number of element_sum's previous layers must > 1"#zhe bian ren wei : element sum ceng doushi duo ge input
                if len(Inputindex_list) > 1:
                    tmp_tileinfo['is_branchin'] = 1
                else:
                    tmp_tileinfo['is_branchin'] = -1
                idx = 0
                previous_layer_dict = self.net[layer_id + Inputindex_list[0]][0][0]
                while previous_layer_dict['type'] == 'element_sum':
                    idx = idx+1
                    previous_layer_dict = self.net[layer_id + Inputindex_list[idx]][0][0]
                previous_output_size = list(map(int, previous_layer_dict['Outputsize']))
                tmp_tileinfo['datanum_branchout'] = previous_layer_dict['Outputchannel']
                    # the data number of each branch output, assume the previous layer generates 1*1*outputchannel each cycle
                tmp_tileinfo['bit_branchout'] = previous_layer_dict['outputbit']
                    # the data precision of each branch output (bit)
                data_size = tmp_tileinfo['datanum_branchout']*tmp_tileinfo['bit_branchout']*len(Inputindex_list)/8
                tmp_tileinfo['ip_acti'] = tmp_tileinfo['datanum_branchout']*tmp_tileinfo['bit_branchout']*len(Inputindex_list)
                tmp_tileinfo['outputdata'] = tmp_tileinfo['datanum_branchout']*tmp_tileinfo['bit_branchout']*len(tmp_tileinfo['Outputindex'])
                    # unit: Byte
                self.global_data_size = self.global_data_size + data_size
                self.global_buf_size = self.global_buf_size + math.pow(2,math.ceil(math.log(data_size,2)))/1024
                    # unit: KB
                self.global_adder_num = self.global_adder_num + previous_layer_dict['Outputchannel']*len(Inputindex_list)//2
                if tmp_tileinfo['bit_branchout']>self.global_adder_bitwidth:
                    self.global_adder_bitwidth = tmp_tileinfo['bit_branchout']
                #下面这些啥意思
            if layer_type == 'conv' or layer_type == 'fc':
                total_xbar_num += tmp_tileinfo['mx'] * tmp_tileinfo['my'] * multiple[layer_id]
                #multiple为有些层会考虑复用 
            self.layerid.append(layer_id)

            tmp_tileinfo['PEnum'] = tmp_tileinfo['mx'] * tmp_tileinfo['my'] * multiple[layer_id]
            num.append(tmp_tileinfo['PEnum'])
            self.params_this_layer.append(tmp_tileinfo['params_this_layer'])
            self.ip_acti.append(tmp_tileinfo['ip_acti'])
            #计算一层需要多少个tile，向上取整。
            #当前层需要的PE数量/一个tile中的pe总数，很粗略，这么算的话还是会浪费资源的，如果值小于1。
            #tmp_tileinfo['tilenum'] = math.ceil(tmp_tileinfo['PEnum'] / self.tile.tile_PE_total_num)
            #在tile  level按输入通道进行映射
            tile_num4Inchls = 0
            tile_num4Ochls = 0
            if layer_type == 'conv' or layer_type == 'pooling':
                tile_num4Inchls = math.ceil((layer_dict['Inputchannel'] * ((layer_dict['Kernelsize']) ** 2)) / (self.tile.xbar_row * self.tile_pe_num[0]))
            elif layer_type == 'fc':
                tile_num4Inchls = math.ceil(int(layer_dict['Infeature']) / (self.tile.xbar_row * self.tile_pe_num[0]))
            elif layer_type == 'element_sum':
                tile_num4Inchls = 0

            if layer_type == 'conv' or layer_type == 'pooling':
                tile_num4Ochls  = math.ceil(layer_dict['Outputchannel']  / (self.tile.xbar_column * self.tile_pe_num[1]))
            elif layer_type == 'fc':
                tile_num4Ochls = math.ceil(int(layer_dict['Outfeature']) / (self.tile.xbar_column * self.tile_pe_num[1]))
            elif layer_type == 'element_sum':
                tile_num4Ochls = 0

            tmp_tileinfo['tilenum'] = math.ceil(tile_num4Inchls * tile_num4Ochls)
            print("temp tile full num ",tmp_tileinfo['tilenum'])
            #先映射所有输入通道，这样可能会造成浪费，有些tile上会只有部分权重
            #tile_mx_num = math.ceil(tmp_tileinfo['mx'] / self.tile.tile_PE_total_num)
            #tile_my_num = tmp_tileinfo['my'] #暂不考虑实际映射了多少通道数，这边就直接这样表示，那么这个值也就是每行tile要concat (tile_my_num-1) 次
            #tmp_tileinfo['tilenum'] = tile_mx_num * tile_my_num 
            ##先取整，得到
            
            if layer_type == 'conv' or layer_type == 'pooling':
                layer_outputDimension = int(layer_dict['Outputchannel'])
            elif layer_type == 'element_sum':
                layer_outputDimension =  tmp_tileinfo['datanum_branchout']
            else:
                layer_outputDimension = int(layer_dict['Outfeature']) 

            self.layerDataPerTile[layer_id] = {}
            perTileInfo={}
            if tile_num4Ochls > 1:
                y_fulcol_tilenum =int((layer_outputDimension  // (self.tile.xbar_column * self.tile_pe_num[1])))
                xy_fulcol_tilenum = y_fulcol_tilenum * tile_num4Inchls
                tile_arr = np.zeros((tile_num4Inchls,(y_fulcol_tilenum+1)))
                #datapertile_fulcol = (self.tile.xbar_column * self.tile_pe_num[0]) * int(layer_dict['outputbit'])*int(layer_dict['Inputbit'])
                datapertile_fulcol = tmp_tileinfo['outputdata']
                for y in range(0, y_fulcol_tilenum):
                    for x in range(0, int(tile_num4Inchls)):
                        idx = x+y*int(tile_num4Inchls)
                        perTileInfo[idx] = {"layer_type": "-1", "Odata": -1, "AccEn": -1, "concatEn": -1, "coord": (0, 0)}
                        perTileInfo[idx]["Odata"] = datapertile_fulcol
                        perTileInfo[idx]["layer_type"] = layer_dict['type']
                        if x == (int(tile_num4Inchls)-1):
                            perTileInfo[idx]["AccEn"] = 0
                            perTileInfo[idx]["concatEn"] = 1
                        else:
                            perTileInfo[idx]["AccEn"] = 1
                            perTileInfo[idx]["concatEn"] = 0

                y_restcols = int(layer_outputDimension % (self.tile.xbar_column * self.tile_pe_num[1]))
                xy_partcol_tilenum = int(tile_num4Inchls)
                for j in range(0,xy_partcol_tilenum):
                    idx = j + y_fulcol_tilenum * tile_num4Inchls
                    perTileInfo[idx]["Odata"] = datapertile_fulcol
                    if j==(int(xy_partcol_tilenum)-1):
                        perTileInfo[idx]["AccEn"] = 0
                        perTileInfo[idx]["concatEn"] = 1
                    else:
                        perTileInfo[idx]["AccEn"] = 1
                        perTileInfo[idx]["concatEn"] = 0
                    
            else:
                if layer_type == 'element_sum':
                    perTileInfo[0] = {"layer_type": "-1", "Odata": -1, "AccEn": -1, "concatEn": -1, "coord": (0, 0)}
                    perTileInfo[0]["layer_type"] = layer_type
                    perTileInfo[0]["Odata"] = tmp_tileinfo['outputdata']
                else:
                    xy_partcol_tilenum = int(tile_num4Inchls)
                    datapertile_partcol = tmp_tileinfo['outputdata']
                    for j in range(0,xy_partcol_tilenum):
                        perTileInfo[j] = {"layer_type": "-1", "Odata": -1, "AccEn": -1, "concatEn": -1, "coord": (0, 0)}
                        perTileInfo[j]["layer_type"] = layer_dict['type']
                        perTileInfo[j]["Odata"] = datapertile_partcol
                        if j == (xy_partcol_tilenum-1):
                            perTileInfo[j]["AccEn"] = 0
                        else:
                            perTileInfo[j]["AccEn"] = 1
                        perTileInfo[j]["concatEn"] = 0
            print("perTileInfo-------------------", perTileInfo)
            self.layerDataPerTile[layer_id].update(perTileInfo)

            
            tmp_tileinfo['max_PE'] = min(tmp_tileinfo['PEnum'], self.tile.tile_PE_total_num)
            start_tileid += tmp_tileinfo['tilenum']

            self.layer_tileinfo.append(tmp_tileinfo)

            inputbit = int(layer_dict['Inputbit'])
            if tmp_tileinfo['type'] == 'conv' or tmp_tileinfo['type'] == 'fc':
                tmp_inbuf_size = math.pow(2,math.ceil(math.log(data_inbuf / tmp_tileinfo['PEnum'],2)))/1024
                tmp_outbuf_size = math.pow(2,math.ceil(math.log(data_outbuf*2 / tmp_tileinfo['tilenum'],2)))/1024 # 2: ping-pong
            else:
                tmp_inbuf_size = 0
                tmp_outbuf_size = 0
            # unit: KB, restricted in 2^M KB
            if tmp_inbuf_size > self.max_inbuf_size:
                self.max_inbuf_size = tmp_inbuf_size
            if tmp_outbuf_size > self.max_outbuf_size:
                self.max_outbuf_size = tmp_outbuf_size

        self.used_tile_num = start_tileid
        assert self.used_tile_num <= self.tile_total_num, "Tile number is not enough"
            # TODO: update weight rewrite in xbar
        print("Total crossbar number:", total_xbar_num)
        self.inLayer_distance = np.zeros([1, self.layer_num])
        self.transLayer_distance = np.zeros([1, self.layer_num])
        self.aggregate_arg = np.zeros([self.layer_num, 2])

    #def DataFlowCpt(self):

    def mapping_matrix_gen(self):
        if self.tile_connection == 0:#tile_connection是啥？
            self.mapping_order = generate_normal_matrix(self.mapping_order.shape[0], self.mapping_order.shape[1])
            print(self.mapping_order)
        elif self.tile_connection == 1:
            self.mapping_order = generate_snake_matrix(self.mapping_order.shape[0], self.mapping_order.shape[1])
            print(self.mapping_order)
        elif self.tile_connection == 2:
            self.mapping_order = generate_hui_matrix(self.mapping_order.shape[0], self.mapping_order.shape[1])
            print("====================mapping order here======================")
            print(self.mapping_order)
        elif self.tile_connection == 3:
            self.mapping_order = generate_zigzag_matrix(self.mapping_order.shape[0], self.mapping_order.shape[1])
            print(self.mapping_order)

    def mapping_net(self):
        self.mapping_matrix_gen()
        for i in range(self.mapping_order.shape[0]):
            for j in range(self.mapping_order.shape[1]):
                if self.mapping_order[i][j] < self.used_tile_num:
                    for layer_id in range(self.layer_num - 1):
                        if self.layer_tileinfo[layer_id]['type'] in ['conv','pooling','fc']:
                            # only allocate tile for conv layers, pooling layers, and fc layers
                            if ((self.mapping_order[i][j] >= self.layer_tileinfo[layer_id]['startid']) &
                                    (self.mapping_order[i][j] < self.layer_tileinfo[layer_id + 1]['startid'])):#这边要看明白每一层从哪开始从哪结束
                                self.mapping_result[i][j] = layer_id
                                #这边把坐下存下来
                                break
                            elif self.mapping_order[i][j] >= self.layer_tileinfo[self.layer_num - 1]['startid']:
                                self.mapping_result[i][j] = self.layer_num - 1
        return self.mapping_result
    
    #according to mapping result ,record current coord and dest coord for each tile

    def calculate_transfer_distance(self):
        for layer_id in range(self.layer_num - 1):
            # Determine the aggregate node for layer 0~N-1
            if self.layer_tileinfo[layer_id]['is_branchout'] == 1:
                # for the layer which is a output layer of one branch and the next layer is element_sum
                if self.layer_tileinfo[layer_id]['type'] in ['conv', 'pooling', 'fc']:
                    src_pos = np.argwhere(self.mapping_result == layer_id)
                    if len(src_pos) == 1: 
                        self.inLayer_distance[0][layer_id] = 0
                        self.aggregate_arg[layer_id] = src_pos[0]
                        self.transLayer_distance[0][layer_id] = abs(src_pos[0][0]-1/2*self.tile_num[0]) + src_pos[0][1]
                    else:
                        mindis_total = 1000
                        for A in range(len(src_pos)):
                            tmp_transLayer_distance = abs(src_pos[A][0]-1/2*self.tile_num[0]) + src_pos[A][1]
                            maxdis_in = 0
                            for i in range(len(src_pos)):
                                if i != A:
                                    dis_in = abs(src_pos[A][0] - src_pos[i][0]) + abs(src_pos[A][1] - src_pos[i][1])
                                    if dis_in > maxdis_in:
                                        maxdis_in = dis_in
                            if (maxdis_in+tmp_transLayer_distance)<mindis_total:
                                self.inLayer_distance[0][layer_id] = maxdis_in
                                self.transLayer_distance[0][layer_id] = tmp_transLayer_distance
                                self.aggregate_arg[layer_id] = src_pos[A]
                                mindis_total = maxdis_in+tmp_transLayer_distance
            else:
                if self.layer_tileinfo[layer_id]['type'] in ['conv', 'pooling', 'fc']:
                    src_pos = np.argwhere(self.mapping_result == layer_id)
                    if len(src_pos) == 1:
                        self.inLayer_distance[0][layer_id] = 0
                        self.aggregate_arg[layer_id] = src_pos[0]
                        maxdis = 0
                        for idx in self.layer_tileinfo[layer_id]['Outputindex']:
                            dst_pos = np.argwhere(self.mapping_result == (layer_id + idx))
                            for i in range(len(dst_pos)):
                                dis = abs(src_pos[0][0] - dst_pos[i][0]) + abs(src_pos[0][1] - dst_pos[i][1])
                                if dis > maxdis:
                                    maxdis = dis
                        self.transLayer_distance[0][layer_id] = maxdis
                    else:
                        mindis_total = 1000
                        for A in range(len(src_pos)):
                            maxdis_in = 0
                            maxdis_out = 0
                            for i in range(len(src_pos)):
                                if i != A:
                                    dis_in = abs(src_pos[A][0] - src_pos[i][0]) + abs(src_pos[A][1] - src_pos[i][1])
                                    if dis_in > maxdis_in:
                                        maxdis_in = dis_in
                            for idx in self.layer_tileinfo[layer_id]['Outputindex']:
                                dst_pos = np.argwhere(self.mapping_result == (layer_id + idx))
                                for j in range(len(dst_pos)):
                                    dis_out = abs(src_pos[A][0] - dst_pos[j][0]) + abs(src_pos[A][1] - dst_pos[j][1])
                                    if dis_out > maxdis_out:
                                        maxdis_out = dis_out
                            tempdis = maxdis_in + maxdis_out
                            if tempdis < mindis_total:
                                self.inLayer_distance[0][layer_id] = maxdis_in
                                self.transLayer_distance[0][layer_id] = maxdis_out
                                self.aggregate_arg[layer_id] = src_pos[A]
                                mindis_total = tempdis
                elif self.layer_tileinfo[layer_id]['type'] == 'element_sum':
                    maxdis_out = 0
                    for idx in self.layer_tileinfo[layer_id]['Outputindex']:
                        dst_pos = np.argwhere(self.mapping_result == (layer_id + idx))
                        for j in range(len(dst_pos)):
                            dis_out = abs(dst_pos[0][0]-1/2*self.tile_num[0]) + dst_pos[0][1]
                            if dis_out > maxdis_out:
                                maxdis_out = dis_out
                    self.inLayer_distance[0][layer_id] = 0
                    self.transLayer_distance[0][layer_id] = maxdis_out
        final_pos = np.argwhere(self.mapping_result == self.layer_num - 1)
        # Determine the aggregate node for layer N (output layer)
        mindis = 1000
        for i in range(len(final_pos)):
            maxdis = 0
            for j in range(len(final_pos)):
                if j != i:
                    dis = abs(final_pos[i][0] - final_pos[j][0]) + abs(final_pos[i][1] - final_pos[j][1])
                    if dis > maxdis:
                        maxdis = dis
            if maxdis < mindis:
                mindis = maxdis
                self.inLayer_distance[0][self.layer_num - 1] = mindis
                self.aggregate_arg[self.layer_num - 1] = final_pos[i]
                self.transLayer_distance[0][self.layer_num - 1] = 0
        # self.total_distance = sum(sum(self.trans_time * (self.inLayer_distance + self.transLayer_distance)))


if __name__ == '__main__':#其作用为只有当本文件作为脚本直接运行时才会执行以下代码，如果是被其他脚本调用import的话，则不执行下面的函数。
    test_SimConfig_path = os.path.join(os.path.dirname(os.path.dirname(os.getcwd())), "SimConfig.ini")
    test_weights_file_path = os.path.join(os.path.dirname(os.path.dirname(os.getcwd())),
                                          "vgg8_params.pth")

    __TestInterface = TrainTestInterface('vgg8_128_9', 'MNSIM.Interface.cifar10', test_SimConfig_path,
                                         test_weights_file_path, 'cpu')
    structure_file = __TestInterface.get_structure()

    test = TCG(structure_file, test_SimConfig_path)
    test.mapping_net()
    test.calculate_transfer_distance()
    # print(test.total_distance)
