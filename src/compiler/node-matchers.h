// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_MATCHERS_H_
#define V8_COMPILER_NODE_MATCHERS_H_

#include <cmath>

#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

// A pattern matcher for nodes.
struct NodeMatcher {
  explicit NodeMatcher(Node* node) : node_(node) {}

  Node* node() const { return node_; }
  const Operator* op() const { return node()->op(); }
  IrOpcode::Value opcode() const { return node()->opcode(); }

  bool HasProperty(Operator::Property property) const {
    return op()->HasProperty(property);
  }
  Node* InputAt(int index) const { return node()->InputAt(index); }

#define DEFINE_IS_OPCODE(Opcode) \
  bool Is##Opcode() const { return opcode() == IrOpcode::k##Opcode; }
  ALL_OP_LIST(DEFINE_IS_OPCODE)
#undef DEFINE_IS_OPCODE

 private:
  Node* node_;
};


// A pattern matcher for abitrary value constants.
template <typename T, IrOpcode::Value kOpcode>
struct ValueMatcher : public NodeMatcher {
  typedef T ValueType;

  explicit ValueMatcher(Node* node)
      : NodeMatcher(node), value_(), has_value_(opcode() == kOpcode) {
    if (has_value_) {
      value_ = OpParameter<T>(node);
    }
  }

  bool HasValue() const { return has_value_; }
  const T& Value() const {
    DCHECK(HasValue());
    return value_;
  }

  bool Is(const T& value) const {
    return this->HasValue() && this->Value() == value;
  }

  bool IsInRange(const T& low, const T& high) const {
    return this->HasValue() && low <= this->Value() && this->Value() <= high;
  }

 private:
  T value_;
  bool has_value_;
};


template <>
inline ValueMatcher<int64_t, IrOpcode::kInt64Constant>::ValueMatcher(Node* node)
    : NodeMatcher(node), value_(), has_value_(false) {
  if (opcode() == IrOpcode::kInt32Constant) {
    value_ = OpParameter<int32_t>(node);
    has_value_ = true;
  } else if (opcode() == IrOpcode::kInt64Constant) {
    value_ = OpParameter<int64_t>(node);
    has_value_ = true;
  }
}


template <>
inline ValueMatcher<uint64_t, IrOpcode::kInt64Constant>::ValueMatcher(
    Node* node)
    : NodeMatcher(node), value_(), has_value_(false) {
  if (opcode() == IrOpcode::kInt32Constant) {
    value_ = OpParameter<uint32_t>(node);
    has_value_ = true;
  } else if (opcode() == IrOpcode::kInt64Constant) {
    value_ = OpParameter<uint64_t>(node);
    has_value_ = true;
  }
}


// A pattern matcher for integer constants.
template <typename T, IrOpcode::Value kOpcode>
struct IntMatcher FINAL : public ValueMatcher<T, kOpcode> {
  explicit IntMatcher(Node* node) : ValueMatcher<T, kOpcode>(node) {}

  bool IsPowerOf2() const {
    return this->HasValue() && this->Value() > 0 &&
           (this->Value() & (this->Value() - 1)) == 0;
  }
  bool IsNegativePowerOf2() const {
    return this->HasValue() && this->Value() < 0 &&
           (-this->Value() & (-this->Value() - 1)) == 0;
  }
};

typedef IntMatcher<int32_t, IrOpcode::kInt32Constant> Int32Matcher;
typedef IntMatcher<uint32_t, IrOpcode::kInt32Constant> Uint32Matcher;
typedef IntMatcher<int64_t, IrOpcode::kInt64Constant> Int64Matcher;
typedef IntMatcher<uint64_t, IrOpcode::kInt64Constant> Uint64Matcher;
#if V8_HOST_ARCH_32_BIT
typedef Int32Matcher IntPtrMatcher;
typedef Uint32Matcher UintPtrMatcher;
#else
typedef Int64Matcher IntPtrMatcher;
typedef Uint64Matcher UintPtrMatcher;
#endif


// A pattern matcher for floating point constants.
template <typename T, IrOpcode::Value kOpcode>
struct FloatMatcher FINAL : public ValueMatcher<T, kOpcode> {
  explicit FloatMatcher(Node* node) : ValueMatcher<T, kOpcode>(node) {}

