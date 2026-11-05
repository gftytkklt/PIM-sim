#include <gtest/gtest.h>
#include "simulator/tile2_0/SIMD.h"
#include "simulator/tile2_0/TaskScheduler.h"

TEST(TileModel, DynamicSIMDLatencyHasNoUninitializedMinimum) {
    struct InspectSIMD : SIMD { using SIMD::SIMD; using ModuleBase::get_process_manager; };
    auto simd=std::make_shared<InspectSIMD>("simd");
    simd->register_processes();
    EXPECT_EQ(simd->get_process_manager().get_process_type("SIMD_computation")->get_latency(),0u);
}
TEST(TileModel, TaskCountersBeginAtZeroAndWrapAtDeclaredBoundaries) {
    TaskCounter counter;
    counter.pt_num=2; counter.batch_num=3;
    EXPECT_FALSE(counter.step());
    EXPECT_TRUE(counter.step());
    EXPECT_EQ(counter.pt_cnt,0); EXPECT_EQ(counter.batch_cnt,1);
    for(int i=0;i<4;++i) counter.step();
    EXPECT_EQ(counter.batch_cnt,0);
}
TEST(TileModel, InvalidTaskShapesAndBankIDsAreRejected) {
    std::array<FmapTask,L1C_BANK> tasks{};
    tasks[0]={1,2,3,16,false};
    EXPECT_THROW(TaskScheduler("bad",tasks),std::invalid_argument);
    tasks[0]={1,0,2,16,false};
    EXPECT_THROW(TaskScheduler("bad",tasks),std::invalid_argument);
    tasks[0]={1,2,2,16,false};
    auto scheduler=std::make_shared<TaskScheduler>("ts",tasks);
    scheduler->register_message_handlers();
    EXPECT_THROW(scheduler->handle_message(GenericMessage("init_task",std::make_tuple(-1,1))),std::invalid_argument);
    EXPECT_THROW(scheduler->handle_message(GenericMessage("init_task",std::make_tuple(1,1))),std::invalid_argument);
    EXPECT_THROW(scheduler->handle_message(GenericMessage("init_task",std::make_tuple(0,0))),std::invalid_argument);
    scheduler->handle_message(GenericMessage("init_task",std::make_tuple(0,1)));
    scheduler->handle_message(GenericMessage("init_task",std::make_tuple(0,2)));
    EXPECT_EQ(scheduler->admitted_batches(),3u);
    EXPECT_EQ(scheduler->completed_batches(),0u);
}
