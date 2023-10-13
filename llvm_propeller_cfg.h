#ifndef AUTOFDO_LLVM_PROPELLER_CFG_H_
#define AUTOFDO_LLVM_PROPELLER_CFG_H_

#include <cstdio>
#include <memory>
#include <numeric>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"  // For "CHECK".
#include "third_party/abseil/absl/container/flat_hash_map.h"
#include "third_party/abseil/absl/functional/function_ref.h"
#include "third_party/abseil/absl/strings/str_format.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/ELFTypes.h"

namespace devtools_crosstool_autofdo {

class CFGNode;
class ControlFlowGraph;
class CFGNodeBundle;

// All instances of CFGEdge are owned by their cfg_.
class CFGEdge final {
 public:
  // Branch kind.
  enum class Kind {
    kBranchOrFallthough,
    kCall,
    kRet,
  };

  CFGEdge(CFGNode *n1, CFGNode *n2, uint64_t weight, Kind kind)
      : src_(n1), sink_(n2), weight_(weight), kind_(kind) {}

  CFGNode * src() const { return src_ ;}
  CFGNode *sink() const { return sink_; }
  uint64_t weight() const { return weight_; }
  Kind kind() const { return kind_; }

  bool IsFallthrough() const { return kind_ == Kind::kBranchOrFallthough; }
  bool IsCall() const { return kind_ == Kind::kCall; }
  bool IsReturn() const { return kind_ == Kind::kRet; }

  // TODO(rahmanl): implement this. Ref: b/154263650
  bool IsFallthroughEdge() const;

  static std::string GetCfgEdgeKindString(Kind kind);

 private:
  friend class ControlFlowGraph;
  friend class PropellerWholeProgramInfo;

  void IncrementWeight(uint64_t increment) { weight_ += increment; }
  void ReplaceSink(CFGNode *sink) { sink_ = sink; }

  static std::string GetDotFormatLabelForEdgeKind(Kind kind);
  // Returns a string to be used as the label in the dot format.
  std::string GetDotFormatLabel() const {
    return absl::StrCat(GetDotFormatLabelForEdgeKind(kind_), "#", weight_);
  }
  CFGNode *src_ = nullptr;
  CFGNode *sink_ = nullptr;
  uint64_t weight_ = 0;
  const Kind kind_;
};

// All instances of CFGNode are owned by their cfg_.
class CFGNode final {
 public:
  explicit CFGNode(uint64_t symbol_ordinal, uint64_t addr, int bb_index,
                   uint64_t size, bool is_landing_pad, ControlFlowGraph *cfg,
                   uint64_t freq = 0)
      : symbol_ordinal_(symbol_ordinal),
        addr_(addr),
        bb_index_(bb_index),
        freq_(freq),
        size_(size),
        is_landing_pad_(is_landing_pad),
        cfg_(cfg) {}

  uint64_t symbol_ordinal() const { return symbol_ordinal_; }
  uint64_t addr() const { return addr_; }
  int bb_index() const { return bb_index_; }
  uint64_t freq() const { return freq_; }
  uint64_t size() const { return size_ ;}
  bool is_landing_pad() const { return is_landing_pad_; }
  ControlFlowGraph* cfg() const { return cfg_; }
  CFGNodeBundle *bundle() const { return bundle_; }
  int64_t bundle_offset() const { return bundle_offset_; }

  const std::vector<CFGEdge *> &intra_outs() const { return intra_outs_; }
  const std::vector<CFGEdge *> &intra_ins() const { return intra_ins_; }
  const std::vector<CFGEdge *> &inter_outs() const { return inter_outs_; }
  const std::vector<CFGEdge *> &inter_ins() const { return inter_ins_; }

  void ForEachInEdgeRef(absl::FunctionRef<void(CFGEdge &edge)> func) const {
    for (CFGEdge *edge : intra_ins_) func(*edge);
    for (CFGEdge *edge : inter_ins_) func(*edge);
  }

  void ForEachOutEdgeRef(absl::FunctionRef<void(CFGEdge &edge)> func) const {
    for (CFGEdge *edge : intra_outs_) func(*edge);
    for (CFGEdge *edge : inter_outs_) func(*edge);
  }

  bool is_entry() const { return bb_index_ == 0; }

  std::string GetName() const;

 private:
  friend class ControlFlowGraph;
  friend class CFGNodeBundle;

  friend void PrintTo(const CFGNode &node, std::ostream *os) {
    *os << absl::StreamFormat(
        "CFGNode {symbol_ordinal: %llu, address: %llX, bb_index: %d, "
        "frequency: "
        "%llu, size: %X, CFG: %p}",
        node.symbol_ordinal(), node.addr(), node.bb_index(), node.freq(),
        node.size(), node.cfg());
  }

  void GrowOnCoallesce(uint64_t size_increment) { size_ += size_increment; }

  // Returns the bb index as a string to be used in the dot format.
  std::string GetDotFormatLabel() const { return absl::StrCat(bb_index_); }

  void set_freq(uint64_t freq) { freq_ = freq; }

  void set_is_landing_pad(bool is_landing_pad) {
    is_landing_pad_ = is_landing_pad;
  }

  void set_bundle(CFGNodeBundle *bundle) {
    DCHECK_EQ(bundle_, nullptr);
    bundle_ = bundle;
  }

  void set_bundle_offset(int64_t bundle_offset) {
    bundle_offset_ = bundle_offset;
  }

