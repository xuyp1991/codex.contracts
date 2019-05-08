#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
using namespace eosio;

typedef name account_name;
typedef name action_name;
typedef uint16_t weight_type;
typedef  checksum256 transaction_id_type;
typedef  checksum256 block_id_type;
typedef  std::vector<char> bytes;
typedef uint64_t permission_name;

#define CORE_SYMBOL symbol(symbol_code("CDX"), 4)
#define BEFORE_ONLINE_TEST 1
using std::string;

namespace config {

   static constexpr eosio::name token_account{"force.token"_n};
   static constexpr eosio::name system_account{"force"_n};
   static constexpr eosio::name active_permission{"active"_n};
   static constexpr eosio::name reward_account{"force.reward"_n};
   static constexpr eosio::name bridge_account{"sys.bridge"_n};
   static constexpr eosio::name match_account{"sys.match"_n};
   static constexpr eosio::name relay_token_account{"relay.token"_n};

   static constexpr uint32_t FROZEN_DELAY = 3*24*60*60;
   static constexpr int NUM_OF_TOP_BPS = 21;

#ifdef BEFORE_ONLINE_TEST   
   static constexpr uint32_t UPDATE_CYCLE = 63;//42;//CONTRACT_UPDATE_CYCLE;//630; 
#else
   static constexpr uint32_t UPDATE_CYCLE = 315;//42;//CONTRACT_UPDATE_CYCLE;//630; 
#endif
   static constexpr uint64_t OTHER_COIN_WEIGHT = 500;
}

