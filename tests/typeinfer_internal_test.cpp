#include <gtest/gtest.h>

#include <memory>

#include "StyioLowering/AstToStyioIRLowerer.hpp"

#include "../src/StyioSema/TypeInfer.cpp"

namespace {

StyioDataType undefined_type() {
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

void expect_matrix_type(
  const StyioDataType& type,
  const std::string& elem,
  size_t rows,
  size_t cols
) {
  EXPECT_TRUE(styio_is_matrix_type(type));
  EXPECT_EQ(styio_matrix_elem_type_name(type), elem);
  EXPECT_EQ(styio_matrix_row_count(type), rows);
  EXPECT_EQ(styio_matrix_col_count(type), cols);
}

class ExposedTypeInferLowerer : public AstToStyioIRLowerer {
 public:
  using StyioSemaContext::binding_info_;
  using StyioSemaContext::binding_info_by_sid_;
  using StyioSemaContext::callable_constraint_solver_stats_;
  using StyioSemaContext::consumed_resource_names_;
  using StyioSemaContext::consumed_resource_names_by_sid_;
  using StyioSemaContext::consumed_task_names_;
  using StyioSemaContext::consumed_task_names_by_sid_;
  using StyioSemaContext::fixed_assignment_names_;
  using StyioSemaContext::fixed_assignment_names_by_sid_;
  using StyioSemaContext::inferred_function_return_types_;
  using StyioSemaContext::inferred_function_return_types_by_sid_;
  using StyioSemaContext::owned_resource_names_;
  using StyioSemaContext::owned_resource_names_by_sid_;
  using StyioSemaContext::resource_binding_types_;
  using StyioSemaContext::resource_binding_types_by_sid_;
  using StyioSemaContext::resource_typestate_dataflow_stats_;
  using StyioSemaContext::resource_typestate_temporary_fact_slots_;
  using StyioSemaContext::snapshot_var_names_;
  using StyioSemaContext::snapshot_var_names_by_sid_;
  using StyioSemaContext::task_outer_resource_names_stack_;
  using StyioSemaContext::task_outer_resource_names_by_sid_stack_;
};

void install_file_handle(
  ExposedTypeInferLowerer& analyzer,
  const std::string& name,
  styio::session::SymbolId sid
) {
  const StyioDataType file_handle = styio_make_file_handle_type("string");
  StyioSemaContext::BindingInfo info;
  info.resource_value = true;
  info.declared_type = file_handle;
  analyzer.record_binding_info(name, sid, info);
  analyzer.record_local_binding_type(name, sid, file_handle);
}

ResourceRedirectAST* close_handle(
  const std::string& name,
  styio::session::SymbolId sid
) {
  return ResourceRedirectAST::Create(
    NameAST::Create(name, sid), EmptyResourceAST::Create());
}

CondFlowAST* conditional_flow(StyioAST* then_branch, StyioAST* else_branch = nullptr) {
  auto* condition = CondAST::Create(LogicType::RAW, BoolAST::Create(true));
  if (else_branch == nullptr) {
    return new CondFlowAST(
      StyioNodeType::CondFlow_True, condition, then_branch);
  }
  return new CondFlowAST(
    StyioNodeType::CondFlow_Both, condition, then_branch, else_branch);
}

TEST(StyioResourceTypestate, conditional_close_unconditional_use_rejected) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);

  std::unique_ptr<CondFlowAST> flow(conditional_flow(
    BlockAST::Create({close_handle("f", sid)}),
    BlockAST::Create({PassAST::Create()})));
  ASSERT_NO_THROW(flow->typeInfer(&analyzer));
  std::unique_ptr<NameAST> use(NameAST::Create("f", sid));
  try {
    use->typeInfer(&analyzer);
    FAIL() << "expected a maybe-closed handle use to fail";
  }
  catch (const StyioTypeError& error) {
    EXPECT_EQ(
      std::string(error.what()),
      "\nStyio.TypeError:\n"
      "use-after-destroy: resource `f` was already destroyed");
  }
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().branch_snapshot_count, 2u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().join_count, 1u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().fact_insertion_count, 1u);
  EXPECT_EQ(
    analyzer.resource_typestate_dataflow_stats().peak_temporary_fact_slots,
    1u);
  EXPECT_EQ(analyzer.resource_typestate_temporary_fact_slots_, 0u);
}

TEST(StyioResourceTypestate, both_branches_close_rejected_after_join) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);

  std::unique_ptr<CondFlowAST> flow(conditional_flow(
    BlockAST::Create({close_handle("f", sid)}),
    BlockAST::Create({close_handle("f", sid)})));
  ASSERT_NO_THROW(flow->typeInfer(&analyzer));
  std::unique_ptr<NameAST> use(NameAST::Create("f", sid));
  EXPECT_THROW(use->typeInfer(&analyzer), StyioTypeError);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().fact_insertion_count, 2u);
  EXPECT_EQ(analyzer.resource_typestate_temporary_fact_slots_, 0u);
}

TEST(StyioResourceTypestate, else_only_close_participates_in_union) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);

  std::unique_ptr<CondFlowAST> flow(conditional_flow(
    BlockAST::Create({PassAST::Create()}),
    BlockAST::Create({close_handle("f", sid)})));
  ASSERT_NO_THROW(flow->typeInfer(&analyzer));
  EXPECT_TRUE(analyzer.is_consumed_resource_name(sid, "f"));
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().branch_snapshot_count, 2u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().join_count, 1u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().fact_insertion_count, 2u);
  EXPECT_EQ(analyzer.resource_typestate_temporary_fact_slots_, 0u);
}

TEST(StyioResourceTypestate, both_branches_open_allow_use) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);

  std::unique_ptr<CondFlowAST> flow(conditional_flow(
    BlockAST::Create({PassAST::Create()}),
    BlockAST::Create({PassAST::Create()})));
  ASSERT_NO_THROW(flow->typeInfer(&analyzer));
  std::unique_ptr<NameAST> use(NameAST::Create("f", sid));
  EXPECT_NO_THROW(use->typeInfer(&analyzer));
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().fact_insertion_count, 0u);
  EXPECT_EQ(
    analyzer.resource_typestate_dataflow_stats().peak_temporary_fact_slots,
    0u);
  EXPECT_EQ(analyzer.resource_typestate_temporary_fact_slots_, 0u);
}

TEST(StyioResourceTypestate, missing_else_is_incoming_identity) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);

  std::unique_ptr<CondFlowAST> flow(conditional_flow(
    BlockAST::Create({close_handle("f", sid)})));
  ASSERT_NO_THROW(flow->typeInfer(&analyzer));
  EXPECT_TRUE(analyzer.is_consumed_resource_name(sid, "f"));
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().branch_snapshot_count, 1u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().join_count, 1u);
}

TEST(StyioResourceTypestate, nested_else_starts_from_incoming_snapshot) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);

  std::unique_ptr<CondFlowAST> flow(conditional_flow(
    conditional_flow(
      BlockAST::Create({close_handle("f", sid)}),
      BlockAST::Create({PassAST::Create()})),
    BlockAST::Create({close_handle("f", sid)})));
  ASSERT_NO_THROW(flow->typeInfer(&analyzer));
  EXPECT_TRUE(analyzer.is_consumed_resource_name(sid, "f"));
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().branch_snapshot_count, 4u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().join_count, 2u);
}

TEST(StyioResourceTypestate, symbol_id_is_authoritative_for_shadowed_names) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto outer_sid = symbols.intern("outer_f");
  const auto shadow_sid = symbols.intern("shadow_f");

  analyzer.record_consumed_resource_name("f", outer_sid);
  EXPECT_TRUE(analyzer.is_consumed_resource_name(outer_sid, "f"));
  EXPECT_FALSE(analyzer.is_consumed_resource_name(shadow_sid, "f"));
  EXPECT_FALSE(analyzer.consumed_resource_names_.contains("f"));

  ExposedTypeInferLowerer fallback_analyzer;
  fallback_analyzer.record_consumed_resource_name(
    "uninterned_f", styio::session::kInvalidSymbolId);
  EXPECT_TRUE(fallback_analyzer.is_consumed_resource_name(
    styio::session::kInvalidSymbolId, "uninterned_f"));
}

TEST(StyioResourceTypestate, rebind_erase_reopens_joined_handle) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable types;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(types, symbols);
  const auto sid = symbols.intern("f");
  install_file_handle(analyzer, "f", sid);
  analyzer.record_consumed_resource_name("f", sid);

  analyzer.erase_consumed_resource_name("f", sid);
  std::unique_ptr<NameAST> use(NameAST::Create("f", sid));
  EXPECT_NO_THROW(use->typeInfer(&analyzer));
}

TEST(StyioResourceTypestate, main_block_starts_a_fresh_counter_epoch) {
  ExposedTypeInferLowerer analyzer;
  std::unique_ptr<MainBlockAST> first(MainBlockAST::Create({
    conditional_flow(
      BlockAST::Create({PassAST::Create()}),
      BlockAST::Create({PassAST::Create()})),
  }));
  ASSERT_NO_THROW(first->typeInfer(&analyzer));
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().branch_snapshot_count, 2u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().join_count, 1u);

  std::unique_ptr<MainBlockAST> second(MainBlockAST::Create({PassAST::Create()}));
  ASSERT_NO_THROW(second->typeInfer(&analyzer));
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().branch_snapshot_count, 0u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().join_count, 0u);
  EXPECT_EQ(analyzer.resource_typestate_dataflow_stats().fact_insertion_count, 0u);
  EXPECT_EQ(
    analyzer.resource_typestate_dataflow_stats().peak_temporary_fact_slots,
    0u);
  EXPECT_EQ(analyzer.resource_typestate_temporary_fact_slots_, 0u);
}

TEST(StyioSemaCallableConstraint, WorklistBoundsReverseChainAndQuiescence) {
  constexpr std::size_t constraint_count = 256;
  CallableTypeUnifier chain_unifier;
  std::vector<CallableTypeTerm> variables;
  variables.reserve(constraint_count + 1);
  for (std::size_t index = 0; index <= constraint_count; ++index) {
    variables.push_back(chain_unifier.fresh());
  }
  CallableTypeTerm seed = callable_concrete_term(kI64Type);
  for (std::size_t index = 0; index < constraint_count; ++index) {
    seed = callable_list_term(std::move(seed));
  }
  chain_unifier.unify(variables.front(), seed, "reverse-chain seed");

  std::vector<CallableTypeConstraint> chain;
  chain.reserve(constraint_count);
  for (std::size_t origin = 0; origin < constraint_count; ++origin) {
    const std::size_t dependency = constraint_count - origin - 1;
    CallableTypeConstraint constraint;
    constraint.kind = CallableConstraintKind::Iterable;
    constraint.subject = variables[dependency];
    constraint.result = variables[dependency + 1];
    chain.push_back(std::move(constraint));
  }
  const auto chain_stats =
    reduce_callable_constraints(chain_unifier, chain);
  EXPECT_TRUE(chain.empty());
  EXPECT_EQ(chain_stats.attempt_count, 2 * constraint_count - 1);
  EXPECT_EQ(chain_stats.requeue_count, constraint_count - 1);
  EXPECT_EQ(
    chain_stats.attempt_count,
    chain_stats.input_constraint_count + chain_stats.requeue_count);
  EXPECT_LE(chain_stats.peak_frontier_count, constraint_count);
  EXPECT_LE(chain_stats.peak_blocked_count, constraint_count);
  EXPECT_LE(chain_stats.peak_live_waiter_count, constraint_count);
  EXPECT_LE(
    chain_stats.peak_scheduler_storage_slots,
    3 * constraint_count + 2 * (constraint_count + 1));

  CallableTypeUnifier blocked_unifier;
  std::vector<CallableTypeConstraint> blocked;
  blocked.reserve(constraint_count);
  for (std::size_t origin = 0; origin < constraint_count; ++origin) {
    CallableTypeConstraint constraint;
    constraint.kind = CallableConstraintKind::Numeric;
    constraint.subject = blocked_unifier.fresh();
    blocked.push_back(std::move(constraint));
  }
  const auto blocked_stats =
    reduce_callable_constraints(blocked_unifier, blocked);
  ASSERT_EQ(blocked.size(), constraint_count);
  EXPECT_EQ(blocked_stats.input_constraint_count, constraint_count);
  EXPECT_EQ(blocked_stats.attempt_count, constraint_count);
  EXPECT_EQ(blocked_stats.requeue_count, 0u);
  EXPECT_EQ(
    blocked_stats.attempt_count,
    blocked_stats.input_constraint_count + blocked_stats.requeue_count);
  EXPECT_LE(blocked_stats.peak_frontier_count, constraint_count);
  EXPECT_EQ(blocked_stats.peak_blocked_count, constraint_count);
  EXPECT_EQ(blocked_stats.peak_live_waiter_count, constraint_count);
  EXPECT_LE(
    blocked_stats.peak_scheduler_storage_slots,
    5 * constraint_count);
  for (std::size_t origin = 0; origin < constraint_count; ++origin) {
    ASSERT_EQ(blocked[origin].subject.kind, CallableTypeTerm::Kind::Variable);
    EXPECT_EQ(blocked[origin].subject.variable, origin);
  }
}

TEST(StyioSemaCallableConstraint, WorklistPreservesOriginDiagnosticPriority) {
  CallableTypeUnifier unifier;
  const CallableTypeTerm x = unifier.fresh();
  const CallableTypeTerm y = unifier.fresh();
  std::vector<CallableTypeConstraint> constraints(4);
  constraints[0].kind = CallableConstraintKind::Numeric;
  constraints[0].subject = x;
  constraints[1].kind = CallableConstraintKind::Numeric;
  constraints[1].subject = y;
  constraints[2].kind = CallableConstraintKind::Iterable;
  constraints[2].subject = callable_list_term(
    callable_concrete_term(styio_data_type_from_name("string")));
  constraints[2].result = y;
  constraints[3].kind = CallableConstraintKind::Iterable;
  constraints[3].subject = callable_list_term(
    callable_concrete_term(styio_data_type_from_name("bool")));
  constraints[3].result = x;

  try {
    (void)reduce_callable_constraints(unifier, constraints);
    FAIL() << "expected the lower-origin numeric constraint to fail";
  }
  catch (const StyioTypeError& error) {
    EXPECT_EQ(
      std::string(error.what()),
      "\nStyio.TypeError:\n"
      "callable constraint `numeric(bool)` is not satisfied by type `bool`");
  }
}

TEST(StyioSemaCallableConstraint, WorklistQueuesStrictFanoutOnlyOnce) {
  CallableTypeScheme scheme;
  scheme.name = "strict_fanout";
  scheme.quantified_variables = {0};
  CallableTypeConstraint numeric;
  numeric.kind = CallableConstraintKind::Numeric;
  numeric.subject = callable_variable_term(0);
  scheme.constraints.push_back(numeric);
  CallableTypeConstraint comparable = numeric;
  comparable.kind = CallableConstraintKind::Comparable;
  scheme.constraints.push_back(comparable);
  scheme.constraints.push_back(numeric);
  CallableTypeConstraint binder;
  binder.kind = CallableConstraintKind::Iterable;
  binder.subject = callable_list_term(callable_concrete_term(kI64Type));
  binder.result = callable_variable_term(0);
  scheme.constraints.push_back(binder);
  scheme.constraints.push_back(binder);

  std::unordered_map<std::uint32_t, StyioDataType> bindings;
  const auto stats = solve_callable_constraint_instance(scheme, bindings);
  EXPECT_EQ(stats.input_constraint_count, 5u);
  EXPECT_EQ(stats.attempt_count, 8u);
  EXPECT_EQ(stats.requeue_count, 3u);
  EXPECT_EQ(stats.strict_binding_event_count, 1u);
  EXPECT_EQ(stats.attempt_count,
            stats.input_constraint_count + stats.requeue_count);
}

