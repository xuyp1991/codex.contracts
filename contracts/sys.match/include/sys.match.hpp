#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
using namespace eosio;

#define CORE_SYMBOL symbol(symbol_code("CDX"), 4)
#define NULL_SYMBOL symbol(symbol_code(""), 0)
#define NULL_NAME  name(0)
namespace relay{
   typedef name account_name;

   const uint32_t INTERVAL_BLOCKS = 172800;///*172800*/ 24 * 3600 * 1000 / config::block_interval_ms;
   const account_name relay_token_acc = name("relay.token"_n);
   const account_name token_account_name = name("force.token"_n);
   const account_name escrow = name("sys.match"_n);
   class [[eosio::contract("sys.match")]] exchange : public contract {
      public:
         using contract::contract;

         ACTION create(symbol base, name base_chain, symbol base_sym, symbol quote, name quote_chain, symbol quote_sym, uint32_t fee_rate, account_name exc_acc);
         ACTION match( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, uint32_t bid_or_ask );
         ACTION cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id);
         ACTION done(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp);
         ACTION mark(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym);
         ACTION claim(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, account_name exc_acc, account_name fee_acc);
         ACTION freeze(uint32_t id);
         ACTION unfreeze(uint32_t id);

         using create_action = action_wrapper<"create"_n, &exchange::create>;
         using match_action = action_wrapper<"match"_n, &exchange::match>;
         using cancel_action = action_wrapper<"cancel"_n, &exchange::cancel>;
         using done_action = action_wrapper<"done"_n, &exchange::done>;
         using mark_action = action_wrapper<"mark"_n, &exchange::mark>;
         using claim_action = action_wrapper<"claim"_n, &exchange::claim>;
         using freeze_action = action_wrapper<"freeze"_n, &exchange::freeze>;
         using unfreeze_action = action_wrapper<"unfreeze"_n, &exchange::unfreeze>;

      private:
         TABLE pair {
            uint32_t id;
         
            name        base_chain;
            symbol base_sym;

            name        quote_chain;
            symbol quote_sym;
            
            uint32_t primary_key() const { return id; }
         };


         TABLE trading_pair{
            uint32_t id;
            uint32_t pair_id;
            
            symbol base;
            name        base_chain;
            symbol base_sym;

            symbol quote;
            name        quote_chain;
            symbol quote_sym;
            
            uint32_t    fee_rate;
            account_name exc_acc;
            asset       fees_base;
            asset       fees_quote;
            uint32_t    frozen;
            
            uint32_t primary_key() const { return id; }
            uint128_t by_pair_sym() const { return (uint128_t(base.raw()) << 64) | quote.raw(); }
         };
         
         TABLE order {
            uint64_t        id;
            uint32_t        pair_id;
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
         typedef eosio::multi_index<"rawpairs"_n, pair> raw_pairs;
         typedef eosio::multi_index<"pairs"_n, trading_pair,
            indexed_by< "idxkey"_n, const_mem_fun<trading_pair, uint128_t, &trading_pair::by_pair_sym>>
         > trading_pairs;    
         typedef eosio::multi_index<"orderbook"_n, order,
            indexed_by< "idxkey"_n, const_mem_fun<order, uint128_t, &order::by_pair_price>>
         > orderbook;    
         typedef eosio::multi_index<"deals"_n, deal_info,
            indexed_by< "idxkey"_n, const_mem_fun<deal_info, uint64_t, &deal_info::by_pair_and_block_height>>
         > deals; 
      private:
         void alter_pair_precision(symbol base, symbol quote);
      
         void alter_pair_fee_rate(symbol base, symbol quote, uint32_t fee_rate);
         
         void alter_pair_exc_acc(symbol base, symbol quote, account_name exc_acc);

         void done_helper(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask);
      
         void match_for_bid( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price);
         
         void match_for_ask( uint32_t pair_id, account_name payer, account_name receiver, asset base, asset price);
         asset calcfee(asset quant, uint64_t fee_rate);

         inline symbol get_pair_base( uint32_t pair_id ) const;
         inline symbol get_pair_quote( uint32_t pair_id ) const;
         inline account_name get_exchange_account( uint32_t pair_id ) const;
         inline uint32_t get_pair( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym );
         inline uint32_t get_pair_id( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) const;
         inline asset get_avg_price( uint32_t block_height, name base_chain, symbol base_sym, name quote_chain = name(0), symbol quote_sym = CORE_SYMBOL ) const;
         
         inline void upd_mark( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, asset sum, asset vol );
         void insert_order(
                  orderbook &orders, 
                  uint32_t pair_id, 
                  uint32_t bid_or_ask, 
                  asset base, 
                  asset price, 
                  account_name payer, 
                  account_name receiver);
         static asset to_asset( account_name code, name chain, symbol sym, const asset& a );
         static asset convert( symbol expected_symbol, const asset& a );
         static int64_t precision(uint64_t decimals);
   };
}
