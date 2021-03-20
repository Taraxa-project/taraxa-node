/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "Common.h"
#include "Guards.h"
#include "db.h"

namespace dev {
namespace db {

class MemoryDB : public DatabaseFace {
 public:
  std::string lookup(Slice _key) const override;
  void insert(Slice _key, Slice _value) override;

  size_t size() const { return m_db.size(); }

 private:
  std::unordered_map<std::string, std::string> m_db;
  mutable Mutex m_mutex;
};
}  // namespace db
}  // namespace dev