TEST(StyioSemaCallableConstraint, WorklistDeduplicatesSameStepRefinement) {
  std::unordered_map<std::uint32_t, StyioDataType> bindings;
  const CallableTypeTerm variable = callable_variable_term(0);
  const StyioDataType plain = styio_make_list_type("i64");
  StyioDataType richer = plain;
  richer.state = StyioTypeState::Ready;
  const auto result = run_callable_constraint_worklist(
    2,
    1,
    [&](std::size_t id, CallableBindingDelta& binding_delta)
    {
      if (id == 0) {
        return bindings.count(0) != 0;
      }
      const std::size_t delta_storage = binding_delta.storage_slots();
      EXPECT_TRUE(match_callable_term_to_concrete(
        variable, plain, bindings, "plain fact", &binding_delta));
      EXPECT_TRUE(match_callable_term_to_concrete(
        variable, richer, bindings, "richer fact", &binding_delta));
      EXPECT_EQ(binding_delta.storage_slots(), delta_storage);
      return true;
    },
    [](std::size_t)
    {
      return std::optional<std::uint32_t>(0);
    },
    [](CallableBindingDelta&)
    {
      return std::size_t{0};
    });
  ASSERT_EQ(bindings.count(0), 1u);
  EXPECT_TRUE(bindings.at(0).equals(richer));
  EXPECT_TRUE(result.residual_origins.empty());
  EXPECT_EQ(result.stats.input_constraint_count, 2u);
  EXPECT_EQ(result.stats.attempt_count, 3u);
  EXPECT_EQ(result.stats.requeue_count, 1u);
  EXPECT_EQ(result.stats.strict_binding_event_count, 1u);
  EXPECT_LE(result.stats.peak_scheduler_storage_slots, 8u);
}

TEST(StyioSemaCallableConstraint, WorklistDefaultsOnceAndRetainsRichBindings) {
  CallableTypeScheme numeric;
  numeric.name = "numeric_default";
  numeric.quantified_variables = {0};
  CallableTypeConstraint numeric_constraint;
  numeric_constraint.kind = CallableConstraintKind::Numeric;
  numeric_constraint.subject = callable_variable_term(0);
  numeric.constraints.push_back(numeric_constraint);
  std::unordered_map<std::uint32_t, StyioDataType> numeric_bindings;
  const auto numeric_stats =
    solve_callable_constraint_instance(numeric, numeric_bindings);
  ASSERT_EQ(numeric_bindings.count(0), 1u);
  EXPECT_TRUE(numeric_bindings.at(0).equals(kI64Type));
  EXPECT_EQ(numeric_stats.defaulted_variable_count, 1u);
  EXPECT_EQ(numeric_stats.attempt_count, 2u);
  EXPECT_EQ(numeric_stats.requeue_count, 1u);

  CallableTypeScheme mixed = numeric;
  mixed.name = "mixed_default";
  CallableTypeConstraint comparable = numeric_constraint;
  comparable.kind = CallableConstraintKind::Comparable;
  mixed.constraints.push_back(comparable);
  std::unordered_map<std::uint32_t, StyioDataType> mixed_bindings;
  try {
    (void)solve_callable_constraint_instance(mixed, mixed_bindings);
    FAIL() << "expected mixed constraints to remain underconstrained";
  }
  catch (const StyioTypeError& error) {
    EXPECT_EQ(
      std::string(error.what()),
      "\nStyio.TypeError:\n"
      "call to `mixed_default` is underconstrained at `numeric('0)`; "
      "add a concrete surrounding annotation");
  }

  CallableTypeScheme cloneable;
  cloneable.name = "rich_cloneable";
  cloneable.quantified_variables = {0};
  CallableTypeConstraint clone_constraint;
  clone_constraint.kind = CallableConstraintKind::Cloneable;
  clone_constraint.subject = callable_variable_term(0);
  cloneable.constraints.push_back(clone_constraint);
  StyioDataType rich = styio_make_list_type("i64");
  rich.capabilities |= styio_caps(StyioTypeCapability::Cloneable);
  std::unordered_map<std::uint32_t, StyioDataType> rich_bindings{{0, rich}};
  EXPECT_NO_THROW(
    (void)solve_callable_constraint_instance(cloneable, rich_bindings));
  EXPECT_TRUE(rich_bindings.at(0).equals(rich));

  const CallableTypeTerm variable = callable_variable_term(0);
  const StyioDataType plain = styio_make_list_type("i64");
  StyioDataType richer = plain;
  richer.state = StyioTypeState::Ready;
  std::unordered_map<std::uint32_t, StyioDataType> representation_bindings;
  EXPECT_TRUE(match_callable_term_to_concrete(
    variable, plain, representation_bindings, "plain fact"));
  EXPECT_TRUE(match_callable_term_to_concrete(
    variable, richer, representation_bindings, "richer fact"));
  EXPECT_TRUE(representation_bindings.at(0).equals(richer));
  StyioDataType incompatible = richer;
  incompatible.state = StyioTypeState::Done;
  EXPECT_THROW(
    (void)match_callable_term_to_concrete(
      variable,
      incompatible,
      representation_bindings,
      "incompatible rich fact"),
    StyioTypeError);
}

TEST(StyioSemaCallableConstraint, WorklistDoesNotTouchBindingFacts) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);
  const std::vector<StyioDataType> representative_types{
    styio_make_list_type("i64"),
    styio_make_dict_type("string", "i64"),
    styio_make_matrix_type("f64", 2, 3),
    styio_make_task_type("i64"),
    styio_make_file_handle_type("string"),
    styio_make_std_stream_type(StdStreamKind::Stdout),
  };
  for (std::size_t index = 0;
       index < representative_types.size();
       ++index) {
    StyioSemaContext::BindingInfo info;
    info.final_slot = index % 2 == 0;
    info.dynamic_slot = index % 3 == 0;
    info.resource_value = index >= 3;
    info.value_kind = index == 0
                        ? StyioSemaContext::BindingValueKind::ListHandle
                        : (index == 1
                             ? StyioSemaContext::BindingValueKind::DictHandle
                             : (index == 2
                                  ? StyioSemaContext::BindingValueKind::MatrixHandle
                                  : (index == 3
                                       ? StyioSemaContext::BindingValueKind::TaskHandle
                                       : StyioSemaContext::BindingValueKind::Unknown)));
    info.declared_type = representative_types[index];
    const std::string name = "binding_fact_" + std::to_string(index);
    analyzer.binding_info_[name] = info;
    analyzer.binding_info_by_sid_[symbols.intern(name)] = info;
  }
  const auto name_facts = analyzer.binding_info_;
  const auto sid_facts = analyzer.binding_info_by_sid_;
  analyzer.callable_constraint_solver_stats_.attempt_count = 99;

  std::unique_ptr<MainBlockAST> empty(MainBlockAST::Create({}));
  ASSERT_NO_THROW(analyzer.prepare_callable_type_schemes(empty.get()));
  EXPECT_EQ(analyzer.callable_constraint_solver_stats().attempt_count, 0u);
  CallableTypeScheme discharge;
  discharge.name = "binding_fact_discharge";
  discharge.quantified_variables = {0};
  CallableTypeConstraint cloneable;
  cloneable.kind = CallableConstraintKind::Cloneable;
  cloneable.subject = callable_variable_term(0);
  discharge.constraints.push_back(cloneable);
  std::unordered_map<std::uint32_t, StyioDataType> discharge_bindings{
    {0, styio_make_list_type("i64")},
  };
  ASSERT_NO_THROW(
    (void)solve_callable_constraint_instance(discharge, discharge_bindings));
  auto expect_same = [](const auto& actual, const auto& expected)
  {
    EXPECT_EQ(actual.final_slot, expected.final_slot);
    EXPECT_EQ(actual.dynamic_slot, expected.dynamic_slot);
    EXPECT_EQ(actual.resource_value, expected.resource_value);
    EXPECT_EQ(actual.value_kind, expected.value_kind);
    EXPECT_TRUE(actual.declared_type.equals(expected.declared_type));
  };
  ASSERT_EQ(analyzer.binding_info_.size(), name_facts.size());
  for (const auto& [name, expected] : name_facts) {
    ASSERT_TRUE(analyzer.binding_info_.contains(name));
    expect_same(analyzer.binding_info_.at(name), expected);
  }
  ASSERT_EQ(analyzer.binding_info_by_sid_.size(), sid_facts.size());
  for (const auto& [sid, expected] : sid_facts) {
    ASSERT_TRUE(analyzer.binding_info_by_sid_.contains(sid));
    expect_same(analyzer.binding_info_by_sid_.at(sid), expected);
  }
}

TEST(StyioSemaCallableConstraint, WorklistRetainsSccRelationsAndAstIsolation) {
  ExposedTypeInferLowerer analyzer;
  std::unique_ptr<MainBlockAST> first(MainBlockAST::Create({
    SimpleFuncAST::Create(
      NameAST::Create("identity"),
      true,
      {ParamAST::Create(NameAST::Create("value"))},
      NameAST::Create("value")),
    SimpleFuncAST::Create(
      NameAST::Create("echo"),
      true,
      {ParamAST::Create(NameAST::Create("value"))},
      FuncCallAST::Create(
        NameAST::Create("echo"),
        {NameAST::Create("value")})),
    SimpleFuncAST::Create(
      NameAST::Create("bounce_left"),
      true,
      {ParamAST::Create(NameAST::Create("value"))},
      FuncCallAST::Create(
        NameAST::Create("bounce_right"),
        {NameAST::Create("value")})),
    SimpleFuncAST::Create(
      NameAST::Create("bounce_right"),
      true,
      {ParamAST::Create(NameAST::Create("value"))},
      FuncCallAST::Create(
        NameAST::Create("bounce_left"),
        {NameAST::Create("value")})),
  }));
  ASSERT_NO_THROW(analyzer.prepare_callable_type_schemes(first.get()));

  const auto* identity = analyzer.find_callable_type_scheme("identity");
  const auto* echo = analyzer.find_callable_type_scheme("echo");
  const auto* bounce_left =
    analyzer.find_callable_type_scheme("bounce_left");
  const auto* bounce_right =
    analyzer.find_callable_type_scheme("bounce_right");
  ASSERT_NE(identity, nullptr);
  ASSERT_NE(echo, nullptr);
  ASSERT_NE(bounce_left, nullptr);
  ASSERT_NE(bounce_right, nullptr);
  EXPECT_FALSE(identity->recursive_group);
  EXPECT_TRUE(echo->recursive_group);
  EXPECT_TRUE(bounce_left->recursive_group);
  EXPECT_TRUE(bounce_right->recursive_group);
  EXPECT_EQ(identity->quantified_variables,
            std::vector<std::uint32_t>({0}));
  EXPECT_EQ(echo->quantified_variables,
            std::vector<std::uint32_t>({0, 1}));
  EXPECT_EQ(bounce_left->quantified_variables,
            echo->quantified_variables);
  EXPECT_EQ(bounce_right->quantified_variables,
            echo->quantified_variables);
  EXPECT_EQ(
    identity->canonical_relation,
    "forall '0. ('0)->'0 using usage('0:{consume,shared_borrow})");
  EXPECT_EQ(
    echo->canonical_relation,
    "forall '0,'1. ('0)->'1 using usage('0:{shared_borrow})");
  EXPECT_EQ(bounce_left->canonical_relation, echo->canonical_relation);
  EXPECT_EQ(bounce_right->canonical_relation, echo->canonical_relation);
  EXPECT_EQ(analyzer.callable_constraint_solver_stats().run_count, 3u);
  EXPECT_EQ(
    analyzer.callable_constraint_solver_stats().input_constraint_count,
    0u);

  std::unique_ptr<MainBlockAST> second(MainBlockAST::Create({
    SimpleFuncAST::Create(
      NameAST::Create("second_identity"),
      true,
      {ParamAST::Create(NameAST::Create("next"))},
      NameAST::Create("next")),
  }));
  ASSERT_NO_THROW(analyzer.prepare_callable_type_schemes(second.get()));
  EXPECT_EQ(analyzer.find_callable_type_scheme("identity"), nullptr);
  ASSERT_NE(analyzer.find_callable_type_scheme("second_identity"), nullptr);
  EXPECT_EQ(analyzer.callable_constraint_solver_stats().run_count, 1u);
  EXPECT_EQ(
    analyzer.callable_constraint_solver_stats().input_constraint_count,
    0u);
  EXPECT_EQ(analyzer.callable_constraint_solver_stats().attempt_count, 0u);
}

TEST(StyioTypeInferInternal, MatrixLiteralAndReturnHelpersCoverAnonymousBranches) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");
  const StyioDataType matrix_f64_1x2 = styio_make_matrix_type("f64", 1, 2);

  {
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("takes_arg"),
      false,
      {ParamAST::Create(NameAST::Create("value"), TypeAST::Create("i64"))},
      TypeAST::Create("i64"),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("value"))})));
    EXPECT_EQ(params_of_func_def(fn.get()).size(), 1u);
  }

  EXPECT_EQ(merge_matrix_elem_types(undefined_type(), i64).name, "i64");
  EXPECT_EQ(merge_matrix_elem_types(i64, undefined_type()).name, "i64");
  EXPECT_EQ(merge_matrix_elem_types(i64, f64).name, "f64");
  EXPECT_EQ(
    merge_matrix_elem_types(StyioDataType{StyioDataTypeOption::Integer, "int", 32}, i64).name,
    "i64");
  EXPECT_THROW((void)merge_matrix_elem_types(i64, string_type), StyioTypeError);

  {
    std::unique_ptr<StyioAST> scalar(IntAST::Create("1"));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, scalar.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> empty(ListAST::Create({}));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, empty.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> bad_row(ListAST::Create({IntAST::Create("1")}));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, bad_row.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> empty_row(ListAST::Create({ListAST::Create({})}));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, empty_row.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> ragged(ListAST::Create({
      ListAST::Create({IntAST::Create("1")}),
      ListAST::Create({IntAST::Create("2"), IntAST::Create("3")})
    }));
    EXPECT_THROW((void)infer_matrix_literal_info(&analyzer, ragged.get()), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> mixed(ListAST::Create({
      ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
    }));
    MatrixLiteralInfo info = infer_matrix_literal_info(&analyzer, mixed.get());
    EXPECT_EQ(info.elem_type.name, "f64");
    EXPECT_EQ(info.rows, 1u);
    EXPECT_EQ(info.cols, 2u);
  }
  {
    std::unique_ptr<ListAST> empty(ListAST::Create({}));
    EXPECT_TRUE(infer_list_literal_type(&analyzer, empty.get()).isUndefined());
  }
  {
    std::unique_ptr<ListAST> already_matrix(ListAST::Create({IntAST::Create("1")}));
    already_matrix->setDataType(styio_make_matrix_type("i64", 1, 1));
    expect_matrix_type(infer_list_literal_type(&analyzer, already_matrix.get()), "i64", 1, 1);
  }

  EXPECT_TRUE(apply_matrix_literal_context(&analyzer, nullptr, matrix_f64_1x2).isUndefined());
  {
    std::unique_ptr<StyioAST> scalar(IntAST::Create("1"));
    EXPECT_TRUE(apply_matrix_literal_context(&analyzer, scalar.get(), matrix_f64_1x2).isUndefined());
    EXPECT_TRUE(apply_matrix_literal_context(&analyzer, scalar.get(), i64).isUndefined());
  }
  {
    std::unique_ptr<StyioAST> ret(ReturnAST::Create(ListAST::Create({
      ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})
    })));
    expect_matrix_type(apply_matrix_literal_context(&analyzer, ret.get(), matrix_f64_1x2), "f64", 1, 2);
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({
      ListAST::Create({ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")})})
    }));
    block->set_followings({
      ListAST::Create({ListAST::Create({IntAST::Create("3"), IntAST::Create("4")})})
    });
    expect_matrix_type(apply_matrix_literal_context(&analyzer, block.get(), matrix_f64_1x2), "i64", 1, 2);
  }

  EXPECT_NO_THROW(require_matrix_return_compatible_latest("plain", i64, i64));
  EXPECT_NO_THROW(require_matrix_return_compatible_latest("matrix_ok", matrix_f64_1x2, matrix_f64_1x2));
  EXPECT_THROW(require_matrix_return_compatible_latest("matrix_bad", matrix_f64_1x2, i64), StyioTypeError);
}

