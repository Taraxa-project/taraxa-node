#include "chain_config.hpp"

#include <sstream>

namespace taraxa::conf::chain_config {
using std::stringstream;

ChainConfig ChainConfig::from_json(string const& json_str) {
  Json::Value json(Json::objectValue);
  stringstream(json_str) >> json;
  return from_json(json);
}

ChainConfig ChainConfig::from_json(Json::Value const& json) {
  return {
      json["replay_protection_service_range"].asUInt64(),
      DagBlock(json["dag_genesis_block"].toStyledString()),
      ChainParams(json["eth"].toStyledString()),
  };
}

decltype(ChainConfig::PREDEF) const ChainConfig::PREDEF([] {
  decltype(PREDEF)::val_t ret;
  // Here go predefined configs e.g. mainnet, testnet
  ret["default"] = ChainConfig::from_json(string() +
                                          R"(
{
  "replay_protection_service_range": 10,
  "dag_genesis_block": {
    "level": 0,
    "tips": [],
    "trxs": [],
    "sig": "b7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f25be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701",
    "hash": "c9524784c4bf29e6facdd94ef7d214b9f512cdfd0f68184432dab85d053cbc69",
    "sender": "de2b1203d72d3549ee2f733b00b2789414c7cea5",
    "pivot": "0000000000000000000000000000000000000000000000000000000000000000",
    "timestamp": 1564617600
  },
  "eth": {
    "sealEngine": "TaraxaSealEngine",
    "params": {
      "accountStartNonce": "0x0",
      "homesteadForkBlock": "0x0",
      "EIP150ForkBlock": "0x0",
      "EIP158ForkBlock": "0x0",
      "byzantiumForkBlock": "0x0",
      "constantinopleForkBlock": "0x0",
      "constantinopleFixForkBlock": "0x0",
      "networkID": "0x42",
      "chainID": "0x42",
      "maximumExtraDataSize": "0x20",
      "tieBreakingGas": false,
      "minGasLimit": "0x1388",
      "maxGasLimit": "7fffffffffffffff",
      "gasLimitBoundDivisor": "0x1",
      "minimumDifficulty": "0x0",
      "difficultyBoundDivisor": "0x0800",
      "durationLimit": "0x0d",
      "blockReward": "0x0",
      "allowFutureBlocks": true
    },
    "genesis": {
      "nonce": "0x0",
      "difficulty": "0x0",
      "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
      "author": "0x0000000000000000000000000000000000000000",
      "timestamp": "0x0",
      "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
      "extraData": "0x11bbe8db4e347b4e8c937c1c8370e4b5ed33adb3db69cbdb7a38e1e50b1b82fa",
      "gasLimit": "7fffffffffffffff"
    },
    "accounts": {
      "de2b1203d72d3549ee2f733b00b2789414c7cea5": {
        "balance": "9007199254740991"
      },
      "0000000000000000000000000000000000000001": {
        "precompiled": {
          "name": "ecrecover",
          "linear": {
            "base": 3000,
            "word": 0
          }
        }
      },
      "0000000000000000000000000000000000000002": {
        "precompiled": {
          "name": "sha256",
          "linear": {
            "base": 60,
            "word": 12
          }
        }
      },
      "0000000000000000000000000000000000000003": {
        "precompiled": {
          "name": "ripemd160",
          "linear": {
            "base": 600,
            "word": 120
          }
        }
      },
      "0000000000000000000000000000000000000004": {
        "precompiled": {
          "name": "identity",
          "linear": {
            "base": 15,
            "word": 3
          }
        }
      },
      "0000000000000000000000000000000000000005": {
        "precompiled": {
          "name": "modexp",
          "startingBlock": "0x42ae50"
        }
      },
      "0000000000000000000000000000000000000006": {
        "precompiled": {
          "name": "alt_bn128_G1_add",
          "startingBlock": "0x42ae50",
          "linear": {
            "base": 500,
            "word": 0
          }
        }
      },
      "0000000000000000000000000000000000000007": {
        "precompiled": {
          "name": "alt_bn128_G1_mul",
          "startingBlock": "0x42ae50",
          "linear": {
            "base": 40000,
            "word": 0
          }
        }
      },
      "0000000000000000000000000000000000000008": {
        "precompiled": {
          "name": "alt_bn128_pairing_product",
          "startingBlock": "0x42ae50"
        }
      }
    }
  }
}
)");
  return move(ret);
});

}  // namespace taraxa::conf::chain_config
