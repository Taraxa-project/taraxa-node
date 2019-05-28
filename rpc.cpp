#include "rpc.hpp"
#include "dag_block.hpp"
#include "full_node.hpp"
#include "pbft_chain.hpp"
#include "pbft_manager.hpp"
#include "transaction.hpp"
#include "util.hpp"
#include "vote.h"
#include "wallet.hpp"

namespace taraxa {

Rpc::Rpc(boost::asio::io_context &io, RpcConfig const &conf_rpc,
         std::shared_ptr<FullNode> node)
    : conf_(conf_rpc), io_context_(io), acceptor_(io), node_(node) {
  LOG(log_si_) << "Taraxa RPC started at port: " << conf_.port << std::endl;
}
std::shared_ptr<Rpc> Rpc::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "Rpc: " << e.what() << std::endl;
    assert(false);
    return nullptr;
  }
}

void Rpc::start() {
  if (!stopped_) {
    return;
  }
  stopped_ = false;
  boost::asio::ip::tcp::endpoint ep(conf_.address, conf_.port);
  acceptor_.open(ep.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

  boost::system::error_code ec;
  acceptor_.bind(ep, ec);
  if (ec) {
    LOG(log_er_) << "RPC cannot bind ... " << ec.message() << "\n";
    throw std::runtime_error(ec.message());
  }
  acceptor_.listen();

  LOG(log_nf_) << "Rpc is listening on port " << conf_.port << std::endl;
  waitForAccept();
}

void Rpc::waitForAccept() {
  std::shared_ptr<RpcConnection> connection(
      std::make_shared<RpcConnection>(getShared(), node_));
  acceptor_.async_accept(
      connection->getSocket(),
      [this, connection](boost::system::error_code const &ec) {
        if (!ec) {
          connection->read();
        } else {
          if (stopped_) return;

          LOG(log_er_) << "Error! Rpc async_accept error ... " << ec.message()
                       << "\n";
          throw std::runtime_error(ec.message());
        }
        if (stopped_) return;
        waitForAccept();
      });
}

void Rpc::stop() {
  if (stopped_) return;
  stopped_ = true;
  acceptor_.close();
}

std::shared_ptr<RpcConnection> RpcConnection::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "RpcConnection: " << e.what() << std::endl;
    return nullptr;
  }
}

RpcConnection::RpcConnection(std::shared_ptr<Rpc> rpc,
                             std::shared_ptr<FullNode> node)
    : rpc_(rpc), node_(node), socket_(rpc->getIoContext()) {
  responded_.clear();
}

void RpcConnection::read() {
  auto this_sp = getShared();
  boost::beast::http::async_read(
      socket_, buffer_, request_,
      [this_sp](boost::system::error_code const &ec, size_t byte_transfered) {
        if (!ec) {
          LOG(this_sp->rpc_->log_tr_) << "Read ... " << std::endl;

          // define response handler
          auto replier([this_sp](std::string const &msg) {
            // prepare response content
            std::string body = msg;
            this_sp->write_response(msg);
            // async write
            boost::beast::http::async_write(
                this_sp->socket_, this_sp->response_,
                [this_sp](boost::system::error_code const &ec,
                          size_t byte_transfered) {});
          });
          // pass response handler
          if (this_sp->request_.method() == boost::beast::http::verb::post) {
            std::shared_ptr<RpcHandler> rpc_handler(
                new RpcHandler(this_sp->rpc_, this_sp->node_,
                               this_sp->request_.body(), replier));
            try {
              rpc_handler->processRequest();
            } catch (...) {
              throw;
            }
          }
        } else {
          LOG(this_sp->rpc_->log_er_)
              << "Error! RPC conncetion read fail ... " << ec.message() << "\n";
        }
        (void)byte_transfered;
      });
}

