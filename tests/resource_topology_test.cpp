#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "StyioAST/AST.hpp"
#include "StyioException/Exception.hpp"
#include "StyioIR/GenIR/SGIR.hpp"
#include "StyioIR/GenIR/SIOIR.hpp"
#include "StyioIR/StyioIR.hpp"
#include "StyioLowering/AstToStyioIRLowerer.hpp"
#include "StyioParser/Parser.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioRuntime/HandleTable.hpp"
#include "StyioToString/ToStringVisitor.hpp"
#include "StyioUtil/ResourceNames.hpp"

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

StyioDataType named_state_type(const std::string& name, StyioTypeState state) {
  StyioDataType type{StyioDataTypeOption::Defined, name, 0};
  type.state = state;
  return type;
}

} // namespace

TEST(StyioResourceTopology, EnumNamesCoverAllResourceTopologyKinds) {
  EXPECT_EQ(rt::to_string(rt::NodeKind::Program), "Program");
  EXPECT_EQ(rt::to_string(rt::NodeKind::DriverSource), "DriverSource");
  EXPECT_EQ(rt::to_string(rt::NodeKind::Handle), "Handle");
  EXPECT_EQ(rt::to_string(rt::NodeKind::StreamOp), "StreamOp");
  EXPECT_EQ(rt::to_string(rt::NodeKind::StateSlot), "StateSlot");
  EXPECT_EQ(rt::to_string(rt::NodeKind::HiddenLedger), "HiddenLedger");
  EXPECT_EQ(rt::to_string(rt::NodeKind::Sink), "Sink");
  EXPECT_EQ(rt::to_string(rt::NodeKind::Task), "Task");
  EXPECT_EQ(rt::to_string(rt::NodeKind::FailureDomain), "FailureDomain");
  EXPECT_EQ(rt::to_string(rt::NodeKind::Value), "Value");

  EXPECT_EQ(rt::to_string(rt::EdgeKind::Flow), "Flow");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Intent), "Intent");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Ownership), "Ownership");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Borrow), "Borrow");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Mutation), "Mutation");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Backpressure), "Backpressure");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Commit), "Commit");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::HappensBefore), "HappensBefore");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Failure), "Failure");
  EXPECT_EQ(rt::to_string(rt::EdgeKind::Placement), "Placement");

  EXPECT_EQ(rt::to_string(rt::TypeState::Unknown), "Unknown");
  EXPECT_EQ(rt::to_string(rt::TypeState::Declared), "Declared");
  EXPECT_EQ(rt::to_string(rt::TypeState::Open), "Open");
  EXPECT_EQ(rt::to_string(rt::TypeState::Eof), "Eof");
  EXPECT_EQ(rt::to_string(rt::TypeState::Closed), "Closed");
  EXPECT_EQ(rt::to_string(rt::TypeState::Materialized), "Materialized");
  EXPECT_EQ(rt::to_string(rt::TypeState::Ready), "Ready");

  EXPECT_EQ(rt::to_string(static_cast<rt::NodeKind>(255)), "UnknownNode");
  EXPECT_EQ(rt::to_string(static_cast<rt::EdgeKind>(255)), "UnknownEdge");
  EXPECT_EQ(rt::to_string(static_cast<rt::TypeState>(255)), "UnknownState");
}

TEST(StyioResourceTopology, ResourceNameAndDefaultIrFactoriesCoverFallbacks) {
  EXPECT_STREQ(styio_std_stream_family_name(static_cast<StdStreamKind>(999)), "");
  EXPECT_EQ(styio_std_stream_resource_label(static_cast<StdStreamKind>(999)), "@");

  std::unique_ptr<SGResId> empty_id(SGResId::Create());
  EXPECT_EQ(empty_id->as_str(), "");
  EXPECT_FALSE(empty_id->has_history_selector);
  EXPECT_EQ(empty_id->history_offset, 0);

  std::unique_ptr<SIOStdStreamPull> default_pull(SIOStdStreamPull::Create());
  EXPECT_EQ(default_pull->result_type.option, StyioDataTypeOption::Integer);
  EXPECT_EQ(default_pull->result_type.name, "i64");
  EXPECT_EQ(default_pull->result_type.num_of_bit, 64u);
}

TEST(StyioResourceTopology, GraphApiCoversSparseEdgesAndInvalidLookups) {
  rt::Graph graph;

  graph.add_edge(
    rt::EdgeKind::Flow,
    std::numeric_limits<std::size_t>::max(),
    1,
    "missing");
  EXPECT_TRUE(graph.edges().empty());
  EXPECT_THROW(graph.node(0), std::out_of_range);

  const std::size_t first = graph.add_node(
    rt::NodeKind::Value,
    "first",
    rt::Capability::Pull | rt::Capability::Clone | rt::Capability::Task,
    rt::TypeState::Ready);
  const std::size_t second = graph.add_node(
    rt::NodeKind::Sink,
    "second",
    rt::Capability::None,
    rt::TypeState::Closed);
  graph.add_edge(rt::EdgeKind::Placement, first, second, "place");

  EXPECT_EQ(graph.node(first).label, "first");
  EXPECT_EQ(graph.node_count(rt::NodeKind::Value), 1u);
  EXPECT_EQ(graph.edge_count(rt::EdgeKind::Placement), 1u);
  EXPECT_NE(graph.debug_string().find("pull|clone|task"), std::string::npos)
    << graph.debug_string();
  EXPECT_NE(graph.debug_string().find("Placement place"), std::string::npos)
    << graph.debug_string();
}