TEST(StyioTypeInferInternal, FlowAndMatchMergeHelpersCoverAnonymousBranches) {
  const StyioDataType undefined = undefined_type();
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");
  const StyioDataType list_i64 = styio_make_list_type("i64");

  EXPECT_EQ(merge_cond_flow_branch_type(undefined, undefined, i64).name, "i64");
  EXPECT_TRUE(merge_cond_flow_branch_type(undefined, undefined, undefined).isUndefined());
  EXPECT_EQ(merge_cond_flow_branch_type(undefined, f64, undefined).name, "f64");
  EXPECT_EQ(merge_cond_flow_branch_type(i64, undefined, undefined).name, "i64");
  EXPECT_EQ(merge_cond_flow_branch_type(i64, f64, undefined).name, "f64");
  EXPECT_TRUE(merge_cond_flow_branch_type(string_type, list_i64, undefined).isUndefined());

  EXPECT_EQ(merge_match_value_type(undefined, string_type).name, "string");
  EXPECT_EQ(merge_match_value_type(i64, undefined).name, "i64");
  EXPECT_EQ(merge_match_value_type(string_type, i64).name, "string");
  EXPECT_EQ(merge_match_value_type(i64, f64).name, "f64");
  {
    styio::session::SymbolInterner symbols;
    styio::session::TypeTable type_table;
    AstToStyioIRLowerer analyzer;
    analyzer.attach_type_table(type_table, symbols);
    EXPECT_EQ(merge_match_value_type(i64, i64, &analyzer).name, "i64");
    EXPECT_EQ(type_table.size(), 1u);
    EXPECT_EQ(merge_match_value_type(string_type, string_type, &analyzer).name, "string");
    EXPECT_EQ(type_table.size(), 2u);
  }
  EXPECT_EQ(
    merge_match_value_type(
      styio_data_type_from_name("bool"),
      StyioDataType{StyioDataTypeOption::Char, "char", 8}).name,
    "i64");
  EXPECT_TRUE(merge_match_value_type(list_i64, styio_make_dict_type("string", "i64")).isUndefined());

  EXPECT_TRUE(match_result_type_supported(undefined));
  EXPECT_TRUE(match_result_type_supported(i64));
  EXPECT_TRUE(match_result_type_supported(string_type));
  EXPECT_FALSE(match_result_type_supported(list_i64));

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<CondFlowAST> then_only_binding(new CondFlowAST(
      StyioNodeType::CondFlow_Both,
      CondAST::Create(LogicType::RAW, BoolAST::Create(true)),
      BlockAST::Create({
        FinalBindAST::Create(VarAST::Create(NameAST::Create("then_only")), IntAST::Create("1"))
      }),
      BlockAST::Create({PassAST::Create()})));
    EXPECT_NO_THROW(then_only_binding->typeInfer(&analyzer));
    EXPECT_EQ(analyzer.local_binding_types.find("then_only"), analyzer.local_binding_types.end());
  }
}

TEST(StyioTypeInferInternal, CollectionListOperationAndTaskPullEdgesStayExplicit) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType bool_type = styio_data_type_from_name("bool");

  analyzer.local_binding_types["items"] = styio_make_list_type("bool");
  {
    std::unique_ptr<StyioAST> name(NameAST::Create("items"));
    EXPECT_EQ(infer_collection_elem_type(&analyzer, name.get()).name, "bool");
  }
  {
    std::unique_ptr<StyioAST> literal(ListAST::Create({IntAST::Create("1"), FloatAST::Create("2.5")}));
    EXPECT_EQ(infer_collection_elem_type(&analyzer, literal.get()).name, "i64");
  }
  {
    std::unique_ptr<StyioAST> scalar(StringAST::Create("plain"));
    EXPECT_EQ(infer_collection_elem_type(&analyzer, scalar.get()).name, "i64");
  }

  EXPECT_TRUE(type_is_bool(bool_type));
  EXPECT_FALSE(type_is_bool(i64));
  EXPECT_TRUE(type_is_text_serializable_iterable(styio_make_list_type("i64")));
  EXPECT_TRUE(type_is_text_serializable_iterable(styio_make_dict_type("string", "f64")));
  EXPECT_TRUE(type_is_text_serializable_iterable(styio_make_range_type("i64")));
  EXPECT_FALSE(type_is_text_serializable_iterable(styio_make_dict_type("i64", "f64")));
  EXPECT_FALSE(type_is_text_serializable_iterable(styio_make_list_type(styio_make_file_handle_type("i64").name)));

  {
    analyzer.local_binding_types["not_list"] = i64;
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("not_list"),
      NameAST::Create("push"),
      {IntAST::Create("1")}));
    EXPECT_THROW(call->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["file_handles"] =
      styio_make_list_type(styio_make_file_handle_type("i64").name);
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("file_handles"),
      NameAST::Create("push"),
      {ResourceReceiverAST::Create("file")}));
    EXPECT_THROW(call->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["ints"] = styio_make_list_type("i64");
    std::unique_ptr<FuncCallAST> wrong_count(FuncCallAST::Create(
      NameAST::Create("ints"),
      NameAST::Create("push"),
      {}));
    EXPECT_THROW(wrong_count->typeInfer(&analyzer), StyioTypeError);

    std::unique_ptr<FuncCallAST> wrong_value(FuncCallAST::Create(
      NameAST::Create("ints"),
      NameAST::Create("push"),
      {StringAST::Create("bad")}));
    EXPECT_THROW(wrong_value->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["task_string"] = styio_make_task_type("string");
    std::unique_ptr<FlowBindAST> bad_target(FlowBindAST::CreateAwait(
      NameAST::Create("task_string"),
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64"))));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, AttachedTypeTableInternsBindingTypesThroughSema) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  AstToStyioIRLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  {
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("count")),
      IntAST::Create("1")));
    ASSERT_NO_THROW(bind->typeInfer(&analyzer));
  }

  ASSERT_TRUE(analyzer.local_binding_types.contains("count"));
  const auto count_id = type_table.intern(analyzer.local_binding_types.at("count"), symbols);
  EXPECT_NE(count_id, styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.size(), 1u);

  {
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("other_count")),
      IntAST::Create("2")));
    ASSERT_NO_THROW(bind->typeInfer(&analyzer));
  }

  ASSERT_TRUE(analyzer.local_binding_types.contains("other_count"));
  EXPECT_EQ(type_table.intern(analyzer.local_binding_types.at("other_count"), symbols), count_id);
  EXPECT_EQ(type_table.size(), 1u);

  styio::session::SymbolInterner compare_symbols;
  styio::session::TypeTable compare_table;
  AstToStyioIRLowerer compare_analyzer;
  compare_analyzer.attach_type_table(compare_table, compare_symbols);

  std::unique_ptr<StyioAST> literal(IntAST::Create("7"));
  const StyioDataType literal_type = infer_expr_type(&compare_analyzer, literal.get());
  EXPECT_EQ(compare_table.size(), 0u);

  EXPECT_TRUE(func_param_accepts_arg(literal_type, literal_type, &compare_analyzer));
  const auto literal_id = compare_table.intern(literal_type, compare_symbols);
  EXPECT_NE(literal_id, styio::session::kInvalidTypeId);
  EXPECT_EQ(compare_table.size(), 1u);

  std::unique_ptr<ListAST> homogeneous(ListAST::Create({
    IntAST::Create("1"),
    IntAST::Create("2"),
  }));
  EXPECT_EQ(infer_list_literal_type(&compare_analyzer, homogeneous.get()).name, "list[i64]");
  EXPECT_EQ(compare_table.intern(literal_type, compare_symbols), literal_id);
  EXPECT_EQ(compare_table.size(), 1u);

  styio::session::SymbolInterner tuple_symbols;
  styio::session::TypeTable tuple_table;
  AstToStyioIRLowerer tuple_analyzer;
  tuple_analyzer.attach_type_table(tuple_table, tuple_symbols);

  {
    auto* first = TypeAST::Create("i64");
    auto* second = TypeAST::Create("i64");
    std::unique_ptr<TupleAST> tuple(TupleAST::Create({first, second}));
    EXPECT_EQ(tuple_table.size(), 0u);
    tuple->typeInfer(&tuple_analyzer);
    EXPECT_FALSE(tuple->isConsistent());
    ASSERT_TRUE(styio_is_shaped_tuple_type(tuple->getDataType()));
    EXPECT_EQ(tuple->getDataType().name, "(i64,i64)");
    EXPECT_EQ(tuple->getDataType().tuple_elements->size(), 2u);
    EXPECT_EQ(tuple_table.size(), 1u);
  }

  {
    auto* first = TypeAST::Create("i64");
    auto* second = TypeAST::Create("f64");
    std::unique_ptr<TupleAST> tuple(TupleAST::Create({first, second}));
    tuple->typeInfer(&tuple_analyzer);
    EXPECT_FALSE(tuple->isConsistent());
    ASSERT_TRUE(styio_is_shaped_tuple_type(tuple->getDataType()));
    EXPECT_EQ(tuple->getDataType().name, "(i64,f64)");
    EXPECT_EQ(tuple_table.size(), 2u);
  }

  styio::session::SymbolInterner assignable_symbols;
  styio::session::TypeTable assignable_table;
  AstToStyioIRLowerer assignable_analyzer;
  assignable_analyzer.attach_type_table(assignable_table, assignable_symbols);
  const StyioDataType list_i64 = styio_make_list_type("i64");
  EXPECT_TRUE(container_value_assignable(list_i64, list_i64, &assignable_analyzer));
  const auto list_id = assignable_table.intern(list_i64, assignable_symbols);
  EXPECT_NE(list_id, styio::session::kInvalidTypeId);
  EXPECT_EQ(assignable_table.size(), 1u);

  styio::session::SymbolInterner method_symbols;
  styio::session::TypeTable method_table;
  AstToStyioIRLowerer method_analyzer;
  method_analyzer.attach_type_table(method_table, method_symbols);
  {
    std::unique_ptr<ResourceMethodDefAST> method(ResourceMethodDefAST::Create(
      "file",
      "typed_echo",
      false,
      false,
      {ParamAST::Create(NameAST::Create("value"), TypeAST::Create("i64"))},
      ReturnAST::Create(StringAST::Create("ok"))));
    EXPECT_EQ(method_table.size(), 0u);
    ASSERT_NO_THROW(method->typeInfer(&method_analyzer));
    EXPECT_EQ(method_table.size(), 2u);
    EXPECT_NE(method_table.intern(styio_data_type_from_name("i64"), method_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_NE(method_table.intern(styio_data_type_from_name("string"), method_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_EQ(method_table.size(), 2u);
  }

  styio::session::SymbolInterner function_symbols;
  styio::session::TypeTable function_table;
  AstToStyioIRLowerer function_analyzer;
  function_analyzer.attach_type_table(function_table, function_symbols);
  {
    std::unique_ptr<FunctionAST> function(FunctionAST::Create(
      NameAST::Create("typed_function_decl"),
      false,
      {
        ParamAST::Create(NameAST::Create("value"), TypeAST::Create("i64")),
        ParamAST::Create(NameAST::Create("items"), TypeAST::Create(styio_make_list_type("string"))),
      },
      TypeAST::Create(styio_make_matrix_type("f64", 1, 2)),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})));
    EXPECT_EQ(function_table.size(), 0u);
    ASSERT_NO_THROW(function->typeInfer(&function_analyzer));
    EXPECT_NE(function_table.intern(styio_data_type_from_name("i64"), function_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_NE(function_table.intern(styio_make_list_type("string"), function_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_NE(function_table.intern(styio_make_matrix_type("f64", 1, 2), function_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_EQ(function_table.size(), 3u);
  }
  {
    std::unique_ptr<SimpleFuncAST> simple(SimpleFuncAST::Create(
      NameAST::Create("typed_simple_decl"),
      {
        ParamAST::Create(NameAST::Create("text"), TypeAST::Create("string")),
      },
      TypeAST::Create("f64"),
      FloatAST::Create("1.0")));
    ASSERT_NO_THROW(simple->typeInfer(&function_analyzer));
    EXPECT_NE(function_table.intern(styio_data_type_from_name("string"), function_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_NE(function_table.intern(styio_data_type_from_name("f64"), function_symbols),
              styio::session::kInvalidTypeId);
    EXPECT_EQ(function_table.size(), 5u);
  }
}

TEST(StyioTypeInferInternal, AttachedTypeTableInternsContainerLiteralTypesThroughSema) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  AstToStyioIRLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  {
    std::unique_ptr<ListAST> untyped_list(ListAST::Create({}));
    EXPECT_THROW(untyped_list->typeInfer(&analyzer), StyioTypeError);
  }
  std::unique_ptr<ListAST> list(ListAST::Create({}));
  list->setDataType(styio_make_list_type("i64"));
  ASSERT_NO_THROW(list->typeInfer(&analyzer));
  EXPECT_EQ(type_table.size(), 1u);
  const auto list_id = type_table.intern(list->getDataType(), symbols);
  EXPECT_NE(list_id, styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.resolve(list_id).option, StyioDataTypeOption::List);
  EXPECT_EQ(type_table.size(), 1u);

  {
    std::unique_ptr<DictAST> untyped_dict(DictAST::Create());
    EXPECT_THROW(untyped_dict->typeInfer(&analyzer), StyioTypeError);
  }
  std::unique_ptr<DictAST> dict(DictAST::Create());
  dict->setDataType(styio_make_dict_type("string", "i64"));
  ASSERT_NO_THROW(dict->typeInfer(&analyzer));
  EXPECT_EQ(type_table.size(), 2u);
  const auto dict_id = type_table.intern(dict->getDataType(), symbols);
  EXPECT_NE(dict_id, styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.resolve(dict_id).option, StyioDataTypeOption::Dict);
  EXPECT_EQ(type_table.size(), 2u);

  std::unique_ptr<ListAST> same_list(ListAST::Create({}));
  same_list->setDataType(styio_make_list_type("i64"));
  ASSERT_NO_THROW(same_list->typeInfer(&analyzer));
  EXPECT_EQ(type_table.intern(same_list->getDataType(), symbols), list_id);
  EXPECT_EQ(type_table.size(), 2u);
}

TEST(StyioTypeInferInternal, AttachedTypeTableInternsSemaContextSideMapWrites) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  AstToStyioIRLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType bool_type = styio_data_type_from_name("bool");
  const StyioDataType string_type = styio_data_type_from_name("string");
  const StyioDataType list_string = styio_make_list_type("string");
  const StyioDataType matrix_f64 = styio_make_matrix_type("f64", 1, 2);

  EXPECT_EQ(type_table.size(), 0u);

  const auto local_sid = symbols.intern("side_local");
  analyzer.record_local_binding_type("side_local", local_sid, i64);
  EXPECT_EQ(type_table.intern(i64, symbols), styio::session::TypeId{1});
  EXPECT_EQ(type_table.size(), 1u);

  const auto resource_sid = symbols.intern("side_resource");
  analyzer.record_resource_binding_type("side_resource", resource_sid, list_string);
  EXPECT_NE(type_table.intern(list_string, symbols), styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.size(), 2u);

  const auto method_local_sid = symbols.intern("side_method_local");
  analyzer.record_resource_method_dynamic_local_binding_type(
    "side_method_local",
    method_local_sid,
    matrix_f64);
  EXPECT_NE(type_table.intern(matrix_f64, symbols), styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.size(), 3u);

  const auto info_sid = symbols.intern("side_info");
  StyioSemaContext::BindingInfo info;
  info.declared_type = f64;
  analyzer.record_binding_info("side_info", info_sid, info);
  EXPECT_NE(type_table.intern(f64, symbols), styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.size(), 4u);

  const auto native_sid = symbols.intern("side_native");
  StyioSemaContext::NativeFunctionType native_type;
  native_type.return_type = string_type;
  native_type.arg_types = {bool_type, list_string};
  analyzer.record_native_function_def("side_native", native_sid, native_type);
  EXPECT_NE(type_table.intern(string_type, symbols), styio::session::kInvalidTypeId);
  EXPECT_NE(type_table.intern(bool_type, symbols), styio::session::kInvalidTypeId);
  EXPECT_EQ(type_table.size(), 6u);
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesFunctionDefinitionsBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const auto fn_sid = symbols.intern("by_sid");
  const auto param_sid = symbols.intern("value");
  std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
    NameAST::Create("by_sid", fn_sid),
    false,
    {ParamAST::Create(NameAST::Create("value", param_sid), TypeAST::Create("i64"))},
    TypeAST::Create("i64"),
    BlockAST::Create({ReturnAST::Create(NameAST::Create("value", param_sid))})));

  ASSERT_NO_THROW(fn->typeInfer(&analyzer));
  ASSERT_NE(analyzer.find_function_def(fn_sid, "by_sid"), nullptr);
  analyzer.func_defs.clear();

  std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
    NameAST::Create("by_sid", fn_sid),
    {IntAST::Create("41")}));
  EXPECT_EQ(infer_expr_type(&analyzer, call.get()).name, "i64");
  EXPECT_NO_THROW(call->typeInfer(&analyzer));
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesNativeFunctionDefinitionsBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const auto native_sid = symbols.intern("native_by_sid");
  StyioSemaContext::NativeFunctionType native_type;
  native_type.return_type = styio_data_type_from_name("i64");
  native_type.arg_types.push_back(styio_data_type_from_name("i64"));
  analyzer.record_native_function_def("native_by_sid", native_sid, native_type);
  ASSERT_TRUE(analyzer.native_func_defs_by_sid.contains(native_sid));

  analyzer.native_func_defs.clear();
  std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
    NameAST::Create("native_by_sid", native_sid),
    {IntAST::Create("41")}));
  EXPECT_EQ(infer_expr_type(&analyzer, call.get()).name, "i64");
  EXPECT_NO_THROW(call->typeInfer(&analyzer));

  std::unique_ptr<FuncCallAST> wrong_arity(FuncCallAST::Create(
    NameAST::Create("native_by_sid", native_sid),
    {}));
  EXPECT_THROW(wrong_arity->typeInfer(&analyzer), StyioTypeError);
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesLocalBindingTypesBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const auto local_sid = symbols.intern("by_sid_local");
  std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
    VarAST::Create(NameAST::Create("by_sid_local", local_sid), TypeAST::Create("i64")),
    IntAST::Create("7")));

  ASSERT_NO_THROW(bind->typeInfer(&analyzer));
  const StyioDataType* recorded =
    analyzer.find_local_binding_type(local_sid, "by_sid_local");
  ASSERT_NE(recorded, nullptr);
  EXPECT_EQ(recorded->name, "i64");

  analyzer.local_binding_types.clear();
  std::unique_ptr<NameAST> sid_read(NameAST::Create("by_sid_local", local_sid));
  EXPECT_EQ(infer_expr_type(&analyzer, sid_read.get()).name, "i64");

  std::unique_ptr<NameAST> spelling_read(NameAST::Create("by_sid_local"));
  EXPECT_EQ(infer_expr_type(&analyzer, spelling_read.get()).name, "i64");

  const auto numeric_sid = symbols.intern("by_sid_numeric");
  analyzer.record_local_binding_type(
    "by_sid_numeric",
    numeric_sid,
    styio_data_type_from_name("f64"));
  analyzer.local_binding_types.clear();
  std::unique_ptr<BinOpAST> self_add(BinOpAST::Create(
    StyioOpType::Self_Add_Assign,
    NameAST::Create("by_sid_numeric", numeric_sid),
    IntAST::Create("1")));
  EXPECT_NO_THROW(self_add->typeInfer(&analyzer));
  EXPECT_EQ(self_add->getType().name, "f64");

  const auto lhs_sid = symbols.intern("by_sid_lhs");
  const auto rhs_sid = symbols.intern("by_sid_rhs");
  analyzer.record_local_binding_type("by_sid_lhs", lhs_sid, styio_data_type_from_name("f64"));
  analyzer.record_local_binding_type("by_sid_rhs", rhs_sid, styio_data_type_from_name("i64"));
  analyzer.local_binding_types.clear();
  std::unique_ptr<BinOpAST> sum(BinOpAST::Create(
    StyioOpType::Binary_Add,
    NameAST::Create("by_sid_lhs", lhs_sid),
    NameAST::Create("by_sid_rhs", rhs_sid)));
  EXPECT_NO_THROW(sum->typeInfer(&analyzer));
  EXPECT_EQ(sum->getType().name, "f64");

  const auto stdin_sid = symbols.intern("by_sid_stdin_source");
  const auto collect_sid = symbols.intern("by_sid_stdin_collect");
  analyzer.record_local_binding_type(
    "by_sid_stdin_source",
    stdin_sid,
    styio_make_std_stream_type(StdStreamKind::Stdin, "string"));
  analyzer.local_binding_types.clear();
  std::unique_ptr<HandleAcquireAST> collect(HandleAcquireAST::Create(
    VarAST::Create(NameAST::Create("by_sid_stdin_collect", collect_sid)),
    NameAST::Create("by_sid_stdin_source", stdin_sid),
    HandleAcquireAST::BindMode::Flex));
  EXPECT_NO_THROW(collect->typeInfer(&analyzer));
  const StyioDataType* collected_type =
    analyzer.find_local_binding_type(collect_sid, "by_sid_stdin_collect");
  ASSERT_NE(collected_type, nullptr);
  EXPECT_EQ(collected_type->name, "list[string]");

  const auto task_sid = symbols.intern("by_sid_task");
  const auto pull_target_sid = symbols.intern("by_sid_pull_target");
  analyzer.record_local_binding_type("by_sid_task", task_sid, styio_make_task_type("i64"));
  analyzer.record_local_binding_type(
    "by_sid_pull_target",
    pull_target_sid,
    styio_data_type_from_name("i64"));
  analyzer.local_binding_types.clear();
  std::unique_ptr<HandleAcquireAST> pull(HandleAcquireAST::Create(
    VarAST::Create(NameAST::Create("by_sid_pull_target", pull_target_sid)),
    NameAST::Create("by_sid_task", task_sid)));
  EXPECT_NO_THROW(pull->typeInfer(&analyzer));
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesFixedAssignmentsBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);
  const StyioDataType i64 = styio_data_type_from_name("i64");

  const auto fixed_sid = symbols.intern("fixed_by_sid");
  std::unique_ptr<FinalBindAST> final_bind(FinalBindAST::Create(
    VarAST::Create(NameAST::Create("fixed_by_sid", fixed_sid), TypeAST::Create("i64")),
    IntAST::Create("1")));
  ASSERT_NO_THROW(final_bind->typeInfer(&analyzer));
  EXPECT_TRUE(analyzer.fixed_assignment_names_by_sid_.contains(fixed_sid));

  analyzer.fixed_assignment_names_.clear();
  analyzer.binding_info_.clear();
  std::unique_ptr<FlexBindAST> flex_rebind(FlexBindAST::Create(
    VarAST::Create(NameAST::Create("fixed_by_sid", fixed_sid)),
    IntAST::Create("2")));
  EXPECT_THROW(flex_rebind->typeInfer(&analyzer), StyioSyntaxError);

  std::unique_ptr<ParallelAssignAST> parallel_rebind(ParallelAssignAST::Create(
    {NameAST::Create("fixed_by_sid", fixed_sid)},
    {IntAST::Create("3")}));
  EXPECT_THROW(parallel_rebind->typeInfer(&analyzer), StyioTypeError);

  const auto flow_sid = symbols.intern("fixed_flow_target");
  analyzer.record_local_binding_type("fixed_flow_target", flow_sid, i64);
  analyzer.record_fixed_assignment_name("fixed_flow_target", flow_sid);
  analyzer.local_binding_types.clear();
  analyzer.fixed_assignment_names_.clear();
  analyzer.binding_info_.clear();
  std::unique_ptr<FlowBindAST> flow_rebind(FlowBindAST::Create(
    IntAST::Create("4"),
    VarAST::Create(NameAST::Create("fixed_flow_target", flow_sid))));
  EXPECT_THROW(flow_rebind->typeInfer(&analyzer), StyioTypeError);

  const auto task_sid = symbols.intern("fixed_pull_task");
  const auto pull_target_sid = symbols.intern("fixed_pull_target");
  analyzer.record_local_binding_type("fixed_pull_task", task_sid, styio_make_task_type("i64"));
  analyzer.record_local_binding_type("fixed_pull_target", pull_target_sid, i64);
  analyzer.record_fixed_assignment_name("fixed_pull_target", pull_target_sid);
  analyzer.local_binding_types.clear();
  analyzer.fixed_assignment_names_.clear();
  analyzer.binding_info_.clear();
  std::unique_ptr<HandleAcquireAST> task_pull(HandleAcquireAST::Create(
    VarAST::Create(NameAST::Create("fixed_pull_target", pull_target_sid)),
    NameAST::Create("fixed_pull_task", task_sid)));
  EXPECT_THROW(task_pull->typeInfer(&analyzer), StyioTypeError);
}


