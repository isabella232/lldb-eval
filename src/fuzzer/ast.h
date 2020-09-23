/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_AST_H
#define INCLUDE_AST_H

#include <cinttypes>
#include <memory>
#include <string>
#include <variant>

namespace eval_fuzzer {

class BinaryExpr;
class UnaryExpr;
class VariableExpr;
class IntegerConstant;
class DoubleConstant;

enum class UnOp: unsigned char {
  Plus, Neg, LogicalNot, BitNot, EnumLast = BitNot,
};

enum class BinOp: unsigned char {
  // Arithmetic operators
  Plus,
  Minus,
  Mult,
  Div,
  Mod,
  // Logical operators
  LogicalAnd,
  LogicalOr,
  // Bitwise operators
  BitAnd,
  BitOr,
  BitXor,
  Shl,
  Shr,
  // Comparison operators
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  // Used to determine the last enum element
  EnumLast = Ge,
};

using Expr = std::variant<
    IntegerConstant
  , DoubleConstant
  , VariableExpr
  , BinaryExpr
  , UnaryExpr
>;
constexpr size_t NUM_EXPR_KINDS = std::variant_size_v<Expr>;

class BinaryExpr {
  std::unique_ptr<Expr> m_lhs;
  std::unique_ptr<Expr> m_rhs;
  BinOp m_op;
  bool m_gen_parens;

public:
  BinaryExpr(Expr lhs, BinOp op, Expr rhs, bool gen_parens);

  const Expr& lhs() const;
  const Expr& rhs() const;
  BinOp op() const;
  bool gen_parens() const;
  int precedence() const;
};

class UnaryExpr {
  std::unique_ptr<Expr> m_expr;
  UnOp m_op;
  bool m_gen_parens;

 public:
  UnaryExpr(UnOp op, Expr expr, bool gen_parens);

  UnOp op() const;
  const Expr& expr() const;
  bool gen_parens() const;
};

class VariableExpr {
  std::string m_name;
  bool m_gen_parens;

 public:
  explicit VariableExpr(std::string name, bool gen_parens);

  const std::string& name() const;
  bool gen_parens() const;
};

class IntegerConstant {
  uint64_t m_value;
  bool m_gen_parens;

 public:
  IntegerConstant(uint64_t value, bool gen_parens);

  uint64_t value() const;
  bool gen_parens() const;
};

class DoubleConstant {
  double m_value;
  bool m_gen_parens;

 public:
  DoubleConstant(double value, bool gen_parens);

  double value() const;
  bool gen_parens() const;
};

std::string stringify_expr(const Expr& expr);
void dump_expr(const Expr& expr);

} // namespace eval_fuzzer

#endif // INCLUDE_AST_H
