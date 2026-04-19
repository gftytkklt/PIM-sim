#ifndef TILE2_0MODULES_H
#define TILE2_0MODULES_H

// 建模原则：
// 1. 每个模块都继承自ModuleBase，利用其提供的信号管理和进程管理功能。
// 2. 每个模块在构造函数中定义自己的输入输出信号，并在register_processes方法中注册自己的进程。
// 3. 进程的触发条件、执行逻辑、完成条件和结束条件由模块自己实现，利用ModuleBase提供的信号接口进行通信。
// 4. 模块之间通过连接信号进行通信，连接关系由Simulator管理，模块只需要关注自己的信号和进程逻辑。
// 5. 每个模块可以维护自己的性能统计数据，并通过get_module_specific_stats接口提供给Simulator汇总。
// 6. 模块内部可以使用私有成员变量来维护状态，例如TaskScheduler的任务计数器和批次信息，SIMD的计算状态和延迟等。
// 7. 发起进程请求在exec函数中进行，完成条件在finish函数中进行检查，结束条件在end函数中进行处理，符合握手语义。
// 8. 使用raise_process和invalidate_process接口来管理进程状态，避免直接操作信号值，保持模块内部逻辑清晰。
// 9. 如果一个进程发起模块A的执行，但通过模块B的信号判断完成条件，A驱动信号的发起和撤销应以A的握手为准（例如TS发起xbar计算，但和SIMD握手）

#include "TaskScheduler.h"
#include "Crossbar.h"
#include "SIMD.h"
#include "L1C.h"

#endif