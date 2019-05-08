
#pragma once
#include <eosiolib/time.hpp>
#include <../../codexlib/config.hpp>
#include<../../force.token/include/force.token.hpp>
#include<../../relay.token/include/relay.token.hpp>

#define NULL_SYMBOL symbol(symbol_code(""), 0)
#define NULL_NAME  name(0)
namespace relay{
   typedef name account_name;

   static const uint32_t INTERVAL_BLOCKS = 172800;///*172800*/ 24 * 3600 * 1000 / config::block_interval_ms;
   const int64_t MAX_FEE_RATE = 10000;

   class [[eosio::contract("sys.match")]] exchange : public contract {
      public:
         using contract::contract;

         ACTION regex(account_name exc_acc);
         ACTION create(symbol base, name base_chain, symbol base_sym, symbol quote, name quote_chain, symbol quote_sym, account_name exc_acc);
         ACTION match( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, uint32_t bid_or_ask, account_name exc_acc, string referer );
         ACTION cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id);
         ACTION done(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp);
         ACTION mark(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym);
         ACTION claimrelay(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, account_name exc_acc, account_name fee_acc);   
         ACTION freeze(uint32_t id);    
         ACTION unfreeze(uint32_t id);
         ACTION setfee(account_name exc_acc, uint32_t pair_id, uint32_t type, uint32_t rate, name chain, asset fee);

         using regex_action = action_wrapper<"regex"_n, &exchange::regex>;
         using create_action = action_wrapper<"create"_n, &exchange::create>;
         using match_action = action_wrapper<"match"_n, &exchange::match>;
         using cancel_action = action_wrapper<"cancel"_n, &exchange::cancel>;
         using done_action = action_wrapper<"done"_n, &exchange::done>;
         using mark_action = action_wrapper<"mark"_n, &exchange::mark>;
         using claimrelay_action = action_wrapper<"claimrelay"_n, &exchange::claimrelay>;
         using freeze_action = action_wrapper<"freeze"_n, &exchange::freeze>;
         using unfreeze_action = action_wrapper<"unfreeze"_n, &exchange::unfreeze>;
         using setfee_action = action_wrapper<"setfee"_n, &exchange::setfee>;

      private:
         TABLE exc {
            account_name exc_acc;
            
            uint64_t primary_key() const { return exc_acc.value; }
         };

      //private:
         TABLE trading_pair{
            uint32_t id;
            
            symbol base;
            name        base_chain;
            symbol base_sym;

            symbol quote;
            name        quote_chain;
            symbol quote_sym;
            
            //uint32_t    fee_rate;
            account_name exc_acc;
            //asset       fees_base;
            //asset       fees_quote;
            uint32_t    frozen;
            
            uint32_t primary_key() const { return id; }
            uint128_t by_pair_sym() const { return (uint128_t(base.raw()) << 64) | quote.raw(); }
         };
         
