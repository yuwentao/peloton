/**
 * @brief Test cases for limit node.
 *
 * Copyright(c) 2015, CMU
 */

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/types.h"
#include "common/value.h"
#include "executor/logical_tile.h"
#include "executor/limit_executor.h"
#include "executor/logical_tile_factory.h"
#include "planner/limit_node.h"
#include "storage/data_table.h"


#include "executor/executor_tests_util.h"
#include "executor/mock_executor.h"
#include "harness.h"

using ::testing::NotNull;
using ::testing::Return;

namespace nstore {
namespace test {

namespace {

void RunTest(
    executor::LimitExecutor &executor,
    size_t  expected_num_tiles,
    oid_t   expected_first_oid,
    size_t  expected_num_tuples_returned){

  EXPECT_TRUE(executor.Init());

  std::vector<std::unique_ptr<executor::LogicalTile>> result_tiles;
  while(executor.Execute()){
    result_tiles.emplace_back(executor.GetOutput());
  }

  EXPECT_EQ(expected_num_tiles, result_tiles.size());

  if(result_tiles.size() > 0)
    EXPECT_EQ(expected_first_oid, *(result_tiles[0]->begin()));

  size_t actual_num_tuples_returned = 0;
  for(auto &tile : result_tiles){
    actual_num_tuples_returned += tile->GetTupleCount();
  }

  EXPECT_EQ(expected_num_tuples_returned, actual_num_tuples_returned);
}

TEST(LimitTests, NonLeafLimitOffsetTest){
  size_t tile_size = 50;
  size_t offset = tile_size / 2, limit = tile_size;

  // Create the plan node
  planner::LimitNode node(limit, offset);

  // Create and set up executor
  auto& txn_manager = TransactionManager::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  executor::LimitExecutor executor(&node, txn);
  MockExecutor child_executor;
  executor.AddChild(&child_executor);

  EXPECT_CALL(child_executor, DInit())
  .WillOnce(Return(true));

  EXPECT_CALL(child_executor, DExecute())
  .WillOnce(Return(true))
  .WillOnce(Return(true));  // even no need to call the child for 3rd time

  // Create a table and wrap it in logical tile
  std::unique_ptr<storage::DataTable> data_table(ExecutorTestsUtil::CreateTable(tile_size));
  ExecutorTestsUtil::PopulateTable(data_table.get(), tile_size*3);

  std::unique_ptr<executor::LogicalTile> source_logical_tile1(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(0)));

  std::unique_ptr<executor::LogicalTile> source_logical_tile2(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(1)));

  EXPECT_CALL(child_executor, GetOutput())
  .WillOnce(Return(source_logical_tile1.release()))
  .WillOnce(Return(source_logical_tile2.release()));

  RunTest(executor, 2, offset, limit);

  txn_manager.CommitTransaction(txn);
  txn_manager.EndTransaction(txn);
}

TEST(LimitTests, NonLeafSkipAllTest){
  size_t tile_size = 50;
  size_t offset = tile_size * 10, limit = tile_size;

  // Create the plan node
  planner::LimitNode node(limit, offset);

  // Create and set up executor
  auto& txn_manager = TransactionManager::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  executor::LimitExecutor executor(&node, txn);
  MockExecutor child_executor;
  executor.AddChild(&child_executor);

  EXPECT_CALL(child_executor, DInit())
  .WillOnce(Return(true));

  EXPECT_CALL(child_executor, DExecute())
  .WillOnce(Return(true))
  .WillOnce(Return(true))
  .WillOnce(Return(false));

  // Create a table and wrap it in logical tile
  std::unique_ptr<storage::DataTable> data_table(ExecutorTestsUtil::CreateTable(tile_size));
  ExecutorTestsUtil::PopulateTable(data_table.get(), tile_size*3);

  std::unique_ptr<executor::LogicalTile> source_logical_tile1(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(0)));

  std::unique_ptr<executor::LogicalTile> source_logical_tile2(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(1)));

  EXPECT_CALL(child_executor, GetOutput())
  .WillOnce(Return(source_logical_tile1.release()))
  .WillOnce(Return(source_logical_tile2.release()));

  RunTest(executor, 0, INVALID_OID, 0);

  txn_manager.CommitTransaction(txn);
  txn_manager.EndTransaction(txn);
}

TEST(LimitTests, NonLeafReturnAllTest){
  size_t tile_size = 50;
  size_t offset = 0, limit = tile_size * 10;

  // Create the plan node
  planner::LimitNode node(limit, offset);

  // Create and set up executor
  auto& txn_manager = TransactionManager::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  executor::LimitExecutor executor(&node, txn);
  MockExecutor child_executor;
  executor.AddChild(&child_executor);

  EXPECT_CALL(child_executor, DInit())
  .WillOnce(Return(true));

  EXPECT_CALL(child_executor, DExecute())
  .WillOnce(Return(true))
  .WillOnce(Return(true))
  .WillOnce(Return(false));

  // Create a table and wrap it in logical tile
  std::unique_ptr<storage::DataTable> data_table(ExecutorTestsUtil::CreateTable(tile_size));
  ExecutorTestsUtil::PopulateTable(data_table.get(), tile_size*3);

  std::unique_ptr<executor::LogicalTile> source_logical_tile1(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(0)));

  std::unique_ptr<executor::LogicalTile> source_logical_tile2(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(1)));

  EXPECT_CALL(child_executor, GetOutput())
  .WillOnce(Return(source_logical_tile1.release()))
  .WillOnce(Return(source_logical_tile2.release()));

  RunTest(executor, 2, offset, tile_size*2);

  txn_manager.CommitTransaction(txn);
  txn_manager.EndTransaction(txn);
}

TEST(LimitTests, NonLeafHugeLimitTest){
  size_t tile_size = 50;
  size_t offset = tile_size / 2, limit = tile_size * 10;

  // Create the plan node
  planner::LimitNode node(limit, offset);

  // Create and set up executor
  auto& txn_manager = TransactionManager::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  executor::LimitExecutor executor(&node, txn);
  MockExecutor child_executor;
  executor.AddChild(&child_executor);

  EXPECT_CALL(child_executor, DInit())
  .WillOnce(Return(true));

  EXPECT_CALL(child_executor, DExecute())
  .WillOnce(Return(true))
  .WillOnce(Return(true))
  .WillOnce(Return(false));

  // Create a table and wrap it in logical tile
  std::unique_ptr<storage::DataTable> data_table(ExecutorTestsUtil::CreateTable(tile_size));
  ExecutorTestsUtil::PopulateTable(data_table.get(), tile_size*3);

  std::unique_ptr<executor::LogicalTile> source_logical_tile1(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(0)));

  std::unique_ptr<executor::LogicalTile> source_logical_tile2(
      executor::LogicalTileFactory::WrapTileGroup(data_table->GetTileGroup(1)));

  EXPECT_CALL(child_executor, GetOutput())
  .WillOnce(Return(source_logical_tile1.release()))
  .WillOnce(Return(source_logical_tile2.release()));

  RunTest(executor, 2, offset, tile_size*2 - offset);

  txn_manager.CommitTransaction(txn);
  txn_manager.EndTransaction(txn);
}


}

} // namespace test
} // namespace nstore