TEST(StyioResourceTopology, BuilderReusesCachedAstNodesForRepeatedVisits) {
  auto* repeated = BinOpAST::Create(
    StyioOpType::Binary_Add,
    IntAST::Create("1"),
    IntAST::Create("2"));
  repeated->RHS = repeated->LHS;
  auto root = program({PrintAST::Create({repeated})});

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_EQ(result.graph.node_count(rt::NodeKind::Value), 2u)
    << result.graph.debug_string();
}

TEST(StyioResourceTopology, SparseAstFallbacksCoverTraversalEdges) {
  StyioDataType stdout_stream = styio_make_std_stream_type(StdStreamKind::Stdout, "string");
  StyioDataType readable_opaque{StyioDataTypeOption::Defined, "opaque-readable", 0};
  readable_opaque.capabilities = styio_caps(StyioTypeCapability::Readable);

  auto root = program({
    AttrAST::Create(NameAST::Create("opaque"), IntAST::Create("1")),
    ResourceMethodDefAST::Create(
      "file",
      "drop_via_close",
      false,
      false,
      {},
      FuncCallAST::Create(
        ResourceReceiverAST::Create("file"),
        NameAST::Create("close"),
        {})),
    ResourceMethodDefAST::Create(
      "file",
      "drop_via_arg",
      false,
      false,
      {},
      FuncCallAST::Create(
        NameAST::Create("wrap"),
        {ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())})),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("typed_stdout"), TypeAST::Create(stdout_stream)),
      IntAST::Create("1")),
    FuncCallAST::Create(
      NameAST::Create("typed_stdout"),
      NameAST::Create("write"),
      {StringAST::Create("out")}),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("readable"), TypeAST::Create(readable_opaque)),
      IntAST::Create("2")),
    FuncCallAST::Create(
      NameAST::Create("readable"),
      NameAST::Create("peek"),
      {}),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("job")),
      TaskGroupLaunchAST::Create({
        TaskBlockAST::Create(BlockAST::Create({PrintAST::Create({IntAST::Create("3")})})),
      })),
    PrintAST::Create({NameAST::Create("job")}),
    new VarAST(NameAST::Create("standalone")),
    ResourceOrderAST::Create(IntAST::Create("4"), IntAST::Create("5")),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_NE(graph.find("value:attr"), std::string::npos) << graph;
  EXPECT_NE(graph.find("resource-method:@file::drop_via_close"), std::string::npos) << graph;
  EXPECT_NE(graph.find("resource-method:@file::drop_via_arg"), std::string::npos) << graph;
  EXPECT_NE(graph.find("binding:typed_stdout"), std::string::npos) << graph;
  EXPECT_NE(graph.find("resource-method:write"), std::string::npos) << graph;
  EXPECT_NE(graph.find("resource-method:peek"), std::string::npos) << graph;
  EXPECT_NE(graph.find("task_ref"), std::string::npos) << graph;
  EXPECT_NE(graph.find("var:standalone"), std::string::npos) << graph;
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::HappensBefore), 1u);

  rt::BuildOptions options;
  options.require_close_owner = false;
  auto permissive_root = program({
    FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-permissive.txt"), false),
  });
  rt::BuildResult permissive = rt::build(permissive_root.get(), options);

  EXPECT_TRUE(permissive.report.ok()) << permissive.report.message();
}

TEST(StyioResourceTopology, ResourceOrderTraversalVisitsCyclesBeforeConflict) {
  auto root = program({
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("log")),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-cycle.log"), true)),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t1")),
      BlockAST::Create({
        FuncCallAST::Create(NameAST::Create("log"), NameAST::Create("write"), {StringAST::Create("a")}),
      })),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t2")),
      BlockAST::Create({PrintAST::Create({IntAST::Create("2")})})),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t3")),
      BlockAST::Create({PrintAST::Create({IntAST::Create("3")})})),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("t4")),
      BlockAST::Create({
        FuncCallAST::Create(NameAST::Create("log"), NameAST::Create("write"), {StringAST::Create("b")}),
      })),
    ResourceOrderAST::Create(NameAST::Create("t1"), NameAST::Create("t2")),
    ResourceOrderAST::Create(NameAST::Create("t1"), NameAST::Create("t3")),
    ResourceOrderAST::Create(NameAST::Create("t2"), NameAST::Create("t3")),
    ResourceOrderAST::Create(NameAST::Create("t3"), NameAST::Create("t2")),
  });

  rt::BuildResult result = rt::build(root.get());

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("unordered exclusive resource borrow"), std::string::npos)
    << result.report.message();
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::HappensBefore), 4u);
}

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

