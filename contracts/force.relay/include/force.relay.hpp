#pragma once
#include <../../codexlib/config.hpp>

namespace force {   
   using std::vector;
   using std::string;

   class [[eosio::contract("force.relay")]] relay : public contract {
      public:
         using contract::contract;

         struct action {
            account_name               account;
            action_name                name;
            vector<permission_level>   authorization;
            bytes                      data;

            EOSLIB_SERIALIZE( action, (account)(name)(authorization)(data) )
         };

         struct block_type {
            public:
               account_name            producer = name{0};
               uint32_t                num = 0;
               checksum256             id;
               checksum256             previous;
               uint16_t                confirmed = 1;
               checksum256             transaction_mroot;
               checksum256             action_mroot;
               checksum256             mroot;

               bool is_nil() const {
                  return this->producer == name{0};
               }

               uint32_t block_num() const {
                  return num;
               }

               bool operator == (const block_type &m) const {
                  return id == m.id
                     && num == m.num
                     && transaction_mroot == m.transaction_mroot
                     && action_mroot == m.action_mroot
                     && mroot == m.mroot;
               }

               EOSLIB_SERIALIZE( block_type,
                     (producer)(num)(id)(previous)(confirmed)
                     (transaction_mroot)(action_mroot)(mroot)
                     )
            };

      private:
         struct unconfirm_block {
            public:
               block_type           base;
               asset                confirm = asset{0,CORE_SYMBOL};
               vector<action>       actions;
               vector<account_name> confirmeds;

               bool operator < (const unconfirm_block &m) const {
                  return base.num < m.base.num;
               }

               void confirm_by(const account_name& committer) {
                  const auto itr = std::find(confirmeds.begin(), confirmeds.end(), committer);
                  eosio_assert(itr == confirmeds.end(), "committer has confirmed");
                  confirmeds.push_back(committer);
               }

               EOSLIB_SERIALIZE( unconfirm_block, (base)(confirm)(actions)(confirmeds) )
            };

         // block relay stat
         TABLE block_relay_stat {
            public:
               name       chain;
               block_type last;
               vector<unconfirm_block> unconfirms;

               uint64_t primary_key() const { return chain.value; }

               EOSLIB_SERIALIZE( block_relay_stat, (chain)(last)(unconfirms) )
            };

         typedef eosio::multi_index<"relaystat"_n, block_relay_stat> relaystat_table;

         // map_handler
         TABLE map_handler {
            name         chain;
            name         name;
            account_name actaccount;
            action_name  actname;

            account_name relayacc;

            account_name account;
            bytes        data;

            uint64_t primary_key() const { return name.value; }

            EOSLIB_SERIALIZE( map_handler, (chain)(name)(actaccount)(actname)(relayacc)(account)(data) )
         };

         struct handler_action {
            name        chain;
            checksum256 block_id;
            action      act;

            EOSLIB_SERIALIZE( handler_action, (chain)(block_id)(act) )
         };

         TABLE relay_transfer {
            name         chain;
            account_name transfer;
            asset        deposit = asset{0,CORE_SYMBOL};
            block_type   last;

            uint64_t primary_key() const { return transfer.value; }

            EOSLIB_SERIALIZE( relay_transfer, (chain)(transfer)(deposit)(last) )
         };

         // channel
         TABLE channel {
            name                   chain;
            checksum256            id;
            asset                  deposit_sum = asset{0,CORE_SYMBOL};

            uint64_t primary_key() const { return chain.value; }

            EOSLIB_SERIALIZE( channel, (chain)(id)(deposit_sum) )
         };

         typedef eosio::multi_index<"channels"_n,  channel>         channels_table;
         typedef eosio::multi_index<"transfers"_n, relay_transfer>  transfers_table;
         typedef eosio::multi_index<"handlers"_n,  map_handler>     handlers_table;
      private:
         void onblockimp( const name chain, const block_type& block, const vector<action>& actions );
         void onaction( const block_type& block, const action& act, const map_handler& handler );
         void new_transfer( name chain, account_name transfer, const asset& deposit );

      public:
         void ontransfer( const account_name from, const account_name to,
                  const asset& quantity, const std::string& memo);
      public:
         /// @abi action
         ACTION commit( const name chain,
                     const account_name transfer,
                     const block_type& block,
                     const vector<action>& actions );
         /// @abi action
         ACTION onblock( const name chain, const block_type& block );
         /// @abi action
         ACTION newchannel( const name chain, const checksum256 id );
         /// @abi action
         ACTION newmap( const name chain, const name type,
                     const account_name act_account, const action_name act_name,
                     const account_name account, const account_name relayacc, const bytes data );
         using commit_action = action_wrapper<"commit"_n, &relay::commit>;
         using onblock_action = action_wrapper<"onblock"_n, &relay::onblock>;
         using newchannel_action = action_wrapper<"newchannel"_n, &relay::newchannel>;
         using newmap_action = action_wrapper<"newmap"_n, &relay::newmap>;
   };
} /// namespace eosio