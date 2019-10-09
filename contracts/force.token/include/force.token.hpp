#pragma once
#include <../../codexlib/global.hpp>

namespace force {
   using std::string;
   static constexpr uint32_t PRE_CAST_NUM = 5184000;
   static constexpr uint32_t STABLE_CAST_NUM = 1209600;
   static constexpr double WEAKEN_CAST_NUM = 2.5;

   struct [[eosio::table, eosio::contract("force.token")]] account {
      asset    balance;

      uint64_t primary_key()const { return balance.symbol.raw(); }
   };
   typedef eosio::multi_index<"accounts"_n, account> accounts;

   struct [[eosio::table, eosio::contract("force.token")]] currency_stats {
      asset          supply;
      asset          max_supply;
      account_name   issuer;

      uint64_t primary_key()const { return supply.symbol.raw(); }
   };
   typedef eosio::multi_index<"stat"_n, currency_stats> stats;

   struct [[eosio::table, eosio::contract("force.token")]] coin_cast {
      asset    balance ;
      uint32_t   finish_block = 0;

      uint64_t primary_key()const { return static_cast<uint64_t>(finish_block); }
   };
   typedef eosio::multi_index<"coincast"_n, coin_cast> coincasts;
   
   class [[eosio::contract("force.token")]] token : public contract {
      public:
         using contract::contract;

         ACTION create( account_name issuer,
                     asset        maximum_supply);

         ACTION issue( account_name to, asset quantity, string memo );

         ACTION transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         ACTION fee( account_name payer, asset quantity );

         ACTION castcoin(account_name from,account_name to,asset quantity);
         ACTION takecoin(account_name to);

         using create_action = action_wrapper<"create"_n, &token::create>;
         using issue_action = action_wrapper<"issue"_n, &token::issue>;
         using transfer_action = action_wrapper<"transfer"_n, &token::transfer>;
         using fee_action = action_wrapper<"fee"_n, &token::fee>;
         using castcoin_action = action_wrapper<"castcoin"_n, &token::castcoin>;
         using takecoin_action = action_wrapper<"takecoin"_n, &token::takecoin>;

      public:
         static asset get_balance( account_name owner, symbol sym ) {
            accounts accountstable( config::token_account, owner.value );
            const auto& ac = accountstable.get( sym.raw() );
            return ac.balance;
         }

      private:
         void sub_balance( account_name owner, asset value );
         void add_balance( account_name owner, asset value, account_name ram_payer );
   };

}