void RpcConnection::write_response(std::string const &msg) {
  if (!responded_.test_and_set()) {
    response_.set("Content-Type", "application/json");
    response_.set("Access-Control-Allow-Origin", "*");
    response_.set("Access-Control-Allow-Headers",
                  "Accept, Accept-Language, Content-Language, Content-Type");
    response_.set("Connection", "close");
    response_.result(boost::beast::http::status::ok);
    response_.body() = msg;
    response_.prepare_payload();
  } else {
    assert(false && "RPC already responded ...\n");
  }
}

RpcHandler::RpcHandler(
    std::shared_ptr<Rpc> rpc, std::shared_ptr<FullNode> node,
    std::string const &body,
    std::function<void(std::string const &msg)> const &response_handler)
    : rpc_(rpc),
      node_(node),
      body_(body),
      in_doc_(taraxa::strToJson(body_)),
      replier_(response_handler) {}

std::shared_ptr<RpcHandler> RpcHandler::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "RpcHandler: " << e.what() << std::endl;
    return nullptr;
  }
}

void RpcHandler::processRequest() {
  try {
    std::string action = in_doc_.get<std::string>("action");
    std::string res;
    if (!rpc_) {
      std::cerr << "Error! RPC unavailable in RpcHandler \n" << std::endl;
      return;
    }
    if (!node_) {
      LOG(rpc_->log_er_) << "Node unavailable \n" << std::endl;
      return;
    }
    auto &log_time = node_->getTimeLogger();
    if (action == "insert_dag_block") {
      try {
        blk_hash_t pivot = blk_hash_t(in_doc_.get<std::string>("pivot"));
        vec_blk_t tips = asVector<blk_hash_t, std::string>(in_doc_, "tips");
        sig_t signature = sig_t(
            "777777777777777777777777777777777777777777777777777777777777777777"
            "777777777777777777777777777777777777777777777777777777777777777");
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        addr_t sender = addr_t(in_doc_.get<std::string>("sender"));

        DagBlock blk(pivot, 0, tips, {}, signature, hash, sender);
        res = blk.getJsonStr();
        node_->insertBlock(std::move(blk));
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "insert_stamped_dag_block") {
      try {
        blk_hash_t pivot = blk_hash_t(in_doc_.get<std::string>("pivot"));
        vec_blk_t tips = asVector<blk_hash_t, std::string>(in_doc_, "tips");
        sig_t signature = sig_t(
            "777777777777777777777777777777777777777777777777777777777777777777"
            "777777777777777777777777777777777777777777777777777777777777777");
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        addr_t sender = addr_t(in_doc_.get<std::string>("sender"));
        time_stamp_t stamp = in_doc_.get<time_stamp_t>("stamp");
        DagBlock blk(pivot, 0, tips, {}, signature, hash, sender);
        res = blk.getJsonStr();
        node_->insertBlock(std::move(blk));
        node_->setDagBlockTimeStamp(hash, stamp);
        res += ("\n Block stamped at: " + std::to_string(stamp));
      } catch (std::exception &e) {
        res = e.what();
      }
    }

    else if (action == "get_dag_block") {
      try {
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        auto blk = node_->getDagBlock(hash);
        if (!blk) {
          res = "Block not available \n";
        } else {
          time_stamp_t stamp = node_->getDagBlockTimeStamp(hash);
          res = blk->getJsonStr() + "\ntime_stamp: " + std::to_string(stamp);
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_dag_block_children") {
      try {
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        time_stamp_t stamp = in_doc_.get<time_stamp_t>("stamp");

        std::vector<std::string> children;
        children = node_->getDagBlockChildren(hash, stamp);
        for (auto const &child : children) {
          res += (child + '\n');
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_dag_block_siblings") {
      try {
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        time_stamp_t stamp = in_doc_.get<time_stamp_t>("stamp");

        std::vector<std::string> siblings;
        siblings = node_->getDagBlockSiblings(hash, stamp);
        for (auto const &sibling : siblings) {
          res += (sibling + '\n');
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_dag_block_tips") {
      try {
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        time_stamp_t stamp = in_doc_.get<time_stamp_t>("stamp");

        std::vector<std::string> tips;
        tips = node_->getDagBlockTips(hash, stamp);
        for (auto const &tip : tips) {
          res += (tip + '\n');
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_dag_block_pivot_chain") {
      try {
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        time_stamp_t stamp = in_doc_.get<time_stamp_t>("stamp");

        std::vector<std::string> pivot_chain;
        pivot_chain = node_->getDagBlockPivotChain(hash, stamp);
        for (auto const &pivot : pivot_chain) {
          res += (pivot + '\n');
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_dag_block_subtree") {
      try {
        blk_hash_t hash = blk_hash_t(in_doc_.get<std::string>("hash"));
        time_stamp_t stamp = in_doc_.get<time_stamp_t>("stamp");

        std::vector<std::string> subtree;
        subtree = node_->getDagBlockSubtree(hash, stamp);
        for (auto const &v : subtree) {
          res += (v + '\n');
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_dag_block_epfriend") {
      try {
        blk_hash_t from_hash =
            blk_hash_t(in_doc_.get<std::string>("from_hash"));
        blk_hash_t to_hash = blk_hash_t(in_doc_.get<std::string>("to_hash"));

        std::vector<std::string> epfriend;
        epfriend = node_->getDagBlockEpFriend(from_hash, to_hash);
        for (auto const &v : epfriend) {
          res += (v + '\n');
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "send_coin_transaction") {
      try {
        secret_t sk = secret_t(in_doc_.get<std::string>("secret"));
        bal_t nonce = in_doc_.get<uint64_t>("nonce");
        bal_t value = in_doc_.get<uint64_t>("value");
        val_t gas_price = val_t(in_doc_.get<std::string>("gas_price"));
        val_t gas = val_t(in_doc_.get<std::string>("gas"));
        addr_t receiver = addr_t(in_doc_.get<std::string>("receiver"));
        bytes data;
        // get trx receiving time stamp
        auto now = getCurrentTimeMilliSeconds();
        Transaction trx(nonce, value, gas_price, gas, receiver, data, sk);
        LOG(log_time) << "Transaction " << trx.getHash()
                      << " received at: " << now;
        node_->insertTransaction(trx);
        res = trx.getJsonStr();
      } catch (std::exception &e) {
        res = e.what();
      }

    } else if (action == "create_test_coin_transactions") {
      try {
        secret_t sk = secret_t(in_doc_.get<std::string>("secret"));
        uint delay = in_doc_.get<uint>("delay");
        uint number = in_doc_.get<uint>("number");
        uint seed = in_doc_.get<uint>("seed");
        bytes data;
        // get trx receiving time stamp
        uint rnd = 1234567891;
        for (auto i = 0; i < number; ++i) {
          uint t = seed + i + 31432;
          rnd = t ^ (rnd <<= 2);
          auto now = getCurrentTimeMilliSeconds();
          Transaction trx(rnd, rnd, val_t(i + seed), val_t(i + seed),
                          addr_t(i * seed), data, sk);
          LOG(log_time) << "Transaction " << trx.getHash()
                        << " received at: " << now;
          node_->insertTransaction(trx);
          thisThreadSleepForMicroSeconds(delay);
        }
        res = "Number of " + std::to_string(number) + " created";
      } catch (std::exception &e) {
        res = e.what();
      }

    } else if (action == "get_num_proposed_blocks") {
      try {
        auto num_prop_block = node_->getNumProposedBlocks();
        res = std::to_string(num_prop_block);
        LOG(log_time) << "Number of proposed block " << num_prop_block
                      << std::endl;
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "send_pbft_schedule_block") {
      try {
        blk_hash_t prev_blk =
            blk_hash_t(in_doc_.get<std::string>("prev_pivot"));
        uint64_t timestamp = in_doc_.get<uint64_t>("timestamp");
        sig_t sig = sig_t(in_doc_.get<std::string>("sig"));
        vec_blk_t blk_order =
            asVector<blk_hash_t, std::string>(in_doc_, "blk_order");

        std::vector<size_t> trx_sizes =
            asVector<size_t, size_t>(in_doc_, "trx_sizes");
        assert(blk_order.size() == trx_sizes.size());
        std::vector<size_t> trx_modes =
            asVector<size_t, size_t>(in_doc_, "trx_modes");

        std::vector<std::vector<uint>> vec_trx_modes(trx_sizes.size());
        size_t count = 0;
        for (auto i = 0; i < trx_sizes.size(); ++i) {
          for (auto j = 0; j < trx_sizes[i]; ++j) {
            vec_trx_modes[i].emplace_back(trx_modes[count++]);
          }
        }

        TrxSchedule sche(blk_order, vec_trx_modes);
        ScheduleBlock sche_blk(prev_blk, timestamp, sche);
        res = sche_blk.getJsonStr();
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_account_address") {
      try {
        addr_t addr = node_->getAddress();
        res = addr.toString();
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "set_account_balance") {
      try {
        addr_t addr = in_doc_.get<addr_t>("address");
        bal_t bal = in_doc_.get<bal_t>("balance");
        node_->setBalance(addr, bal);
        res = "Set " + addr.toString() +
              " balance: " + boost::lexical_cast<std::string>(bal) + "\n";
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_account_balance") {
      try {
        addr_t addr = in_doc_.get<addr_t>("address");
        auto bal = node_->getBalance(addr);
        if (!bal.second) {
          res = "Account " + addr.toString() + " is not available\n";
        } else {
          res = "Get " + addr.toString() +
                " balance: " + boost::lexical_cast<std::string>(bal.first) +
                "\n";
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_peer_count") {
      try {
        auto peer = node_->getPeerCount();
        res = std::to_string(peer);
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_all_peers") {
      try {
        auto peers = node_->getAllPeers();
        for (auto const &peer : peers) {
          res += peer.toString() + "\n";
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    }
    // PBFT
    else if (action == "should_speak") {
      try {
        blk_hash_t blockhash = in_doc_.get<blk_hash_t>("blockhash");
        PbftVoteTypes type =
            static_cast<PbftVoteTypes>(in_doc_.get<int>("type"));
        uint64_t period = in_doc_.get<uint64_t>("period");
        size_t step = in_doc_.get<size_t>("step");
        if (node_->shouldSpeak(blockhash, type, period, step)) {
          res = "True";
        } else {
          res = "False";
        }
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "place_vote") {
      try {
        blk_hash_t blockhash = in_doc_.get<blk_hash_t>("blockhash");
        PbftVoteTypes type =
            static_cast<PbftVoteTypes>(in_doc_.get<int>("type"));
        uint64_t period = in_doc_.get<uint64_t>("period");
        size_t step = in_doc_.get<size_t>("step");

        // put vote into vote queue
        node_->placeVote(blockhash, type, period, step);
        // broadcast vote
        node_->broadcastVote(blockhash, type, period, step);

        res = "Place vote successfully";
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "get_votes") {
      try {
        uint64_t period = in_doc_.get<uint64_t>("period");

        std::vector<Vote> votes = node_->getVotes(period);
        VoteQueue vote_queue;
        res = vote_queue.getJsonStr(votes);
      } catch (std::exception &e) {
        res = e.what();
      }
    } else if (action == "draw_graph") {
      std::string filename = in_doc_.get<std::string>("filename");
      node_->drawGraph(filename);
      res = "Dag is drwan as " + filename + " on the server side ...";
    } else {
      res = "Unknown action " + action;
    }
    res += "\n";
    replier_(res);
  } catch (std::exception const &err) {
    replier_(err.what());
  }
}  // namespace taraxa
}  // namespace taraxa
