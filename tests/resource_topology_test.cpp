#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioIR/StyioIR.hpp"
#include "StyioLowering/AstToStyioIRLowerer.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioRuntime/HandleTable.hpp"
#include "StyioToString/ToStringVisitor.hpp"

namespace rt = styio::resource_topology;

namespace {

std::unique_ptr<MainBlockAST>
program(std::vector<StyioAST*> stmts) {
  return std::unique_ptr<MainBlockAST>(MainBlockAST::Create(std::move(stmts)));
}

std::vector<std::pair<size_t, size_t>>
line_seps(const std::string& src) {
  std::vector<std::pair<size_t, size_t>> seps;
  size_t line_start = 0;
  size_t line_len = 0;
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i] == '\n') {
      seps.emplace_back(line_start, line_len);
      line_start = i + 1;
      line_len = 0;
    }
    else {
      line_len += 1;
    }
  }
  if (!src.empty() && src.back() != '\n') {
    seps.emplace_back(line_start, line_len);
  }
  return seps;
}

void free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* token : tokens) {
    delete token;
  }
  tokens.clear();
}

void typecheck_nightly(const std::string& src) {
  auto tokens = StyioTokenizer::tokenize(src);
  StyioContext* ctx = StyioContext::Create(
    "<resource-topology-test>",
    src,
    line_seps(src),
    tokens,
    false);
  MainBlockAST* ast = nullptr;
  auto cleanup = [&]()
  {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };
  try {
    ast = parse_main_block_with_engine_latest(*ctx, StyioParserEngine::Nightly);
    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    cleanup();
  }
  catch (...) {
    cleanup();
    throw;
  }
}

std::string lower_nightly_ir(const std::string& src) {
  auto tokens = StyioTokenizer::tokenize(src);
  StyioContext* ctx = StyioContext::Create(
    "<resource-topology-lowering-test>",
    src,
    line_seps(src),
    tokens,
    false);
  MainBlockAST* ast = nullptr;
  auto cleanup = [&]()
  {
    delete ast;
    delete ctx;
    free_tokens(tokens);
    StyioAST::destroy_all_tracked_nodes();
  };
  try {
    ast = parse_main_block_with_engine_latest(*ctx, StyioParserEngine::Nightly);
    AstToStyioIRLowerer analyzer;
    ast->typeInfer(&analyzer);
    StyioIR* ir = ast->toStyioIR(&analyzer);
    StyioRepr repr;
    std::string out = ir->toString(&repr);
    cleanup();
    return out;
  }
  catch (...) {
    cleanup();
    throw;
  }
}

void expect_type_error_contains(const std::string& src, const std::string& needle) {
  try {
    typecheck_nightly(src);
    FAIL() << "expected type error containing `" << needle << "`";
  }
  catch (const StyioTypeError& ex) {
    EXPECT_NE(std::string(ex.what()).find(needle), std::string::npos)
      << ex.what();
  }
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

TEST(StyioResourceTopology, EmptyResourceDestroySinkConsumesWithoutScopeDrop) {
  auto root = program({
    ResourceRedirectAST::Create(
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-destroy.txt"), false),
      EmptyResourceAST::Create()),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_NE(result.graph.debug_string().find("destroy"), std::string::npos)
    << result.graph.debug_string();
  EXPECT_EQ(result.graph.debug_string().find("scope-exit-drop"), std::string::npos)
    << result.graph.debug_string();
}

TEST(StyioResourceTopology, RejectsUnorderedExclusiveResourceBorrowsAcrossBlocks) {
  auto root = program({
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("log")),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-log.txt"), true)),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t1")),
      BlockAST::Create({
        FuncCallAST::Create(
          NameAST::Create("log"),
          NameAST::Create("write"),
          {StringAST::Create("a")}),
      })),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t2")),
      BlockAST::Create({
        FuncCallAST::Create(
          NameAST::Create("log"),
          NameAST::Create("write"),
          {StringAST::Create("b")}),
      })),
  });

  rt::BuildResult result = rt::build(root.get());

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("unordered exclusive resource borrow"), std::string::npos)
    << result.report.message();
}

