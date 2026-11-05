import numpy as np
import os
import pandas as pd

class CsvTransformer():
        def __init__(self,homepath):#TCG类的构造函数，在创建对象时会被自动执行
            work_path = homepath
            print('--------------------------------------work_path---------------------------------', work_path)
            work_path += '/MNSIM/NoC/to_interconnect'
            self.ActiPath = os.path.join(work_path, "num_tiles_per_layer.csv")
            self.ParamPath = os.path.join(work_path, "ip_activation.csv")

        def Param2CSV(self, params):
            #file_exist = os.path.isfile(self.ParamPath)
            #if file_exist == True:
            #    os.system('rm -rf ' + self.ParamPath)
            #os.mkfile(self.ParamPath)
            for i in range(len(params)):
                params[i] = params[i] * 1
            df = pd.DataFrame(params)
            print('df',df)
            df.to_csv(self.ParamPath, index=False, header=False)

        def mappingArray2csv(self,matrix,layer_num):
            # 用于存储每个元素及其坐标
            element_dict = {} #key为元素值，value为坐标
            # 用于存储重复元素及其坐标和次数
            duplicates = {}

            # 遍历矩阵，记录每个元素的坐标
            for i in range(len(matrix)):
                for j in range(len(matrix[i])):
                    element = (matrix[i][j])
                    if element == -1:
                        continue
                    if element in element_dict:
                        if element not in duplicates:
                            duplicates[element] = {"count": 1}
                            #duplicates[element] = {"coordinates": [element_dict[element]], "count": 1}
                        #duplicates[element]["coordinates"].append((i, j))
                        duplicates[element]["count"] += 1
                    else:
                        element_dict[element] = (i, j)

            # 只返回有重复的元素及其坐标和次数
            #return {k: v for k, v in duplicates.items()}    #k：代表键值，v代表value，这边即在返回整个字典的键值对
            csv_vec = np.ones((layer_num,1))
            sorted_keys = sorted(duplicates.keys())

            print(sorted_keys)
            for i in range(layer_num):
                print(i)
                if i in sorted_keys:
                    csv_vec[i] = duplicates[i]["count"]
                else: continue
            csv_vec = csv_vec.astype(int)
            print(csv_vec)
            df=pd.DataFrame(csv_vec)
            df.to_csv(self.ActiPath, index=False,header=False)
