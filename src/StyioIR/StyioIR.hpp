#pragma once
#ifndef STYIO_IR_BASE_H_
#define STYIO_IR_BASE_H_

// [C++ STL]
#include <string>
#include <vector>

// [LLVM]
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

// [Styio]
#include "../StyioToString/ToStringVisitor.hpp"
#include "../StyioCodeGen/CodeGenVisitor.hpp"
#include "IRDecl.hpp"

class StyioIR
{
public:
  virtual ~StyioIR() {}

  /* StyioAST to String */
  virtual std::string toString(StyioRepr* visitor, int indent = 0) = 0;

  /* Get LLVM Type */
  virtual llvm::Type* toLLVMType(StyioToLLVM* visitor) = 0;

  /* LLVM IR Generator */
  virtual llvm::Value* toLLVMIR(StyioToLLVM* visitor) = 0;

  /* True when this node belongs to the active StyioIR surface. */
  virtual bool is_active() const = 0;

  /* Collect direct child IR node pointers for non-recursive traversal/destruction.
     Default: no children (leaf nodes). Override in non-leaf IR node classes. */
  virtual void collect_children(std::vector<StyioIR*>& out) {}
};

template <class T>
inline void
styio_delete_ir_nodes(std::vector<T*>& nodes) noexcept {
  for (auto* node : nodes) {
    delete node;
  }
  nodes.clear();
}

/// Non-recursive iterative IR subtree deletion — avoids stack overflow
/// on deep IR trees. Uses collect_children() to enumerate all nodes
/// in pre-order, then deletes in reverse (post-order).
template <class T>
inline void
styio_delete_ir_nodes_iterative(std::vector<T*>& nodes) noexcept {
  if (nodes.empty()) return;

  // Phase 1: collect all reachable nodes (pre-order via explicit stack)
  std::vector<StyioIR*> all;
  for (auto* root : nodes) {
    if (!root) continue;
    std::vector<StyioIR*> stack;
    stack.push_back(root);
    while (!stack.empty()) {
      StyioIR* n = stack.back();
      stack.pop_back();
      all.push_back(n);
      // Push children for continued traversal
      size_t before = stack.size();
      n->collect_children(stack);
      // Reverse children so left-to-right order is preserved
      std::reverse(stack.begin() + before, stack.end());
    }
  }

  // Phase 2: delete in reverse (post-order equivalent)
  for (auto it = all.rbegin(); it != all.rend(); ++it) {
    delete *it;
  }
  nodes.clear();
}

/// Non-recursive IR subtree destruction (TASK-08).
/// Uses collect_children() to walk the tree without recursion,
/// calls destructors in post-order, then frees memory.
/// When an IR arena is active, individual deallocations are no-ops.
inline void destroy_ir_subtree(StyioIR* root) {
  if (!root) return;

  // Phase 1: collect all reachable nodes in pre-order
  std::vector<StyioIR*> all;
  std::vector<StyioIR*> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    StyioIR* n = stack.back();
    stack.pop_back();
    all.push_back(n);
    size_t before = stack.size();
    n->collect_children(stack);
    std::reverse(stack.begin() + before, stack.end());
  }

  // Phase 2: call destructors in reverse (post-order)
  for (auto it = all.rbegin(); it != all.rend(); ++it) {
    (*it)->~StyioIR();
  }

  // Phase 3: free memory (no-op when arena is active)
  if (all.empty()) return;
  auto* header = reinterpret_cast<styio::session_alloc::AllocationHeader*>(
    static_cast<std::byte*>(static_cast<void*>(all[0])) - sizeof(styio::session_alloc::AllocationHeader));
  if (header->arena == nullptr) {
    // Heap-allocated: free each node individually
    for (auto* n : all) {
      auto* h = reinterpret_cast<styio::session_alloc::AllocationHeader*>(
        static_cast<std::byte*>(static_cast<void*>(n)) - sizeof(styio::session_alloc::AllocationHeader));
      ::operator delete(static_cast<void*>(h));
    }
  }
  // Arena-allocated: arena reset handles bulk cleanup later
}

template <class Derived>
class StyioIRTraits : public StyioIR
{
public:
  std::string toString(StyioRepr* visitor, int indent = 0) override {
    return visitor->toString(static_cast<Derived*>(this), indent);
  }

  llvm::Type* toLLVMType(StyioToLLVM* visitor) override {
    return visitor->toLLVMType(static_cast<Derived*>(this));
  }

  llvm::Value* toLLVMIR(StyioToLLVM* visitor) override {
    return visitor->toLLVMIR(static_cast<Derived*>(this));
  }

  bool is_active() const override {
    return true;
  }
};

#endif // STYIO_IR_BASE_H_
