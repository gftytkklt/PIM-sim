"""PIM-sim: performance analysis and visualization entry point.

perf.py is the main entry point for the full analysis pipeline:
  ONNX loading → graph analysis → Booksim NoC → results.

For programmatic use, import from analysis.py and plotting.py directly.
"""

import time
from analysis import (
    perf_analysis, get_opt_str, get_noc_perf, load_noc_perf,
    power_analysis, brkdown_stat, brkdown_analysis, get_data_percentage,
)
from plotting import (
    plot_all_comm, plot_comm, plot_perf, plot_bw_perf,
    plot_brkdown, plot_bw, plot_grouped_bars, plot_xbarsize, plot_pipeline,
)
from onnx_analysis import make_hw_info
from logger import Logger

home_path = __import__('os').getcwd()
logger = Logger(f"{home_path}/runs", "perf.log")


if __name__ == "__main__":
    bw_list = [1]
    xbar_size = (256, 256)
    hw_info = make_hw_info(xbar_size, 8, (0, 0), 1)

    begin_time = time.time()
    mapping_result, comm_result = perf_analysis(models_dir="models", hwinfo=hw_info)
    logger.info(f"Total Time: {time.time() - begin_time}")

    logger.info("Start running plot_pipeline.")
    plot_pipeline()