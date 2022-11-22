/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "DefinitelyAssignedIFields.h"

#include "AbstractDomain.h"
#include "BaseIRAnalyzer.h"
#include "ConstantAbstractDomain.h"
#include "ConstantPropagationAnalysis.h"
#include "IRCode.h"
#include "IRInstruction.h"
#include "PatriciaTreeMapAbstractEnvironment.h"
#include "PatriciaTreeSetAbstractDomain.h"
#include "ReducedProductAbstractDomain.h"
#include "Resolver.h"
#include "StlUtil.h"
#include "Timer.h"
#include "TypeUtil.h"
#include "Walkers.h"

namespace {

using namespace ir_analyzer;

using BoolDomain = sparta::ConstantAbstractDomain<bool>;

/**
 * For each register, whether it represents the `this` parameter.
 **/
using ParamDomainEnvironment =
    sparta::PatriciaTreeMapAbstractEnvironment<reg_t, BoolDomain>;

/**
 * Set of fields that have been read even through they were not written to.
 **/
using ReadUnwrittenFieldDomainEnvironment =
    sparta::PatriciaTreeSetAbstractDomain<DexField*>;

/**
 * Set of fields that have been written to before ever having been read. This is
 * realized via the reverse adaptor, as we want to compute the intersection on
 * joins.
 **/
class WrittenUnreadFieldDomainEnvironment final
    : public sparta::AbstractDomainReverseAdaptor<
          sparta::PatriciaTreeSetAbstractDomain<DexField*>,
          WrittenUnreadFieldDomainEnvironment> {
 public:
  using AbstractDomainReverseAdaptor::AbstractDomainReverseAdaptor;

  // Some older compilers complain that the class is not default constructible.
  // We intended to use the default constructors of the base class (via using
  // AbstractDomainReverseAdaptor::AbstractDomainReverseAdaptor), but some
  // compilers fail to catch this. So we insert a redundant '= default'.
  WrittenUnreadFieldDomainEnvironment() = default;
};

// The result of analyzing a constructor tells us...
// - which fields of the constructor's declaring class were definitely assigned,
//   i.e. not read before written to
// - whether the 'this' parameter escaped
struct AnalysisResult {
  std::unordered_set<const DexField*> definitely_assigned_ifields;
  bool may_this_have_escaped{false};
  bool operator==(const AnalysisResult& other) const {
    return definitely_assigned_ifields == other.definitely_assigned_ifields &&
           may_this_have_escaped == other.may_this_have_escaped;
  }
};

// We track...
// - for each register, whether it represents the `this` parameter
// - which fields of the constructor's declaring class might have been read even
//   though they were never written to
// - which fields of the constructor's declaring class were written to before
//   ever having been read
// - whether ths 'this' parameter may have escaped
class ConstructorAnalysisEnvironment final
    : public sparta::ReducedProductAbstractDomain<
          ConstructorAnalysisEnvironment,
          ParamDomainEnvironment,
          ReadUnwrittenFieldDomainEnvironment,
          WrittenUnreadFieldDomainEnvironment,
          BoolDomain> {
 public:
  using ReducedProductAbstractDomain::ReducedProductAbstractDomain;
  ConstructorAnalysisEnvironment()
      : ReducedProductAbstractDomain(
            std::make_tuple(ParamDomainEnvironment::top(),
                            ReadUnwrittenFieldDomainEnvironment(),
                            WrittenUnreadFieldDomainEnvironment(),
                            BoolDomain(false))) {}

  static void reduce_product(std::tuple<ParamDomainEnvironment,
                                        ReadUnwrittenFieldDomainEnvironment,
                                        WrittenUnreadFieldDomainEnvironment,
                                        BoolDomain>&) {}

  const ParamDomainEnvironment& get_params() const {
    return ReducedProductAbstractDomain::get<0>();
  }

  const ReadUnwrittenFieldDomainEnvironment& get_read_unwritten_fields() const {
    return ReducedProductAbstractDomain::get<1>();
  }

  const WrittenUnreadFieldDomainEnvironment& get_written_unread_fields() const {
    return ReducedProductAbstractDomain::get<2>();
  }

  bool may_this_have_escaped() const {
    auto value = ReducedProductAbstractDomain::get<3>();
    return !value.get_constant() || *value.get_constant();
  }

  void mutate_params(std::function<void(ParamDomainEnvironment*)> f) {
    apply<0>(std::move(f));
  }

  void add_read_unwritten_field(DexField* f) {
    apply<1>(
        [&](ReadUnwrittenFieldDomainEnvironment* domain) { domain->add(f); });
  }

  void add_written_unread_field(DexField* f) {
    apply<2>([&](WrittenUnreadFieldDomainEnvironment* domain) {
      domain->unwrap().add(f);
    });
  }

  void set_this_escaped() {
    ReducedProductAbstractDomain::apply<3>(
        [&](BoolDomain* domain) { *domain = BoolDomain(true); });
  }

  AnalysisResult get_analysis_result(DexClass* cls) const {
    AnalysisResult res{{}, may_this_have_escaped()};
    for (auto field : cls->get_ifields()) {
      if (get_written_unread_fields().unwrap().contains(field)) {
        always_assert(!get_read_unwritten_fields().contains(field));
        res.definitely_assigned_ifields.insert(field);
      }
    }
    return res;
  }
};

const IRInstruction* get_first_load_param(const cfg::ControlFlowGraph& cfg) {
  const auto param_insns = InstructionIterable(cfg.get_param_instructions());
  auto& mie = *param_insns.begin();
  const auto insn = mie.insn;
  always_assert(insn->opcode() == IOPCODE_LOAD_PARAM_OBJECT);
  return insn;
}

class Analyzer final : public BaseIRAnalyzer<ConstructorAnalysisEnvironment> {
 public:
  Analyzer(const cfg::ControlFlowGraph& cfg,
           const DexType* declaring_type,
           const std::function<const AnalysisResult*(DexMethod*)>&
               get_analysis_result)
      : BaseIRAnalyzer(cfg),
        m_declaring_type(declaring_type),
        m_get_analysis_result(get_analysis_result),
        m_super_type(type_class(declaring_type)->get_super_class()),
        m_first_load_param(get_first_load_param(cfg)) {
    MonotonicFixpointIterator::run({});
  }

