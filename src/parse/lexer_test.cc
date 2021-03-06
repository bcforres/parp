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

// TODO(bcf): Add tests for invalid tokens

#include <sstream>
#include <vector>

#include "gtest/gtest.h"

#include "expr/expr.h"
#include "expr/number.h"
#include "parse/lexer.h"
#include "util/char_class.h"
#include "util/text_stream.h"
#include "test/util.h"

using expr::Char;
using expr::Float;
using expr::Int;
using expr::String;
using expr::Symbol;
using Lock = gc::Lock<expr::Expr>;

namespace parse {

namespace {

class LexerTest : public test::TestBase {};

void VerifyTokens(Lexer* lexer, const std::vector<Token>& expected) {
  for (const auto& expect : expected) {
    EXPECT_EQ(expect, lexer->NextToken());
  }

  EXPECT_EQ(Token::Type::TOK_EOF, lexer->NextToken().type);
}

}  // namespace

TEST_F(LexerTest, Basic) {
  const char* kStr =
      "  ;;; The FACT procedure computes the factorial\n"
      "  ;;; of a non-negative integer.\n"
      "  (define fact\n"
      "   (lambda (n)\n"
      "    (if (= n 0)\n"
      "     1 ;Base case: return 1\n"
      "     (* n (fact (- n 1))))))\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::LPAREN, {&kFilename, 3, 3}, nullptr},
      {Token::Type::ID, {&kFilename, 3, 4}, Symbol::New("define")},
      {Token::Type::ID, {&kFilename, 3, 11}, Symbol::New("fact")},
      {Token::Type::LPAREN, {&kFilename, 4, 4}, nullptr},
      {Token::Type::ID, {&kFilename, 4, 5}, Symbol::New("lambda")},
      {Token::Type::LPAREN, {&kFilename, 4, 12}, nullptr},
      {Token::Type::ID, {&kFilename, 4, 13}, Symbol::New("n")},
      {Token::Type::RPAREN, {&kFilename, 4, 14}, nullptr},
      {Token::Type::LPAREN, {&kFilename, 5, 5}, nullptr},
      {Token::Type::ID, {&kFilename, 5, 6}, Symbol::New("if")},
      {Token::Type::LPAREN, {&kFilename, 5, 9}, nullptr},
      {Token::Type::ID, {&kFilename, 5, 10}, Symbol::New("=")},
      {Token::Type::ID, {&kFilename, 5, 12}, Symbol::New("n")},
      {Token::Type::NUMBER, {&kFilename, 5, 14}, new Int(0)},
      {Token::Type::RPAREN, {&kFilename, 5, 15}, nullptr},
      {Token::Type::NUMBER, {&kFilename, 6, 6}, new Int(1)},
      {Token::Type::LPAREN, {&kFilename, 7, 6}, nullptr},
      {Token::Type::ID, {&kFilename, 7, 7}, Symbol::New("*")},
      {Token::Type::ID, {&kFilename, 7, 9}, Symbol::New("n")},
      {Token::Type::LPAREN, {&kFilename, 7, 11}, nullptr},
      {Token::Type::ID, {&kFilename, 7, 12}, Symbol::New("fact")},
      {Token::Type::LPAREN, {&kFilename, 7, 17}, nullptr},
      {Token::Type::ID, {&kFilename, 7, 18}, Symbol::New("-")},
      {Token::Type::ID, {&kFilename, 7, 20}, Symbol::New("n")},
      {Token::Type::NUMBER, {&kFilename, 7, 22}, new Int(1)},
      {Token::Type::RPAREN, {&kFilename, 7, 23}, nullptr},
      {Token::Type::RPAREN, {&kFilename, 7, 24}, nullptr},
      {Token::Type::RPAREN, {&kFilename, 7, 25}, nullptr},
      {Token::Type::RPAREN, {&kFilename, 7, 26}, nullptr},
      {Token::Type::RPAREN, {&kFilename, 7, 27}, nullptr},
      {Token::Type::RPAREN, {&kFilename, 7, 28}, nullptr},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, Empty) {
  const char* kStr = "";

  const std::string kFilename = "foo";
  const std::vector<Token> kExpected;

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, NoTrailingNewine) {
  const std::string kFilename = "foo";
  const struct {
    const char* str;
    std::vector<Token> expected;
  } kTests[] = {
      {"abc", {{Token::Type::ID, {&kFilename, 1, 1}, Symbol::New("abc")}}},

      {"#t\n", {{Token::Type::BOOL, {&kFilename, 1, 1}, expr::True()}}},
      {"1\n", {{Token::Type::NUMBER, {&kFilename, 1, 1}, new Int(1)}}},
      {"#\\c\n", {{Token::Type::CHAR, {&kFilename, 1, 1}, new Char('c')}}},
      {"\"def\"",
       {{Token::Type::STRING, {&kFilename, 1, 1}, new String("def")}}},
  };

  for (const auto& test : kTests) {
    std::istringstream s(test.str);
    util::TextStream stream(&s, kFilename);
    Lexer lexer{stream};

    VerifyTokens(&lexer, test.expected);
  }
}

TEST_F(LexerTest, IdTest) {
  const char* kStr =
      "abc\n"

      // special initial
      "!\n"
      "$\n"
      "%\n"
      "&\n"
      "*\n"
      "/\n"
      ":\n"
      "<\n"
      "=\n"
      ">\n"
      "?\n"
      "^\n"
      "_\n"
      "~\n"

      // special initial with letters
      "~a\n"

      // <peculiar identifier>
      "+\n"
      "-\n"
      "...\n"

      // <special subsequent>
      "a+\n"
      "b-\n"
      "c.\n"
      "c@\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::ID, {&kFilename, 1, 1}, Symbol::New("abc")},
      {Token::Type::ID, {&kFilename, 2, 1}, Symbol::New("!")},
      {Token::Type::ID, {&kFilename, 3, 1}, Symbol::New("$")},
      {Token::Type::ID, {&kFilename, 4, 1}, Symbol::New("%")},
      {Token::Type::ID, {&kFilename, 5, 1}, Symbol::New("&")},
      {Token::Type::ID, {&kFilename, 6, 1}, Symbol::New("*")},
      {Token::Type::ID, {&kFilename, 7, 1}, Symbol::New("/")},
      {Token::Type::ID, {&kFilename, 8, 1}, Symbol::New(":")},
      {Token::Type::ID, {&kFilename, 9, 1}, Symbol::New("<")},
      {Token::Type::ID, {&kFilename, 10, 1}, Symbol::New("=")},
      {Token::Type::ID, {&kFilename, 11, 1}, Symbol::New(">")},
      {Token::Type::ID, {&kFilename, 12, 1}, Symbol::New("?")},
      {Token::Type::ID, {&kFilename, 13, 1}, Symbol::New("^")},
      {Token::Type::ID, {&kFilename, 14, 1}, Symbol::New("_")},
      {Token::Type::ID, {&kFilename, 15, 1}, Symbol::New("~")},
      {Token::Type::ID, {&kFilename, 16, 1}, Symbol::New("~a")},
      {Token::Type::ID, {&kFilename, 17, 1}, Symbol::New("+")},
      {Token::Type::ID, {&kFilename, 18, 1}, Symbol::New("-")},
      {Token::Type::ID, {&kFilename, 19, 1}, Symbol::New("...")},
      {Token::Type::ID, {&kFilename, 20, 1}, Symbol::New("a+")},
      {Token::Type::ID, {&kFilename, 21, 1}, Symbol::New("b-")},
      {Token::Type::ID, {&kFilename, 22, 1}, Symbol::New("c.")},
      {Token::Type::ID, {&kFilename, 23, 1}, Symbol::New("c@")},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, BoolTest) {
  const char* kStr =
      "#t\n"
      "#f\n"
      "#T\n"
      "#F\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::BOOL, {&kFilename, 1, 1}, expr::True()},
      {Token::Type::BOOL, {&kFilename, 2, 1}, expr::False()},
      {Token::Type::BOOL, {&kFilename, 3, 1}, expr::True()},
      {Token::Type::BOOL, {&kFilename, 4, 1}, expr::False()},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, NumTest) {
  const char* kStr =
      "#b1\n"
      "#o1\n"
      "#d1\n"
      "#x1\n"

      "#i1\n"
      "#e1\n"
      "#i#b1\n"
      "#i#o1\n"
      "#e#d1\n"
      "#e#x1\n"
      "#b#e1\n"
      "#o#e1\n"
      "#d#i1\n"
      "#x#i1\n"

      "3\n"
      "+2\n"
      "-2\n"
      "4##\n"
      "5.7\n"
      "5##.##7\n"
      "7.2###\n"
      ".3###\n"

      "1s0\n"
      "1f1\n"
      "1d2\n"
      "1l3\n";

// TODO(bcf): Enable when these are supported.
#if 0
      "3/4\n"

      "5@4\n"
      "10+7i\n"
      "10-7i\n"

      "+13i\n"
      "-14i\n"

      "+i\n"
      "-i\n"

      "2e-10\n"
      "2e+10\n"
      "#i3###.##4e-27d@4##.#5e14\n";
#endif

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::NUMBER, {&kFilename, 1, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 2, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 3, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 4, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 5, 1}, new Float(1)},
      {Token::Type::NUMBER, {&kFilename, 6, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 7, 1}, new Float(1.0)},
      {Token::Type::NUMBER, {&kFilename, 8, 1}, new Float(1.0)},
      {Token::Type::NUMBER, {&kFilename, 9, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 10, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 11, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 12, 1}, new Int(1)},
      {Token::Type::NUMBER, {&kFilename, 13, 1}, new Float(1.0)},
      {Token::Type::NUMBER, {&kFilename, 14, 1}, new Float(1.0)},
      {Token::Type::NUMBER, {&kFilename, 15, 1}, new Int(3)},
      {Token::Type::NUMBER, {&kFilename, 16, 1}, new Int(2)},
      {Token::Type::NUMBER, {&kFilename, 17, 1}, new Int(-2)},
      {Token::Type::NUMBER, {&kFilename, 18, 1}, new Float(400)},
      {Token::Type::NUMBER, {&kFilename, 19, 1}, new Float(5.7)},
      {Token::Type::NUMBER, {&kFilename, 20, 1}, new Float(500.007)},
      {Token::Type::NUMBER, {&kFilename, 21, 1}, new Float(7.2)},
      {Token::Type::NUMBER, {&kFilename, 22, 1}, new Float(0.3)},
      {Token::Type::NUMBER, {&kFilename, 23, 1}, new Float(1.0)},
      {Token::Type::NUMBER, {&kFilename, 24, 1}, new Float(10.0)},
      {Token::Type::NUMBER, {&kFilename, 25, 1}, new Float(100.0)},
      {Token::Type::NUMBER, {&kFilename, 26, 1}, new Float(1000.0)},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, CharTest) {
  std::string input;
  const int kAsciiChars = 127;
  for (int i = 0, line = 0; i < kAsciiChars; ++i) {
    if (util::IsDelim(i)) {
      continue;
    }
    ++line;
    input += "#\\";
    input.push_back(i);
    input += "\n";
  }
  const std::string kFilename = "foo";

  std::vector<Token> expected;
  for (int i = 0, line = 0; i < kAsciiChars; ++i) {
    if (util::IsDelim(i)) {
      continue;
    }
    ++line;
    Token expect = {Token::Type::CHAR,
                    {&kFilename, line, 1},
                    new Char(static_cast<char>(i))};
    expected.push_back(expect);
  }

  std::istringstream s(input);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, expected);
}

