#pragma once

namespace taraxa {

static constexpr uint16_t c_node_major_version = 0;
static constexpr uint16_t c_node_minor_version = 1;

// Any time a change in the network protocol is introduced this version should be increased
static constexpr uint16_t c_network_protocol_version = 2;

// Major version is modified when DAG blocks, pbft blocks and any basic building blocks of our blockchan is modified
// in the db
static constexpr uint16_t c_database_major_version = 0;
// Minor version should be modified when changes to the database are made in the tables that can be rebuilt from the
// basic tables
static constexpr uint16_t c_database_minor_version = 1;

}  // namespace taraxa