/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <vector>

class DexClass;
class IRInstruction;
class PassManager;

class RedundantCheckCastRemover {
 public:
  static const std::string& get_name() {
    static const std::string name("Remove_Redundant_CheckCast");
    return name;
  }

  explicit RedundantCheckCastRemover(PassManager& mgr,
                                     const std::vector<DexClass*>& scope);
  void run();

 private:
  static bool can_remove_check_cast(const std::vector<IRInstruction*>&);

  PassManager& m_mgr;
  const std::vector<DexClass*>& m_scope;
};