  bool IsMinusZero() const {
    return this->Is(0.0) && std::signbit(this->Value());
  }
  bool IsNaN() const { return this->HasValue() && std::isnan(this->Value()); }
};

typedef FloatMatcher<float, IrOpcode::kFloat32Constant> Float32Matcher;
typedef FloatMatcher<double, IrOpcode::kFloat64Constant> Float64Matcher;
typedef FloatMatcher<double, IrOpcode::kNumberConstant> NumberMatcher;


// A pattern matcher for heap object constants.
template <typename T>
struct HeapObjectMatcher FINAL
    : public ValueMatcher<Unique<T>, IrOpcode::kHeapConstant> {
  explicit HeapObjectMatcher(Node* node)
      : ValueMatcher<Unique<T>, IrOpcode::kHeapConstant>(node) {}
};


// For shorter pattern matching code, this struct matches both the left and
// right hand sides of a binary operation and can put constants on the right
// if they appear on the left hand side of a commutative operation.
template <typename Left, typename Right>
struct BinopMatcher : public NodeMatcher {
  explicit BinopMatcher(Node* node)
      : NodeMatcher(node), left_(InputAt(0)), right_(InputAt(1)) {
    if (HasProperty(Operator::kCommutative)) PutConstantOnRight();
  }
  BinopMatcher(Node* node, bool allow_input_swap)
      : NodeMatcher(node), left_(InputAt(0)), right_(InputAt(1)) {
    if (allow_input_swap) PutConstantOnRight();
  }

  typedef Left LeftMatcher;
  typedef Right RightMatcher;

  const Left& left() const { return left_; }
  const Right& right() const { return right_; }

  bool IsFoldable() const { return left().HasValue() && right().HasValue(); }
  bool LeftEqualsRight() const { return left().node() == right().node(); }

 protected:
  void SwapInputs() {
    std::swap(left_, right_);
    node()->ReplaceInput(0, left().node());
    node()->ReplaceInput(1, right().node());
  }

 private:
  void PutConstantOnRight() {
    if (left().HasValue() && !right().HasValue()) {
      SwapInputs();
    }
  }

  Left left_;
  Right right_;
};

typedef BinopMatcher<Int32Matcher, Int32Matcher> Int32BinopMatcher;
typedef BinopMatcher<Uint32Matcher, Uint32Matcher> Uint32BinopMatcher;
typedef BinopMatcher<Int64Matcher, Int64Matcher> Int64BinopMatcher;
typedef BinopMatcher<Uint64Matcher, Uint64Matcher> Uint64BinopMatcher;
typedef BinopMatcher<IntPtrMatcher, IntPtrMatcher> IntPtrBinopMatcher;
typedef BinopMatcher<UintPtrMatcher, UintPtrMatcher> UintPtrBinopMatcher;
typedef BinopMatcher<Float64Matcher, Float64Matcher> Float64BinopMatcher;
typedef BinopMatcher<NumberMatcher, NumberMatcher> NumberBinopMatcher;


template <class BinopMatcher, IrOpcode::Value kMulOpcode,
          IrOpcode::Value kShiftOpcode>
struct ScaleMatcher {
  explicit ScaleMatcher(Node* node, bool allow_power_of_two_plus_one = false)
      : scale_(-1), power_of_two_plus_one_(false) {
    if (node->InputCount() < 2) return;
    BinopMatcher m(node);
    if (node->opcode() == kShiftOpcode) {
      if (m.right().HasValue()) {
        typename BinopMatcher::RightMatcher::ValueType value =
            m.right().Value();
        if (value >= 0 && value <= 3) {
          scale_ = static_cast<int>(value);
        }
      }
    } else if (node->opcode() == kMulOpcode) {
      if (m.right().HasValue()) {
        typename BinopMatcher::RightMatcher::ValueType value =
            m.right().Value();
        if (value == 1) {
          scale_ = 0;
        } else if (value == 2) {
          scale_ = 1;
        } else if (value == 4) {
          scale_ = 2;
        } else if (value == 8) {
          scale_ = 3;
        } else if (allow_power_of_two_plus_one) {
          if (value == 3) {
            scale_ = 1;
            power_of_two_plus_one_ = true;
          } else if (value == 5) {
            scale_ = 2;
            power_of_two_plus_one_ = true;
          } else if (value == 9) {
            scale_ = 3;
            power_of_two_plus_one_ = true;
          }
        }
      }
    }
  }

