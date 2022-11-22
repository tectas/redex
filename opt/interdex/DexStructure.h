/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>
#include <vector>

#include "DexClass.h"
#include "InitClassesWithSideEffects.h"
#include "Pass.h"
#include "Util.h"

namespace interdex {

struct ReserveRefsInfo {
  size_t frefs;
  size_t trefs;
  size_t mrefs;

  ReserveRefsInfo()
      : ReserveRefsInfo(/* frefs */ 0, /* trefs */ 0, /* mrefs */ 0) {}

  ReserveRefsInfo(size_t frefs, size_t trefs, size_t mrefs)
      : frefs(frefs), trefs(trefs), mrefs(mrefs) {}
};

using MethodRefs = std::unordered_set<DexMethodRef*>;
using FieldRefs = std::unordered_set<DexFieldRef*>;
using TypeRefs = std::unordered_set<DexType*>;

struct DexInfo {
  bool primary{false};
  bool coldstart{false};
  bool background{false};
  bool extended{false};
  bool scroll{false};
  bool betamap_ordered{false};
};

class DexStructure {
 public:
  DexStructure() : m_linear_alloc_size(0) {}

  const DexClasses& get_all_classes() const { return m_classes; }

  /**
   * Only call this if you know what you are doing. This will leave the
   * current instance is in an unusable state.
   */
  DexClasses take_all_classes() { return std::move(m_classes); }

  /**
   * Tries to add the specified class. Returns false if it doesn't fit.
   */
  bool add_class_if_fits(const MethodRefs& clazz_mrefs,
                         const FieldRefs& clazz_frefs,
                         const TypeRefs& clazz_trefs,
                         const interdex::TypeRefs& pending_init_class_fields,
                         const interdex::TypeRefs& pending_init_class_types,
                         size_t linear_alloc_limit,
                         size_t field_refs_limit,
                         size_t method_refs_limit,
                         size_t type_refs_limit,
                         DexClass* clazz);

  void add_class_no_checks(const MethodRefs& clazz_mrefs,
                           const FieldRefs& clazz_frefs,
                           const TypeRefs& clazz_trefs,
                           const interdex::TypeRefs& pending_init_class_fields,
                           const interdex::TypeRefs& pending_init_class_types,
                           unsigned laclazz,
                           DexClass* clazz);

  void add_refs_no_checks(const MethodRefs& clazz_mrefs,
                          const FieldRefs& clazz_frefs,
                          const TypeRefs& clazz_trefs,
                          const interdex::TypeRefs& pending_init_class_fields,
                          const interdex::TypeRefs& pending_init_class_types);

  void resolve_init_classes(const init_classes::InitClassesWithSideEffects*
                                init_classes_with_side_effects,
                            const interdex::FieldRefs& frefs,
                            const interdex::TypeRefs& trefs,
                            const interdex::TypeRefs& itrefs,
                            interdex::TypeRefs* pending_init_class_fields,
                            interdex::TypeRefs* pending_init_class_types);

  bool has_tref(DexType* type) const { return m_trefs.count(type); }

  void check_refs_count();

 private:
  size_t m_linear_alloc_size;
  TypeRefs m_trefs;
  MethodRefs m_mrefs;
  FieldRefs m_frefs;
  interdex::TypeRefs m_pending_init_class_fields;
  interdex::TypeRefs m_pending_init_class_types;
  std::vector<DexClass*> m_classes;
};

class DexesStructure {
 public:
  const DexClasses& get_current_dex_classes() const {
    return m_current_dex.get_all_classes();
  }

  bool current_dex_has_tref(DexType* type) const {
    return m_current_dex.has_tref(type);
  }

  size_t get_num_coldstart_dexes() const { return m_info.num_coldstart_dexes; }

  size_t get_num_extended_dexes() const {
    return m_info.num_extended_set_dexes;
  }

  size_t get_num_scroll_dexes() const { return m_info.num_scroll_dexes; }

  size_t get_num_dexes() const { return m_info.num_dexes; }

  size_t get_num_mixedmode_dexes() const { return m_info.num_mixed_mode_dexes; }

  size_t get_num_secondary_dexes() const { return m_info.num_secondary_dexes; }

  size_t get_num_classes() const { return m_classes.size(); }

