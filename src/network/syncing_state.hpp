#pragma once

#include <libp2p/Common.h>

#include <atomic>
#include <optional>
#include <shared_mutex>

namespace taraxa {

class SyncingState {
 public:
  void set_pbft_syncing(bool syncing, const std::optional<dev::p2p::NodeID>& peer_id = {});
  void set_dag_syncing(bool syncing, const std::optional<dev::p2p::NodeID>& peer_id = {});

  bool is_syncing() const;
  bool is_pbft_syncing() const;
  bool is_dag_syncing() const;

  dev::p2p::NodeID syncing_peer() const;

 private:
  void set_peer(const dev::p2p::NodeID& peer_id);

 private:
  std::atomic<bool> pbft_syncing_{false};
  std::atomic<bool> dag_syncing_{false};

  // Peer id that the node is syncing with
  dev::p2p::NodeID peer_id_;
  mutable std::shared_mutex peer_mutex_;
};

}  // namespace taraxa
