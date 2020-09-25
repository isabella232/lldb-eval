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

#include "ast.h"
#include "ast_visitor.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <sstream>
#include <utility>

namespace eval_fuzzer {

struct BinOpInfo {
  int precedence;
  const char* symbol;
};

/**
 * Precedence and symbol for each binary operator.
 *
 * Precedence values are taken from the following:
 * https://en.cppreference.com/w/cpp/language/operator_precedence
 */
static const BinOpInfo BIN_OP_TABLE[] = {
  {6,  "+"  }, // BinOp::Plus
  {6,  "-"  }, // BinOp::Minus
  {5,  "*"  }, // BinOp::Mult
  {5,  "/"  }, // BinOp::Div
  {5,  "%"  }, // BinOp::Mod
  {14, "&&" }, // BinOp::LogicalAnd
  {15, "||" }, // BinOp::LogicalOr
  {11, "&"  }, // BinOp::BitAnd
  {13, "|"  }, // BinOp::BitOr
  {12, "^"  }, // BinOp::BitXor
  {7,  "<<" }, // BinOp::Shl
  {7,  ">>" }, // BinOp::Shr
  {10, "==" }, // BinOp::Eq
  {10, "!=" }, // BinOp::Ne
  {9,  "<"  }, // BinOp::Lt
  {9,  "<=" }, // BinOp::Le
  {9,  ">"  }, // BinOp::Gt
  {9,  ">=" }, // BinOp::Ge
};

static const char* UN_OP_TABLE[((size_t) UnOp::EnumLast) + 1] = {
  "+", // UnOp::Plus
  "-", // UnOp::Neg
  "!", // UnOp::LogicalNot
  "~", // UnOp::BitNot
};

BinaryExpr::BinaryExpr(Expr lhs, BinOp op, Expr rhs, bool gen_parens)
  : m_lhs(std::make_unique<Expr>(std::move(lhs)))
  , m_rhs(std::make_unique<Expr>(std::move(rhs)))
  , m_op(op)
  , m_gen_parens(gen_parens)
{}

const Expr& BinaryExpr::lhs() const { return *m_lhs; }
const Expr& BinaryExpr::rhs() const { return *m_rhs; }
BinOp BinaryExpr::op() const { return m_op; }
bool BinaryExpr::gen_parens() const { return m_gen_parens; }
int BinaryExpr::precedence() const {
  return BIN_OP_TABLE[(size_t) m_op].precedence;
}

VariableExpr::VariableExpr(std::string name, bool gen_parens)
  : m_name(std::move(name))
  , m_gen_parens(gen_parens)
{}

const std::string& VariableExpr::name() const { return m_name; }
bool VariableExpr::gen_parens() const { return m_gen_parens; }

UnaryExpr::UnaryExpr(UnOp op, Expr expr, bool gen_parens)
  : m_expr(std::make_unique<Expr>(std::move(expr)))
  , m_op(op)
  , m_gen_parens(gen_parens)
{}

UnOp UnaryExpr::op() const { return m_op; }
const Expr& UnaryExpr::expr() const { return *m_expr; }
bool UnaryExpr::gen_parens() const { return m_gen_parens; }

IntegerConstant::IntegerConstant(uint64_t value, bool gen_parens)
  : m_value(value)
  , m_gen_parens(gen_parens)
{}

uint64_t IntegerConstant::value() const { return m_value; }
bool IntegerConstant::gen_parens() const { return m_gen_parens; }

DoubleConstant::DoubleConstant(double value, bool gen_parens)
  : m_value(value)
  , m_gen_parens(gen_parens)
{}

double DoubleConstant::value() const { return m_value; }
bool DoubleConstant::gen_parens() const { return m_gen_parens; }

/**
 * A visitor that converts an expression into a string.
 */
class ExprPrinter final: public DefaultVisitor {
  using DefaultVisitor::visit;

  std::ostringstream m_os;

