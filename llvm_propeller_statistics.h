#ifndef AUTOFDO_LLVM_PROPELLER_STATISTICS_H_
#define AUTOFDO_LLVM_PROPELLER_STATISTICS_H_

#include <cstdint>
#include <utility>

#include "llvm_propeller_cfg.h"
#include "third_party/abseil/absl/algorithm/container.h"

namespace devtools_crosstool_autofdo {
struct PropellerStats {
  int binary_mmap_num = 0;
  int perf_file_parsed = 0;
  uint64_t br_counters_accumulated = 0;
  uint64_t edges_with_same_src_sink_but_different_type = 0;
  uint64_t cfgs_created = 0;
  // Number of CFGs which have hot landing pads.
  int cfgs_with_hot_landing_pads = 0;
  uint64_t nodes_created = 0;
  absl::flat_hash_map<CFGEdge::Kind, int64_t> edges_created_by_kind;
  absl::flat_hash_map<CFGEdge::Kind, int64_t> total_edge_weight_by_kind;
  uint64_t duplicate_symbols = 0;
  uint64_t bbaddrmap_function_does_not_have_symtab_entry = 0;
  uint64_t original_intra_score = 0;
  uint64_t optimized_intra_score = 0;
  uint64_t original_inter_score = 0;
  uint64_t optimized_inter_score = 0;
  uint64_t hot_functions = 0;

  int64_t total_edges_created() const {
    return absl::c_accumulate(
        edges_created_by_kind, 0,
        [](int64_t psum, const std::pair<CFGEdge::Kind, int64_t> &entry) {
          return psum + entry.second;
        });
  }

  int64_t total_edge_weight_created() const {
    return absl::c_accumulate(
        total_edge_weight_by_kind, 0,
        [](int64_t psum, const std::pair<CFGEdge::Kind, int64_t> &entry) {
          return psum + entry.second;
        });
  }

  // Merge two copies of stats.
  PropellerStats & operator += (const PropellerStats &s) {
    binary_mmap_num += s.binary_mmap_num;
    perf_file_parsed += s.perf_file_parsed;
    br_counters_accumulated += s.br_counters_accumulated;
    edges_with_same_src_sink_but_different_type +=
        s.edges_with_same_src_sink_but_different_type;
    cfgs_created += s.cfgs_created;
    cfgs_with_hot_landing_pads += s.cfgs_with_hot_landing_pads;
    nodes_created += s.nodes_created;
    for (auto [edge_kind, count] : s.edges_created_by_kind)
      edges_created_by_kind[edge_kind] += count;
    for (auto [edge_kind, weight] : s.total_edge_weight_by_kind)
      total_edge_weight_by_kind[edge_kind] += weight;
    duplicate_symbols += s.duplicate_symbols;
    bbaddrmap_function_does_not_have_symtab_entry +=
        s.bbaddrmap_function_does_not_have_symtab_entry;
    original_intra_score += s.original_intra_score;
    optimized_intra_score += s.optimized_intra_score;
    original_inter_score += s.original_inter_score;
    optimized_inter_score += s.optimized_inter_score;
    hot_functions += s.hot_functions;

    return *this;
  }
};
}  // namespace devtools_crosstool_autofdo
#endif  // AUTOFDO_LLVM_PROPELLER_STATISTICS_H_
