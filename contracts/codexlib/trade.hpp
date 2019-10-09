#pragma once

#include "global.hpp"

namespace trade {

   

   constexpr inline int64_t precision( uint64_t decimals ) {
      constexpr uint64_t res_size = 18;
      constexpr int64_t res[res_size] = 
         {  1, 10, 100, 1000, 10000, 
            100000, 
            1000000, 
            10000000,
            100000000,
            1000000000,
            10000000000,
            100000000000,
            1000000000000,
            10000000000000,
            100000000000000,
            1000000000000000,
            10000000000000000,
            100000000000000000 };

      if( decimals < res_size ) {
         return res[decimals];
      } else {
         auto p10 = res[res_size - 1];
         for( auto p = static_cast<int64_t>(decimals - res_size + 1); 
              p > 0; --p ) {
            p10 *= 10;
         }
         return p10;
      }
   }

   int countChar(const string &source,const char dest) {
      int result = 0;
      for(auto char_temp:source) {
         if (char_temp == dest) {
            result ++;
         }
      }

      return result;
   }

   void splitMemo( std::vector<std::string>& results, const std::string& memo, char separator ) {
      auto start = memo.cbegin();
      auto end = memo.cend();

      for( auto it = start; it != end; ++it ) {
         if( *it == separator ) {
            results.emplace_back(start, it);
            start = it + 1;
         }
      }
      if (start != end) results.emplace_back(start, end);
   }

   enum class func_typ : uint64_t {
      match              = 1,
      bridge_addmortgage = 2,
      bridge_exchange    = 3,
      trade_type_count   = 4
   };

   struct sys_bridge_addmort {
      eosio::name  trade_name;
      account_name trade_maker = name{0};
      uint64_t     type        = 0;

      explicit sys_bridge_addmort( const std::string& memo ) {
         std::vector<std::string> memoParts;
         memoParts.reserve( 8 );
         splitMemo( memoParts, memo, ';' );

         eosio_assert( memoParts.size() == 3, "memo is not adapted with bridge" );

         this->trade_name = name{ global_func::string_to_name( memoParts[0].c_str() ) };
         this->trade_maker = name{ global_func::string_to_name( memoParts[1].c_str() ) };
         this->type = atoi( memoParts[2].c_str() );

         // FIXME: should use enum
         eosio_assert( this->type == 1 || this->type == 3,
                       "type is not adapted with bridge addmort" );
      }
   };

   struct sys_bridge_exchange {
      eosio::name  trade_name;
      account_name trade_maker;
      account_name recv       ;
      uint64_t     type;

      explicit sys_bridge_exchange( const std::string& memo ) {
         std::vector<std::string> memoParts;
         memoParts.reserve( 8 );
         splitMemo( memoParts, memo, ';' );

         eosio_assert( memoParts.size() == 4, "memo is not adapted with bridge" );

         this->trade_name = name{ global_func::string_to_name( memoParts[0].c_str() ) };
         this->trade_maker = name{ global_func::string_to_name( memoParts[1].c_str() ) };
         this->recv = name{ global_func::string_to_name( memoParts[2].c_str() ) };
         this->type = atoi( memoParts[3].c_str() );

         // FIXME: should use enum
         eosio_assert( this->type == 1 || this->type == 2,
                       "type is not adapted with bridge exchange" );
      }
   };



}