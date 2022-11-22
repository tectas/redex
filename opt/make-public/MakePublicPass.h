/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Pass.h"

/**
 * This pass currently makes things public except direct methods.
 * If we want to make direct methods public, we should make them static first
 * and change the related opcodes from invoke-direct to invoke-static.
 */
class MakePublicPass : public Pass {
 public:
  MakePublicPass() : Pass("MakePublicPass") {}

  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;
};
