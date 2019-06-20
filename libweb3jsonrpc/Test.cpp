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
/** @file Test.cpp
 * @authors:
 *   Dimitry Khokhlov <dimitry@ethdev.com>
 * @date 2016
 */

#include "Test.h"
#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>
#include "types.hpp"

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::rpc;
using namespace jsonrpc;
using namespace taraxa;

Test::Test(std::shared_ptr<taraxa::FullNode>& _full_node): full_node_(_full_node) {}

namespace
{
string logEntriesToLogHash(eth::LogEntries const& _logs)
{
    RLPStream s;
    s.appendList(_logs.size());
    for (eth::LogEntry const& l : _logs)
        l.streamRLP(s);
    return toJS(sha3(s.out()));
}

h256 stringToHash(string const& _hashString)
{
    try
    {
        return h256(_hashString);
    }
    catch (BadHexCharacter const&)
    {
        throw JsonRpcException(Errors::ERROR_RPC_INVALID_PARAMS);
    }
}
}

Json::Value Test::insert_dag_block(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {
            blk_hash_t pivot = blk_hash_t(param1["pivot"].asString());
            
            vec_blk_t tips = asVector<blk_hash_t>(param1["tips"]);
            taraxa::sig_t signature = taraxa::sig_t(
                "777777777777777777777777777777777777777777777777777777777777777777"
                "777777777777777777777777777777777777777777777777777777777777777");
            blk_hash_t hash = blk_hash_t(param1["hash"].asString());
            addr_t sender = addr_t(param1["sender"].asString());

            DagBlock blk(pivot, 0, tips, {}, signature, hash, sender);
            res = blk.getJsonStr();
            node->insertBlock(std::move(blk));
        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}


Json::Value Test::insert_stamped_dag_block(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block_children(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block_siblings(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block_tips(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block_pivot_chain(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block_subtree(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_dag_block_epfriend(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::send_coin_transaction(const Json::Value& param1)
{
    printf("send_coin_transaction");
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {
            auto &log_time = node->getTimeLogger();
            secret_t sk = secret_t(param1["secret"].asString());
            bal_t nonce = param1["nonce"].asUInt64();
            bal_t value = param1["value"].asUInt64();
            val_t gas_price = val_t(param1["gas_price"].asString());
            val_t gas = val_t(param1["gas"].asString());
            addr_t receiver = addr_t(param1["receiver"].asString());
            bytes data;
            // get trx receiving time stamp
            auto now = getCurrentTimeMilliSeconds();
            taraxa::Transaction trx(nonce, value, gas_price, gas, receiver, data, sk);
            LOG(log_time) << "Transaction " << trx.getHash()
                        << " received at: " << now;
            node->insertTransaction(trx);
            res = trx.getJsonStr();
        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::create_test_coin_transactions(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_num_proposed_blocks(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::send_pbft_schedule_block(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_account_address(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::set_account_balance(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_account_balance(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_peer_count(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_all_peers(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::node_stop(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::node_reset(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::node_start(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::should_speak(const Json::Value& param1)
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::place_vote(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::get_votes(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

Json::Value Test::draw_graph(const Json::Value& param1) 
{
    Json::Value res;
    try 
    {
        if(auto node = full_node_.lock()) 
        {

        }
      } catch (std::exception &e) {
        res = e.what();
      }
      return res;
}

