/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Pass.h"

class DexMethod;
class IRCode;

// A pass to remove recursive locks, usually exposed during inlining.
class RemoveRecursiveLocksPass : public Pass {
 public:
  RemoveRecursiveLocksPass() : Pass("RemoveRecursiveLocksPass") {}

  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;

  // For testing, only. As long as the run will not fail, it is permissible
  // to give method=nullptr.
  static bool run(DexMethod* method, IRCode* code);
};