TEST(StyioTypeInferInternal, SymbolInternerResolvesResourceBindingTypesBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);
  const StyioDataType i64 = styio_data_type_from_name("i64");

  const auto res_sid = symbols.intern("my_resource");
  analyzer.record_resource_binding_type("my_resource", res_sid, i64);
  EXPECT_TRUE(analyzer.resource_binding_types_by_sid_.contains(res_sid));
  EXPECT_EQ(analyzer.resource_binding_types_.at("my_resource").name, "i64");

  analyzer.resource_binding_types_.clear();
  const StyioDataType* by_sid = analyzer.find_resource_binding_type(res_sid, "my_resource");
  ASSERT_NE(by_sid, nullptr);
  EXPECT_EQ(by_sid->name, "i64");

  analyzer.record_resource_binding_type("string_only_resource", styio::session::kInvalidSymbolId, i64);
  EXPECT_TRUE(analyzer.resource_binding_types_.contains("string_only_resource"));

  analyzer.resource_binding_types_.clear();
  analyzer.resource_binding_types_by_sid_.clear();
  EXPECT_EQ(analyzer.find_resource_binding_type(res_sid, "my_resource"), nullptr);

  analyzer.record_resource_binding_type("infer_target", res_sid, i64);
  std::unique_ptr<ResourceRefAST> ref(ResourceRefAST::Create(
    NameAST::Create("infer_target", res_sid)));
  ASSERT_NO_THROW(ref->typeInfer(&analyzer));
  EXPECT_EQ(ref->getDataType().name, "i64");

  analyzer.resource_binding_types_.clear();
  analyzer.resource_binding_types_by_sid_.clear();
  analyzer.resource_binding_types_["string_resource"] = i64;
  std::unique_ptr<ResourceRefAST> str_ref(ResourceRefAST::Create(
    NameAST::Create("string_resource")));
  ASSERT_NO_THROW(str_ref->typeInfer(&analyzer));
  EXPECT_EQ(str_ref->getDataType().name, "i64");
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesSnapshotVarsBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const auto snapshot_sid = symbols.intern("snapshot_by_sid");
  std::unique_ptr<SnapshotDeclAST> snapshot(SnapshotDeclAST::Create(
    NameAST::Create("snapshot_by_sid", snapshot_sid),
    FileResourceAST::Create(StringAST::Create("data.txt"), false)));

  ASSERT_NO_THROW(snapshot->typeInfer(&analyzer));
  EXPECT_TRUE(analyzer.snapshot_var_names_by_sid_.contains(snapshot_sid));
  EXPECT_TRUE(analyzer.is_snapshot_var("snapshot_by_sid"));

  analyzer.snapshot_var_names_.clear();
  EXPECT_TRUE(analyzer.is_snapshot_var("snapshot_by_sid"));
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesConsumedTasksBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);
  const StyioDataType i64 = styio_data_type_from_name("i64");

  const auto task_sid = symbols.intern("consumed_task_by_sid");
  const auto pull_target_sid = symbols.intern("consumed_pull_target");
  analyzer.record_local_binding_type("consumed_task_by_sid", task_sid, styio_make_task_type("i64"));
  analyzer.record_local_binding_type("consumed_pull_target", pull_target_sid, i64);
  analyzer.record_consumed_task_name("consumed_task_by_sid", task_sid);
  EXPECT_TRUE(analyzer.consumed_task_names_by_sid_.contains(task_sid));

  analyzer.local_binding_types.clear();
  analyzer.consumed_task_names_.clear();
  std::unique_ptr<HandleAcquireAST> repeated_pull(HandleAcquireAST::Create(
    VarAST::Create(NameAST::Create("consumed_pull_target", pull_target_sid)),
    NameAST::Create("consumed_task_by_sid", task_sid)));
  EXPECT_THROW(repeated_pull->typeInfer(&analyzer), StyioTypeError);

  const auto await_task_sid = symbols.intern("await_task_by_sid");
  const auto await_target_sid = symbols.intern("await_target_by_sid");
  analyzer.record_local_binding_type("await_task_by_sid", await_task_sid, styio_make_task_type("i64"));
  analyzer.record_consumed_task_name("await_task_by_sid", await_task_sid);
  analyzer.local_binding_types.clear();
  analyzer.consumed_task_names_.clear();
  std::unique_ptr<FlowBindAST> repeated_await(FlowBindAST::CreateAwait(
    NameAST::Create("await_task_by_sid", await_task_sid),
    VarAST::Create(NameAST::Create("await_target_by_sid", await_target_sid), TypeAST::Create("i64"))));
  EXPECT_THROW(repeated_await->typeInfer(&analyzer), StyioTypeError);

  analyzer.consumed_task_names_by_sid_.clear();
  analyzer.consumed_task_names_.insert("string_consumed_task");
  std::unique_ptr<FlowBindAST> string_fallback(FlowBindAST::CreateAwait(
    NameAST::Create("string_consumed_task"),
    VarAST::Create(NameAST::Create("string_consumed_target"), TypeAST::Create("i64"))));
  analyzer.local_binding_types["string_consumed_task"] = styio_make_task_type("i64");
  EXPECT_THROW(string_fallback->typeInfer(&analyzer), StyioTypeError);
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesConsumedResourcesBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const auto resource_sid = symbols.intern("destroyed_resource_by_sid");
  const StyioDataType file_handle = styio_make_file_handle_type("i64");
  StyioSemaContext::BindingInfo resource_info;
  resource_info.resource_value = true;
  resource_info.value_kind = StyioSemaContext::BindingValueKind::I64;
  resource_info.declared_type = file_handle;
  analyzer.record_binding_info("destroyed_resource_by_sid", resource_sid, resource_info);
  analyzer.record_local_binding_type("destroyed_resource_by_sid", resource_sid, file_handle);
  analyzer.binding_info_.clear();
  analyzer.local_binding_types.clear();

  std::unique_ptr<ResourceRedirectAST> destroy(ResourceRedirectAST::Create(
    NameAST::Create("destroyed_resource_by_sid", resource_sid),
    EmptyResourceAST::Create()));
  ASSERT_NO_THROW(destroy->typeInfer(&analyzer));
  EXPECT_TRUE(analyzer.consumed_resource_names_by_sid_.contains(resource_sid));

  analyzer.consumed_resource_names_.clear();
  std::unique_ptr<NameAST> use_after_destroy(NameAST::Create(
    "destroyed_resource_by_sid",
    resource_sid));
  EXPECT_THROW(use_after_destroy->typeInfer(&analyzer), StyioTypeError);

  analyzer.consumed_resource_names_by_sid_.clear();
  analyzer.consumed_resource_names_.insert("string_destroyed_resource");
  std::unique_ptr<NameAST> string_use_after_destroy(NameAST::Create("string_destroyed_resource"));
  EXPECT_THROW(string_use_after_destroy->typeInfer(&analyzer), StyioTypeError);
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesOwnedResourcesBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);

  const auto resource_sid = symbols.intern("outer_resource_by_sid");
  StyioSemaContext::BindingInfo resource_info;
  resource_info.resource_value = true;
  resource_info.value_kind = StyioSemaContext::BindingValueKind::I64;
  resource_info.declared_type = styio_make_file_handle_type("i64");
  analyzer.record_binding_info("outer_resource_by_sid", resource_sid, resource_info);
  analyzer.record_owned_resource_name("outer_resource_by_sid", resource_sid);
  EXPECT_TRUE(analyzer.owned_resource_names_by_sid_.contains(resource_sid));

  analyzer.binding_info_.clear();
  analyzer.owned_resource_names_.clear();
  std::unique_ptr<TaskBlockAST> task(TaskBlockAST::Create(BlockAST::Create({
    ResourceRedirectAST::Create(
      NameAST::Create("outer_resource_by_sid", resource_sid),
      EmptyResourceAST::Create())
  })));
  EXPECT_THROW((*task).typeInfer(&analyzer), StyioTypeError);
  EXPECT_TRUE(analyzer.task_outer_resource_names_stack_.empty());
  EXPECT_TRUE(analyzer.task_outer_resource_names_by_sid_stack_.empty());

  analyzer.task_outer_resource_names_stack_.clear();
  analyzer.task_outer_resource_names_by_sid_stack_.clear();
  analyzer.owned_resource_names_by_sid_.clear();
  analyzer.owned_resource_names_.insert("string_outer_resource");
  analyzer.binding_info_["string_outer_resource"] = resource_info;
  std::unique_ptr<TaskBlockAST> string_task(TaskBlockAST::Create(BlockAST::Create({
    ResourceRedirectAST::Create(
      NameAST::Create("string_outer_resource"),
      EmptyResourceAST::Create())
  })));
  EXPECT_THROW((*string_task).typeInfer(&analyzer), StyioTypeError);
  EXPECT_TRUE(analyzer.task_outer_resource_names_stack_.empty());
  EXPECT_TRUE(analyzer.task_outer_resource_names_by_sid_stack_.empty());
}