  const uint64_t symbol_ordinal_;
  const uint64_t addr_;
  // Zero-based index of the basic block in the function. A zero value indicates
  // the entry basic block.
  const int bb_index_;
  uint64_t freq_ = 0;
  uint64_t size_ = 0;
  bool is_landing_pad_ = false;
  ControlFlowGraph * const cfg_;

  std::vector<CFGEdge *> intra_outs_ = {};  // Intra function edges.
  std::vector<CFGEdge *> intra_ins_ = {};   // Intra function edges.
  std::vector<CFGEdge *> inter_outs_ = {};  // Calls to other functions.
  std::vector<CFGEdge *> inter_ins_ = {};   // Returns from other functions.

  CFGNodeBundle *bundle_ = nullptr;
  int64_t bundle_offset_ = 0;
};

struct CFGNodePtrLessComparator {
  bool operator()(const CFGNode* a,
                  const CFGNode* b) const {
    if (!a && b) return true;
    if (a && !b) return false;
    if (!a && !b) return false;
    return a->symbol_ordinal() < b->symbol_ordinal();
  }
};

struct CFGNodeUniquePtrLessComparator {
  bool operator()(const std::unique_ptr<CFGNode> &a,
                  const std::unique_ptr<CFGNode> &b) const {
    return CFGNodePtrLessComparator()(a.get(), b.get());
  }
};

class ControlFlowGraph {
 public:
  explicit ControlFlowGraph(const llvm::SmallVectorImpl<llvm::StringRef> &names)
      : names_(names.begin(), names.end()) {}
  explicit ControlFlowGraph(llvm::SmallVectorImpl<llvm::StringRef> &&names)
      : names_(std::move(names)) {}

  int n_landing_pads() const { return n_landing_pads_; }
  int n_hot_landing_pads() const { return n_hot_landing_pads_; }

  CFGNode *GetEntryNode() const {
    CHECK(!nodes_.empty());
    return nodes_.begin()->get();
  }

  llvm::StringRef GetPrimaryName() const {
    CHECK(!names_.empty());
    return names_.front();
  }

  bool IsHot() const {
    if (nodes_.empty()) return false;
    return hot_tag_;
  }

  template <class Visitor>
  void ForEachNodeRef(Visitor v) {
    for (auto &N : nodes_) v(*N);
  }

  // Creates a CFGNode for every BBAddrMap entry and associates integer ordinals
  // to the nodes from ordinal to ordinal+nodes.size()-1. ControlFlowGraph will
  // take ownership of the created nodes.
  void CreateNodes(const llvm::object::BBAddrMap &func_bb_addr_map,
                   uint64_t ordinal);

  // Create edge and take ownership. Note: the caller must be responsible for
  // not creating duplicated edges.
  CFGEdge *CreateEdge(CFGNode *from, CFGNode *to, uint64_t weight,
                      CFGEdge::Kind kind);

  // Computes and sets node frequencies based edge weights. This must be called
  // after constructing all nodes and edges.
  void CalculateNodeFreqs();

  const llvm::SmallVectorImpl<llvm::StringRef> &names() const {
    return names_;
  }
  const std::set<std::unique_ptr<CFGNode>, CFGNodeUniquePtrLessComparator>
      &nodes() const {
    return nodes_;
  }

  const std::vector<std::unique_ptr<CFGEdge>> &intra_edges() const {
    return intra_edges_;
  }

  const std::vector<std::unique_ptr<CFGEdge>> &inter_edges() const {
    return inter_edges_;
  }

  // APIs for test purposes.
  static std::unique_ptr<ControlFlowGraph> CreateForTest(llvm::StringRef name) {
    return std::make_unique<ControlFlowGraph>(
        llvm::SmallVector<llvm::StringRef, 4>({name}));
  }

  CFGNode *InsertNodeForTest(std::unique_ptr<CFGNode> node) {
    return nodes_.insert(std::move(node)).first->get();
  }

  // Writes the dot format of CFG into the given stream. The second argument
  // specifies a the layout by mapping every hot basic block's bb_index_ to its
  // position in the layout. Fall-through edges will be colored differently
  // (red) in the dot format.
  void WriteDotFormat(std::ostream &os,
      const absl::flat_hash_map<int, int> &layout_index_map) const;

 private:
  friend class MockPropellerWholeProgramInfo;
  friend class PropellerProfWriter;

  bool hot_tag_ = false;
  int n_landing_pads_ = 0;
  int n_hot_landing_pads_ = 0;

  // Function names associated with this CFG: The first name is the primary
  // function name and the rest are aliases. The primary name is necessary.
  llvm::SmallVector<llvm::StringRef, 3> names_;

  // CFGs own all nodes. Nodes here are *strictly* sorted by addresses /
  // ordinals.
  std::set<std::unique_ptr<CFGNode>, CFGNodeUniquePtrLessComparator> nodes_;

  // CFGs own all edges. All edges are owned by their src's CFGs and they
  // appear exactly once in one of the following two fields. The src and sink
  // nodes of each edge contain a pointer to the edge, which means, each edge is
  // recorded exactly twice in Nodes' inter_ins_, inter_outs, intra_ints or
  // intra_out_.
  std::vector<std::unique_ptr<CFGEdge>> intra_edges_;
  std::vector<std::unique_ptr<CFGEdge>> inter_edges_;
};
}  // namespace devtools_crosstool_autofdo
#endif  // AUTOFDO_LLVM_PROPELLER_CFG_H_