         TABLE order {
            uint64_t        id;
            uint32_t        pair_id;
            account_name    exc_acc;
            account_name    maker;
            account_name    receiver;
            asset           base;
            asset           price;
            uint32_t        bid_or_ask;
            time_point_sec  timestamp;

            uint64_t primary_key() const { return id; }
            uint128_t by_pair_price() const { 
               //print("\n by_pair_price: order: id=", id, ", pair_id=", pair_id, ", bid_or_ask=", bid_or_ask,", base=", base,", price=", price,", maker=", maker, ", key=", (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t)price.amount);
               return (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 1 : 0)) << 64 | (uint64_t)price.amount; }
         };
         
         TABLE deal_info {
            uint32_t    id;
            uint32_t    pair_id;

            asset       sum;
            asset       vol;
            
            // [reset_block_height .. block_height_end]
            uint32_t    reset_block_height;// include 
            uint32_t    block_height_end;// include
            
            uint64_t primary_key() const { return id; }
            uint64_t by_pair_and_block_height() const {
               return (uint64_t(pair_id) << 32) | block_height_end;
            }
         };
         
         TABLE fee_info {
            uint32_t    id;
            account_name exc_acc;
            uint32_t    pair_id;
            
            uint32_t    type;
            uint32_t    rate;
            name        chain;
            asset       fee;
            
            asset       fees_base;
            asset       fees_quote;
            
            uint64_t primary_key() const { return id; }
            uint128_t by_exc_and_pair() const {
               return (uint128_t(exc_acc.value) << 64) | pair_id;
            }
         };
         typedef eosio::multi_index<"exchanges"_n, exc> exchanges;
         typedef eosio::multi_index<"pairs"_n, trading_pair,
            indexed_by< "idxkey"_n, const_mem_fun<trading_pair, uint128_t, &trading_pair::by_pair_sym>>
         > trading_pairs; 
         typedef eosio::multi_index<"orderbook"_n, order,
            indexed_by< "idxkey"_n, const_mem_fun<order, uint128_t, &order::by_pair_price>>
         > orderbook;       
         typedef eosio::multi_index<"deals"_n, deal_info,
            indexed_by< "idxkey"_n, const_mem_fun<deal_info, uint64_t, &deal_info::by_pair_and_block_height>>
         > deals;
         typedef eosio::multi_index<"fees"_n, fee_info,
            indexed_by< "idxkey"_n, const_mem_fun<fee_info, uint128_t, &fee_info::by_exc_and_pair>>
         > fees; 

      private:
 	      void done_helper(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask);
         void match_for_bid( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, account_name exc_acc, string referer);
         void match_for_ask( uint32_t pair_id, account_name payer, account_name receiver, asset base, asset price, account_name exc_acc, string referer);
         asset calcfee(asset quant, uint64_t fee_rate);

         inline void get_pair(uint32_t pair_id, name &base_chain, symbol &base_sym, name &quote_chain, symbol &quote_sym) const;
         inline symbol get_pair_base( uint32_t pair_id ) const;
         inline symbol get_pair_quote( uint32_t pair_id ) const;
         inline void check_pair( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym );
         inline uint32_t get_pair_id( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) const;
         
         inline void upd_mark( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, asset sum, asset vol );

         void insert_order(
                        orderbook &orders, 
                        uint32_t pair_id, 
                        account_name exc_acc,
                        uint32_t bid_or_ask, 
                        asset base, 
                        asset price, 
                        account_name payer, 
                        account_name receiver);



      public:
         static asset get_avg_price( uint32_t block_height, name base_chain, symbol base_sym, name quote_chain = name(0), symbol quote_sym = CORE_SYMBOL ) {
            deals   deals_table(config::match_account, config::match_account.value);
            asset   avg_price = asset(0, quote_sym);

            uint32_t pair_id = 0xFFFFFFFF;
            
            trading_pairs   pairs_table(config::match_account, config::match_account.value);

            auto lower_key = std::numeric_limits<uint64_t>::lowest();
            auto lower = pairs_table.lower_bound( lower_key );
            auto upper_key = std::numeric_limits<uint64_t>::max();
            auto upper = pairs_table.upper_bound( upper_key );
            auto itr = lower;
            
            for ( itr = lower; itr != upper; ++itr ) {
               if (itr->base_chain == base_chain && itr->base_sym == base_sym && 
                     itr->quote_chain == quote_chain && itr->quote_sym == quote_sym) {
                  pair_id = itr->id;
                  break;
               }
            }
            if (itr == upper) {
               return avg_price;
            }
            
            lower_key = ((uint64_t)pair_id << 32) | 0;
            auto idx_deals = deals_table.template get_index<"idxkey"_n>();
            auto itr1 = idx_deals.lower_bound(lower_key);
            if (!(itr1 != idx_deals.end() && itr1->pair_id == pair_id)) {
               return avg_price;
            }
            
            lower_key = ((uint64_t)pair_id << 32) | block_height;
            itr1 = idx_deals.lower_bound(lower_key);
            if (itr1 == idx_deals.cend()) itr1--;

            if (itr1->vol.amount > 0 && block_height >= itr1->reset_block_height) 
               avg_price = itr1->sum * precision(itr1->vol.symbol.precision()) / itr1->vol.amount;

            return avg_price;
         }

         static asset to_asset( account_name code, name chain, symbol sym, const asset& a ) {
            asset b;
            symbol expected_symbol;
            
            // if (chain.value == 0) {
            //    eosio::token t(config::token_account_name);
            
            //    expected_symbol = t.get_supply(sym.name()).symbol ;
            // } else {
            //    relay::token t(config::relay_token_account);
            
            //    expected_symbol = t.get_supply(chain, sym.name()).symbol ;
            // }

            b = convert(expected_symbol, a);
            return b;
         }

         static asset convert( symbol expected_symbol, const asset& a ) {
            int64_t factor;
            asset b;
            
            if (expected_symbol.precision() >= a.symbol.precision()) {
               factor = precision( expected_symbol.precision() ) / precision( a.symbol.precision() );
               b = asset( a.amount * factor, expected_symbol );
            }
            else {
               factor =  precision( a.symbol.precision() ) / precision( expected_symbol.precision() );
               b = asset( a.amount / factor, expected_symbol );
               
            }
            return b;
         }
         static int64_t precision(uint64_t decimals) 
         {
            int64_t p10 = 1;
            int64_t p = (int64_t)decimals;
            while( p > 0  ) {
               p10 *= 10; --p;
            }
            return p10;
         }
         
   };
}