  void analyze_instruction(
      const IRInstruction* insn,
      ConstructorAnalysisEnvironment* current_state) const override {
    if (current_state->may_this_have_escaped()) {
      // Nothing matters anymore
      return;
    }

    const auto set_current_state_at = [&](reg_t reg, bool wide,
                                          BoolDomain value) {
      current_state->mutate_params([&](ParamDomainEnvironment* env) {
        env->set(reg, value);
        if (wide) {
          env->set(reg + 1, BoolDomain::top());
        }
      });
    };

    auto opcode = insn->opcode();
    if (opcode::is_a_move(opcode)) {
      const auto value = current_state->get_params().get(insn->src(0));
      set_current_state_at(insn->dest(), insn->dest_is_wide(), value);
      return;
    }

    DexMethod* invoked_ctor_on_this{nullptr};
    for (src_index_t src_idx = 0; src_idx < insn->srcs_size(); src_idx++) {
      auto src = insn->src(src_idx);
      auto param_value = current_state->get_params().get(src);
      auto may_be_first_param =
          !param_value.get_constant() || *param_value.get_constant();
      if (!may_be_first_param) {
        continue;
      }
      if (opcode::is_an_iput(opcode) && src_idx == 1) {
        auto field_ref = insn->get_field();
        DexField* field = resolve_field(field_ref, FieldSearch::Instance);
        if (field != nullptr && field->get_class() == m_declaring_type) {
          if (!current_state->get_read_unwritten_fields().contains(field)) {
            current_state->add_written_unread_field(field);
          }
        }
        continue;
      } else if (opcode::is_an_iget(opcode) && src_idx == 0) {
        auto field_ref = insn->get_field();
        DexField* field = resolve_field(field_ref, FieldSearch::Instance);
        if (field != nullptr && field->get_class() == m_declaring_type) {
          if (!current_state->get_written_unread_fields().unwrap().contains(
                  field)) {
            current_state->add_read_unwritten_field(field);
          }
        }
        continue;
      } else if (opcode == OPCODE_INVOKE_DIRECT && src_idx == 0) {
        auto method_ref = insn->get_method();
        if (method::is_init(method_ref)) {
          DexMethod* method = resolve_method(method_ref, MethodSearch::Direct);
          if (method != nullptr) {
            auto method_class = method->get_class();
            if (method_class == m_declaring_type ||
                method_class == m_super_type) {
              invoked_ctor_on_this = method;
              continue;
            }
          }
        }
      }
      // 'this' may have escaped
      current_state->set_this_escaped();
      return;
    }

    if (invoked_ctor_on_this) {
      // Run this after the loop over the src registers, to make sure we abort
      // when the `this` parameter escapes
      const auto* analysis_result = m_get_analysis_result(invoked_ctor_on_this);
      if (invoked_ctor_on_this->get_class() == m_declaring_type) {
        for (auto field : type_class(m_declaring_type)->get_ifields()) {
          if (analysis_result->definitely_assigned_ifields.count(field)) {
            // If we haven't read the field yet, then we can also mark the field
            // as written.
            if (!current_state->get_read_unwritten_fields().contains(field)) {
              current_state->add_written_unread_field(field);
            }
            continue;
          }
          // If not definitely assigned by that ctor, then we can just give up
          // here and mark all unwritten fields as read. Later, the intersection
          // is computed across all ctors anyway.
          if (!current_state->get_written_unread_fields().unwrap().contains(
                  field)) {
            current_state->add_read_unwritten_field(field);
          }
        }
      }
      if (analysis_result->may_this_have_escaped) {
        current_state->set_this_escaped();
        return;
      }
    }

    if (insn->has_dest()) {
      bool is_first_parameter = insn == m_first_load_param;
      const auto value = BoolDomain(is_first_parameter);
      set_current_state_at(insn->dest(), insn->dest_is_wide(), value);
    }
  }

