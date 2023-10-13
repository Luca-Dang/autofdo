// Copyright 2011 Google Inc. All Rights Reserved.
// Author: dehao@google.com (Dehao Chen)

// Class to build the map from instruction address to its info.

#include "instruction_map.h"

#include <string.h>

#include <cstdint>
#include <string>

#include "addr2line.h"
#include "symbol_map.h"

namespace devtools_crosstool_autofdo {

void InstructionMap::BuildPerFunctionInstructionMap(const std::string &name,
                                                    uint64_t start_addr,
                                                    uint64_t end_addr) {
  if (start_addr >= end_addr) {
    return;
  }

  // Make sure nobody has set up inst_map_ yet.
  CHECK(inst_map_.empty());

  start_addr_ = start_addr;
  inst_map_.resize(end_addr - start_addr);
  for (uint64_t addr = start_addr; addr < end_addr; addr++) {
    InstInfo *info = &inst_map_[addr - start_addr];
    addr2line_->GetInlineStack(addr, &info->source_stack);
    if (info->source_stack.size() > 0) {
      symbol_map_->AddSourceCount(name, info->source_stack, 0, 1, 1,
                                  SymbolMap::PERFDATA);
    }
  }
}

}  // namespace devtools_crosstool_autofdo