TEST(StyioTypeInferInternal, SymbolInternerResolvesBindingInfoBySymbolId) {
  styio::session::SymbolInterner symbols;
  styio::session::TypeTable type_table;
  ExposedTypeInferLowerer analyzer;
  analyzer.attach_type_table(type_table, symbols);
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType list_i64 = styio_make_list_type("i64");

  const auto resource_sid = symbols.intern("binding_info_resource");
  StyioSemaContext::BindingInfo resource_info;
  resource_info.resource_value = true;
  resource_info.value_kind = StyioSemaContext::BindingValueKind::ListHandle;
  resource_info.declared_type = list_i64;
  analyzer.record_binding_info("binding_info_resource", resource_sid, resource_info);
  analyzer.record_local_binding_type("binding_info_resource", resource_sid, list_i64);
  ASSERT_TRUE(analyzer.binding_info_by_sid_.contains(resource_sid));

  analyzer.binding_info_.clear();
  analyzer.local_binding_types.clear();
  std::unique_ptr<FinalBindAST> illegal_copy(FinalBindAST::Create(
    VarAST::Create(NameAST::Create("copied_resource")),
    NameAST::Create("binding_info_resource", resource_sid)));
  EXPECT_THROW(illegal_copy->typeInfer(&analyzer), StyioTypeError);

  const auto final_sid = symbols.intern("binding_info_final_slot");
  StyioSemaContext::BindingInfo final_info;
  final_info.final_slot = true;
  final_info.declared_type = i64;
  final_info.value_kind = StyioSemaContext::BindingValueKind::I64;
  analyzer.record_binding_info("binding_info_final_slot", final_sid, final_info);
  analyzer.binding_info_.clear();
  std::unique_ptr<ParallelAssignAST> final_parallel(ParallelAssignAST::Create(
    {NameAST::Create("binding_info_final_slot", final_sid)},
    {IntAST::Create("1")}));
  EXPECT_THROW(final_parallel->typeInfer(&analyzer), StyioTypeError);

  const auto task_sid = symbols.intern("binding_info_task");
  const auto target_sid = symbols.intern("binding_info_task_target");
  StyioSemaContext::BindingInfo target_info;
  target_info.declared_type = i64;
  target_info.value_kind = StyioSemaContext::BindingValueKind::I64;
  analyzer.record_binding_info("binding_info_task_target", target_sid, target_info);
  analyzer.record_local_binding_type("binding_info_task", task_sid, styio_make_task_type("i64"));
  analyzer.binding_info_.clear();
  analyzer.local_binding_types.clear();
  std::unique_ptr<HandleAcquireAST> task_pull(HandleAcquireAST::Create(
    VarAST::Create(NameAST::Create("binding_info_task_target", target_sid)),
    NameAST::Create("binding_info_task", task_sid)));
  EXPECT_NO_THROW(task_pull->typeInfer(&analyzer));
}

TEST(StyioTypeInferInternal, BinOpFlowBindAndIterSeqBranchesStayExplicit) {
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["lhs"] = styio_data_type_from_name("i64");
    auto* nested_rhs = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      FloatAST::Create("2.5"));
    nested_rhs->setDType(styio_data_type_from_name("f64"));
    std::unique_ptr<StyioAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Add,
      NameAST::Create("lhs"),
      nested_rhs));
    expr->typeInfer(&analyzer);
    EXPECT_EQ(static_cast<BinOpAST*>(expr.get())->getType().name, "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<BinOpAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Add,
      FloatAST::Create("1.25"),
      IntAST::Create("2")));
    expr->typeInfer(&analyzer);
    EXPECT_EQ(expr->getType().name, "f64");
  }
  {
    AstToStyioIRLowerer analyzer;
    auto* lhs = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      IntAST::Create("2"));
    auto* rhs = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("3"),
      FloatAST::Create("4.5"));
    std::unique_ptr<BinOpAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Mul,
      lhs,
      rhs));
    expr->typeInfer(&analyzer);
    EXPECT_EQ(expr->getType().name, "f64");
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["task"] = styio_make_task_type("i64");
    std::unique_ptr<FlowBindAST> await_ok(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("out")),
      IntAST::Create("0")
    ));
    await_ok->typeInfer(&analyzer);
    EXPECT_EQ(await_ok->getResultType().name, "i64");
    EXPECT_EQ(analyzer.local_binding_types["out"].name, "i64");
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["task"] = styio_make_task_type("i64");
    std::unique_ptr<FlowBindAST> bad_fallback(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("out"), TypeAST::Create("i64")),
      StringAST::Create("bad")
    ));
    EXPECT_THROW(bad_fallback->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["slot"] = styio_data_type_from_name("i64");
    std::unique_ptr<FlowBindAST> bad_target(FlowBindAST::Create(
      StringAST::Create("value"),
      VarAST::Create(NameAST::Create("slot"))
    ));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<StyioAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"route"})}
    ));
    EXPECT_THROW(iter->typeInfer(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<IterSeqAST> iter(IterSeqAST::Create(
      ListAST::Create({IntAST::Create("1")}),
      {HashTagNameAST::Create({"route"})}
    ));
    EXPECT_THROW(analyzer.typeInfer(iter.get()), StyioTypeError);
  }

  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["scalar"] = styio_data_type_from_name("i64");
    std::unique_ptr<ParallelAssignAST> bad_target(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("scalar"), IntAST::Create("0"))},
      {IntAST::Create("1")}));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    analyzer.local_binding_types["handles"] =
      styio_make_list_type(styio_make_file_handle_type("i64").name);
    std::unique_ptr<ParallelAssignAST> bad_elem(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("handles"), IntAST::Create("0"))},
      {FileResourceAST::Create(StringAST::Create("input.txt"), false)}));
    EXPECT_THROW(bad_elem->typeInfer(&analyzer), StyioTypeError);
  }
  {
    AstToStyioIRLowerer analyzer;
    std::unique_ptr<TupleAST> tuple(TupleAST::Create({NameAST::Create("unknown"), IntAST::Create("1")}));
    EXPECT_THROW(tuple->typeInfer(&analyzer), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, MatrixIntrinsicDictAndResourceHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType matrix_i64_2x3 = styio_make_matrix_type("i64", 2, 3);
  const StyioDataType matrix_f64_3x2 = styio_make_matrix_type("f64", 3, 2);

  analyzer.local_binding_types["mi23"] = matrix_i64_2x3;
  analyzer.local_binding_types["mf32"] = matrix_f64_3x2;
  analyzer.local_binding_types["scalar"] = i64;
  analyzer.local_binding_types["text"] = styio_data_type_from_name("string");

  auto infer_call = [&](FuncCallAST* call)
  {
    std::unique_ptr<FuncCallAST> owner(call);
    return infer_matrix_intrinsic_type(&analyzer, owner.get());
  };

  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_zeros"), {IntAST::Create("2"), IntAST::Create("3")})),
    "f64",
    2,
    3);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_zeros_i64"), {IntAST::Create("2"), IntAST::Create("3")})),
    "i64",
    2,
    3);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_identity"), {IntAST::Create("4")})),
    "f64",
    4,
    4);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_identity_i64"), {IntAST::Create("4")})),
    "i64",
    4,
    4);
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_rows"), {NameAST::Create("mi23")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_cols"), {NameAST::Create("mi23")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_shape"), {NameAST::Create("mi23")})).name,
    "list[i64]");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_get"),
      {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_set"),
      {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1"), IntAST::Create("9")})).name,
    "i64");
  expect_matrix_type(
    infer_call(FuncCallAST::Create(NameAST::Create("transpose"), {NameAST::Create("mi23")})),
    "i64",
    3,
    2);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_add"),
      {NameAST::Create("mi23"), NameAST::Create("mi23")})),
    "i64",
    2,
    3);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_hadamard"),
      {NameAST::Create("mi23"), NameAST::Create("mi23")})),
    "i64",
    2,
    3);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(
      NameAST::Create("mat_scale"),
      {NameAST::Create("mi23"), FloatAST::Create("2.5")})),
    "f64",
    2,
    3);
  expect_matrix_type(
    infer_call(FuncCallAST::Create(
      NameAST::Create("matmul"),
      {NameAST::Create("mi23"), NameAST::Create("mf32")})),
    "f64",
    2,
    2);
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("dot"), {NameAST::Create("mi23"), NameAST::Create("mi23")})).name,
    "i64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("norm"), {NameAST::Create("mi23")})).name,
    "f64");
  EXPECT_EQ(
    infer_call(FuncCallAST::Create(NameAST::Create("mat_sum"), {NameAST::Create("mi23")})).name,
    "i64");
  EXPECT_TRUE(
    infer_call(FuncCallAST::Create(NameAST::Create("not_matrix_intrinsic"), {})).isUndefined());
  EXPECT_TRUE(
    infer_matrix_intrinsic_type(&analyzer, nullptr).isUndefined());
  {
    std::unique_ptr<FuncCallAST> method_call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("rows"),
      {NameAST::Create("mi23")}));
    EXPECT_TRUE(infer_matrix_intrinsic_type(&analyzer, method_call.get()).isUndefined());
  }
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_zeros"), {IntAST::Create("2")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_zeros"), {StringAST::Create("bad"), IntAST::Create("2")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_rows"), {NameAST::Create("scalar")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(
      NameAST::Create("mat_set"),
      {NameAST::Create("mi23"), IntAST::Create("0"), IntAST::Create("1"), StringAST::Create("bad")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("mat_scale"), {NameAST::Create("mi23"), NameAST::Create("text")})),
    StyioTypeError);
  EXPECT_THROW(
    (void)infer_call(FuncCallAST::Create(NameAST::Create("matmul"), {NameAST::Create("mi23"), NameAST::Create("mi23")})),
    StyioTypeError);

  EXPECT_EQ(merge_numeric_elem_type(i64, i64).name, "i64");
  EXPECT_THROW((void)merge_numeric_elem_type(i64, styio_data_type_from_name("string")), StyioTypeError);
  expect_matrix_type(matrix_binary_result(matrix_i64_2x3, i64, StyioOpType::Binary_Mul), "i64", 2, 3);
  expect_matrix_type(matrix_binary_result(matrix_i64_2x3, matrix_i64_2x3, StyioOpType::Binary_Add), "i64", 2, 3);
  expect_matrix_type(matrix_binary_result(matrix_i64_2x3, matrix_f64_3x2, StyioOpType::Binary_Mul), "f64", 2, 2);
  EXPECT_THROW((void)matrix_binary_result(matrix_i64_2x3, matrix_f64_3x2, StyioOpType::Binary_Add), StyioTypeError);
  EXPECT_THROW((void)matrix_binary_result(matrix_i64_2x3, styio_data_type_from_name("string"), StyioOpType::Binary_Mul), StyioTypeError);
  EXPECT_THROW((void)matrix_binary_result(matrix_i64_2x3, i64, StyioOpType::Binary_Add), StyioTypeError);
  EXPECT_THROW((void)matrix_binary_result(matrix_i64_2x3, matrix_i64_2x3, StyioOpType::Binary_Div), StyioTypeError);
  EXPECT_TRUE(matrix_binary_result(i64, f64, StyioOpType::Binary_Add).isUndefined());

  {
    std::unique_ptr<DictAST> empty(DictAST::Create());
    EXPECT_TRUE(infer_dict_literal_type(&analyzer, empty.get()).isUndefined());
  }
  {
    std::unique_ptr<DictAST> mixed(DictAST::Create({
      {StringAST::Create("a"), IntAST::Create("1")},
      {StringAST::Create("b"), FloatAST::Create("2.5")},
    }));
    EXPECT_EQ(infer_dict_literal_type(&analyzer, mixed.get()).name, "dict[string,f64]");
  }
  {
    std::unique_ptr<DictAST> bad_key(DictAST::Create({
      {IntAST::Create("1"), IntAST::Create("2")},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, bad_key.get()), StyioTypeError);
  }
  {
    std::unique_ptr<DictAST> bad_value(DictAST::Create({
      {StringAST::Create("resource"), ResourceReceiverAST::Create("file")},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, bad_value.get()), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, FunctionReturnAndReceiverScanHelpersStayExplicit) {
  AstToStyioIRLowerer analyzer;
  const StyioDataType matrix_f64_1x2 = styio_make_matrix_type("f64", 1, 2);

  {
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("typed"),
      false,
      {},
      TypeAST::Create("f64"),
      BlockAST::Create({ReturnAST::Create(FloatAST::Create("1.25"))})
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "f64");
  }
  {
    analyzer.push_active_function_body("matrix_fn");
    analyzer.record_inferred_function_return_type(matrix_f64_1x2);
    analyzer.pop_active_function_body();
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("matrix_fn"),
      false,
      {},
      TypeAST::Create(styio_make_matrix_type("f64")),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("m"))})
    ));
    expect_matrix_type(func_ret_type_of_def(&analyzer, fn.get()), "f64", 1, 2);
  }
  {
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_typed"),
      {},
      TypeAST::Create("bool"),
      BoolAST::Create(true)
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "bool");
  }
  {
    analyzer.push_active_function_body("simple_inferred");
    analyzer.record_inferred_function_return_type(styio_data_type_from_name("string"));
    analyzer.pop_active_function_body();
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_inferred"),
      {},
      BoolAST::Create(true)
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "string");
  }
  {
    analyzer.push_active_function_body("simple_matrix");
    analyzer.record_inferred_function_return_type(matrix_f64_1x2);
    analyzer.pop_active_function_body();
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_matrix"),
      {},
      TypeAST::Create(styio_make_matrix_type("f64")),
      NameAST::Create("matrix_value")
    ));
    expect_matrix_type(func_ret_type_of_def(&analyzer, fn.get()), "f64", 1, 2);
  }
  {
    styio::session::SymbolInterner symbols;
    styio::session::TypeTable type_table;
    ExposedTypeInferLowerer sid_analyzer;
    sid_analyzer.attach_type_table(type_table, symbols);
    const auto matrix_sid = symbols.intern("matrix_return_by_sid");
    sid_analyzer.push_active_function_body("matrix_return_by_sid", matrix_sid);
    sid_analyzer.record_inferred_function_return_type(matrix_f64_1x2);
    sid_analyzer.pop_active_function_body();
    ASSERT_TRUE(sid_analyzer.inferred_function_return_types_by_sid_.contains(matrix_sid));
    sid_analyzer.inferred_function_return_types_.clear();

    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("matrix_return_by_sid", matrix_sid),
      false,
      {},
      TypeAST::Create(styio_make_matrix_type("f64")),
      BlockAST::Create({ReturnAST::Create(NameAST::Create("m"))})
    ));
    expect_matrix_type(func_ret_type_of_def(&sid_analyzer, fn.get()), "f64", 1, 2);

    const auto simple_sid = symbols.intern("simple_return_by_sid");
    sid_analyzer.push_active_function_body("simple_return_by_sid", simple_sid);
    sid_analyzer.record_inferred_function_return_type(styio_data_type_from_name("string"));
    sid_analyzer.pop_active_function_body();
    sid_analyzer.inferred_function_return_types_.clear();
    std::unique_ptr<SimpleFuncAST> simple(SimpleFuncAST::Create(
      NameAST::Create("simple_return_by_sid", simple_sid),
      {},
      BoolAST::Create(true)
    ));
    EXPECT_EQ(func_ret_type_of_def(&sid_analyzer, simple.get()).name, "string");
  }
  {
    std::unique_ptr<SimpleFuncAST> fn(SimpleFuncAST::Create(
      NameAST::Create("simple_tail"),
      {},
      IntAST::Create("7")
    ));
    EXPECT_EQ(func_ret_type_of_def(&analyzer, fn.get()).name, "i64");
  }
  EXPECT_TRUE(func_ret_type_of_def(&analyzer, IntAST::Create("1")).isUndefined());

  EXPECT_FALSE(body_consumes_receiver(&analyzer, nullptr, "file"));
  EXPECT_TRUE(match_tail_value_expected(ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_TRUE(match_tail_value_expected(AttrAST::Create(NameAST::Create("obj"), NameAST::Create("field"))));
  EXPECT_FALSE(match_tail_value_expected(PassAST::Create()));
  EXPECT_FALSE(static_i64_literal(IntAST::Create("999999999999999999999999")).has_value());
  EXPECT_EQ(
    binding_value_kind_for_type(styio_make_range_type("i64")),
    StyioSemaContext::BindingValueKind::ListHandle);
  EXPECT_EQ(
    binding_value_kind_for_type(styio_make_task_type("i64")),
    StyioSemaContext::BindingValueKind::TaskHandle);
  EXPECT_FALSE(resource_method_statement_preface_supported_latest(nullptr));
  EXPECT_TRUE(resource_method_statement_preface_supported_latest(PassAST::Create()));
  EXPECT_TRUE(resource_method_value_preface_shape_supported_latest(
    FlexBindAST::Create(VarAST::Create(NameAST::Create("x")), IntAST::Create("1"))));
  EXPECT_TRUE(resource_method_value_preface_supported_latest(
    &analyzer,
    FlexBindAST::Create(VarAST::Create(NameAST::Create("flex_pref")), IntAST::Create("1"))));
  EXPECT_FALSE(resource_method_value_preface_shape_supported_latest(ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_FALSE(resource_method_value_preface_supported_latest(&analyzer, ReturnAST::Create(IntAST::Create("1"))));
  EXPECT_TRUE(resource_method_scalar_value_type_supported_latest(styio_data_type_from_name("char")));
  EXPECT_TRUE(resource_method_local_container_type_supported_latest(styio_make_matrix_type("i64", 1, 1)));
  EXPECT_FALSE(resource_method_local_value_type_supported_latest(styio_make_file_handle_type("i64")));
  {
    auto* bind = FlexBindAST::Create(
      VarAST::Create(NameAST::Create("flex_local")),
      FloatAST::Create("1.25"));
    EXPECT_EQ(resource_method_preface_bind_type_latest(&analyzer, bind).name, "f64");
    bind_resource_method_preface_type_latest(&analyzer, bind);
    EXPECT_EQ(analyzer.local_binding_types["flex_local"].name, "f64");
    delete bind;
  }
  {
    auto* bind = FinalBindAST::Create(
      VarAST::Create(NameAST::Create("pref"), TypeAST::Create("i64")),
      IntAST::Create("1"));
    EXPECT_EQ(resource_method_preface_bind_type_latest(&analyzer, bind).name, "i64");
    bind_resource_method_preface_type_latest(&analyzer, bind);
    EXPECT_EQ(analyzer.local_binding_types["pref"].name, "i64");
    delete bind;
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
    block->set_followings({ReturnAST::Create(IntAST::Create("9"))});
    EXPECT_TRUE(resource_method_body_contains_return_latest(block.get()));
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({ReturnAST::Create(IntAST::Create("9"))}));
    EXPECT_TRUE(resource_method_body_contains_return_latest(block.get()));
  }
  {
    std::unique_ptr<BlockAST> body(BlockAST::Create({
      FinalBindAST::Create(VarAST::Create(NameAST::Create("local"), TypeAST::Create("i64")), IntAST::Create("1")),
      ReturnAST::Create(NameAST::Create("local"))
    }));
    EXPECT_EQ(resource_method_simple_result_type_latest(&analyzer, body.get()).name, "i64");
    EXPECT_EQ(analyzer.local_binding_types.find("local"), analyzer.local_binding_types.end());
  }
  {
    std::unique_ptr<StyioAST> redirect(ResourceRedirectAST::Create(
      ResourceReceiverAST::Create("file"),
      EmptyResourceAST::Create()
    ));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, redirect.get(), "file"));
  }
  {
    std::unique_ptr<StyioAST> nested(ResourceWriteAST::Create(
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("other"), EmptyResourceAST::Create())
    ));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, nested.get(), "file"));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, nested.get(), "other"));
    EXPECT_FALSE(body_consumes_receiver(&analyzer, nested.get(), "missing"));
  }
  {
    std::unique_ptr<StyioAST> bin(BinOpAST::Create(
      StyioOpType::Binary_Add,
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      IntAST::Create("1")));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, bin.get(), "file"));
  }
  {
    std::unique_ptr<StyioAST> attr(AttrAST::Create(
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      NameAST::Create("path")));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, attr.get(), "file"));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> drain(ResourceMethodDefAST::Create(
      "file",
      "drain",
      false,
      false,
      {},
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())
    ));
    EXPECT_NO_THROW(drain->typeInfer(&analyzer));
    std::unique_ptr<StyioAST> call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("drain"),
      {}));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, call.get(), "file"));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> nullable_param(ResourceMethodDefAST::Create(
      "file",
      "nullable",
      false,
      false,
      {nullptr},
      PassAST::Create()));
    EXPECT_NO_THROW(nullable_param->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<FunctionAST> tuple_return(FunctionAST::Create(
      NameAST::Create("tuple_return"),
      false,
      {},
      TypeTupleAST::Create({TypeAST::Create("i64"), TypeAST::Create("f64")}),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})));
    ASSERT_NO_THROW(tuple_return->typeInfer(&analyzer));
    std::unique_ptr<FuncCallAST> tuple_call(FuncCallAST::Create(
      NameAST::Create("tuple_return"), {}));
    EXPECT_THROW(tuple_call->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<StyioAST> block(BlockAST::Create({
      PassAST::Create(),
      FuncCallAST::Create(
        NameAST::Create("plain"),
        {ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())})
    }));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, block.get(), "file"));
  }
  {
    std::unique_ptr<BlockAST> block(BlockAST::Create({PassAST::Create()}));
    block->set_followings({
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())
    });
    EXPECT_TRUE(body_consumes_receiver(&analyzer, block.get(), "file"));
  }
  {
    analyzer.local_binding_types["fh"] = styio_make_file_handle_type("i64");
    std::unique_ptr<FlexBindAST> bad_handle(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("copy")),
      NameAST::Create("fh")));
    EXPECT_THROW(bad_handle->typeInfer(&analyzer), StyioTypeError);

    std::unique_ptr<FlexBindAST> bad_empty(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("empty")),
      EmptyResourceAST::Create()));
    EXPECT_THROW(bad_empty->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::vector<ResourceEffectAST::Handler> handlers;
    handlers.emplace_back("io", IntAST::Create("1"));
    std::unique_ptr<ResourceEffectAST> bad_handler(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), styio_data_type_from_name("string")),
      nullptr,
      false,
      std::move(handlers),
      true));
    EXPECT_THROW(bad_handler->typeInfer(&analyzer), StyioTypeError);
  }
}