  bool matches() const { return scale_ != -1; }
  int scale() const { return scale_; }
  bool power_of_two_plus_one() const { return power_of_two_plus_one_; }

 private:
  int scale_;
  bool power_of_two_plus_one_;
};

typedef ScaleMatcher<Int32BinopMatcher, IrOpcode::kInt32Mul,
                     IrOpcode::kWord32Shl> Int32ScaleMatcher;
typedef ScaleMatcher<Int64BinopMatcher, IrOpcode::kInt64Mul,
                     IrOpcode::kWord64Shl> Int64ScaleMatcher;


template <class BinopMatcher, IrOpcode::Value kAddOpcode,
          IrOpcode::Value kMulOpcode, IrOpcode::Value kShiftOpcode>
struct AddMatcher : public BinopMatcher {
  static const IrOpcode::Value kOpcode = kAddOpcode;
  typedef ScaleMatcher<BinopMatcher, kMulOpcode, kShiftOpcode> Matcher;

  AddMatcher(Node* node, bool allow_input_swap)
      : BinopMatcher(node, allow_input_swap),
        scale_(-1),
        power_of_two_plus_one_(false) {
    Initialize(node, allow_input_swap);
  }
  explicit AddMatcher(Node* node)
      : BinopMatcher(node, node->op()->HasProperty(Operator::kCommutative)),
        scale_(-1),
        power_of_two_plus_one_(false) {
    Initialize(node, node->op()->HasProperty(Operator::kCommutative));
  }

  bool HasIndexInput() const { return scale_ != -1; }
  Node* IndexInput() const {
    DCHECK(HasIndexInput());
    return this->left().node()->InputAt(0);
  }
  int scale() const {
    DCHECK(HasIndexInput());
    return scale_;
  }
  bool power_of_two_plus_one() const { return power_of_two_plus_one_; }

 private:
  void Initialize(Node* node, bool allow_input_swap) {
    Matcher left_matcher(this->left().node(), true);
    if (left_matcher.matches()) {
      scale_ = left_matcher.scale();
      power_of_two_plus_one_ = left_matcher.power_of_two_plus_one();
      return;
    }

    if (!allow_input_swap) {
      return;
    }

    Matcher right_matcher(this->right().node(), true);
    if (right_matcher.matches()) {
      scale_ = right_matcher.scale();
      power_of_two_plus_one_ = right_matcher.power_of_two_plus_one();
      this->SwapInputs();
      return;
    }

    if (this->right().opcode() == kAddOpcode &&
        this->left().opcode() != kAddOpcode) {
      this->SwapInputs();
    }
  }

  int scale_;
  bool power_of_two_plus_one_;
};

typedef AddMatcher<Int32BinopMatcher, IrOpcode::kInt32Add, IrOpcode::kInt32Mul,
                   IrOpcode::kWord32Shl> Int32AddMatcher;
typedef AddMatcher<Int64BinopMatcher, IrOpcode::kInt64Add, IrOpcode::kInt64Mul,
                   IrOpcode::kWord64Shl> Int64AddMatcher;


template <class AddMatcher>
struct BaseWithIndexAndDisplacementMatcher {
  BaseWithIndexAndDisplacementMatcher(Node* node, bool allow_input_swap)
      : matches_(false),
        index_(NULL),
        scale_(0),
        base_(NULL),
        displacement_(NULL) {
    Initialize(node, allow_input_swap);
  }

  explicit BaseWithIndexAndDisplacementMatcher(Node* node)
      : matches_(false),
        index_(NULL),
        scale_(0),
        base_(NULL),
        displacement_(NULL) {
    Initialize(node, node->op()->HasProperty(Operator::kCommutative));
  }

  bool matches() const { return matches_; }
  Node* index() const { return index_; }
  int scale() const { return scale_; }
  Node* base() const { return base_; }
  Node* displacement() const { return displacement_; }

 private:
  bool matches_;
  Node* index_;
  int scale_;
  Node* base_;
  Node* displacement_;

