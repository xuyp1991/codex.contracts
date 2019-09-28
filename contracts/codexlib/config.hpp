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

enum  class func_type:uint64_t {
   match=1,
   bridge_addmortgage,
   bridge_exchange,
   trade_type_count
};

namespace global_func {
   uint64_t char_to_symbol( char c ) {
   if( c >= 'a' && c <= 'z' )
      return (c - 'a') + 6;
   if( c >= '1' && c <= '5' )
      return (c - '1') + 1;
   return 0;
}

uint64_t string_to_name( const char* str ) {
   uint64_t name = 0;
   int i = 0;
   for ( ; str[i] && i < 12; ++i) {
      // NOTE: char_to_symbol() returns char type, and without this explicit
      // expansion to uint64 type, the compilation fails at the point of usage
      // of string_to_name(), where the usage requires constant (compile time) expression.
      name |= (char_to_symbol(str[i]) & 0x1f) << (64 - 5 * (i + 1));
   }

   // The for-loop encoded up to 60 high bits into uint64 'name' variable,
   // if (strlen(str) > 12) then encode str[12] into the low (remaining)
   // 4 bits of 'name'
   if (i == 12)
      name |= char_to_symbol(str[12]) & 0x0F;
   return name;
}
}



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