TEST_F(LexerTest, CharTestSpaceNewline) {
  const char* kStr =
      "#\\space\n"
      "#\\newline\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::CHAR, {&kFilename, 1, 1}, new Char(' ')},
      {Token::Type::CHAR, {&kFilename, 2, 1}, new Char('\n')},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, StringTest) {
  const char* kStr =
      "\"abc\"\n"
      "\"\\abc\"\n"
      "\"a\\bc\"\n"
      "\"\\\\abc\"\n"
      "\"\\\"abc\"\n"
      "\"foo\\\\abc\"\n"
      "\"foo\\\"abc\"\n"
      "\"abc\\\\\"\n"
      "\"abc\\\"\"\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::STRING, {&kFilename, 1, 1}, new String("abc")},
      {Token::Type::STRING, {&kFilename, 2, 1}, new String("abc")},
      {Token::Type::STRING, {&kFilename, 3, 1}, new String("abc")},
      {Token::Type::STRING, {&kFilename, 4, 1}, new String("\\abc")},
      {Token::Type::STRING, {&kFilename, 5, 1}, new String("\"abc")},
      {Token::Type::STRING, {&kFilename, 6, 1}, new String("foo\\abc")},
      {Token::Type::STRING, {&kFilename, 7, 1}, new String("foo\"abc")},
      {Token::Type::STRING, {&kFilename, 8, 1}, new String("abc\\")},
      {Token::Type::STRING, {&kFilename, 9, 1}, new String("abc\"")},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

TEST_F(LexerTest, OtherTest) {
  const char* kStr =
      "(\n"
      ")\n"
      "#(\n"
      "'\n"
      "`\n"
      ",\n"
      ",@\n"
      ".\n";

  const std::string kFilename = "foo";

  const std::vector<Token> kExpected = {
      {Token::Type::LPAREN, {&kFilename, 1, 1}, nullptr},
      {Token::Type::RPAREN, {&kFilename, 2, 1}, nullptr},
      {Token::Type::POUND_PAREN, {&kFilename, 3, 1}, nullptr},
      {Token::Type::QUOTE, {&kFilename, 4, 1}, nullptr},
      {Token::Type::BACKTICK, {&kFilename, 5, 1}, nullptr},
      {Token::Type::COMMA, {&kFilename, 6, 1}, nullptr},
      {Token::Type::COMMA_AT, {&kFilename, 7, 1}, nullptr},
      {Token::Type::DOT, {&kFilename, 8, 1}, nullptr},
  };

  std::istringstream s(kStr);
  util::TextStream stream(&s, kFilename);
  Lexer lexer{stream};

  VerifyTokens(&lexer, kExpected);
}

}  // namespace parse
