/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Pass.h"

class ShortenSrcStringsPass : public Pass {
 public:
  ShortenSrcStringsPass() : Pass("ShortenSrcStringsPass") {}

  void bind_config() override {
    bind("filename_mappings", "redex-src-strings-map.txt", m_filename_mappings);
    trait(Traits::Pass::unique, true);
  }

  void run_pass(DexStoresVector&, ConfigFiles&, PassManager&) override;

 private:
  std::string m_filename_mappings;
};