TEST(StyioResourceTopology, ResourceTopologyDeclWriteAndSelectorBuildPendingCommitEdges) {
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

TEST(StyioResourceTopology, SnapshotDeclBuildsStateSlotAndResourceEdges) {
  auto root = program({
    SnapshotDeclAST::Create(
      NameAST::Create("snap"),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-snap.txt"), false)),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::StateSlot), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Flow), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Mutation), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Ownership), 1u);
  EXPECT_NE(result.graph.debug_string().find("snapshot:snap"), std::string::npos)
    << result.graph.debug_string();
}

TEST(StyioResourceTopology, StreamZipBuildsIterBackpressureAndBodyEdges) {
  auto root = program({
    StreamZipAST::Create(
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-zip-a.txt"), false),
      {ParamAST::Create(NameAST::Create("a"))},
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-zip-b.txt"), false),
      {ParamAST::Create(NameAST::Create("b"))},
      PrintAST::Create({NameAST::Create("a"), NameAST::Create("b")})),
  });

  rt::BuildResult result = rt::build(root.get());

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::StreamOp), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Backpressure), 2u);
  EXPECT_NE(result.graph.debug_string().find("stream:zip"), std::string::npos)
    << result.graph.debug_string();
  EXPECT_NE(result.graph.debug_string().find("zip-a"), std::string::npos)
    << result.graph.debug_string();
  EXPECT_NE(result.graph.debug_string().find("zip-b"), std::string::npos)
    << result.graph.debug_string();
  EXPECT_NE(result.graph.debug_string().find("zip-body"), std::string::npos)
    << result.graph.debug_string();
}

TEST(StyioResourceTopology, ValueContainersAndGuardsBuildNestedValueNodes) {
  auto root = program({
    PrintAST::Create({
      TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")}),
      SetAST::Create({StringAST::Create("a"), StringAST::Create("b")}),
      DictAST::Create({
        {StringAST::Create("k"), IntAST::Create("3")},
      }),
      FallbackAST::Create(IntAST::Create("4"), IntAST::Create("5")),
      GuardSelectorAST::Create(NameAST::Create("candidate"), BoolAST::Create(true)),
      EqProbeAST::Create(NameAST::Create("candidate"), IntAST::Create("6")),
      WaveMergeAST::Create(BoolAST::Create(true), IntAST::Create("7"), IntAST::Create("8")),
      WaveDispatchAST::Create(
        BoolAST::Create(false),
        BlockAST::Create({PrintAST::Create({StringAST::Create("left")})}),
        BlockAST::Create({PrintAST::Create({StringAST::Create("right")})})),
    }),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  for (const char* needle : {
         "value:tuple",
         "value:set",
         "value:dict",
         "value:fallback",
         "value:guard",
         "value:eq_probe",
         "value:wave_merge",
         "value:wave_dispatch",
       }) {
    EXPECT_NE(graph.find(needle), std::string::npos) << needle << "\n" << graph;
  }
}

TEST(StyioResourceTopology, DictAccessTypeHintsUseExplicitBindingTypes) {
  auto dict_type = styio_make_dict_type("string", "i64");
  auto root = program({
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("lookup"), TypeAST::Create(dict_type)),
      DictAST::Create({
        {StringAST::Create("answer"), IntAST::Create("42")},
      })),
    PrintAST::Create({
      new ListOpAST(
        StyioNodeType::Access_By_Name,
        NameAST::Create("lookup"),
        StringAST::Create("answer")),
    }),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_NE(graph.find("binding:lookup"), std::string::npos) << graph;
  EXPECT_NE(graph.find("value:listop"), std::string::npos) << graph;
}

TEST(StyioResourceTopology, RejectsLocalResourceTopologyResourceDecl) {
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
  EXPECT_THROW(
    rt::validate_or_throw(local.get(), "block-resource-topology"),
    StyioTypeError);
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

TEST(StyioResourceTopology, StandaloneResourcesUsePreciseLabelsAndCapabilities) {
  auto root = program({
    EmptyResourceAST::Create(),
    ResourceReceiverAST::Create("stdin"),
    ResourceReceiverAST::Create("stdout"),
    ResourceReceiverAST::Create("stderr"),
    ResourceReceiverAST::Create("file"),
    FileResourceAST::Create(nullptr, false),
    FileResourceAST::Create(NameAST::Create("path_expr"), false),
    FileResourceAST::Create(NameAST::Create("auto_expr"), true),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::Handle), 4u);
  EXPECT_GE(result.graph.node_count(rt::NodeKind::DriverSource), 3u);
  EXPECT_NE(graph.find("@()"), std::string::npos) << graph;
  EXPECT_NE(graph.find("receiver:@stdin"), std::string::npos) << graph;
  EXPECT_NE(graph.find("receiver:@stdout"), std::string::npos) << graph;
  EXPECT_NE(graph.find("receiver:@stderr"), std::string::npos) << graph;
  EXPECT_NE(graph.find("receiver:@file"), std::string::npos) << graph;
  EXPECT_NE(graph.find("@file(<null>)"), std::string::npos) << graph;
  EXPECT_NE(graph.find("@file(<expr>)"), std::string::npos) << graph;
  EXPECT_NE(graph.find("@{<expr>}"), std::string::npos) << graph;
}

TEST(StyioResourceTopology, BindingsTasksStatesAndResourceFamiliesBuildEdges) {
  auto resource_type = styio_make_topology_resource_type(
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64},
    StyioResourceShapeKind::Recent,
    3);
  auto generic_stream = styio_make_std_stream_type(StdStreamKind::Stdout, "string");
  generic_stream.has_std_stream_kind = false;
  auto topology_resource = styio_make_topology_resource_type(
    StyioDataType{StyioDataTypeOption::Integer, "i64", 64},
    StyioResourceShapeKind::Scalar);

  auto root = program({
    ResourceDeclAST::Create(
      {{NameAST::Create("hist"), TypeAST::Create(resource_type)}},
      BlockAST::Create({
        ResourceRedirectAST::Create(
          IntAST::Create("1"),
          ResourceRefAST::Create(NameAST::Create("hist"))),
      })),
    PrintAST::Create({
      ResourceRefAST::CreateSelector(NameAST::Create("hist"), ResourceSelectorKind::SnapshotAll),
    }),
    StateDeclAST::Create(
      nullptr,
      NameAST::Create("acc"),
      IntAST::Create("10"),
      nullptr,
      NameAST::Create("acc")),
    StateDeclAST::Create(
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      IntAST::Create("2")),
    PrintAST::Create({NameAST::Create("acc")}),
    HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("lines")),
      StdStreamAST::Create(StdStreamKind::Stdin),
      HandleAcquireAST::BindMode::Flex),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("job")),
      TaskBlockAST::Create(BlockAST::Create({
        PrintAST::Create({IntAST::Create("3")}),
      }))),
    PrintAST::Create({NameAST::Create("job")}),
    FlexBindAST::Create(
      VarAST::Create(NameAST::Create("pipe")),
      BlockAST::Create({
        PrintAST::Create({IntAST::Create("4")}),
      })),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("plain")),
      IntAST::Create("5")),
    ResourceOrderAST::Create(NameAST::Create("job"), NameAST::Create("plain")),
    ResourceOrderAST::Create(NameAST::Create("pipe"), NameAST::Create("job")),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("ready"), TypeAST::Create(named_state_type("ready", StyioTypeState::Ready))),
      IntAST::Create("6")),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("done"), TypeAST::Create(named_state_type("done", StyioTypeState::Done))),
      IntAST::Create("7")),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("closed"), TypeAST::Create(named_state_type("closed", StyioTypeState::Closed))),
      IntAST::Create("8")),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("materialized"), TypeAST::Create(named_state_type("materialized", StyioTypeState::Materialized))),
      IntAST::Create("9")),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("streamish"), TypeAST::Create(generic_stream)),
      IntAST::Create("10")),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("resourceish"), TypeAST::Create(topology_resource)),
      IntAST::Create("11")),
    FuncCallAST::Create(
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-method.txt"), false),
      NameAST::Create("write"),
      {StringAST::Create("file")}),
    FuncCallAST::Create(
      StdStreamAST::Create(StdStreamKind::Stdout),
      NameAST::Create("write"),
      {StringAST::Create("stdout")}),
    FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("write"),
      {StringAST::Create("receiver")}),
    FuncCallAST::Create(
      ResourceRefAST::Create(NameAST::Create("hist")),
      NameAST::Create("write"),
      {IntAST::Create("12")}),
    FuncCallAST::Create(
      NameAST::Create("streamish"),
      NameAST::Create("write"),
      {StringAST::Create("generic")}),
    FuncCallAST::Create(
      NameAST::Create("resourceish"),
      NameAST::Create("write"),
      {IntAST::Create("13")}),
    FuncCallAST::Create(
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-unknown-method.txt"), false),
      NameAST::Create("flush"),
      {}),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_GE(result.graph.node_count(rt::NodeKind::StateSlot), 3u);
  EXPECT_GE(result.graph.node_count(rt::NodeKind::Task), 1u);
  EXPECT_GE(result.graph.node_count(rt::NodeKind::FailureDomain), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::Failure), 1u);
  EXPECT_GE(result.graph.edge_count(rt::EdgeKind::HappensBefore), 2u);
  EXPECT_NE(graph.find("resource:@hist"), std::string::npos) << graph;
  EXPECT_NE(graph.find("committed-snapshot-read"), std::string::npos) << graph;
  EXPECT_NE(graph.find("state:acc"), std::string::npos) << graph;
  EXPECT_NE(graph.find("state:<anonymous>"), std::string::npos) << graph;
  EXPECT_NE(graph.find("state-init"), std::string::npos) << graph;
  EXPECT_NE(graph.find("handle:lines"), std::string::npos) << graph;
  EXPECT_NE(graph.find("task:block"), std::string::npos) << graph;
  EXPECT_NE(graph.find("binding:pipe"), std::string::npos) << graph;
  EXPECT_NE(graph.find("binding:ready caps=none state=Ready"), std::string::npos) << graph;
  EXPECT_NE(graph.find("binding:done caps=none state=Ready"), std::string::npos) << graph;
  EXPECT_NE(graph.find("binding:closed caps=none state=Closed"), std::string::npos) << graph;
  EXPECT_NE(graph.find("binding:materialized caps=none state=Materialized"), std::string::npos) << graph;
}

