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
/** @file Common.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Common.h"

#include <libdevcore/Base64.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/Terminal.h>

#include "BlockHeader.h"
#include "Exceptions.h"

using namespace std;
using namespace dev;
using namespace ::taraxa::aleth;

namespace taraxa::aleth {

const unsigned c_protocolVersion = 63;

Address toAddress(std::string const& _s) {
  try {
    auto b = fromHex(_s.substr(0, 2) == "0x" ? _s.substr(2) : _s, WhenError::Throw);
    if (b.size() == 20) return Address(b);
  } catch (BadHexCharacter&) {
  }
  BOOST_THROW_EXCEPTION(InvalidAddress());
}

}  // namespace taraxa::aleth
