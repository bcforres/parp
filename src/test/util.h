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

#ifndef TEST_UTIL_H_
#define TEST_UTIL_H_

#include "gtest/gtest.h"
#include "gc/gc.h"

namespace test {

class TestBase : public testing::Test {
 protected:
  void SetUp() final {
    // Clear all objects from heap
    gc::Gc::Get().Purge();

    TestSetUp();
  }

  void TearDown() final {
    TestTearDown();

    // Clear all objects from heap
    gc::Gc::Get().Purge();
  }

  // Test fixtures should override these instead.
  virtual void TestSetUp() {}
  virtual void TestTearDown() {}
};

}  // namespace test

#endif  // TEST_UTIL_H_