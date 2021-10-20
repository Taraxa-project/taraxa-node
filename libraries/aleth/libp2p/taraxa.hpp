#pragma once

#include "libp2p/Common.h"

namespace dev::p2p {

struct TaraxaNetworkConfig {
  unsigned ideal_peer_count = 11;
  unsigned peer_stretch = 7;
  bool is_boot_node = false;
  uint expected_parallelism = 1;
  std::chrono::seconds peer_healthcheck_interval{30};
  std::chrono::seconds peer_healthcheck_timeout{1};
  std::chrono::milliseconds main_loop_interval{100};
  std::chrono::seconds log_active_peers_interval{30};
};

class CapabilityFace;

struct Capability {
  std::shared_ptr<CapabilityFace> const ref;
  unsigned const message_count = 0;

  Capability(std::shared_ptr<CapabilityFace> ref, unsigned message_count)
      : ref(move(ref)), message_count(message_count) {}
};

struct SessionCapability : Capability {
  unsigned const version = 0;
  unsigned const offset = 0;

  SessionCapability(Capability const& cap, unsigned version, unsigned offset)
      : Capability(cap), version(version), offset(offset) {}
};

using CapabilityNameAndVersion = CapDesc;
using Capabilities = std::map<CapabilityNameAndVersion, Capability>;
using SessionCapabilities = std::map<std::string, SessionCapability>;

}  // namespace dev::p2p