TEST(StyioResourceTopology, FailsClosedForUnknownResourcesAndInvalidWindows) {
  auto root = program({
    PrintAST::Create({
      ResourceRefAST::Create(NameAST::Create("missing")),
    }),
    StateDeclAST::Create(
      IntAST::Create("0"),
      nullptr,
      nullptr,
      nullptr,
      SeriesIntrinsicAST::Create(
        NameAST::Create("price"),
        SeriesIntrinsicOp::Avg,
        IntAST::Create("0"))),
    StateDeclAST::Create(
      IntAST::Create("not-a-number"),
      NameAST::Create("bad"),
      IntAST::Create("1"),
      nullptr,
      IntAST::Create("2")),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("unknown resource: @missing"), std::string::npos)
    << result.report.message();
  EXPECT_NE(result.report.message().find("state window must be a positive integer"), std::string::npos)
    << result.report.message();
  EXPECT_NE(result.report.message().find("series intrinsic window must be a positive integer"), std::string::npos)
    << result.report.message();
  EXPECT_NE(graph.find("resource-ref:@missing"), std::string::npos) << graph;
  EXPECT_NE(graph.find("state:<anonymous>"), std::string::npos) << graph;
  EXPECT_NE(graph.find("state:bad"), std::string::npos) << graph;
  EXPECT_NE(graph.find("hidden-ledger:series"), std::string::npos) << graph;
}

TEST(StyioResourceTopology, NullRootsSparseWritesAndVarInitializersFailClosed) {
  {
    rt::BuildResult result = rt::build(static_cast<MainBlockAST*>(nullptr));
    ASSERT_FALSE(result.report.ok());
    EXPECT_NE(result.report.message().find("main block is null"), std::string::npos)
      << result.report.message();
  }

  {
    rt::BuildResult result = rt::build(static_cast<BlockAST*>(nullptr));
    ASSERT_FALSE(result.report.ok());
    EXPECT_NE(result.report.message().find("block is null"), std::string::npos)
      << result.report.message();
  }

  auto root = program({
    ResourceWriteAST::Create(StringAST::Create("payload"), nullptr),
    new VarAST(NameAST::Create("initialized"), TypeAST::Create("i64"), IntAST::Create("42")),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  ASSERT_FALSE(result.report.ok());
  EXPECT_NE(result.report.message().find("write target must have push capability (missing node)"), std::string::npos)
    << result.report.message();
  EXPECT_NE(graph.find("sink:resource_write"), std::string::npos) << graph;
  EXPECT_NE(graph.find("value:styio.ast.string"), std::string::npos) << graph;
  EXPECT_NE(graph.find("value:styio.ast."), std::string::npos) << graph;
  EXPECT_EQ(graph.find("var:initialized"), std::string::npos) << graph;

  auto sparse_root = program({
    FinalBindAST::Create(VarAST::Create(NameAST::Create("missing_rhs")), nullptr),
    FinalBindAST::Create(VarAST::Create(NameAST::Create("")), nullptr),
  });
  rt::BuildResult sparse = rt::build(sparse_root.get());
  const std::string sparse_graph = sparse.graph.debug_string();

  EXPECT_TRUE(sparse.report.ok()) << sparse.report.message();
  EXPECT_NE(sparse_graph.find("binding:missing_rhs"), std::string::npos) << sparse_graph;
  EXPECT_NE(sparse_graph.find("binding:"), std::string::npos) << sparse_graph;
}

TEST(StyioResourceTopology, ResourceMethodConsumeScannerWalksBinopsAndConditions) {
  auto root = program({
    ResourceMethodDefAST::Create(
      "file",
      "drop_bin",
      false,
      false,
      {},
      BlockAST::Create({
        BinOpAST::Create(
          StyioOpType::Binary_Add,
          IntAST::Create("0"),
          ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())),
      })),
    ResourceMethodDefAST::Create(
      "file",
      "drop_cond",
      false,
      false,
      {},
      CondAST::Create(
        LogicType::AND,
        BoolAST::Create(false),
        ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()))),
    ResourceMethodDefAST::Create(
      "file",
      "drop_raw_cond",
      false,
      false,
      {},
      CondAST::Create(
        LogicType::RAW,
        ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()))),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("bin_log")),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-bin.log"), false)),
    FuncCallAST::Create(NameAST::Create("bin_log"), NameAST::Create("drop_bin"), {}),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("cond_log")),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-cond.log"), false)),
    FuncCallAST::Create(NameAST::Create("cond_log"), NameAST::Create("drop_cond"), {}),
    FinalBindAST::Create(
      VarAST::Create(NameAST::Create("raw_log")),
      FileResourceAST::Create(StringAST::Create("/tmp/styio-rtg-raw.log"), false)),
    FuncCallAST::Create(NameAST::Create("raw_log"), NameAST::Create("drop_raw_cond"), {}),
  });

  rt::BuildResult result = rt::build(root.get());
  const std::string graph = result.graph.debug_string();

  EXPECT_TRUE(result.report.ok()) << result.report.message();
  EXPECT_NE(graph.find("resource-method:@file::drop_bin"), std::string::npos) << graph;
  EXPECT_NE(graph.find("resource-method:@file::drop_cond"), std::string::npos) << graph;
  EXPECT_NE(graph.find("resource-method:@file::drop_raw_cond"), std::string::npos) << graph;
  EXPECT_NE(graph.find("value:binop"), std::string::npos) << graph;
  EXPECT_NE(graph.find("value:condition"), std::string::npos) << graph;
  EXPECT_NE(graph.find("Mutation resource-method:drop_bin"), std::string::npos) << graph;
  EXPECT_NE(graph.find("Mutation resource-method:drop_cond"), std::string::npos) << graph;
  EXPECT_NE(graph.find("Mutation resource-method:drop_raw_cond"), std::string::npos) << graph;
}

