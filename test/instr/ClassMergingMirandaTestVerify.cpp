/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "verify/VerifyUtil.h"

TEST_F(PostVerify, MergeablesRemoval) {
  auto sa1 = find_class_named(classes, "Lcom/facebook/redextest/SubA1;");
  auto sa2 = find_class_named(classes, "Lcom/facebook/redextest/SubA2;");
  auto sb1 = find_class_named(classes, "Lcom/facebook/redextest/SubB1;");
  auto sb2 = find_class_named(classes, "Lcom/facebook/redextest/SubB2;");
  auto sc1 = find_class_named(classes, "Lcom/facebook/redextest/SubC1;");
  auto sc2 = find_class_named(classes, "Lcom/facebook/redextest/SubC2;");
  auto sd1 = find_class_named(classes, "Lcom/facebook/redextest/SubD1;");
  auto sd2 = find_class_named(classes, "Lcom/facebook/redextest/SubD2;");
  verify_type_erased(sa1);
  verify_type_erased(sa2);
  verify_type_erased(sb1);
  verify_type_erased(sb2);
  verify_type_erased(sc1);
  verify_type_erased(sc2);
  verify_type_erased(sd1);
  verify_type_erased(sd2);
}
