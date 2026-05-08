#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioRuntime/HandleTable.hpp"

namespace rt = styio::resource_topology;

namespace {

std::unique_ptr<MainBlockAST>
program(std::vector<StyioAST*> stmts) {
  return std::unique_ptr<MainBlockAST>(MainBlockAST::Create(std::move(stmts)));
}

} // namespace

TEST(StyioResourceTopology, FileWriteOwnsCloseCapableSource) {
  auto root = program({
    ResourceWriteAST::Create(
      StringAST::Create("hello"),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-file.txt"), false)),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::DriverSource), 1u);
  EXPECT_GE(result.graph.node_count(rt::NodeKind::Sink), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Ownership), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Commit), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Backpressure), 1u);
}

TEST(StyioResourceTopology, RejectsRedirectToReadOnlyStdin) {
  auto root = program({
    ResourceRedirectAST::Create(
      StringAST::Create("hello"),
      StdStreamAST::Create(StdStreamKind::Stdin)),
  });

  rt::BuildResult result = rt::build(root.get());

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("push capability"), std::string::npos)
    << result.report.message();
  EXPECT_THROW(
    rt::validate_or_throw(root.get(), "test-resource-topology"),
    StyioTypeError);
}

TEST(StyioResourceTopology, SeriesIntrinsicRequiresStateOwner) {
  auto root = program({
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("x")),
      SeriesIntrinsicAST::Create(
        NameAST::Create("price"),
        SeriesIntrinsicOp::Avg,
        IntAST::Create("5"))),
  });

  rt::BuildResult result = rt::build(root.get());

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("series intrinsic must be owned"), std::string::npos)
    << result.report.message();
}

TEST(StyioResourceTopology, StateOwnedSeriesIntrinsicCreatesHiddenLedger) {
  auto root = program({
    StateDeclAST::Create(
      IntAST::Create("3"),
      nullptr,
      nullptr,
      VarAST::Create(NameAST::Create("ma")),
      SeriesIntrinsicAST::Create(
        NameAST::Create("price"),
        SeriesIntrinsicOp::Avg,
        IntAST::Create("3"))),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::StateSlot), 1u);
  EXPECT_GE(result.graph.node_count(rt::NodeKind::HiddenLedger), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Mutation), 1u);
}

TEST(StyioResourceTopology, TopologyV2DeclWriteAndSelectorBuildPendingCommitEdges) {
  auto resource_type = styio_make_topology_resource_type(
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64},
    StyioResourceShapeKind::Recent,
    2);
  auto root = program({
    ResourceDeclAST::Create({
      {NameAST::Create("x"), TypeAST::Create(resource_type)},
    }),
    ResourceRedirectAST::Create(
      IntAST::Create("1"),
      ResourceRefAST::Create(NameAST::Create("x"))),
    PrintAST::Create({
      ResourceRefAST::CreateSelector(NameAST::Create("x"), ResourceSelectorKind::Offset, -1),
    }),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::StateSlot), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Commit), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Mutation), 1u);
  EXPECT_NE(result.graph.debug_string().find("pending-write"), std::string::npos)
    << result.graph.debug_string();
  EXPECT_NE(result.graph.debug_string().find("committed-snapshot-read"), std::string::npos)
    << result.graph.debug_string();
}

TEST(StyioResourceTopology, RejectsLocalTopologyV2ResourceDecl) {
  auto resource_type = styio_make_topology_resource_type(
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64},
    StyioResourceShapeKind::Fixed,
    2);
  auto local = std::unique_ptr<BlockAST>(BlockAST::Create({
    ResourceDeclAST::Create({
      {NameAST::Create("x"), TypeAST::Create(resource_type)},
    }),
  }));

  rt::BuildResult result = rt::build(local.get());

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("resource declarations are top-level only"), std::string::npos)
    << result.report.message();
}

TEST(StyioResourceTopology, HandleTableReleaseAllClosesAndRecyclesSlots) {
  StyioHandleTable table;
  int closed = 0;

  const auto h1 = table.acquire(StyioHandleTable::HandleKind::File, new int(1));
  const auto h2 = table.acquire(StyioHandleTable::HandleKind::File, new int(2));

  ASSERT_NE(h1, 0);
  ASSERT_NE(h2, 0);
  EXPECT_EQ(table.size(), 2u);

  const std::size_t released = table.release_all(
    StyioHandleTable::HandleKind::File,
    [&closed](void* raw) {
      delete static_cast<int*>(raw);
      ++closed;
    });

  EXPECT_EQ(released, 2u);
  EXPECT_EQ(closed, 2);
  EXPECT_EQ(table.size(), 0u);

  const auto h3 = table.acquire(StyioHandleTable::HandleKind::File, new int(3));
  EXPECT_TRUE(h3 == h1 || h3 == h2);
  EXPECT_TRUE(table.release(
    h3,
    StyioHandleTable::HandleKind::File,
    [](void* raw) {
      delete static_cast<int*>(raw);
    }));
  EXPECT_EQ(table.size(), 0u);
}
