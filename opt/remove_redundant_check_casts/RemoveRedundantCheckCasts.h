/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "CheckCastConfig.h"
#include "Pass.h"

namespace check_casts {

class RemoveRedundantCheckCastsPass : public Pass {
 public:
  RemoveRedundantCheckCastsPass() : Pass("RemoveRedundantCheckCastsPass") {}

  void bind_config() override;
  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;

 private:
  CheckCastConfig m_config;
};

} // namespace check_casts