TEST(StyioToStringContract, CoversSparseAstAndIrReprBranches) {
  StyioRepr repr;

  {
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("p"), TypeAST::Create("i64")));
    std::unique_ptr<OptArgAST> opt_arg(OptArgAST::Create(NameAST::Create("rest")));
    std::unique_ptr<OptKwArgAST> opt_kw(OptKwArgAST::Create(NameAST::Create("opts")));
    EXPECT_NE(repr.toString(param.get()).find("p : i64"), std::string::npos);
    EXPECT_NE(repr.toString(opt_arg.get()).find("{  }"), std::string::npos);
    EXPECT_NE(repr.toString(opt_kw.get()).find("{  }"), std::string::npos);
  }

  {
    std::unique_ptr<VarTupleAST> empty(VarTupleAST::Create({}));
    std::unique_ptr<VarTupleAST> non_empty(VarTupleAST::Create({
      VarAST::Create(NameAST::Create("lhs"), TypeAST::Create("i64")),
      VarAST::Create(NameAST::Create("rhs"), TypeAST::Create("string")),
    }));
    EXPECT_NE(repr.toString(empty.get()).find("[ ]"), std::string::npos);
    const std::string tuple_repr = repr.toString(non_empty.get());
    EXPECT_NE(tuple_repr.find("lhs : i64"), std::string::npos) << tuple_repr;
    EXPECT_NE(tuple_repr.find("rhs : string"), std::string::npos) << tuple_repr;
  }

  {
    std::unique_ptr<IterSeqAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1"), IntAST::Create("2")}),
      {HashTagNameAST::Create({"hot"}), HashTagNameAST::Create({"cold"})}));
    const std::string iter_repr = repr.toString(iter.get());
    EXPECT_NE(iter_repr.find("hot"), std::string::npos) << iter_repr;
    EXPECT_NE(iter_repr.find("cold"), std::string::npos) << iter_repr;
  }

  {
    std::unique_ptr<InfiniteAST> original(new InfiniteAST());
    std::unique_ptr<InfiniteAST> incremental(new InfiniteAST(IntAST::Create("0"), IntAST::Create("1")));
    EXPECT_NE(repr.toString(original.get()).find("{ }"), std::string::npos);
    EXPECT_NE(repr.toString(incremental.get()).find("Increment"), std::string::npos);
  }

  {
    std::unique_ptr<StructAST> ast(StructAST::Create(
      NameAST::Create("Pair"),
      {
        ParamAST::Create(NameAST::Create("first"), TypeAST::Create("i64")),
        ParamAST::Create(NameAST::Create("second"), TypeAST::Create("string")),
      }));
    const std::string struct_repr = repr.toString(ast.get());
    EXPECT_NE(struct_repr.find("first"), std::string::npos) << struct_repr;
    EXPECT_NE(struct_repr.find("second"), std::string::npos) << struct_repr;
  }

  {
    std::unique_ptr<FmtStrAST> fmt(FmtStrAST::Create(
      {"a", "b", "c"},
      {NameAST::Create("x"), IntAST::Create("2")}));
    const std::string fmt_repr = repr.toString(fmt.get());
    EXPECT_NE(fmt_repr.find("x"), std::string::npos) << fmt_repr;
    EXPECT_NE(fmt_repr.find("\n"), std::string::npos) << fmt_repr;
  }

  {
    std::unique_ptr<ResourceAST> resources(ResourceAST::Create({
      {StringAST::Create("one.txt"), "text"},
      {FileResourceAST::Create(StringAST::Create("two.txt"), true), "file"},
    }));
    const std::string resource_repr = repr.toString(resources.get());
    EXPECT_NE(resource_repr.find("one.txt"), std::string::npos) << resource_repr;
    EXPECT_NE(resource_repr.find("two.txt"), std::string::npos) << resource_repr;
  }

  {
    std::unique_ptr<ResourceDeclAST> decl(ResourceDeclAST::Create(
      {
        {NameAST::Create("input"), TypeAST::Create("i64")},
        {NameAST::Create("output"), TypeAST::Create("string")},
      },
      BlockAST::Create({PassAST::Create()})));
    const std::string decl_repr = repr.toString(decl.get());
    EXPECT_NE(decl_repr.find("driver"), std::string::npos) << decl_repr;
    EXPECT_NE(decl_repr.find("output"), std::string::npos) << decl_repr;
  }

  {
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("tupled"),
      false,
      {ParamAST::Create(NameAST::Create("x"), TypeAST::Create("i64"))},
      TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("x"))})));
    const std::string fn_repr = repr.toString(fn.get());
    EXPECT_NE(fn_repr.find("ret_type"), std::string::npos) << fn_repr;
  }

  {
    std::unique_ptr<IteratorAST> iter(IteratorAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {ParamAST::Create(NameAST::Create("item"))},
      {
        CheckEqualAST::Create({IntAST::Create("1")}),
        CheckEqualAST::Create({IntAST::Create("2")}),
      }));
    const std::string iter_repr = repr.toString(iter.get());
    EXPECT_NE(iter_repr.find("check.equal"), std::string::npos) << iter_repr;
    EXPECT_NE(iter_repr.find("\n"), std::string::npos) << iter_repr;
  }

  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({
      PrintAST::Create({IntAST::Create("1")}),
    }));
    block->set_followings({
      CheckEqualAST::Create({IntAST::Create("2")}),
      new CheckIsinAST(NameAST::Create("needle")),
    });
    const std::string block_repr = repr.toString(block.get());
    EXPECT_NE(block_repr.find("check.equal"), std::string::npos) << block_repr;
    EXPECT_NE(block_repr.find("check.isin"), std::string::npos) << block_repr;
  }

  {
    std::unique_ptr<CODPAST> chain(CODPAST::Create(
      "map",
      {IntAST::Create("1"), IntAST::Create("2")},
      nullptr,
      CODPAST::Create("filter", {BoolAST::Create(true)})));
    const std::string chain_repr = chain->toString(&repr);
    EXPECT_NE(chain_repr.find("CODP.map"), std::string::npos) << chain_repr;
    EXPECT_NE(chain_repr.find("CODP.filter"), std::string::npos) << chain_repr;
  }

  {
    auto list_expr = []() {
      return ListAST::Create({IntAST::Create("1"), IntAST::Create("2")});
    };
    const std::vector<StyioNodeType> one_slot_ops = {
      StyioNodeType::Access,
      StyioNodeType::Access_By_Index,
      StyioNodeType::Access_By_Name,
      StyioNodeType::Get_Index_By_Value,
      StyioNodeType::Get_Indices_By_Many_Values,
      StyioNodeType::Append_Value,
      StyioNodeType::Remove_Item_By_Index,
      StyioNodeType::Remove_Item_By_Value,
      StyioNodeType::Remove_Items_By_Many_Indices,
      StyioNodeType::Remove_Items_By_Many_Values,
      StyioNodeType::Get_Index_By_Item_From_Right,
    };
    for (StyioNodeType op : one_slot_ops) {
      std::unique_ptr<ListOpAST> node(new ListOpAST(op, list_expr(), IntAST::Create("0")));
      EXPECT_FALSE(repr.toString(node.get()).empty());
    }
    std::unique_ptr<ListOpAST> reversed(new ListOpAST(StyioNodeType::Get_Reversed, list_expr()));
    EXPECT_FALSE(repr.toString(reversed.get()).empty());
    std::unique_ptr<ListOpAST> inserted(new ListOpAST(
      StyioNodeType::Insert_Item_By_Index,
      list_expr(),
      IntAST::Create("0"),
      IntAST::Create("9")));
    EXPECT_NE(repr.toString(inserted.get()).find("Value:"), std::string::npos);
  }

  {
    std::unique_ptr<CondFlowAST> fallback(new CondFlowAST(
      StyioNodeType::Empty,
      CondAST::Create(LogicType::RAW, BoolAST::Create(true)),
      PrintAST::Create({IntAST::Create("1")})));
    EXPECT_FALSE(repr.toString(fallback.get()).empty());
  }

  {
    std::unique_ptr<SGVar> first(SGVar::Create(
      SGResId::Create("first"),
      SGType::Create(StyioDataType{StyioDataTypeOption::Integer, "i64", 64})));
    std::unique_ptr<SGVar> second(SGVar::Create(
      SGResId::Create("second"),
      SGType::Create(StyioDataType{StyioDataTypeOption::String, "string", 0})));
    std::unique_ptr<SGStruct> structure(SGStruct::Create({first.get(), second.get()}));
    const std::string struct_repr = repr.toString(structure.get());
    EXPECT_NE(struct_repr.find("styio.ir.struct"), std::string::npos) << struct_repr;
    EXPECT_NE(struct_repr.find("first"), std::string::npos) << struct_repr;
    EXPECT_NE(struct_repr.find("second"), std::string::npos) << struct_repr;
  }

  {
    std::unique_ptr<SGExternBlock> ext(SGExternBlock::Create(
      "c",
      "int a(void);",
      {"one.h", "two.h"},
      {"a", "b"}));
    const std::string ext_repr = repr.toString(ext.get());
    EXPECT_NE(ext_repr.find("one.h,two.h"), std::string::npos) << ext_repr;
    EXPECT_NE(ext_repr.find("a,b"), std::string::npos) << ext_repr;
  }

  {
    std::unique_ptr<SGEntry> empty_entry(SGEntry::Create({}));
    EXPECT_NE(repr.toString(empty_entry.get()).find("{ }"), std::string::npos);
  }

  {
    std::unique_ptr<SIOInstantPull> pull(SIOInstantPull::Create(SGConstString::Create("input.txt")));
    const std::string pull_repr = repr.toString(pull.get());
    EXPECT_NE(pull_repr.find("input.txt"), std::string::npos) << pull_repr;
  }
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