TEST(StyioTypeInferInternal, TailDictResourceAndFunctionReturnHelperEdgesStayExplicit) {
  AstToStyioIRLowerer analyzer;

  EXPECT_TRUE(match_branch_tail_type(&analyzer, nullptr).isUndefined());
  EXPECT_TRUE(match_branch_tail_type(&analyzer, BlockAST::Create({})).isUndefined());
  EXPECT_EQ(match_branch_tail_type(&analyzer, BlockAST::Create({IntAST::Create("3")})).name, "i64");
  EXPECT_EQ(match_branch_tail_type(&analyzer, IntAST::Create("4")).name, "i64");
  EXPECT_TRUE(match_branch_tail_type(&analyzer, PassAST::Create()).isUndefined());
  EXPECT_THROW((void)match_branch_tail_type(&analyzer, NameAST::Create("missing_value")), StyioTypeError);
  EXPECT_THROW(
    (void)match_branch_tail_type(&analyzer, ReturnAST::Create(PassAST::Create())),
    StyioTypeError);

  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, nullptr).isUndefined());
  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, ReturnAST::Create(nullptr)).isUndefined());
  EXPECT_EQ(function_body_tail_type_latest(&analyzer, ReturnAST::Create(IntAST::Create("5"))).name, "i64");
  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, BlockAST::Create({})).isUndefined());
  EXPECT_EQ(function_body_tail_type_latest(&analyzer, BlockAST::Create({IntAST::Create("6")})).name, "i64");
  EXPECT_EQ(function_body_tail_type_latest(&analyzer, IntAST::Create("7")).name, "i64");
  EXPECT_TRUE(function_body_tail_type_latest(&analyzer, PassAST::Create()).isUndefined());

  {
    std::unique_ptr<DictAST> dict(DictAST::Create({
      {StringAST::Create("ok"), IntAST::Create("1")},
      {StringAST::Create("bad"), ListAST::Create({IntAST::Create("2")})},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, dict.get()), StyioTypeError);
  }
  {
    std::unique_ptr<DictAST> dict(DictAST::Create({
      {StringAST::Create("skip"), PassAST::Create()},
      {StringAST::Create("ok"), IntAST::Create("3")},
    }));
    EXPECT_EQ(infer_dict_literal_type(&analyzer, dict.get()).name, "dict[string,i64]");
  }

  EXPECT_EQ(resource_family_for_expr(&analyzer, nullptr), "");
  EXPECT_EQ(resource_family_for_expr(&analyzer, ResourceRefAST::Create(NameAST::Create("res"))), "resource");

  {
    analyzer.push_active_function_body("matrix_fn");
    analyzer.record_inferred_function_return_type(styio_make_matrix_type("f64", 2, 3));
    analyzer.pop_active_function_body();
    std::unique_ptr<FunctionAST> fn(FunctionAST::Create(
      NameAST::Create("matrix_fn"),
      false,
      {},
      TypeAST::Create(styio_make_matrix_type("f64", 1, 1)),
      BlockAST::Create({ReturnAST::Create(ListAST::Create({
        ListAST::Create({FloatAST::Create("1.0")})
      }))})));
    StyioDataType ret = func_ret_type_of_def(&analyzer, fn.get());
    expect_matrix_type(ret, "f64", 2, 3);
  }
  {
    std::unique_ptr<PassAST> pass(PassAST::Create());
    EXPECT_TRUE(func_ret_type_of_def(&analyzer, pass.get()).isUndefined());
  }
}

TEST(StyioTypeInferInternal, HelperFallbacksAndBindingEdgesStayExplicit) {
  AstToStyioIRLowerer analyzer;

  EXPECT_TRUE(params_of_func_def(IntAST::Create("1")).empty());
  EXPECT_EQ(type_convert_target_type(NumPromoTy::Bool_To_Int).name, "i64");
  EXPECT_EQ(type_convert_target_type(NumPromoTy::Int_To_Float).name, "f64");
  EXPECT_TRUE(type_convert_target_type(static_cast<NumPromoTy>(99)).isUndefined());
  EXPECT_EQ(type_convert_source_fallback_type(NumPromoTy::Bool_To_Int).name, "bool");
  EXPECT_EQ(type_convert_source_fallback_type(NumPromoTy::Int_To_Float).name, "i64");
  EXPECT_TRUE(type_convert_source_fallback_type(static_cast<NumPromoTy>(99)).isUndefined());
  EXPECT_TRUE(resource_method_preface_bind_type_latest(&analyzer, PassAST::Create()).isUndefined());
  EXPECT_FALSE(resource_method_body_contains_return_latest(nullptr));
  EXPECT_TRUE(resource_method_simple_result_type_latest(&analyzer, nullptr).isUndefined());
  EXPECT_EQ(
    resource_method_value_tail_return_latest(
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))}))->getNodeType(),
    StyioNodeType::Return);
  EXPECT_EQ(
    resource_method_value_tail_return_latest(BlockAST::Create({PassAST::Create()})),
    nullptr);
  EXPECT_EQ(
    resource_method_value_tail_return_latest(BlockAST::Create({
      ReturnAST::Create(IntAST::Create("1")),
      PassAST::Create(),
    })),
    nullptr);
  EXPECT_EQ(
    resource_method_value_tail_return_latest(BlockAST::Create({
      ReturnAST::Create(IntAST::Create("1")),
      ReturnAST::Create(IntAST::Create("2")),
    })),
    nullptr);
  {
    std::unique_ptr<BlockAST> unsupported_preface(BlockAST::Create({
      FinalBindAST::Create(
        VarAST::Create(NameAST::Create("bad_handle_local"), TypeAST::Create(styio_make_file_handle_type("i64"))),
        StringAST::Create("input.txt")),
      ReturnAST::Create(IntAST::Create("1"))
    }));
    EXPECT_TRUE(resource_method_simple_result_type_latest(&analyzer, unsupported_preface.get()).isUndefined());
  }
  EXPECT_THROW(
    (void)resource_method_simple_result_type_latest(
      &analyzer,
      BlockAST::Create({
        FinalBindAST::Create(VarAST::Create(NameAST::Create("ok")), IntAST::Create("1")),
        PassAST::Create(),
        ReturnAST::Create(TypeConvertAST::Create(IntAST::Create("bad"), NumPromoTy::Bool_To_Int)),
      })),
    StyioTypeError);

  EXPECT_TRUE(infer_expr_type(&analyzer, nullptr).isUndefined());
  EXPECT_EQ(infer_expr_type(&analyzer, CharAST::Create("c")).name, "char");
  EXPECT_EQ(infer_expr_type(&analyzer, FlowBindAST::Create(
    IntAST::Create("1"),
    VarAST::Create(NameAST::Create("flow")))).name, "undefined");
  {
    auto* match = MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, nullptr));
    match->setDataType(styio_data_type_from_name("i64"));
    std::unique_ptr<MatchCasesAST> owner(match);
    EXPECT_EQ(infer_expr_type(&analyzer, owner.get()).name, "i64");
  }
  EXPECT_TRUE(infer_expr_type(&analyzer, AttrAST::Create(
    NameAST::Create("obj"),
    IntAST::Create("1"))).isUndefined());
  EXPECT_TRUE(infer_expr_type(&analyzer, new ListOpAST(
    StyioNodeType::Access_By_Index,
    IntAST::Create("1"),
    IntAST::Create("0"))).isUndefined());
  EXPECT_TRUE(infer_expr_type(&analyzer, new ListOpAST(
    StyioNodeType::Access_By_Slice,
    IntAST::Create("1"),
    IntAST::Create("0"))).isUndefined());
  EXPECT_TRUE(infer_expr_type(&analyzer, new ListOpAST(
    StyioNodeType::Access_By_Name,
    IntAST::Create("1"),
    NameAST::Create("field"))).isUndefined());
  EXPECT_EQ(infer_expr_type(&analyzer, WaveMergeAST::Create(
    BoolAST::Create(true),
    IntAST::Create("1"),
    FloatAST::Create("2.5"))).name, "f64");

  analyzer.local_binding_types["dict"] = styio_make_dict_type("string", "string");
  EXPECT_EQ(infer_expr_type(&analyzer, AttrAST::Create(
    NameAST::Create("dict"),
    NameAST::Create("values"))).name, "list[string]");
  {
    std::unique_ptr<ResourceMethodDefAST> prop(ResourceMethodDefAST::Create(
      "file",
      "custom_prop",
      true,
      false,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(prop->typeInfer(&analyzer));
    analyzer.local_binding_types["fh2"] = styio_make_file_handle_type("i64");
    EXPECT_EQ(infer_expr_type(&analyzer, AttrAST::Create(
      NameAST::Create("fh2"),
      NameAST::Create("custom_prop"))).name, "i64");
  }
  StyioDataType stdout_handle{
    StyioDataTypeOption::Defined,
    "@stdout",
    0,
    StyioHandleFamily::Stream,
    StyioTypeState::Ready,
    0,
    "string",
    "",
    true,
    static_cast<int>(StdStreamKind::Stdout),
    StyioValueFamily::StreamHandle};
  EXPECT_EQ(resource_family_for_type(stdout_handle), "stdout");
  EXPECT_EQ(
    resource_family_for_type(styio_make_topology_resource_type(
      styio_data_type_from_name("i64"),
      StyioResourceShapeKind::Sequence)),
    "resource");
  EXPECT_FALSE(resource_effect_index_operation_supported_latest(&analyzer, nullptr));
  EXPECT_FALSE(resource_effect_index_operation_supported_latest(
    &analyzer,
    new ListOpAST(StyioNodeType::Access_By_Name, NameAST::Create("dict"), NameAST::Create("key"))));
  EXPECT_FALSE(resource_effect_iterator_operation_supported_latest(&analyzer, nullptr));
  EXPECT_FALSE(resource_effect_operation_supported_latest(&analyzer, nullptr));

  EXPECT_THROW(TypeConvertAST::Create(IntAST::Create("1"), NumPromoTy::Bool_To_Int)->typeInfer(&analyzer), StyioTypeError);
  EXPECT_THROW(TypeConvertAST::Create(FloatAST::Create("1.0"), NumPromoTy::Int_To_Float)->typeInfer(&analyzer), StyioTypeError);
  {
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("p")));
    std::unique_ptr<OptArgAST> opt_arg(OptArgAST::Create(NameAST::Create("arg")));
    std::unique_ptr<OptKwArgAST> opt_kw(OptKwArgAST::Create(NameAST::Create("kw")));
    EXPECT_NO_THROW(param->typeInfer(&analyzer));
    EXPECT_NO_THROW(opt_arg->typeInfer(&analyzer));
    EXPECT_NO_THROW(opt_kw->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("char_slot")),
      CharAST::Create("x")));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_EQ(bind->getVar()->getDType()->getDataType().name, "char");
  }
  {
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("tuple_slot")),
      TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")})));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_FALSE(bind->getVar()->getDType()->getDataType().isUndefined());
  }
  {
    analyzer.local_binding_types["matrix_value"] = styio_make_matrix_type("i64", 1, 2);
    std::unique_ptr<FlexBindAST> bind(FlexBindAST::Create(
      VarAST::Create(NameAST::Create("matrix_exact"), TypeAST::Create(styio_make_matrix_type("i64", 1, 2))),
      NameAST::Create("matrix_value")));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<BinOpAST> expr(BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      IntAST::Create("2")));
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("typed_sum"), TypeAST::Create("i64")),
      expr.release()));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_EQ(static_cast<BinOpAST*>(bind->getValue())->getType().name, "i64");
  }
  {
    auto* match = MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, nullptr));
    match->setDataType(styio_data_type_from_name("i64"));
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("match_bound")),
      match));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
    EXPECT_EQ(bind->getVar()->getDType()->getDataType().name, "i64");
  }
}

