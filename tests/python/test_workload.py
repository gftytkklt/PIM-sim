import json
from pathlib import Path
import tempfile
import unittest

import numpy as np
import onnx
from onnx import helper, numpy_helper, TensorProto
import pimapping
from pim_sim.onnx_analysis import load_kernel, make_hw_info, make_opt_info
from pim_sim.paths import WORKLOAD_TIMING

ROOT = Path(__file__).resolve().parents[2]

class WorkloadPython(unittest.TestCase):
    def test_real_onnx_mapping_to_cycle_engine(self):
        graph = helper.make_graph([
            helper.make_node('Conv',['x','w0'],['mid'],name='conv0'),
            helper.make_node('Conv',['mid','w1'],['y'],name='conv1')], 'two-conv',
            [helper.make_tensor_value_info('x',TensorProto.FLOAT,[1,1,1,1])],
            [helper.make_tensor_value_info('y',TensorProto.FLOAT,[1,1,1,1])],
            [numpy_helper.from_array(np.ones((1,1,1,1),dtype=np.float32),name=n) for n in ['w0','w1']])
        model=helper.make_model(graph,opset_imports=[helper.make_opsetid('',13)])
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'chain.onnx'
            onnx.save(model,path)
            kernels=load_kernel(str(path))
        workload=pimapping.export_workload(kernels,make_hw_info((1,1),1,(1,2),1),
                                           make_opt_info(False,False),WORKLOAD_TIMING.read_text())
        result=json.loads(pimapping.simulate_workload(workload,100))
        self.assertTrue(result['completed'])
        self.assertEqual(result['stop_reason'],'Completed')
        self.assertEqual(result['makespan_cycles'],7)
        self.assertEqual(result['completed_tasks'],2)
        self.assertEqual(result['sent_bytes'],result['received_bytes'])
        self.assertEqual(result,json.loads(pimapping.simulate_workload(workload,100)))
        incomplete=json.loads(pimapping.simulate_workload(workload,3))
        self.assertFalse(incomplete['completed'])
        self.assertEqual(incomplete['stop_reason'],'MaxCycles')
        self.assertIsNone(incomplete['latency_ns'])
        damaged=json.loads(workload)
        damaged['dependencies'][0]['bytes']=123
        with self.assertRaises(ValueError):
            pimapping.simulate_workload(json.dumps(damaged),100)

if __name__ == '__main__':
    unittest.main()