TEST(StyioResourceTopology, ResourceUseAfterCloseStillRequiresRebind) {
  const std::string src =
    "log = @(\"log.txt\")\n"
    "log.close()\n"
    "log.path\n";

  expect_type_error_contains(src, "use-after-destroy");
}

TEST(StyioResourceTopology, FlexResourceRebindClearsDestroyedReceiver) {
  const std::string src =
    "log = @(\"log.txt\")\n"
    "log.close()\n"
    "log = @(\"log.txt\")\n"
    "log.path\n";

  EXPECT_NO_THROW(typecheck_nightly(src));
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

TEST(StyioResourceTopology, UserDefinedConsumingMethodLowersByInliningBody) {
  const std::string src =
    "@file::close = () => { @file -> @() }\n"
    "log := @(\"log.txt\")\n"
    "log.close()\n";

  std::string ir = lower_nightly_ir(src);

  EXPECT_NE(ir.find("styio.ir.handle_acquire"), std::string::npos) << ir;
  EXPECT_NE(ir.find("styio.ir.handle_release"), std::string::npos) << ir;
}

TEST(StyioResourceTopology, UserDefinedResourceMethodArgumentsInlineIntoBody) {
  const std::string src =
    "@file::emit = (text: string) => { @file.write(text) }\n"
    "log := @(\"log.txt\")\n"
    "log.emit(\"a\")\n";

  std::string ir = lower_nightly_ir(src);

  EXPECT_NE(ir.find("styio.ir.handle_acquire"), std::string::npos) << ir;
  EXPECT_NE(ir.find("styio.ir.resource_write"), std::string::npos) << ir;
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

TEST(StyioResourceTopology, HandleTableStubReservationReusesFreedSlots) {
  StyioHandleTable table;
  table.invalidate(0);
  EXPECT_EQ(table.size(), 0u);

  int payload = 42;
  const auto released_id = table.acquire(StyioHandleTable::HandleKind::File, &payload);
  ASSERT_NE(released_id, 0);
  EXPECT_TRUE(table.release(released_id, StyioHandleTable::HandleKind::File));

  const auto stub_id = table.reserve_stub(StyioHandleTable::HandleKind::Task);
  EXPECT_EQ(stub_id, released_id);
  EXPECT_TRUE(table.contains(stub_id));
  EXPECT_EQ(table.lookup(stub_id, StyioHandleTable::HandleKind::Task), nullptr);
  EXPECT_EQ(table.size(), 1u);

  table.invalidate(stub_id);
  EXPECT_FALSE(table.contains(stub_id));
  EXPECT_EQ(table.size(), 0u);
  table.invalidate(stub_id);
}
