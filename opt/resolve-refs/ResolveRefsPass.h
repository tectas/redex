/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ExternalRefsManglingPass.h"

/**
 * A field or method being referenced by an instruction could be a pure `ref`.
 * In which, the ref points to a class where the field/method is not actually
 * defined. This is allowed in dex bytecode. However, it adds complexity to
 * Redex's optimizations.
 *
 * The motivation of this pass is to resolve all
 * method/field references to its definition in the most accurate way possible.
 * It is supposed to be done early on, so that the rest of the optimizations
 * don't have to deal with the distinction between a `ref` and a `def`.
 *
 * Unlike RebindRefs, the goal here is to bind the method/field reference to the
 * most accurate one possible to produce an accurate reachability graph of the
 * program. Therefore, the number of unique method references is not a concern.
 */

namespace impl {
struct RefStats;
} // namespace impl

class ResolveRefsPass : public ExternalRefsManglingPass {
 public:
  ResolveRefsPass() : ExternalRefsManglingPass("ResolveRefsPass") {}

  void bind_config() override {
    ExternalRefsManglingPass::bind_config();
    bind("desuperify", true, m_desuperify,
         "Convert invoke-super calls to invoke-virtual where possible");
    trait(Traits::Pass::atleast, 1);
  }

  void eval_pass(DexStoresVector& stores,
                 ConfigFiles& conf,
                 PassManager& mgr) override {
    ExternalRefsManglingPass::eval_pass(stores, conf, mgr);
  }

  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;

 private:
  impl::RefStats refine_virtual_callsites(DexMethod* method, bool desuperify);
  bool m_desuperify;
};
