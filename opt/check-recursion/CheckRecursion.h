/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Pass.h"

class CheckRecursionPass : public Pass {
 public:
  CheckRecursionPass() : Pass("CheckRecursionPass") {}

  int bad_recursion_count{4};

  void bind_config() override {
    bind("bad_recursion_count", 4, bad_recursion_count);
  }

  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;
};