  void Initialize(Node* node, bool allow_input_swap) {
    // The BaseWithIndexAndDisplacementMatcher canonicalizes the order of
    // displacements and scale factors that are used as inputs, so instead of
    // enumerating all possible patterns by brute force, checking for node
    // clusters using the following templates in the following order suffices to
    // find all of the interesting cases (S = index * scale, B = base input, D =
    // displacement input):
    // (S + (B + D))
    // (S + (B + B))
    // (S + D)
    // (S + B)
    // ((S + D) + B)
    // ((S + B) + D)
    // ((B + D) + B)
    // ((B + B) + D)
    // (B + D)
    // (B + B)
    if (node->InputCount() < 2) return;
    AddMatcher m(node, allow_input_swap);
    Node* left = m.left().node();
    Node* right = m.right().node();
    Node* displacement = NULL;
    Node* base = NULL;
    Node* index = NULL;
    Node* scale_expression = NULL;
    bool power_of_two_plus_one = false;
    int scale = 0;
    if (m.HasIndexInput() && left->OwnedBy(node)) {
      index = m.IndexInput();
      scale = m.scale();
      scale_expression = left;
      power_of_two_plus_one = m.power_of_two_plus_one();
      if (right->opcode() == AddMatcher::kOpcode && right->OwnedBy(node)) {
        AddMatcher right_matcher(right);
        if (right_matcher.right().HasValue()) {
          // (S + (B + D))
          base = right_matcher.left().node();
          displacement = right_matcher.right().node();
        } else {
          // (S + (B + B))
          base = right;
        }
      } else if (m.right().HasValue()) {
        // (S + D)
        displacement = right;
      } else {
        // (S + B)
        base = right;
      }
    } else {
      if (left->opcode() == AddMatcher::kOpcode && left->OwnedBy(node)) {
        AddMatcher left_matcher(left);
        Node* left_left = left_matcher.left().node();
        Node* left_right = left_matcher.right().node();
        if (left_matcher.HasIndexInput() && left_left->OwnedBy(left)) {
          if (left_matcher.right().HasValue()) {
            // ((S + D) + B)
            index = left_matcher.IndexInput();
            scale = left_matcher.scale();
            scale_expression = left_left;
            power_of_two_plus_one = left_matcher.power_of_two_plus_one();
            displacement = left_right;
            base = right;
          } else if (m.right().HasValue()) {
            // ((S + B) + D)
            index = left_matcher.IndexInput();
            scale = left_matcher.scale();
            scale_expression = left_left;
            power_of_two_plus_one = left_matcher.power_of_two_plus_one();
            base = left_right;
            displacement = right;
          } else {
            // (B + B)
            index = left;
            base = right;
          }
        } else {
          if (left_matcher.right().HasValue()) {
            // ((B + D) + B)
            index = left_left;
            displacement = left_right;
            base = right;
          } else if (m.right().HasValue()) {
            // ((B + B) + D)
            index = left_left;
            base = left_right;
            displacement = right;
          } else {
            // (B + B)
            index = left;
            base = right;
          }
        }
      } else {
        if (m.right().HasValue()) {
          // (B + D)
          base = left;
          displacement = right;
        } else {
          // (B + B)
          base = left;
          index = right;
        }
      }
    }
    int64_t value = 0;
    if (displacement != NULL) {
      switch (displacement->opcode()) {
        case IrOpcode::kInt32Constant: {
          value = OpParameter<int32_t>(displacement);
          break;
        }
        case IrOpcode::kInt64Constant: {
          value = OpParameter<int64_t>(displacement);
          break;
        }
        default:
          UNREACHABLE();
          break;
      }
      if (value == 0) {
        displacement = NULL;
      }
    }
    if (power_of_two_plus_one) {
      if (base != NULL) {
        // If the scale requires explicitly using the index as the base, but a
        // base is already part of the match, then the (1 << N + 1) scale factor
        // can't be folded into the match and the entire index * scale
        // calculation must be computed separately.
        index = scale_expression;
        scale = 0;
      } else {
        base = index;
      }
    }
    base_ = base;
    displacement_ = displacement;
    index_ = index;
    scale_ = scale;
    matches_ = true;
  }
};

typedef BaseWithIndexAndDisplacementMatcher<Int32AddMatcher>
    BaseWithIndexAndDisplacement32Matcher;
typedef BaseWithIndexAndDisplacementMatcher<Int64AddMatcher>
    BaseWithIndexAndDisplacement64Matcher;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_MATCHERS_H_
