// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOOP_ANALYSIS_H_
#define V8_COMPILER_LOOP_ANALYSIS_H_

#include "src/base/iterator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class LoopFinderImpl;

typedef base::iterator_range<Node**> NodeRange;

// Represents a tree of loops in a graph.
class LoopTree : public ZoneObject {
 public:
  LoopTree(size_t num_nodes, Zone* zone)
      : zone_(zone),
        outer_loops_(zone),
        all_loops_(zone),
        node_to_loop_num_(static_cast<int>(num_nodes), 0, zone),
        loop_nodes_(zone) {}

  // Represents a loop in the tree of loops, including the header nodes,
  // the body, and any nested loops.
  class Loop {
   public:
    Loop* parent() const { return parent_; }
    const ZoneVector<Loop*>& children() const { return children_; }
    size_t HeaderSize() const { return body_start_ - header_start_; }
    size_t BodySize() const { return body_end_ - body_start_; }
    size_t TotalSize() const { return body_end_ - header_start_; }

   private:
    friend class LoopTree;
    friend class LoopFinderImpl;

    explicit Loop(Zone* zone)
        : parent_(nullptr),
          depth_(0),
          children_(zone),
          header_start_(-1),
          body_start_(-1),
          body_end_(-1) {}
    Loop* parent_;
    int depth_;
    ZoneVector<Loop*> children_;
    int header_start_;
    int body_start_;
    int body_end_;
  };

  // Return the innermost nested loop, if any, that contains {node}.
  Loop* ContainingLoop(Node* node) {
    if (node->id() >= static_cast<int>(node_to_loop_num_.size()))
      return nullptr;
    uint8_t num = node_to_loop_num_[node->id()];
    return num > 0 ? &all_loops_[num - 1] : nullptr;
  }

  // Check if the {loop} contains the {node}, either directly or by containing
  // a nested loop that contains {node}.
  bool Contains(Loop* loop, Node* node) {
    for (Loop* c = ContainingLoop(node); c != nullptr; c = c->parent_) {
      if (c == loop) return true;
    }
    return false;
  }

  // Return the list of outer loops.
  const ZoneVector<Loop*>& outer_loops() const { return outer_loops_; }

  // Return the unique loop number for a given loop. Loop numbers start at {1}.
  int LoopNum(Loop* loop) const {
    return 1 + static_cast<int>(loop - &all_loops_[0]);
  }

  // Return a range which can iterate over the header nodes of {loop}.
  NodeRange HeaderNodes(Loop* loop) {
    return NodeRange(&loop_nodes_[0] + loop->header_start_,
                     &loop_nodes_[0] + loop->body_start_);
  }

  // Return a range which can iterate over the body nodes of {loop}.
  NodeRange BodyNodes(Loop* loop) {
    return NodeRange(&loop_nodes_[0] + loop->body_start_,
                     &loop_nodes_[0] + loop->body_end_);
  }

 private:
  friend class LoopFinderImpl;

  Loop* NewLoop() {
    all_loops_.push_back(Loop(zone_));
    Loop* result = &all_loops_.back();
    return result;
  }

  void SetParent(Loop* parent, Loop* child) {
    if (parent != nullptr) {
      parent->children_.push_back(child);
      child->parent_ = parent;
      child->depth_ = parent->depth_ + 1;
    } else {
      outer_loops_.push_back(child);
    }
  }

  Zone* zone_;
  ZoneVector<Loop*> outer_loops_;
  ZoneVector<Loop> all_loops_;
  // TODO(titzer): lift loop count restriction.
  ZoneVector<uint8_t> node_to_loop_num_;
  ZoneVector<Node*> loop_nodes_;
};


class LoopFinder {
 public:
  // Build a loop tree for the entire graph.
  static LoopTree* BuildLoopTree(Graph* graph, Zone* temp_zone);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOOP_ANALYSIS_H_