  size_t get_num_mrefs() const { return m_stats.num_mrefs; }

  size_t get_num_frefs() const { return m_stats.num_frefs; }

  size_t get_num_dmethods() const { return m_stats.num_dmethods; }

  size_t get_num_vmethods() const { return m_stats.num_vmethods; }

  size_t get_frefs_limit() const;
  size_t get_trefs_limit() const;
  size_t get_mrefs_limit() const;

  void set_linear_alloc_limit(int64_t linear_alloc_limit) {
    m_linear_alloc_limit = linear_alloc_limit;
  }

  void set_reserve_frefs(size_t reserve_frefs) {
    m_reserve_refs.frefs = reserve_frefs;
  }

  void set_reserve_trefs(size_t reserve_trefs) {
    m_reserve_refs.trefs = reserve_trefs;
  }

  void set_reserve_mrefs(size_t reserve_mrefs) {
    m_reserve_refs.mrefs = reserve_mrefs;
  }

  void set_min_sdk(int min_sdk) { m_min_sdk = min_sdk; }

  void set_init_classes_with_side_effects(
      const init_classes::InitClassesWithSideEffects*
          init_classes_with_side_effects) {
    m_init_classes_with_side_effects = init_classes_with_side_effects;
  }

  /**
   * Tries to add the class to the current dex. If it can't, it returns false.
   * Throws if the class already exists in the dexes.
   */
  bool add_class_to_current_dex(const MethodRefs& clazz_mrefs,
                                const FieldRefs& clazz_frefs,
                                const TypeRefs& clazz_trefs,
                                const TypeRefs& clazz_itrefs,
                                DexClass* clazz);

  /*
   * Add class to current dex, without any checks.
   * Throws if the class already exists in the dexes.
   */
  void add_class_no_checks(const MethodRefs& clazz_mrefs,
                           const FieldRefs& clazz_frefs,
                           const TypeRefs& clazz_trefs,
                           const TypeRefs& clazz_itrefs,
                           DexClass* clazz);
  void add_class_no_checks(DexClass* clazz) {
    add_class_no_checks(
        MethodRefs(), FieldRefs(), TypeRefs(), TypeRefs(), clazz);
  }
  void add_refs_no_checks(const MethodRefs& clazz_mrefs,
                          const FieldRefs& clazz_frefs,
                          const TypeRefs& clazz_trefs,
                          const TypeRefs& clazz_itrefs);

  void resolve_init_classes(const interdex::FieldRefs& frefs,
                            const interdex::TypeRefs& trefs,
                            const interdex::TypeRefs& itrefs,
                            interdex::TypeRefs* pending_init_class_fields,
                            interdex::TypeRefs* pending_init_class_types);

  /**
   * It returns the classes contained in this dex and moves on to the next dex.
   */
  DexClasses end_dex(DexInfo dex_info);

  bool has_class(DexClass* clazz) const { return m_classes.count(clazz); }

 private:
  void update_stats(const MethodRefs& clazz_mrefs,
                    const FieldRefs& clazz_frefs,
                    DexClass* clazz);

  // NOTE: Keeps track only of the last dex.
  DexStructure m_current_dex;

  // All the classes that end up added in the dexes.
  std::unordered_set<DexClass*> m_classes;

  int64_t m_linear_alloc_limit;
  ReserveRefsInfo m_reserve_refs;
  int m_min_sdk;
  const init_classes::InitClassesWithSideEffects*
      m_init_classes_with_side_effects{nullptr};
  struct DexesInfo {
    size_t num_dexes{0};

    // Number of secondary dexes emitted.
    size_t num_secondary_dexes{0};

    // Number of coldstart dexes emitted.
    size_t num_coldstart_dexes{0};

    // Number of coldstart extended set dexes emitted.
    size_t num_extended_set_dexes{0};

    // Number of dexes containing scroll classes.
    size_t num_scroll_dexes{0};

    // Number of mixed mode dexes;
    size_t num_mixed_mode_dexes{0};
  } m_info;

  struct DexesStats {
    size_t num_static_meths{0};
    size_t num_dmethods{0};
    size_t num_vmethods{0};
    size_t num_mrefs{0};
    size_t num_frefs{0};
  } m_stats;
};

} // namespace interdex