TEST(StyioTypeInferInternal, ErrorGuardsAndUnsupportedBranchesStayExplicit) {
  ExposedTypeInferLowerer analyzer;
  const StyioDataType i64 = styio_data_type_from_name("i64");
  const StyioDataType f64 = styio_data_type_from_name("f64");
  const StyioDataType string_type = styio_data_type_from_name("string");

  {
    std::unique_ptr<ListAST> first_undefined(ListAST::Create({PassAST::Create(), IntAST::Create("1")}));
    EXPECT_EQ(infer_list_literal_type(&analyzer, first_undefined.get()).name, "list[i64]");
    std::unique_ptr<ListAST> next_undefined(ListAST::Create({IntAST::Create("1"), PassAST::Create()}));
    EXPECT_EQ(infer_list_literal_type(&analyzer, next_undefined.get()).name, "list[i64]");
  }
  {
    std::unique_ptr<FunctionAST> tuple_return(FunctionAST::Create(
      NameAST::Create("tuple_decl"),
      false,
      {},
      TypeTupleAST::Create({TypeAST::Create("i64")}),
      BlockAST::Create({ReturnAST::Create(IntAST::Create("1"))})));
    const StyioDataType declared =
      declared_function_return_type_latest(tuple_return.get());
    ASSERT_TRUE(styio_is_shaped_tuple_type(declared));
    EXPECT_EQ(declared.tuple_elements->size(), 1u);
    std::unique_ptr<PassAST> pass(PassAST::Create());
    EXPECT_TRUE(declared_function_return_type_latest(pass.get()).isUndefined());
  }

  EXPECT_FALSE(match_pattern_is_supported(
    nullptr,
    nullptr,
    StyioDataTypeOption::Integer));
  {
    std::unique_ptr<BinCompAST> eq_left(new BinCompAST(
      CompType::EQ,
      NameAST::Create("x"),
      IntAST::Create("1")));
    std::string scrutinee = "x";
    EXPECT_TRUE(match_pattern_is_supported(
      eq_left.get(),
      &scrutinee,
      StyioDataTypeOption::Integer));
  }
  {
    std::unique_ptr<BinCompAST> eq_right(new BinCompAST(
      CompType::EQ,
      IntAST::Create("1"),
      NameAST::Create("x")));
    std::string scrutinee = "x";
    EXPECT_TRUE(match_pattern_is_supported(
      eq_right.get(),
      &scrutinee,
      StyioDataTypeOption::Integer));
  }
  {
    std::unique_ptr<BinCompAST> non_eq(new BinCompAST(
      CompType::NE,
      NameAST::Create("x"),
      IntAST::Create("1")));
    std::string scrutinee = "x";
    EXPECT_FALSE(match_pattern_is_supported(
      non_eq.get(),
      &scrutinee,
      StyioDataTypeOption::Integer));
  }
  EXPECT_TRUE(container_value_assignable(i64, undefined_type()));
  EXPECT_TRUE(container_value_assignable(f64, i64));
  EXPECT_FALSE(container_value_assignable(i64, string_type));

  {
    std::unique_ptr<ResourceEffectAST> effect(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), string_type),
      nullptr,
      false,
      {},
      true));
    apply_stdin_resource_effect_expected_type(effect.get(), undefined_type());
    apply_stdin_resource_effect_expected_type(effect.get(), i64);
    EXPECT_EQ(static_cast<InstantPullAST*>(effect->getOperation())->getDataType().name, "i64");
  }
  {
    std::unique_ptr<ResourceEffectAST> discard(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), string_type),
      nullptr,
      true,
      {},
      false));
    apply_stdin_resource_effect_expected_type(discard.get(), i64);
    EXPECT_EQ(static_cast<InstantPullAST*>(discard->getOperation())->getDataType().name, "string");
    std::unique_ptr<ResourceEffectAST> stdout_effect(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdout), string_type),
      nullptr,
      false,
      {},
      true));
    apply_stdin_resource_effect_expected_type(stdout_effect.get(), i64);
    EXPECT_EQ(static_cast<InstantPullAST*>(stdout_effect->getOperation())->getDataType().name, "string");
  }

  EXPECT_TRUE(infer_predefined_list_operation_type(&analyzer, nullptr).isUndefined());
  EXPECT_TRUE(infer_predefined_string_operation_type(&analyzer, nullptr).isUndefined());
  {
    analyzer.local_binding_types["scalar"] = i64;
    std::unique_ptr<FuncCallAST> list_push(FuncCallAST::Create(
      NameAST::Create("scalar"),
      NameAST::Create("push"),
      {IntAST::Create("1")}));
    EXPECT_TRUE(infer_predefined_list_operation_type(&analyzer, list_push.get()).isUndefined());
    std::unique_ptr<FuncCallAST> string_lines(FuncCallAST::Create(
      NameAST::Create("scalar"),
      NameAST::Create("lines"),
      {}));
    EXPECT_TRUE(infer_predefined_string_operation_type(&analyzer, string_lines.get()).isUndefined());
  }

  {
    std::unique_ptr<SizeOfAST> missing(new SizeOfAST(nullptr));
    EXPECT_THROW(missing->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<SizeOfAST> scalar(new SizeOfAST(IntAST::Create("1")));
    EXPECT_THROW(scalar->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ListOpAST> slice_bad_base(new ListOpAST(
      StyioNodeType::Access_By_Slice,
      IntAST::Create("1"),
      IntAST::Create("0"),
      IntAST::Create("1")));
    EXPECT_THROW(slice_bad_base->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["items"] = styio_make_list_type("i64");
    std::unique_ptr<ListOpAST> slice_bad_end(new ListOpAST(
      StyioNodeType::Access_By_Slice,
      NameAST::Create("items"),
      IntAST::Create("0"),
      StringAST::Create("bad")));
    EXPECT_THROW(slice_bad_end->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<ListOpAST> append_value(new ListOpAST(
      StyioNodeType::Append_Value,
      NameAST::Create("items"),
      IntAST::Create("1")));
    EXPECT_NO_THROW(append_value->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ParallelAssignAST> bad_target(ParallelAssignAST::Create(
      {PassAST::Create()},
      {IntAST::Create("1")}));
    EXPECT_THROW(bad_target->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["matrix_index"] = styio_make_matrix_type("i64", 1, 1);
    std::unique_ptr<ParallelAssignAST> matrix_index(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("matrix_index"), IntAST::Create("0"))},
      {IntAST::Create("1")}));
    EXPECT_THROW(matrix_index->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["handle_list"] = styio_make_list_type(styio_make_file_handle_type("i64").name);
    std::unique_ptr<ParallelAssignAST> handle_index(ParallelAssignAST::Create(
      {new ListOpAST(StyioNodeType::Access_By_Index, NameAST::Create("handle_list"), IntAST::Create("0"))},
      {FileResourceAST::Create(StringAST::Create("input.txt"), false)}));
    EXPECT_THROW(handle_index->typeInfer(&analyzer), StyioTypeError);
  }

  analyzer.local_binding_types["task"] = styio_make_task_type("i64");
  {
    std::unique_ptr<HandleAcquireAST> undeclared_pull(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("out")),
      NameAST::Create("task")));
    EXPECT_THROW(undeclared_pull->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.local_binding_types["out"] = i64;
    analyzer.fixed_assignment_names_.insert("out");
    std::unique_ptr<HandleAcquireAST> final_pull(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("out")),
      NameAST::Create("task")));
    EXPECT_THROW(final_pull->typeInfer(&analyzer), StyioTypeError);
    analyzer.fixed_assignment_names_.erase("out");
  }
  {
    analyzer.local_binding_types["need_string"] = string_type;
    std::unique_ptr<HandleAcquireAST> mismatched_pull(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("need_string")),
      NameAST::Create("task")));
    EXPECT_THROW(mismatched_pull->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<HandleAcquireAST> untyped_source(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("source_out")),
      PassAST::Create()));
    EXPECT_THROW(untyped_source->typeInfer(&analyzer), StyioTypeError);
    StyioSemaContext::BindingInfo clone_info;
    clone_info.resource_value = true;
    clone_info.value_kind = StyioSemaContext::BindingValueKind::ListHandle;
    clone_info.declared_type = styio_make_list_type("i64");
    analyzer.binding_info_["cloneable_list"] = clone_info;
    std::unique_ptr<HandleAcquireAST> final_clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("clone_final")),
      NameAST::Create("cloneable_list")));
    EXPECT_THROW(final_clone->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<ResourceWriteAST> destroy_sink(ResourceWriteAST::Create(
      IntAST::Create("1"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(destroy_sink->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceRedirectAST> bad_destroy(ResourceRedirectAST::Create(
      NameAST::Create("not_resource"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(bad_destroy->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<ResourceRedirectAST> scalar_destroy(ResourceRedirectAST::Create(
      IntAST::Create("1"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(scalar_destroy->typeInfer(&analyzer), StyioTypeError);
  }
  {
    StyioSemaContext::BindingInfo info;
    info.resource_value = true;
    info.declared_type = styio_make_file_handle_type("i64");
    analyzer.binding_info_["fh"] = info;
    analyzer.task_outer_resource_names_stack_.push_back({"fh"});
    std::unique_ptr<ResourceRedirectAST> outer_destroy(ResourceRedirectAST::Create(
      NameAST::Create("fh"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(outer_destroy->typeInfer(&analyzer), StyioTypeError);
    analyzer.task_outer_resource_names_stack_.clear();
    analyzer.consumed_resource_names_.insert("fh");
    std::unique_ptr<ResourceRedirectAST> double_destroy(ResourceRedirectAST::Create(
      NameAST::Create("fh"),
      EmptyResourceAST::Create()));
    EXPECT_THROW(double_destroy->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<ResourceEffectAST> unsupported(ResourceEffectAST::Create(
      IntAST::Create("1"),
      nullptr,
      false,
      {},
      true));
    EXPECT_THROW(unsupported->typeInfer(&analyzer), StyioTypeError);
    std::vector<ResourceEffectAST::Handler> unknown_handlers;
    unknown_handlers.emplace_back("mystery", IntAST::Create("1"));
    std::unique_ptr<ResourceEffectAST> unknown(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      nullptr,
      false,
      std::move(unknown_handlers),
      true));
    EXPECT_THROW(unknown->typeInfer(&analyzer), StyioTypeError);
    std::vector<ResourceEffectAST::Handler> duplicate_handlers;
    duplicate_handlers.emplace_back("io", IntAST::Create("1"));
    duplicate_handlers.emplace_back("io", IntAST::Create("2"));
    std::unique_ptr<ResourceEffectAST> duplicate(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      nullptr,
      false,
      std::move(duplicate_handlers),
      true));
    EXPECT_THROW(duplicate->typeInfer(&analyzer), StyioTypeError);
    std::vector<ResourceEffectAST::Handler> empty_handlers;
    empty_handlers.emplace_back("bounds", EmptyResourceAST::Create());
    std::unique_ptr<ResourceEffectAST> empty_handler(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      nullptr,
      false,
      std::move(empty_handlers),
      true));
    EXPECT_THROW(empty_handler->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<ResourceEffectAST> empty_fallback(ResourceEffectAST::Create(
      InstantPullAST::Create(StdStreamAST::Create(StdStreamKind::Stdin), i64),
      EmptyResourceAST::Create(),
      false,
      {},
      true));
    EXPECT_THROW(empty_fallback->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<FuncCallAST> insert_count(FuncCallAST::Create(
      NameAST::Create("items"),
      NameAST::Create("insert"),
      {IntAST::Create("0")}));
    EXPECT_THROW(insert_count->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FuncCallAST> insert_index(FuncCallAST::Create(
      NameAST::Create("items"),
      NameAST::Create("insert"),
      {StringAST::Create("bad"), IntAST::Create("1")}));
    EXPECT_THROW(insert_index->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["text"] = string_type;
    std::unique_ptr<FuncCallAST> lines_arg(FuncCallAST::Create(
      NameAST::Create("text"),
      NameAST::Create("lines"),
      {IntAST::Create("1")}));
    EXPECT_THROW(lines_arg->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<ResourceMethodDefAST> prop(ResourceMethodDefAST::Create(
      "file",
      "prop_call",
      false,
      true,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(prop->typeInfer(&analyzer));
    std::unique_ptr<FuncCallAST> prop_call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("prop_call"),
      {}));
    EXPECT_THROW(prop_call->typeInfer(&analyzer), StyioTypeError);
    analyzer.local_binding_types["file_handle"] = styio_make_file_handle_type("i64");
  }

  {
    std::unique_ptr<AttrAST> bad_attr(AttrAST::Create(NameAST::Create("text"), IntAST::Create("1")));
    EXPECT_THROW(bad_attr->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<AttrAST> bad_size(AttrAST::Create(IntAST::Create("1"), NameAST::Create("length")));
    EXPECT_THROW(bad_size->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<AttrAST> bad_pressure(AttrAST::Create(IntAST::Create("1"), NameAST::Create("pressure")));
    EXPECT_THROW(bad_pressure->typeInfer(&analyzer), StyioTypeError);
    analyzer.resource_binding_types_["topo"] = styio_make_topology_resource_type(i64, StyioResourceShapeKind::Sequence);
    std::unique_ptr<AttrAST> resource_pressure(AttrAST::Create(NameAST::Create("topo"), NameAST::Create("pressure")));
    EXPECT_THROW(resource_pressure->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<AttrAST> unresolved_method(AttrAST::Create(NameAST::Create("file_handle"), NameAST::Create("missing")));
    EXPECT_THROW(unresolved_method->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<FlowBindAST> bare(FlowBindAST::Create(
      nullptr,
      VarAST::Create(NameAST::Create("bare"))));
    EXPECT_THROW(bare->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FlowBindAST> bare_with_fallback(FlowBindAST::CreateAwait(
      nullptr,
      VarAST::Create(NameAST::Create("bare_fb")),
      IntAST::Create("0")));
    EXPECT_THROW(bare_with_fallback->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FlowBindAST> already_declared(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("out2"))));
    analyzer.local_binding_types["out2"] = i64;
    EXPECT_THROW(already_declared->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<FlowBindAST> await_non_task(FlowBindAST::CreateAwait(
      IntAST::Create("1"),
      VarAST::Create(NameAST::Create("await_scalar"))));
    EXPECT_THROW((*await_non_task).typeInfer(&analyzer), StyioTypeError);
    analyzer.consumed_task_names_.insert("task");
    std::unique_ptr<FlowBindAST> consumed_task(FlowBindAST::CreateAwait(
      NameAST::Create("task"),
      VarAST::Create(NameAST::Create("task_value"))));
    EXPECT_THROW((*consumed_task).typeInfer(&analyzer), StyioTypeError);
  }

  {
    EXPECT_THROW(analyzer.typeInfer(static_cast<MatchCasesAST*>(nullptr)), StyioTypeError);
    std::unique_ptr<MatchCasesAST> null_pattern(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{nullptr, ReturnAST::Create(IntAST::Create("2"))}}, nullptr)));
    EXPECT_THROW(null_pattern->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<MatchCasesAST> unsupported_pattern(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{BoolAST::Create(true), ReturnAST::Create(IntAST::Create("2"))}}, nullptr)));
    EXPECT_THROW(unsupported_pattern->typeInfer(&analyzer), StyioTypeError);
    std::unique_ptr<MatchCasesAST> unsupported_branch(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(ListAST::Create({IntAST::Create("2")}))}}, nullptr)));
    EXPECT_THROW(unsupported_branch->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<DictAST> later_handle_value(DictAST::Create({
      {StringAST::Create("ok"), IntAST::Create("1")},
      {StringAST::Create("bad"), FileResourceAST::Create(StringAST::Create("input.txt"), false)},
    }));
    EXPECT_THROW((void)infer_dict_literal_type(&analyzer, later_handle_value.get()), StyioTypeError);
  }

  {
    analyzer.resource_binding_types_["task_snapshot"] = styio_make_topology_resource_type(
      styio_make_task_type("i64"),
      StyioResourceShapeKind::Fixed,
      2);
    std::unique_ptr<ResourceRefAST> task_slice(ResourceRefAST::CreateSelector(
      NameAST::Create("task_snapshot"),
      ResourceSelectorKind::SnapshotAll));
    EXPECT_THROW(task_slice->typeInfer(&analyzer), StyioTypeError);

    analyzer.resource_binding_types_["huge_history"] = styio_make_topology_resource_type(
      i64,
      StyioResourceShapeKind::Fixed,
      static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1u);
    std::unique_ptr<ResourceRefAST> huge_slice(ResourceRefAST::CreateSelector(
      NameAST::Create("huge_history"),
      ResourceSelectorKind::SnapshotAll));
    EXPECT_THROW(huge_slice->typeInfer(&analyzer), StyioTypeError);
  }
  {
    analyzer.resource_binding_types_["bucket"] = styio_make_topology_resource_type(
      styio_make_matrix_type("i64", 2, 2),
      StyioResourceShapeKind::Fixed,
      2);
    std::unique_ptr<StreamZipAST> raw_matrix_zip(StreamZipAST::Create(
      ResourceRefAST::Create(NameAST::Create("bucket")),
      {ParamAST::Create(NameAST::Create("m"))},
      ListAST::Create({IntAST::Create("1")}),
      {ParamAST::Create(NameAST::Create("rank"))},
      BlockAST::Create({PassAST::Create()})));
    EXPECT_THROW(raw_matrix_zip->typeInfer(&analyzer), StyioTypeError);
  }

  {
    std::unique_ptr<ListAST> undefined_cell_matrix(ListAST::Create({
      ListAST::Create({PassAST::Create()}),
    }));
    MatrixLiteralInfo info = infer_matrix_literal_info(&analyzer, undefined_cell_matrix.get());
    EXPECT_EQ(info.elem_type.name, "i64");
  }
  EXPECT_FALSE(static_i64_literal(nullptr).has_value());
  EXPECT_FALSE(match_tail_value_expected(nullptr));
  EXPECT_EQ(merge_dict_value_types(undefined_type(), i64).name, "i64");
  EXPECT_EQ(merge_dict_value_types(i64, undefined_type()).name, "i64");
  EXPECT_EQ(merge_cond_flow_branch_type(i64, i64, undefined_type()).name, "i64");
  EXPECT_EQ(merge_match_value_type(styio_make_list_type("i64"), styio_make_list_type("i64")).name, "list[i64]");
  EXPECT_TRUE(func_param_accepts_arg(i64, undefined_type()));
  EXPECT_TRUE(task_result_type_from_task_type(i64).isUndefined());
  EXPECT_EQ(task_result_type_from_task_type(styio_make_task_type("unit")).name, "i64");
  EXPECT_EQ(infer_task_block_result_type(&analyzer, nullptr).name, "i64");
  {
    std::unique_ptr<DictAST> second_undefined(DictAST::Create({
      {StringAST::Create("ok"), IntAST::Create("1")},
      {StringAST::Create("skip"), PassAST::Create()},
    }));
    EXPECT_EQ(infer_dict_literal_type(&analyzer, second_undefined.get()).name, "dict[string,i64]");
  }
  {
    std::unique_ptr<FuncCallAST> clone_call(FuncCallAST::Create(
      NameAST::Create("mat_clone"),
      {NameAST::Create("matrix_index")}));
    EXPECT_TRUE(styio_is_matrix_type(infer_matrix_intrinsic_type(&analyzer, clone_call.get())));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> path_prop(ResourceMethodDefAST::Create(
      "file",
      "path",
      false,
      true,
      {},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(path_prop->typeInfer(&analyzer));
    std::unique_ptr<AttrAST> builtin_path(AttrAST::Create(
      NameAST::Create("file_handle"),
      NameAST::Create("path")));
    EXPECT_EQ(infer_expr_type(&analyzer, builtin_path.get()).name, "string");
  }
  {
    std::unique_ptr<FuncCallAST> unknown_call(FuncCallAST::Create(NameAST::Create("missing_fn"), {}));
    EXPECT_TRUE(infer_expr_type(&analyzer, unknown_call.get()).isUndefined());
  }
  {
    std::unique_ptr<FuncCallAST> callee_consumes(FuncCallAST::Create(
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create()),
      NameAST::Create("ignored"),
      {}));
    EXPECT_TRUE(body_consumes_receiver(&analyzer, callee_consumes.get(), "file"));
  }
  {
    std::unique_ptr<BinOpAST> bool_concat(BinOpAST::Create(
      StyioOpType::Binary_Add,
      BoolAST::Create(true),
      BoolAST::Create(false)));
    EXPECT_FALSE(infer_numeric_string_coercion(&analyzer, bool_concat.get(), bool_concat->getLHS(), bool_concat->getRHS()));
    std::unique_ptr<BinOpAST> string_bool(BinOpAST::Create(
      StyioOpType::Binary_Add,
      StringAST::Create("text"),
      BoolAST::Create(false)));
    EXPECT_FALSE(infer_numeric_string_coercion(&analyzer, string_bool.get(), string_bool->getLHS(), string_bool->getRHS()));
  }
  {
    std::unique_ptr<ParamAST> param(ParamAST::Create(NameAST::Create("direct_param")));
    std::unique_ptr<OptArgAST> opt_arg(OptArgAST::Create(NameAST::Create("direct_arg")));
    std::unique_ptr<OptKwArgAST> opt_kw(OptKwArgAST::Create(NameAST::Create("direct_kw")));
    std::unique_ptr<VarTupleAST> var_tuple(VarTupleAST::Create({VarAST::Create(NameAST::Create("tuple_var"))}));
    EXPECT_NO_THROW(analyzer.typeInfer(param.get()));
    EXPECT_NO_THROW(analyzer.typeInfer(opt_arg.get()));
    EXPECT_NO_THROW(analyzer.typeInfer(opt_kw.get()));
    EXPECT_NO_THROW(analyzer.typeInfer(var_tuple.get()));
  }
  {
    analyzer.local_binding_types["already_bound_resource"] = i64;
    std::unique_ptr<HandleAcquireAST> redefined(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("already_bound_resource")),
      FileResourceAST::Create(StringAST::Create("input.txt"), false)));
    EXPECT_THROW(redefined->typeInfer(&analyzer), StyioTypeError);

    analyzer.fixed_assignment_names_.insert("fixed_clone_target");
    std::unique_ptr<HandleAcquireAST> fixed_clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("fixed_clone_target")),
      NameAST::Create("cloneable_list"),
      HandleAcquireAST::BindMode::Flex));
    EXPECT_THROW(fixed_clone->typeInfer(&analyzer), StyioTypeError);

    StyioSemaContext::BindingInfo non_cloneable;
    non_cloneable.resource_value = true;
    non_cloneable.value_kind = StyioSemaContext::BindingValueKind::I64;
    non_cloneable.declared_type = i64;
    analyzer.binding_info_["non_cloneable_resource"] = non_cloneable;
    std::unique_ptr<HandleAcquireAST> bad_clone(HandleAcquireAST::Create(
      VarAST::Create(NameAST::Create("bad_clone_target")),
      NameAST::Create("non_cloneable_resource"),
      HandleAcquireAST::BindMode::Flex));
    EXPECT_THROW(bad_clone->typeInfer(&analyzer), StyioTypeError);
  }
  {
    auto* match = MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({{IntAST::Create("1"), ReturnAST::Create(IntAST::Create("2"))}}, nullptr));
    match->setDataType(i64);
    std::unique_ptr<FinalBindAST> bind(FinalBindAST::Create(
      VarAST::Create(NameAST::Create("match_final_extra")),
      match));
    EXPECT_NO_THROW(bind->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<BinOpAST> missing_rhs(BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      nullptr));
    EXPECT_NO_THROW(missing_rhs->typeInfer(&analyzer));
    EXPECT_TRUE(missing_rhs->getType().isUndefined());

    std::unique_ptr<BinOpAST> self_assign_unknown(BinOpAST::Create(
      StyioOpType::Self_Add_Assign,
      NameAST::Create("missing_self"),
      IntAST::Create("1")));
    EXPECT_NO_THROW(self_assign_unknown->typeInfer(&analyzer));
    EXPECT_EQ(self_assign_unknown->getType().name, "i64");

    std::unique_ptr<BinOpAST> float_unsupported(BinOpAST::Create(
      StyioOpType::Bitwise_AND,
      FloatAST::Create("1.0"),
      FloatAST::Create("2.0")));
    EXPECT_NO_THROW(float_unsupported->typeInfer(&analyzer));

    auto* lhs_bin = BinOpAST::Create(
      StyioOpType::Binary_Add,
      IntAST::Create("1"),
      IntAST::Create("2"));
    lhs_bin->setDType(i64);
    std::unique_ptr<BinOpAST> rhs_unknown(BinOpAST::Create(
      StyioOpType::Binary_Add,
      lhs_bin,
      NameAST::Create("rhs_missing")));
    EXPECT_NO_THROW(rhs_unknown->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ResourceDeclAST> with_driver(ResourceDeclAST::Create(
      {{NameAST::Create("driven_resource"), TypeAST::Create(i64)}},
      BlockAST::Create({PassAST::Create()})));
    EXPECT_NO_THROW(with_driver->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> accepts_untyped(ResourceMethodDefAST::Create(
      "file",
      "accepts_untyped",
      false,
      false,
      {ParamAST::Create(NameAST::Create("value"))},
      ReturnAST::Create(IntAST::Create("1"))));
    EXPECT_NO_THROW(accepts_untyped->typeInfer(&analyzer));
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      ResourceReceiverAST::Create("file"),
      NameAST::Create("accepts_untyped"),
      {IntAST::Create("7")}));
    EXPECT_NO_THROW(call->typeInfer(&analyzer));
  }
  {
    std::unique_ptr<ResourceMethodDefAST> close_again(ResourceMethodDefAST::Create(
      "file",
      "close_again",
      false,
      false,
      {ParamAST::Create(NameAST::Create("ignored"))},
      ResourceRedirectAST::Create(ResourceReceiverAST::Create("file"), EmptyResourceAST::Create())));
    EXPECT_NO_THROW(close_again->typeInfer(&analyzer));
    analyzer.local_binding_types["destroyed_handle"] = styio_make_file_handle_type("i64");
    StyioSemaContext::BindingInfo handle_info;
    handle_info.resource_value = true;
    handle_info.declared_type = styio_make_file_handle_type("i64");
    analyzer.binding_info_["destroyed_handle"] = handle_info;
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("destroyed_handle"),
      NameAST::Create("close_again"),
      {ResourceRedirectAST::Create(NameAST::Create("destroyed_handle"), EmptyResourceAST::Create())}));
    EXPECT_THROW(call->typeInfer(&analyzer), StyioTypeError);
    analyzer.binding_info_.erase("destroyed_handle");
    analyzer.local_binding_types.erase("destroyed_handle");
  }
  {
    std::unique_ptr<SimpleFuncAST> echo(SimpleFuncAST::Create(
      NameAST::Create("echo_untyped"),
      {ParamAST::Create(NameAST::Create("value"))},
      NameAST::Create("value")));
    analyzer.func_defs["echo_untyped"] = echo.get();
    std::unique_ptr<FuncCallAST> call(FuncCallAST::Create(
      NameAST::Create("echo_untyped"),
      {IntAST::Create("5")}));
    EXPECT_NO_THROW(call->typeInfer(&analyzer));
    EXPECT_EQ(infer_expr_type(&analyzer, call.get()).name, "i64");
  }
  {
    analyzer.local_binding_types["final_flow_target"] = i64;
    analyzer.fixed_assignment_names_.insert("final_flow_target");
    std::unique_ptr<FlowBindAST> final_flow(FlowBindAST::Create(
      IntAST::Create("1"),
      VarAST::Create(NameAST::Create("final_flow_target"))));
    EXPECT_THROW(final_flow->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<MatchCasesAST> bad_default(MatchCasesAST::make(
      IntAST::Create("1"),
      CasesAST::Create({}, ReturnAST::Create(ListAST::Create({IntAST::Create("1")})))));
    EXPECT_THROW(bad_default->typeInfer(&analyzer), StyioTypeError);
  }
  {
    std::unique_ptr<MainBlockAST> exported(MainBlockAST::Create({
      ExportDeclAST::Create({"exported_name"}),
    }));
    EXPECT_NO_THROW(exported->typeInfer(&analyzer));
  }
}

}  // namespace

TEST(StyioStructuredFunctionResultsTypeInfer, ShapesTupleAndRejectsInvalidProjection) {
  AstToStyioIRLowerer analyzer;
  auto* tuple = TupleAST::Create({
    IntAST::Create("1"),
    BoolAST::Create(true),
    ListAST::Create({IntAST::Create("7")})});
  std::unique_ptr<TupleAST> owned_tuple(tuple);
  ASSERT_NO_THROW(tuple->typeInfer(&analyzer));
  ASSERT_TRUE(styio_is_shaped_tuple_type(tuple->getDataType()));
  ASSERT_EQ(tuple->getDataType().tuple_elements->size(), 3u);
  EXPECT_EQ((*tuple->getDataType().tuple_elements)[2].name, "list[i64]");

  auto* projected_source = TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")});
  projected_source->typeInfer(&analyzer);
  std::unique_ptr<ListOpAST> out_of_range(new ListOpAST(
    StyioNodeType::Access_By_Index, projected_source, IntAST::Create("2")));
  EXPECT_THROW(out_of_range->typeInfer(&analyzer), StyioTypeError);

  auto* dynamic_source = TupleAST::Create({IntAST::Create("1"), IntAST::Create("2")});
  dynamic_source->typeInfer(&analyzer);
  std::unique_ptr<ListOpAST> nonliteral(new ListOpAST(
    StyioNodeType::Access_By_Index, dynamic_source, FloatAST::Create("0.0")));
  EXPECT_THROW(nonliteral->typeInfer(&analyzer), StyioTypeError);

  std::unique_ptr<TupleAST> empty(TupleAST::Create({}));
  EXPECT_THROW(empty->typeInfer(&analyzer), StyioTypeError);

  std::unique_ptr<TupleAST> single(TupleAST::Create({IntAST::Create("1")}));
  EXPECT_THROW(single->typeInfer(&analyzer), StyioTypeError);
}
