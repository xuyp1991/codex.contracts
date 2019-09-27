#pragma once
#include <../../codexlib/config.hpp>

namespace relay {
   using std::string;

   inline static uint128_t get_account_idx(const name& chain, const asset& a) {
            return (uint128_t(uint64_t(chain.value)) << 64) + uint128_t(a.symbol.raw());
         }

   struct [[eosio::table, eosio::contract("relay.token")]] account {
      uint64_t id;
      asset balance;
      name  chain;

      int128_t      mineage               = 0;         // asset.amount * block height
      uint32_t     mineage_update_height = 0;
      int64_t      pending_mineage       = 0;

      uint64_t  primary_key() const { return id; }
      uint128_t get_index_i128() const { return get_account_idx(chain, balance); }
   };
   typedef eosio::multi_index<"accounts"_n, account,
      indexed_by< "bychain"_n,
                  const_mem_fun<account, uint128_t, &account::get_index_i128 >>> accounts;

   struct [[eosio::table, eosio::contract("relay.token")]] account_next_id {
      uint64_t     id;
      account_name account;

      uint64_t  primary_key() const { return account.value; }
   };
   typedef eosio::multi_index<"accountid"_n, account_next_id> account_next_ids ;

   struct [[eosio::table, eosio::contract("relay.token")]] currency_stats {
      asset        supply;
      asset        max_supply;
      account_name issuer;
      name         chain;

      asset        reward_pool;
      int128_t      total_mineage               = 0;         // asset.amount * block height
      uint32_t     total_mineage_update_height = 0;
      int64_t      total_pending_mineage       = 0;

      uint64_t primary_key() const { return supply.symbol.raw(); }
   };
   typedef multi_index<"stat"_n, currency_stats> stats;
   
   struct [[eosio::table, eosio::contract("relay.token")]] reward_currency {
      uint64_t     id;
      name         chain;
      asset        supply;

      uint64_t primary_key() const { return id; }
      uint128_t get_index_i128() const { return get_account_idx(chain, supply); }
   };
   typedef multi_index<"reward"_n, reward_currency,
               indexed_by< "bychain"_n,
                           const_mem_fun<reward_currency, uint128_t, &reward_currency::get_index_i128 >>> rewards;

   class [[eosio::contract("relay.token")]] token : public contract {
      public:
         using contract::contract;

         struct action {
            account_name from;
            account_name to;
            asset quantity;
            std::string memo;

            EOSLIB_SERIALIZE(action, (from)(to)(quantity)(memo))
         };

         /// @abi action
         //ACTION on( name chain, const checksum256 block_id, const force::relay::action& act );

         /// @abi action
         ACTION create( account_name issuer,
                     name chain,
                     asset maximum_supply );

         /// @abi action
         ACTION issue( name chain, account_name to, asset quantity, string memo );

         /// @abi action
         ACTION destroy( name chain, account_name from, asset quantity, string memo );

         /// @abi action
         ACTION transfer( account_name from,
                        account_name to,
                        name chain,
                        asset quantity,
                        string memo );
                        
         /// @abi action
         ACTION addreward(name chain,asset supply);
         /// @abi action
         ACTION rewardmine(asset quantity);
         /// @abi action
         ACTION claim(name chain,asset quantity,account_name receiver);

         using create_action = action_wrapper<"create"_n, &token::create>;
         using issue_action = action_wrapper<"issue"_n, &token::issue>;
         using destroy_action = action_wrapper<"destroy"_n, &token::destroy>;
         using transfer_action = action_wrapper<"transfer"_n, &token::transfer>;
         using addreward_action = action_wrapper<"addreward"_n, &token::addreward>;
         using rewardmine_action = action_wrapper<"rewardmine"_n, &token::rewardmine>;
         using claim_action = action_wrapper<"claim"_n, &token::claim>;
      public:
         static asset get_supply( name chain, symbol sym ) {
            stats statstable( config::relay_token_account, chain.value );
            const auto& st = statstable.get( sym.raw() );
            return st.supply;
         }
      private:
         void sub_balance( account_name owner, name chain, asset value );
         void add_balance( account_name owner, name chain, asset value, account_name ram_payer );

         int64_t get_current_age(name chain,asset balance,int64_t first,int64_t last);
   };

}