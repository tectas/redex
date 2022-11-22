/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/iostreams/device/mapped_file.hpp>

#include "DexClass.h"
#include "DexDefs.h"
#include "DexIdx.h"
#include "DexStats.h"
#include "DexUtil.h"

class DexLoader {
  std::unique_ptr<DexIdx> m_idx;
  const dex_class_def* m_class_defs;
  DexClasses* m_classes;
  std::unique_ptr<boost::iostreams::mapped_file> m_file;
  const DexLocation* m_location;

 public:
  explicit DexLoader(const DexLocation* location);

  const dex_header* get_dex_header(const char* file_name);
  DexClasses load_dex(const char* file_name,
                      dex_stats_t* stats,
                      int support_dex_version);
  DexClasses load_dex(const dex_header* dh, dex_stats_t* stats);
  void load_dex_class(int num);
  void gather_input_stats(dex_stats_t* stats, const dex_header* dh);
  DexIdx* get_idx() { return m_idx.get(); }
};

DexClasses load_classes_from_dex(const DexLocation* location,
                                 bool balloon = true,
                                 bool throw_on_balloon_error = true,
                                 int support_dex_version = 35);
DexClasses load_classes_from_dex(const DexLocation* location,
                                 dex_stats_t* stats,
                                 bool balloon = true,
                                 bool throw_on_balloon_error = true,
                                 int support_dex_version = 35);
DexClasses load_classes_from_dex(const dex_header* dh,
                                 const DexLocation* location,
                                 bool balloon = true,
                                 bool throw_on_balloon_error = true);
std::string load_dex_magic_from_dex(const DexLocation* location);
void balloon_for_test(const Scope& scope);

static inline const uint8_t* align_ptr(const uint8_t* const ptr,
                                       const size_t alignment) {
  const size_t alignment_error = ((size_t)ptr) % alignment;
  if (alignment_error != 0) {
    return ptr + alignment - alignment_error;
  } else {
    return ptr;
  }
}
