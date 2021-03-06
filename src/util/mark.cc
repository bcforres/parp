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

#include "util/mark.h"

#include <string>

namespace util {

std::ostream& operator<<(std::ostream& stream, Mark const& mark) {
  return stream << *mark.path << ":" << mark.line << ":" << mark.col;
}

bool operator==(const Mark& lhs, const Mark& rhs) {
  return lhs.line == rhs.line && lhs.col == rhs.col && *lhs.path == *rhs.path;
}

}  // namespace util