 private:
  const DexType* m_declaring_type;
  const std::function<const AnalysisResult*(DexMethod*)>& m_get_analysis_result;
  const DexType* m_super_type;
  const IRInstruction* m_first_load_param;
};
} // namespace

namespace constant_propagation {
namespace definitely_assigned_ifields {
std::unordered_set<const DexField*> get_definitely_assigned_ifields(
    const Scope& scope) {
  Timer t("get_definitely_assigned_ifields");
  ConcurrentMap<DexMethod*, std::shared_ptr<AnalysisResult>> analysis_results;
  std::function<const AnalysisResult*(DexMethod*)> get_analysis_result;
  get_analysis_result = [&](DexMethod* ctor) {
    auto res = analysis_results.get(ctor, nullptr);
    if (res) {
      return res.get();
    }
    if (!ctor->is_external() && ctor->get_code()) {
      auto& cfg = ctor->get_code()->cfg();
      Analyzer analyzer(cfg, ctor->get_class(), get_analysis_result);
      const auto& env = analyzer.get_exit_state_at(cfg.exit_block());
      auto cls = type_class(ctor->get_class());
      res = std::make_shared<AnalysisResult>(env.get_analysis_result(cls));
    } else {
      res = std::make_shared<AnalysisResult>();
      // Conservative assumption: All external ctors (without code)
      // except Object::<init> may directly or indirectly read and write own
      // fields.
      if (ctor->get_class() != type::java_lang_Object()) {
        // TODO: Consider using the SummaryGenerator to analyze AOSP classes to
        // find other external constructors where this does not escape.
        res->may_this_have_escaped = true;
      }
    }
    analysis_results.update(
        ctor,
        [&](DexMethod*, std::shared_ptr<AnalysisResult>& value, bool exists) {
          if (exists) {
            always_assert(*value == *res);
            res = value;
          } else {
            value = res;
          }
        });
    return res.get();
  };
  ConcurrentSet<const DexField*> res;
  walk::parallel::classes(scope, [&](DexClass* cls) {
    const auto ctors = cls->get_ctors();
    if (ctors.empty()) {
      // Actually, without a constructor, all fields *are* definitely assigned.
      // However, this class is then uninstantiable, and we've another pass that
      // effectively deals with that.
      return;
    }
    auto definitely_assigned_ifields = cls->get_ifields();
    std20::erase_if(definitely_assigned_ifields, [&](const auto* f) {
      return !can_delete(f) || !can_rename(f);
    });
    for (auto ctor : ctors) {
      auto analysis_result = get_analysis_result(ctor);
      std20::erase_if(definitely_assigned_ifields, [&](auto* f) {
        return !analysis_result->definitely_assigned_ifields.count(f);
      });
    }
    res.insert(definitely_assigned_ifields.begin(),
               definitely_assigned_ifields.end());
  });
  return std::unordered_set<const DexField*>(res.begin(), res.end());
}
} // namespace definitely_assigned_ifields
} // namespace constant_propagation
