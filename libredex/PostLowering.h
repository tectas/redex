/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/optional/optional.hpp>
#include <string>

#include "AssetManager.h"
#include "ConfigFiles.h"
#include "DexClass.h"
#include "DexStore.h"
#include "IODIMetadata.h"

class GatheredTypes;

class PostLowering {
 public:
  static std::unique_ptr<PostLowering> create();

  virtual void sync() = 0;
  virtual void run(const DexStoresVector& stores) = 0;
  virtual void finalize(AssetManager& mgr) = 0;

  virtual std::unordered_map<DexClass*, std::vector<DexMethod*>>
  get_detached_methods() = 0;
  virtual void emit_symbolication_metadata(
      PositionMapper* pos_mapper,
      std::unordered_map<DexMethod*, uint64_t>* method_to_id,
      std::unordered_map<DexCode*, std::vector<DebugLineItem>>*
          code_debug_lines,
      IODIMetadata* iodi_metadata,
      std::vector<DexMethod*>& needs_debug_line_mapping,
      std::set<uint32_t>& signatures) = 0;

  virtual void load_dex_indexes(ConfigFiles& conf,
                                int32_t min_sdk,
                                DexClasses* classes,
                                GatheredTypes& gtypes,
                                const std::string& store_name,
                                size_t dex_number) = 0;

  virtual ~PostLowering(){};
};