TEST(StyioResourceTopology, AllowsOrderedExclusiveResourceBorrowsAcrossBlocks) {
  auto root = program({
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("log")),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-log.txt"), true)),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t1")),
      BlockAST::Create({
        FuncCallAST::Create(
          NameAST::Create("log"),
          NameAST::Create("write"),
          {StringAST::Create("a")}),
      })),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t2")),
      BlockAST::Create({
        FuncCallAST::Create(
          NameAST::Create("log"),
          NameAST::Create("write"),
          {StringAST::Create("b")}),
      })),
    ResourceOrderAST::Create(NameAST::Create("t1"), NameAST::Create("t2")),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::HappensBefore), 1u);
}

TEST(StyioResourceTopology, ResourceMethodCallConsumesReceiverStatically) {
  const std::string src =
    "@file::close = () => { @file -> @() }\n"
    "log := @(\"log.txt\")\n"
    "log.close()\n"
    "log.path\n";

  expect_type_error_contains(src, "use-after-destroy");
}

TEST(StyioResourceTopology, UnknownResourceMethodIsCompileError) {
  const std::string src =
    "log := @(\"log.txt\")\n"
    "log.nope()\n";

  expect_type_error_contains(src, "resource method cannot be resolved");
}

TEST(StyioResourceTopology, FinalResourceMethodCannotBeOverridden) {
  const std::string src =
    "@file::close := () => { @file -> @() }\n"
    "@file::close = () => { 0 }\n"
    "log := @(\"log.txt\")\n";

  expect_type_error_contains(src, "final and cannot be overridden");
}

TEST(StyioResourceTopology, ResourcePropertyCannotBeCalled) {
  const std::string src =
    "log := @(\"log.txt\")\n"
    "log.path()\n";

  expect_type_error_contains(src, "resource property @file::path is not callable");
}

TEST(StyioResourceTopology, ResourceMethodArityMismatchIsCompileError) {
  const std::string src =
    "@file::flush = (mode: i64) => { 0 }\n"
    "log := @(\"log.txt\")\n"
    "log.flush()\n";

  expect_type_error_contains(src, "expects 1 argument(s), got 0");
}

TEST(StyioResourceTopology, RepeatedConsumingMethodCallIsUseAfterDestroy) {
  const std::string src =
    "@file::close = () => { @file -> @() }\n"
    "log := @(\"log.txt\")\n"
    "log.close()\n"
    "log.close()\n";

  expect_type_error_contains(src, "use-after-destroy");
}

TEST(StyioResourceTopology, TaskCannotConsumeOuterResource) {
  const std::string src =
    "log := @(\"log.txt\")\n"
    "job = ||> { log.close() }\n";

  expect_type_error_contains(src, "task cannot consume outer resource");
}

TEST(StyioResourceTopology, FileWriteAndCloseMethodsLowerToIO) {
  const std::string src =
    "log := @(\"log.txt\")\n"
    "log.write(\"a\")\n"
    "log.close()\n";

  std::string ir = lower_nightly_ir(src);

  EXPECT_NE(ir.find("styio.ir.handle_acquire"), std::string::npos) << ir;
  EXPECT_NE(ir.find("styio.ir.resource_write"), std::string::npos) << ir;
  EXPECT_NE(ir.find("styio.ir.handle_release"), std::string::npos) << ir;
}

TEST(StyioResourceTopology, NonConsumingCloseOverrideDoesNotLowerRelease) {
  const std::string src =
    "@file::close = () => { 0 }\n"
    "log := @(\"log.txt\")\n"
    "log.close()\n"
    "log.path\n";

  std::string ir = lower_nightly_ir(src);

  EXPECT_NE(ir.find("styio.ir.handle_acquire"), std::string::npos) << ir;
  EXPECT_EQ(ir.find("styio.ir.handle_release"), std::string::npos) << ir;
}

TEST(StyioResourceTopology, ResourceMethodInfersTransitiveConsume) {
  const std::string src =
    "@file::dispose = () => { @file -> @() }\n"
    "@file::close = () => { @file.dispose() }\n"
    "log := @(\"log.txt\")\n"
    "log.close()\n"
    "log.path\n";

  expect_type_error_contains(src, "use-after-destroy");
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
