/*
 * Copyright (C) 2015 Bailey Forrest <baileycforrest@gmail.com>
 *
 * This file is part of parp.
 *
 * parp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * parp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with parp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <utility>

#include "eval/eval.h"
#include "expr/expr.h"
#include "expr/number.h"
#include "expr/primitive.h"
#include "parse/parse.h"
#include "test/util.h"

using expr::Char;
using expr::Expr;
using expr::Env;
using expr::False;
using expr::Float;
using expr::Int;
using expr::Nil;
using expr::Symbol;
using expr::String;
using expr::True;
using expr::Vector;

namespace eval {

namespace {

gc::Lock<expr::Expr> ParseExpr(const std::string& str) {
  auto exprs = parse::Read(str);
  assert(exprs.size() == 1);
  return exprs[0];
}

gc::Lock<Expr> IntExpr(Int::ValType val) {
  return gc::Lock<Expr>(new Int(val));
}

gc::Lock<Expr> CharExpr(Char::ValType val) {
  return gc::Lock<Expr>(new Char(val));
}

gc::Lock<Expr> FloatExpr(Float::ValType d) {
  return gc::Lock<Expr>(new Float(d));
}

gc::Lock<Expr> StringExpr(std::string str) {
  return gc::Lock<Expr>(new String(std::move(str)));
}

gc::Lock<Expr> SymExpr(std::string str) {
  return gc::Lock<Expr>(Symbol::New(std::move(str)));
}

}  // namespace

class EvalTest : public test::TestBase {
 protected:
  EvalTest() : env_(new Env()) { expr::LoadPrimitives(env_.get()); }

  gc::Lock<expr::Expr> EvalStr(const std::string& str) {
    return Eval(ParseExpr(str).get(), env_.get());
  }

  gc::Lock<expr::Env> env_;
};

TEST_F(EvalTest, SelfEvaluating) {
  EXPECT_EQ(Nil(), Eval(Nil(), env_.get()).get());
  EXPECT_EQ(expr::True(), Eval(expr::True(), env_.get()).get());
  EXPECT_EQ(expr::False(), Eval(expr::False(), env_.get()).get());

  auto num = IntExpr(42);
  EXPECT_EQ(num.get(), Eval(num.get(), env_.get()).get());

  auto character = CharExpr('a');
  EXPECT_EQ(character.get(), Eval(character.get(), env_.get()).get());

  auto str = StringExpr("abc");
  EXPECT_EQ(str.get(), Eval(str.get(), env_.get()).get());

  auto vec =
      gc::Lock<Expr>(new Vector({num.get(), character.get(), str.get()}));
  EXPECT_EQ(vec.get(), Eval(vec.get(), env_.get()).get());
}

TEST_F(EvalTest, Symbol) {
  auto num = gc::make_locked<Int>(42);
  auto symbol = ParseExpr("abc");
  env_->DefineVar(symbol->AsSymbol(), num.get());

  EXPECT_EQ(num.get(), Eval(symbol.get(), env_.get()).get());
}

TEST_F(EvalTest, Quote) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(quote 42)"));
  EXPECT_EQ(*SymExpr("a"), *EvalStr("(quote a)"));
  EXPECT_EQ(*SymExpr("a"), *EvalStr("'a"));
  EXPECT_EQ(*EvalStr("#(a b c)"), *EvalStr("(quote #(a b c))"));
  EXPECT_EQ(*EvalStr("#(a b c)"), *EvalStr("'#(a b c)"));
  EXPECT_EQ(*Nil(), *EvalStr("'()"));

  auto one = gc::make_locked<Int>(1);
  auto two = gc::make_locked<Int>(2);

  gc::Lock<Expr> list(Nil());
  list.reset(new expr::Pair(two.get(), list.get()));
  list.reset(new expr::Pair(one.get(), list.get()));
  list.reset(new expr::Pair(SymExpr("+").get(), list.get()));

  EXPECT_EQ(*list, *EvalStr("(quote (+ 1 2))"));
  EXPECT_EQ(*list, *EvalStr("'(+ 1 2)"));

  list.reset(Nil());
  list.reset(new expr::Pair(SymExpr("a").get(), list.get()));
  list.reset(new expr::Pair(SymExpr("quote").get(), list.get()));

  EXPECT_EQ(*list, *EvalStr("'(quote a)"));
  EXPECT_EQ(*list, *EvalStr("''a"));
}

TEST_F(EvalTest, Lambda) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("((lambda (x) x) 42)"));
  EXPECT_EQ(*IntExpr(8), *EvalStr("((lambda (x) (+ x x)) 4)"));
  // Nested lambda
  EXPECT_EQ(*IntExpr(8), *EvalStr("((lambda (x) ((lambda (y) (+ x y)) 3)) 5)"));

  // Lamda returning lambda
  EXPECT_EQ(*IntExpr(12), *EvalStr("(((lambda () (lambda (x) (+ 5 x)))) 7)"));
}

TEST_F(EvalTest, If) {
  auto n42 = gc::make_locked<Int>(42);
  EXPECT_EQ(*n42, *EvalStr("(if #t 42)"));
  EXPECT_EQ(*Nil(), *EvalStr("(if #f 42)"));

  auto n43 = gc::make_locked<Int>(43);
  EXPECT_EQ(*n42, *EvalStr("(if #t 42 43)"));
  EXPECT_EQ(*n43, *EvalStr("(if #f 42 43)"));
  EXPECT_EQ(*IntExpr(12), *EvalStr("((if #f + *) 3 4)"));
  EXPECT_EQ(*EvalStr("'(3 4 5 6)"), *EvalStr("((lambda x x) 3 4 5 6)"));
  EXPECT_EQ(*EvalStr("'(5 6)"), *EvalStr("((lambda (x y . z) z) 3 4 5 6)"));
}

TEST_F(EvalTest, Set) {
  auto* sym = Symbol::New("foo");
  env_->DefineVar(sym, Nil());
  EvalStr("(set! foo 42)");

  EXPECT_EQ(*IntExpr(42), *env_->Lookup(sym));
}

TEST_F(EvalTest, Cond) {
  EXPECT_EQ(*Nil(), *EvalStr("(cond)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (#t 42))"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (#f 3) (#t 42))"));
  EXPECT_EQ(*Nil(), *EvalStr("(cond (#f 3) (#f 42))"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (#f 3) (else 42))"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(cond (else 42))"));
  EXPECT_EQ(
      *IntExpr(10),
      *EvalStr("(cond (#f 3) ((+ 4 3) => (lambda (x) (+ x 3))) (else 4))"));
}

TEST_F(EvalTest, Case) {
  EXPECT_EQ(*Nil(), *EvalStr("(case 3)"));
  // clang-format off
  EXPECT_EQ(*SymExpr("composite"), *EvalStr(
      "(case (* 2 3)"
      "  ((2 3 5 7) 'prime)"
      "  ((1 4 6 8 9) 'composite))"));
  EXPECT_EQ(*Nil(), *EvalStr(
      "(case (car '(c d))"
      "  ((a) 'a)"
      "  ((b) 'b))"));
  EXPECT_EQ(*SymExpr("consonant"), *EvalStr(
      "(case (car '(c d))"
      "  ((a e i o u) 'vowel)"
      "  ((w y) 'semivowel)"
      "  (else 'consonant))"));
  // clang-format on
}

TEST_F(EvalTest, And) {
  EXPECT_EQ(*True(), *EvalStr("(and (= 2 2) (> 2 1))"));
  EXPECT_EQ(*False(), *EvalStr("(and (= 2 2) (< 2 1))"));
  EXPECT_EQ(*EvalStr("'(f g)"), *EvalStr("(and 1 2 'c '(f g))"));
  EXPECT_EQ(*True(), *EvalStr("(and)"));
}

TEST_F(EvalTest, Or) {
  EXPECT_EQ(*True(), *EvalStr("(or (= 2 2) (> 2 1))"));
  EXPECT_EQ(*True(), *EvalStr("(or (= 2 2) (< 2 1))"));
  EXPECT_EQ(*False(), *EvalStr("(or #f #f #f)"));
  EXPECT_EQ(*EvalStr("'(b c)"), *EvalStr("(or (memq 'b '(a b c)) (/ 3 0))"));
  EXPECT_EQ(*False(), *EvalStr("(or)"));
}

TEST_F(EvalTest, Let) {
  EXPECT_EQ(*IntExpr(6), *EvalStr("(let ((x 2) (y 3)) (* x y))"));
  // clang-format off
  EXPECT_EQ(*IntExpr(35), *EvalStr(
      "(let ((x 2) (y 3))"
      "  (let ((x 7)"
      "        (z (+ x y)))"
      "    (* z x)))"));
  // clang-format on
}

TEST_F(EvalTest, LetStar) {
  // clang-format off
  EXPECT_EQ(*IntExpr(70), *EvalStr(
      "(let ((x 2) (y 3))"
      "  (let* ((x 7)"
      "         (z (+ x y)))"
      "    (* z x)))"));
  // clang-format on
}

TEST_F(EvalTest, LetRec) {
  // clang-format off
  EXPECT_EQ(*True(), *EvalStr(
      "(letrec ((even?"
      "          (lambda (n)"
      "             (if (zero? n)"
      "                 #t"
      "                 (odd? (- n 1)))))"
      "         (odd?"
      "          (lambda (n)"
      "            (if (zero? n)"
      "                #f"
      "                (even? (- n 1))))))"
      "  (even? 88))"));
  // clang-format on
}

TEST_F(EvalTest, Begin) {
  EvalStr("(define x 0)");
  EXPECT_EQ(*IntExpr(6), *EvalStr("(begin (set! x 5) (+ x 1))"));
}

TEST_F(EvalTest, Define) {
  // TODO(bcf): Test define function
  EvalStr("(define foo 42)");
  EXPECT_EQ(*IntExpr(42), *env_->Lookup(Symbol::New("foo")));
}

TEST_F(EvalTest, IsEqv) {
  EXPECT_EQ(*True(), *EvalStr("(eqv? #t #t)"));
  EXPECT_EQ(*True(), *EvalStr("(eqv? #f #f)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? #f #t)"));

  EXPECT_EQ(*True(), *EvalStr("(eqv? 'x 'x)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 'x 'y)"));

  EXPECT_EQ(*True(), *EvalStr("(eqv? 3 3)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 3 4)"));
  EXPECT_EQ(*True(), *EvalStr("(eqv? 3.1 3.1)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 3.1 3.2)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? 3.0 3)"));

  EXPECT_EQ(*True(), *EvalStr("(eqv? #\\c #\\c)"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? #\\c #\\d)"));
  EXPECT_EQ(*True(), *EvalStr("(eqv? '() '())"));

  EXPECT_EQ(*False(), *EvalStr("(eqv? (cons 1 2) (cons 1 2))"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? (lambda () 1) (lambda () 2))"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? #f 'nil)"));
  EXPECT_EQ(*True(), *EvalStr("(let ((p (lambda (x) x))) (eqv? p p))"));
  // clang-format off
  EvalStr(
    "(define gen-counter"
    "  (lambda ()"
    "    (let ((n 0))"
    "      (lambda () (set! n (+ n 1)) n))))");
  // clang-format on

  EXPECT_EQ(*True(), *EvalStr("(let ((g (gen-counter))) (eqv? g g))"));
  EXPECT_EQ(*False(), *EvalStr("(eqv? (gen-counter) (gen-counter))"));

  // clang-format off
  EvalStr(
    "(define gen-loser"
    "  (lambda ()"
    "    (let ((n 0))"
    "      (lambda () (set! n (+ n 1)) 27))))");
  // clang-format on
  EXPECT_EQ(*True(), *EvalStr("(let ((g (gen-loser))) (eqv? g g))"));
  EXPECT_EQ(*True(), *EvalStr("(let ((x '(a))) (eqv? x x))"));

  // TODO(bcf): obj1 and obj2 are pairs, vectors, or strings that denote
  // the same locations in the store (section 3.4).

  // TODO(bcf): obj1 and obj2 are procedures whose location tags are
  // equal (section 4.1.4).
}

TEST_F(EvalTest, IsEq) {
  EXPECT_EQ(*True(), *EvalStr("(eq? 'a 'a)"));
  EXPECT_EQ(*False(), *EvalStr("(eq? '(a) '(a))"));
  EXPECT_EQ(*True(), *EvalStr("(eq? '() '())"));
  EXPECT_EQ(*True(), *EvalStr("(eq? car car)"));
  EXPECT_EQ(*True(), *EvalStr("(let ((x '(a))) (eq? x x))"));
  EXPECT_EQ(*True(), *EvalStr("(let ((x '#())) (eq? x x)) "));
  EXPECT_EQ(*True(), *EvalStr("(let ((p (lambda (x) x))) (eq? p p))"));
}

TEST_F(EvalTest, IsEqual) {
  EXPECT_EQ(*True(), *EvalStr("(equal? 'a 'a)"));
  EXPECT_EQ(*True(), *EvalStr("(equal? '(a) '(a))"));
  EXPECT_EQ(*True(), *EvalStr("(equal? '(a (b) c) '(a (b) c))"));
  EXPECT_EQ(*True(), *EvalStr("(equal? \"abc\" \"abc\")"));
  EXPECT_EQ(*True(), *EvalStr("(equal? 2 2)"));
  EXPECT_EQ(*True(), *EvalStr("(equal? '#(5 'a) '#(5 'a))"));
}

TEST_F(EvalTest, NumberPredicates) {
#if 0
  EXPECT_EQ(*True(), *EvalStr("(complex? 3+4i)"));
#endif
  EXPECT_EQ(*True(), *EvalStr("(complex? 3)"));
  EXPECT_EQ(*True(), *EvalStr("(real? 3)"));
#if 0
  EXPECT_EQ(*True(), *EvalStr("(real? -2.5+0.0i)"));
#endif
#if 0  // TODO(bcf): Support parsing this.
  EXPECT_EQ(*True(), *EvalStr("(real? #e1e10)"));
#endif
#if 0
  EXPECT_EQ(*True(), *EvalStr("(rational? 6/10)"));
  EXPECT_EQ(*True(), *EvalStr("(rational? 6/3)"));
  EXPECT_EQ(*True(), *EvalStr("(integer? 3+0i)"));
#endif
  EXPECT_EQ(*True(), *EvalStr("(integer? 3.0)"));
#if 0
  EXPECT_EQ(*True(), *EvalStr("(integer? 8/4)"));
#endif
  EXPECT_EQ(*True(), *EvalStr("(exact? 3)"));
  EXPECT_EQ(*False(), *EvalStr("(exact? 3.0)"));
}

TEST_F(EvalTest, OpEq) {
  EXPECT_EQ(*False(), *EvalStr("(= 3 4)"));
  EXPECT_EQ(*True(), *EvalStr("(= 3 3 3)"));
  EXPECT_EQ(*True(), *EvalStr("(= 3 3 (+ 2 1))"));
  EXPECT_EQ(*False(), *EvalStr("(= 3 3 3 1)"));

  EXPECT_EQ(*False(), *EvalStr("(= 3.0 4)"));
  EXPECT_EQ(*True(), *EvalStr("(= 3.0 3.0 3.0)"));
  EXPECT_EQ(*False(), *EvalStr("(= 3.0 3 3.0 1)"));
}

TEST_F(EvalTest, OpLt) {
  EXPECT_EQ(*False(), *EvalStr("(< 4 3)"));
  EXPECT_EQ(*False(), *EvalStr("(< 4 4)"));
  EXPECT_EQ(*True(), *EvalStr("(< 3 4)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1 2 3 4)"));
  EXPECT_EQ(*False(), *EvalStr("(< 1 2 3 3)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1 2 3 (+ 4 5))"));

  EXPECT_EQ(*False(), *EvalStr("(< 4.0 3)"));
  EXPECT_EQ(*False(), *EvalStr("(< 4.0 4.0)"));
  EXPECT_EQ(*True(), *EvalStr("(< 3.0 4)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1.0 2.0 3.0 4.0)"));
  EXPECT_EQ(*False(), *EvalStr("(< 1 2.0 3 3.0)"));
  EXPECT_EQ(*True(), *EvalStr("(< 1.0 2 3.0 (+ 4 5.0))"));
}

TEST_F(EvalTest, OpGt) {
  EXPECT_EQ(*False(), *EvalStr("(> 3 4)"));
  EXPECT_EQ(*False(), *EvalStr("(> 3 3)"));
  EXPECT_EQ(*False(), *EvalStr("(> 4 4 3)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3 1)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3 (+ 1 1) 1)"));

  EXPECT_EQ(*False(), *EvalStr("(> 3.0 4.0)"));
  EXPECT_EQ(*False(), *EvalStr("(> 3 3.0)"));
  EXPECT_EQ(*False(), *EvalStr("(> 4.0 4.0 3)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4 3.0)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4.0 3 1.0)"));
  EXPECT_EQ(*True(), *EvalStr("(> 4.0 3 (+ 1 1.0) 1.0)"));
}

TEST_F(EvalTest, OpLe) {
  EXPECT_EQ(*False(), *EvalStr("(<= 5 4)"));
  EXPECT_EQ(*False(), *EvalStr("(<= 5 6 7 8 1)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5 6 7 8)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5 8)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5 8 (+ 5 13))"));

  EXPECT_EQ(*False(), *EvalStr("(<= 5.0 4.0)"));
  EXPECT_EQ(*False(), *EvalStr("(<= 5.0 6 7.0 8 1.0)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5.0 6 7.0 8)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5.0 8.0)"));
  EXPECT_EQ(*True(), *EvalStr("(<= 5.0 8.0 (+ 5.0 13.0))"));
}

TEST_F(EvalTest, OpGe) {
  EXPECT_EQ(*False(), *EvalStr("(>= 4 5)"));
  EXPECT_EQ(*False(), *EvalStr("(>= (- 8 17) 5 5 3 3 2 2)"));
  EXPECT_EQ(*True(), *EvalStr("(>= 3 3 3 3 )"));
  EXPECT_EQ(*True(), *EvalStr("(>= 8 3 1 0 -5)"));

  EXPECT_EQ(*False(), *EvalStr("(>= 4.0 5)"));
  EXPECT_EQ(*False(), *EvalStr("(>= (- 8 17.0) 5.0 5 3.0 3.0 2.0 2)"));
  EXPECT_EQ(*True(), *EvalStr("(>= 3.0 3 3.0 3 )"));
  EXPECT_EQ(*True(), *EvalStr("(>= 8.0 3 1.0 0 -5.0)"));
}

TEST_F(EvalTest, IsZero) {
  EXPECT_EQ(*False(), *EvalStr("(zero? 1)"));
  EXPECT_EQ(*False(), *EvalStr("(zero? 0.1)"));
  EXPECT_EQ(*True(), *EvalStr("(zero? 0)"));
  EXPECT_EQ(*True(), *EvalStr("(zero? (- 1.1 1.1))"));
}

TEST_F(EvalTest, IsPositive) {
  EXPECT_EQ(*False(), *EvalStr("(positive? 0)"));
  EXPECT_EQ(*False(), *EvalStr("(positive? -1)"));
  EXPECT_EQ(*False(), *EvalStr("(positive? (- 1.1 1.1))"));
  EXPECT_EQ(*False(), *EvalStr("(positive? -0.1)"));

  EXPECT_EQ(*True(), *EvalStr("(positive? 1)"));
  EXPECT_EQ(*True(), *EvalStr("(positive? (- 1.1 1.0))"));
  EXPECT_EQ(*True(), *EvalStr("(positive? 0.1)"));
}

TEST_F(EvalTest, IsNegative) {
  EXPECT_EQ(*False(), *EvalStr("(negative? 0)"));
  EXPECT_EQ(*False(), *EvalStr("(negative? 1)"));
  EXPECT_EQ(*False(), *EvalStr("(negative? (- 1.1 1.1))"));
  EXPECT_EQ(*False(), *EvalStr("(negative? 0.1)"));

  EXPECT_EQ(*True(), *EvalStr("(negative? -1)"));
  EXPECT_EQ(*True(), *EvalStr("(negative? (- 1.0 1.1))"));
  EXPECT_EQ(*True(), *EvalStr("(negative? -0.1)"));
}

TEST_F(EvalTest, IsOdd) {
  EXPECT_EQ(*False(), *EvalStr("(odd? 2)"));
  EXPECT_EQ(*True(), *EvalStr("(odd? 3)"));
}

TEST_F(EvalTest, IsEven) {
  EXPECT_EQ(*True(), *EvalStr("(even? 2)"));
  EXPECT_EQ(*False(), *EvalStr("(even? 3)"));
}

TEST_F(EvalTest, Min) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(min 42 43 44)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(min 100 42)"));
  EXPECT_EQ(*FloatExpr(2.0), *EvalStr("(min 3 2.0 10)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(min 100 42 42.1)"));
}

TEST_F(EvalTest, Max) {
  EXPECT_EQ(*IntExpr(4), *EvalStr("(max 3 4)"));
  EXPECT_EQ(*FloatExpr(4), *EvalStr("(max 3.9 4)"));
}

TEST_F(EvalTest, Plus) {
  EXPECT_EQ(*IntExpr(0), *EvalStr("(+)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 22 20)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 22 12 3 5)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(+ 60 -18)"));
  EXPECT_EQ(*FloatExpr(2.0), *EvalStr("(+ 1 1.0)"));
}

TEST_F(EvalTest, Star) {
  EXPECT_EQ(*IntExpr(1), *EvalStr("(*)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(* 21 2)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(* 2 3 7)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(* 21 -2 -1)"));
}

TEST_F(EvalTest, Minus) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(- 84 42)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(- 84 20 22)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(- 22 -20)"));
}

TEST_F(EvalTest, Slash) {
  EXPECT_EQ(*IntExpr(42), *EvalStr("(/ 84 2)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(/ 252 2 3)"));
  EXPECT_EQ(*IntExpr(42), *EvalStr("(/ 504 -6 -2)"));
}

TEST_F(EvalTest, Abs) {
  EXPECT_EQ(*IntExpr(7), *EvalStr("(abs -7)"));
  EXPECT_EQ(*FloatExpr(42.0), *EvalStr("(abs -42.0)"));
}

TEST_F(EvalTest, Quotient) {
  EXPECT_EQ(*IntExpr(3), *EvalStr("(quotient 13 4)"));
  EXPECT_EQ(*IntExpr(-3), *EvalStr("(quotient -13 4)"));
}

TEST_F(EvalTest, Remainder) {
  EXPECT_EQ(*IntExpr(1), *EvalStr("(remainder 13 4)"));
  EXPECT_EQ(*IntExpr(-1), *EvalStr("(remainder -13 4)"));
  EXPECT_EQ(*IntExpr(1), *EvalStr("(remainder 13 -4)"));
  EXPECT_EQ(*IntExpr(-1), *EvalStr("(remainder -13 -4)"));
  EXPECT_EQ(*FloatExpr(-1), *EvalStr("(remainder -13 -4.0)"));
}

TEST_F(EvalTest, Modulo) {
  EXPECT_EQ(*IntExpr(1), *EvalStr("(modulo 13 4)"));
  EXPECT_EQ(*IntExpr(3), *EvalStr("(modulo -13 4)"));
  EXPECT_EQ(*IntExpr(-3), *EvalStr("(modulo 13 -4)"));
  EXPECT_EQ(*IntExpr(-1), *EvalStr("(modulo -13 -4)"));
}

TEST_F(EvalTest, Floor) {
  EXPECT_EQ(*FloatExpr(-5.0), *EvalStr("(floor -4.3)"));
  EXPECT_EQ(*FloatExpr(3.0), *EvalStr("(floor 3.5)"));
}

TEST_F(EvalTest, Ceiling) {
  EXPECT_EQ(*FloatExpr(-4.0), *EvalStr("(ceiling -4.3)"));
  EXPECT_EQ(*FloatExpr(4.0), *EvalStr("(ceiling 3.5)"));
}

TEST_F(EvalTest, Truncate) {
  EXPECT_EQ(*FloatExpr(-4.0), *EvalStr("(truncate -4.3)"));
  EXPECT_EQ(*FloatExpr(3.0), *EvalStr("(truncate 3.5)"));
}

TEST_F(EvalTest, Round) {
  EXPECT_EQ(*FloatExpr(-4.0), *EvalStr("(round -4.3)"));
  EXPECT_EQ(*FloatExpr(4.0), *EvalStr("(round 3.5)"));
#if 0
  EXPECT_EQ(*IntExpr(4), *EvalStr("(round 7/4)"));
#endif
  EXPECT_EQ(*IntExpr(7), *EvalStr("(round 7)"));
}

TEST_F(EvalTest, ExactToInexact) {
  EXPECT_EQ(*FloatExpr(4.0), *EvalStr("(exact->inexact 4)"));
  EXPECT_EQ(*FloatExpr(4.0), *EvalStr("(exact->inexact 4.0)"));
}

TEST_F(EvalTest, InexactToExact) {
  EXPECT_EQ(*IntExpr(4), *EvalStr("(inexact->exact 4)"));
  EXPECT_EQ(*IntExpr(4), *EvalStr("(inexact->exact 4.0)"));
}

TEST_F(EvalTest, NumberToString) {
  EXPECT_EQ(*StringExpr("4"), *EvalStr("(number->string 4)"));
  EXPECT_EQ(*StringExpr("4.25"), *EvalStr("(number->string 4.25)"));
}

TEST_F(EvalTest, StringToNumber) {
  EXPECT_EQ(*IntExpr(100), *EvalStr("(string->number \"100\")"));
  EXPECT_EQ(*IntExpr(256), *EvalStr("(string->number \"100\" 16)"));
  EXPECT_EQ(*FloatExpr(100.0), *EvalStr("(string->number \"1e2\")"));
  EXPECT_EQ(*FloatExpr(1500.0), *EvalStr("(string->number \"15##\")"));
  EXPECT_EQ(*False(), *EvalStr("(string->number \"gg\")"));
}

TEST_F(EvalTest, Not) {
  EXPECT_EQ(*False(), *EvalStr("(not #t)"));
  EXPECT_EQ(*False(), *EvalStr("(not 3)"));
  EXPECT_EQ(*False(), *EvalStr("(not (list 3))"));
  EXPECT_EQ(*True(), *EvalStr("(not #f)"));
  EXPECT_EQ(*False(), *EvalStr("(not '())"));
  EXPECT_EQ(*False(), *EvalStr("(not (list))"));
  EXPECT_EQ(*False(), *EvalStr("(not 'nil)"));
}

TEST_F(EvalTest, IsBoolean) {
  EXPECT_EQ(*True(), *EvalStr("(boolean? #f)"));
  EXPECT_EQ(*False(), *EvalStr("(boolean? 0)"));
  EXPECT_EQ(*False(), *EvalStr("(boolean? '())"));
}

TEST_F(EvalTest, IsPair) {
  EXPECT_EQ(*True(), *EvalStr("(pair? '(a . b))"));
  EXPECT_EQ(*True(), *EvalStr("(pair? '(a b c))"));
  EXPECT_EQ(*False(), *EvalStr("(pair? '())"));
  EXPECT_EQ(*False(), *EvalStr("(pair? '#(a b))"));
}

TEST_F(EvalTest, Cons) {
  EXPECT_EQ(*EvalStr("(cons 'a '())"), *EvalStr("'(a)"));
  EXPECT_EQ(*EvalStr("(cons '(a) '(b c d))"), *EvalStr("'((a) b c d)"));
  EXPECT_EQ(*EvalStr("(cons \"a\" '(b c))"), *EvalStr("'(\"a\" b c)"));
  EXPECT_EQ(*EvalStr("(cons 'a 3)"), *EvalStr("'(a . 3)"));
  EXPECT_EQ(*EvalStr("(cons '(a b) 'c)"), *EvalStr("'((a b) . c)"));
}

TEST_F(EvalTest, Car) {
  EXPECT_EQ(*EvalStr("(car '(a b c))"), *EvalStr("'a"));
  EXPECT_EQ(*EvalStr("(car '((a) b c d))"), *EvalStr("'(a)"));
  EXPECT_EQ(*EvalStr("(car '(1 . 2))"), *EvalStr("1"));
  EXPECT_THROW((void)EvalStr("(car '())"), util::RuntimeException);
}

TEST_F(EvalTest, Cdr) {
  EXPECT_EQ(*EvalStr("(cdr '((a) b c d))"), *EvalStr("'(b c d)"));
  EXPECT_EQ(*EvalStr("(cdr '(1 . 2))"), *EvalStr("2"));
  EXPECT_THROW((void)EvalStr("(cdr '())"), util::RuntimeException);
}

TEST_F(EvalTest, SetCarSetCdr) {
  (void)EvalStr("(define a '(1 . 2))");
  (void)EvalStr("(set-car! a 3)");
  (void)EvalStr("(set-cdr! a 4)");
  EXPECT_EQ(*EvalStr("'(3 . 4)"), *EvalStr("a"));
}

TEST_F(EvalTest, IsList) {
  EXPECT_EQ(*True(), *EvalStr("(list? '(a b c))"));
  EXPECT_EQ(*True(), *EvalStr("(list? '())"));
  EXPECT_EQ(*False(), *EvalStr("(list? '(a . b))"));

  // clang-format off
  EXPECT_EQ(*False(), *EvalStr(
      "(let ((x (list 'a)))"
      "  (set-cdr! x x)"
      "  (list? x))"));
  // clang-format on
}

TEST_F(EvalTest, List) {
  EXPECT_EQ(*EvalStr("(list 'a (+ 3 4) 'c)"), *EvalStr("'(a 7 c)"));
  EXPECT_EQ(*EvalStr("(list)"), *EvalStr("'()"));
}

TEST_F(EvalTest, Length) {
  EXPECT_EQ(*IntExpr(3), *EvalStr("(length '(a b c))"));
  EXPECT_EQ(*IntExpr(3), *EvalStr("(length '(a (b) (c d e)))"));
  EXPECT_EQ(*IntExpr(0), *EvalStr("(length '())"));
}

TEST_F(EvalTest, Append) {
  EXPECT_EQ(*EvalStr("(append '(x) '(y))"), *EvalStr("'(x y)"));
  EXPECT_EQ(*EvalStr("(append '(a) '(b c d))"), *EvalStr("'(a b c d)"));
  EXPECT_EQ(*EvalStr("(append '(a (b)) '((c)))"), *EvalStr("'(a (b) (c))"));
  EXPECT_EQ(*EvalStr("(append '(a b) '(c . d))"), *EvalStr("'(a b c . d)"));
  EXPECT_EQ(*EvalStr("(append '() 'a)"), *EvalStr("'a"));
}

TEST_F(EvalTest, Reverse) {
  EXPECT_EQ(*EvalStr("(reverse '(a b c))"), *EvalStr("'(c b a)"));
  EXPECT_EQ(*EvalStr("(reverse '(a (b c) d (e (f))))"),
            *EvalStr("'((e (f)) d (b c) a)"));
}

TEST_F(EvalTest, ListTail) {
  EXPECT_EQ(*EvalStr("(list-tail '(a b c) 3)"), *EvalStr("'()"));
  EXPECT_EQ(*EvalStr("(list-tail '(a b c d) 3)"), *EvalStr("'(d)"));
  EXPECT_THROW((void)EvalStr("(list-tail '(a b c d) 10)"),
               util::RuntimeException);
}

TEST_F(EvalTest, ListRef) {
  EXPECT_EQ(*EvalStr("(list-ref '(a b c d) 2)"), *EvalStr("'c"));
  EXPECT_EQ(*EvalStr("(list-ref '(a b c d) (inexact->exact (round 1.8)))"),
            *EvalStr("'c"));
}

TEST_F(EvalTest, Memq) {
  EXPECT_EQ(*EvalStr("(memq 'a '(a b c))"), *EvalStr("'(a b c)"));
  EXPECT_EQ(*EvalStr("(memq 'b '(a b c))"), *EvalStr("'(b c)"));
  EXPECT_EQ(*EvalStr("(memq 'a '(b c d))"), *EvalStr("#f"));
  EXPECT_EQ(*EvalStr("(memq (list 'a) '(b (a) c))"), *EvalStr("#f"));
}

TEST_F(EvalTest, Memv) {
  EXPECT_EQ(*EvalStr("(memv 101 '(100 101 102))"), *EvalStr("'(101 102)"));
}

TEST_F(EvalTest, Member) {
  EXPECT_EQ(*EvalStr("(member (list 'a) '(b (a) c))"), *EvalStr("'((a) c)"));
}

TEST_F(EvalTest, AssTest) {
  (void)EvalStr("(define e '((a 1) (b 2) (c 3)))");
  EXPECT_EQ(*EvalStr("(assq 'a e)"), *EvalStr("'(a 1)"));
  EXPECT_EQ(*EvalStr("(assq 'b e)"), *EvalStr("'(b 2)"));
  EXPECT_EQ(*EvalStr("(assq 'd e)"), *False());
  EXPECT_EQ(*EvalStr("(assq (list 'a) '(((a)) ((b)) ((c))))"), *False());
  EXPECT_EQ(*EvalStr("(assoc (list 'a) '(((a)) ((b)) ((c))))"),
            *EvalStr("'((a))"));
  EXPECT_EQ(*EvalStr("(assv 5 '((2 3) (5 7) (11 13)))"), *EvalStr("'(5 7)"));
}

TEST_F(EvalTest, IsSymbol) {
  EXPECT_EQ(*EvalStr("(symbol? 'foo)"), *True());
  EXPECT_EQ(*EvalStr("(symbol? (car '(a b)))"), *True());
  EXPECT_EQ(*EvalStr("(symbol? \"bar\")"), *False());
  EXPECT_EQ(*EvalStr("(symbol? 'nil)"), *True());
  EXPECT_EQ(*EvalStr("(symbol? '())"), *False());
  EXPECT_EQ(*EvalStr("(symbol? #f)"), *False());
}

TEST_F(EvalTest, SymbolToString) {
  EXPECT_EQ(*EvalStr("(symbol->string 'flying-fish)"),
            *StringExpr("flying-fish"));
  EXPECT_EQ(*EvalStr("(symbol->string 'Martin)"), *StringExpr("Martin"));
  EXPECT_EQ(*EvalStr("(symbol->string (string->symbol \"Malvina\"))"),
            *StringExpr("Malvina"));
}

TEST_F(EvalTest, StringToSymbol) {
  EXPECT_EQ(*EvalStr("(string->symbol \"mISSISSIppi\")"),
            *Symbol::NewLock("mISSISSIppi"));
  EXPECT_EQ(
      *EvalStr("(eq? 'JollyWog (string->symbol (symbol->string 'JollyWog)))"),
      *True());
#if 0
  // clang-format off
  EXPECT_EQ(*True(), *EvalStr(
      "(string=? \"K. Harper, M.D.\""
      "  (symbol->string"
      "  (string->symbol \"K. Harper, M.D.\")))"));
  // clang-format on
#endif
}

TEST_F(EvalTest, VectorRef) {
  EXPECT_EQ(*IntExpr(8), *EvalStr("(vector-ref '#(1 1 2 3 5 8 13 21) 5)"));

  // clang-format off
  EXPECT_EQ(*IntExpr(13), *EvalStr(
      "(vector-ref '#(1 1 2 3 5 8 13 21)"
      "  (let ((i (round (* 2 (acos -1)))))"
      "    (if (inexact? i)"
      "        (inexact->exact i)"
      "        i)))"));
  // clang-format on
}

TEST_F(EvalTest, VectorSet) {
  // clang-format off
  EXPECT_EQ(*EvalStr(
      "(let ((vec (vector 0 '(2 2 2 2) \"Anna\")))"
      "  (vector-set! vec 1 '(\"Sue\" \"Sue\"))"
      "  vec)"),
    *EvalStr("#(0 (\"Sue\" \"Sue\") \"Anna\")"));
  // clang-format on
}

TEST_F(EvalTest, VectorListConversion) {
  EXPECT_EQ(*EvalStr("(vector->list '#(dah dah didah))"),
            *EvalStr("'(dah dah didah)"));
  EXPECT_EQ(*EvalStr("(list->vector '(dididit dah))"),
            *EvalStr("#(dididit dah)"));
}

TEST_F(EvalTest, IsProcedure) {
  EXPECT_EQ(*True(), *EvalStr("(procedure? car)"));
  EXPECT_EQ(*False(), *EvalStr("(procedure? 'car)"));
  EXPECT_EQ(*True(), *EvalStr("(procedure? (lambda (x) (* x x)))"));
  EXPECT_EQ(*False(), *EvalStr("(procedure? '(lambda (x) (* x x)))"));
#if 0
  EXPECT_EQ(*True(), *EvalStr("(call-with-current-continuation procedure?)"));
#endif
}

TEST_F(EvalTest, Apply) {
  EXPECT_EQ(*IntExpr(7), *EvalStr("(apply + (list 3 4))"));

  // clang-format off
  (void)EvalStr(
      "(define compose"
      "  (lambda (f g)"
      "    (lambda args"
      "      (f (apply g args)))))");
  // clang-format on

  EXPECT_EQ(*IntExpr(30), *EvalStr("((compose sqrt *) 12 75)"));
}

TEST_F(EvalTest, Map) {
  EXPECT_EQ(*EvalStr("(map cadr '((a b) (d e) (g h)))"), *EvalStr("'(b e h)"));
  EXPECT_EQ(*EvalStr("(map (lambda (n) (expt n n)) '(1 2 3 4 5))"),
            *EvalStr("'(1 4 27 256 3125)"));
  EXPECT_EQ(*EvalStr("(map + '(1 2 3) '(4 5 6))"), *EvalStr("'(5 7 9)"));

  // clang-format off
  EXPECT_EQ(*EvalStr(
      "(let ((count 0))"
      "  (map (lambda (ignored)"
      "         (set! count (+ count 1))"
      "         count)"
      "       '(a b)))"),
    *EvalStr("'(1 2)"));
  // clang-format on
}

TEST_F(EvalTest, ForEach) {
  // clang-format off
  EXPECT_EQ(*EvalStr("#(0 1 4 9 16)"),
      *EvalStr(
          "(let ((v (make-vector 5)))"
          "  (for-each (lambda (i)"
          "              (vector-set! v i (* i i)))"
          "            '(0 1 2 3 4))"
          "  v)"));
  // clang-format on
}

TEST_F(EvalTest, Force) {
  EXPECT_EQ(*EvalStr("(force (delay (+ 1 2)))"), *IntExpr(3));
  EXPECT_EQ(*EvalStr("(let ((p (delay (+ 1 2)))) (list (force p) (force p)))"),
            *EvalStr("'(3 3)"));

  // clang-format off
  (void)EvalStr(
      "(define a-stream"
      "  (letrec ((next"
      "            (lambda (n)"
      "              (cons n (delay (next (+ n 1)))))))"
      "    (next 0)))");
  // clang-format on
  (void)EvalStr("(define head car)");
  (void)EvalStr("(define tail (lambda (stream) (force (cdr stream))))");
  EXPECT_EQ(*IntExpr(2), *EvalStr("(head (tail (tail a-stream)))"));

  (void)EvalStr("(define count 0)");
  // clang-format off
  (void)EvalStr(
      "(define p"
      "  (delay (begin (set! count (+ count 1))"
      "                (if (> count x)"
      "                    count"
      "                    (force p)))))");
  // clang-format on
  (void)EvalStr("(define x 5)");
  EXPECT_EQ(*IntExpr(6), *EvalStr("(force p)"));
  (void)EvalStr("(begin (set! x 10))");
  EXPECT_EQ(*IntExpr(6), *EvalStr("(force p)"));
}

TEST_F(EvalTest, Eval) {
  EXPECT_EQ(*IntExpr(21),
            *EvalStr("(eval '(* 7 3) (scheme-report-environment 5))"));
  // clang-format off
  EXPECT_EQ(*IntExpr(20), *EvalStr(
      "(let ((f (eval '(lambda (f x) (f x x))"
      "               (null-environment 5))))"
      "  (f + 10))"));
  // clang-format on
}

}  // namespace eval
