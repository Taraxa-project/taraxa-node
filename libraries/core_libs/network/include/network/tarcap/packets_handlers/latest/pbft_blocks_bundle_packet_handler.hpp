#pragma once

#include "common/packet_handler.hpp"

namespace taraxa {
class PbftManager;
namespace final_chain {
class FinalChain;
}

}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftSyncingState;

class PbftBlocksBundlePacketHandler : public PacketHandler {
 public:
  PbftBlocksBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                std::shared_ptr<PbftManager> pbft_mgr,
                                std::shared_ptr<final_chain::FinalChain> final_chain,
                                std::shared_ptr<PbftSyncingState> syncing_state, const addr_t& node_addr,
                                const std::string& logs_prefix = "");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kPbftBlocksBundlePacket;
  static constexpr size_t kMaxBlocksInPacket = 10;

 private:
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<final_chain::FinalChain> final_chain_;
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
};

}  // namespace taraxa::network::tarcap