  void visit(const BinaryExpr& e) override {
    bool gen_parens = e.gen_parens();
    if (gen_parens) {
      m_os << "(";
    }

    // Rules for parenthesising the left hand side:
    // 1. If the left hand side is already parenthesised, no need to do
    //    anything.
    // 2. If the left hand side has a strictly lower precedence than ours,
    //    then we will have to emit parens.
    //    Example: We emit `(3 + 4) * 5` instead of `3 + 4 * 5`.
    // 3. If the left hand side has the same precedence as we do, then we
    //    don't have to emit any parens. This is because all lldb-eval
    //    binary operators have left-to-right associativity.
    //    Example: We do not have to emit `(3 - 4) + 5`, `3 - 4 + 5` will also
    //    do.
    const auto* lhs_bin = std::get_if<BinaryExpr>(&e.lhs());
    bool lhs_gen_parens = lhs_bin != nullptr
        && !lhs_bin->gen_parens()
        && lhs_bin->precedence() < e.precedence();

    if (lhs_gen_parens) {
      m_os << "(";
      visit(e.lhs());
      m_os << ")";
    } else {
      visit(e.lhs());
    }

    m_os << " " << BIN_OP_TABLE[(size_t) e.op()].symbol << " ";

    // Rules for parenthesising the right hand side:
    // 1. If the right hand side is already parenthesised, no need to do
    //    anything.
    // 2. If the right hand side has a strictly lower precedence than ours,
    //    then we will have to emit parens.
    //    Example: We emit `5 * (3 + 4)` instead of `5 * 3 + 4`.
    // 3. If the right hand side has the same precedence as we do, then we
    //    should emit parens for good measure. This is because all lldb-eval
    //    binary operators have left-to-right associativity and we do not
    //    want to violate this with respect to the generated AST.
    //    Example: We emit `3 - (4 + 5)` instead of `3 - 4 + 5`. We also
    //    emit `3 + (4 + 5)` instead of `3 + 4 + 5`, even though both
    //    expressions are equivalent.
    const auto* rhs_bin = std::get_if<BinaryExpr>(&e.rhs());
    bool rhs_gen_parens = rhs_bin != nullptr
        && !rhs_bin->gen_parens()
        && rhs_bin->precedence() <= e.precedence();
    if (rhs_gen_parens) {
      m_os << "(";
      visit(e.rhs());
      m_os << ")";
    } else {
      visit(e.rhs());
    }

    if (gen_parens) {
      m_os << ")";
    }
  }

  void visit(const VariableExpr& e) override {
    if (e.gen_parens()) {
      m_os << "(";
    }

    m_os << e.name();

    if (e.gen_parens()) {
      m_os << ")";
    }
  }

  void visit(const IntegerConstant& e) override {
    if (e.gen_parens()) {
      m_os << "(";
    }

    m_os << e.value();

    if (e.gen_parens()) {
      m_os << ")";
    }
  }

  void visit(const DoubleConstant& e) override {
    if (e.gen_parens()) {
      m_os << "(";
    }

    m_os << e.value();

    if (e.gen_parens()) {
      m_os << ")";
    }
  }

  void visit(const UnaryExpr& e) override {
    if (e.gen_parens()) {
      m_os << "(";
    }

    m_os << UN_OP_TABLE[(size_t) e.op()];

    const auto* inner_as_binary = std::get_if<BinaryExpr>(&e.expr());
    const auto* inner_as_unary = std::get_if<UnaryExpr>(&e.expr());
    if (inner_as_binary != nullptr) {
      // Case 1: Inner expression is a binary expression: Just be conservative
      // and emit parentheses
      m_os << "(";
      visit(*inner_as_binary);
      m_os << ")";

    } else if (inner_as_unary != nullptr) {
      // Case 2: Avoiding emitting cases such as `++3` or `--3`
      bool needs_space = (e.op() == UnOp::Plus || e.op() == UnOp::Neg)
          && e.op() == inner_as_unary->op()
          && !inner_as_unary->gen_parens();

      if (needs_space) {
        m_os << ' ';
      }
      visit(*inner_as_unary);
    } else {
      visit(e.expr());
    }

    if (e.gen_parens()) {
      m_os << ")";
    }
  }

 public:
  std::string to_string() const { return m_os.str(); }
};

std::string stringify_expr(const Expr& expr) {
  ExprPrinter printer;
  printer.DefaultVisitor::visit(expr);

  return printer.to_string();
}

/**
 * A visitor that dumps an expression to `stdout` for debugging purposes.
 */
class ExprDumper final: public DefaultVisitor {
  using DefaultVisitor::visit;

  int indent = 0;

  static constexpr int SPACES_PER_INDENTATION = 2;

  void visit(const BinaryExpr& e) override {
    const auto* symbol = BIN_OP_TABLE[(size_t) e.op()].symbol;
    printf("%*sBinary expression of type %s\n", indent, "", symbol);

    indent += SPACES_PER_INDENTATION;

    visit(e.lhs());
    visit(e.rhs());

    indent -= SPACES_PER_INDENTATION;
  }

  void visit(const VariableExpr& e) override {
    printf("%*sVariable expression for identifier `%s`\n", indent, "", e.name().c_str());
  }

  void visit(const IntegerConstant& e) override {
    printf("%*sInteger constant with value `%" PRIu64 "`\n", indent, "", e.value());
  }

  void visit(const DoubleConstant& e) override {
    printf("%*sDouble constant with value `%f`\n", indent, "", e.value());
  }

  void visit(const UnaryExpr& e) override {
    const auto* symbol = UN_OP_TABLE[(size_t) e.op()];
    printf("%*sUnary expression of type %s\n", indent, "", symbol);

    indent += SPACES_PER_INDENTATION;

    visit(e.expr());

    indent -= SPACES_PER_INDENTATION;
  }
};

void dump_expr(const Expr& expr) {
  ExprDumper dumper;
  dumper.DefaultVisitor::visit(expr);
}

} // namespace eval_fuzzer
