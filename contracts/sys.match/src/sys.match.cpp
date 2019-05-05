#include <sys.match.hpp>
#include <../../forceio.token/include/forceio.token.hpp>
#include <../../relay.token/include/relay.token.hpp>
namespace relay{

   uint128_t compute_pair_index(symbol base, symbol quote)
   {
      uint128_t idxkey = (uint128_t(base.raw()) << 64) | quote.raw();
      return idxkey;
   }

   uint128_t compute_orderbook_lookupkey(uint32_t pair_id, uint32_t bid_or_ask, uint64_t value) {
      auto lookup_key = (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | value;
      return lookup_key;
   }

   void inline_transfer(account_name from, account_name to, name chain, asset quantity, string memo ) {
      // if (chain.value == 0) {
      //    action(
      //            permission_level{ from, N(active) },
      //            config::token_account_name, N(transfer),
      //            std::make_tuple(from, to, quantity, memo)
      //    ).send();
      // } else {
      //    action(
      //            permission_level{ from, N(active) },
      //            relay_token_acc, N(transfer),
      //            std::make_tuple(from, to, chain, quantity, memo)
      //    ).send();
      // }
   }

   ACTION exchange::create(symbol base, name base_chain, symbol base_sym, symbol quote, name quote_chain, symbol quote_sym, uint32_t fee_rate, account_name exc_acc) {
      require_auth( exc_acc );

      eosio_assert(base.is_valid(), "invalid base symbol");
      eosio_assert(quote.is_valid(), "invalid quote symbol");
      
      trading_pairs trading_pairs_table(_self,_self.value);

      uint32_t pair_id = get_pair(base_chain, base_sym, quote_chain, quote_sym);
      uint128_t idxkey = compute_pair_index(base, quote);

      print("\n base=", base, ", base_chain=", base_chain,", base_sym=", base_sym, "quote=", quote, ", quote_chain=", quote_chain, ", quote_sym=", quote_sym, "\n");

      auto idx = trading_pairs_table.template get_index<"idxkey"_n>();
      auto itr = idx.find(idxkey);

      eosio_assert(itr == idx.end(), "trading pair already created");

      auto pk = trading_pairs_table.available_primary_key();
      trading_pairs_table.emplace( _self, [&]( auto& p ) {
         p.id = (uint32_t)pk;
         p.pair_id      = pair_id;
         p.base         = base;
         p.base_chain   = base_chain;
         p.base_sym     = base_sym;//base_sym.raw() | (base.raw() & 0xff);
         p.quote        = quote;
         p.quote_chain  = quote_chain;
         p.quote_sym    = base_sym;//quote_sym.raw() | (quote.raw() & 0xff);
         p.fee_rate     = fee_rate;
         p.exc_acc      = exc_acc;
         p.fees_base    = to_asset(relay_token_acc, base_chain, base_sym, asset(0, base_sym));
         p.fees_quote   = to_asset(relay_token_acc, quote_chain, quote_sym, asset(0, quote_sym));
         p.frozen       = 0;
      });
   }
   ACTION exchange::match( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, uint32_t bid_or_ask ) {

   }
   ACTION exchange::cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id) {

   }
   ACTION exchange::done(account_name exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp) {

   }
   ACTION exchange::mark(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym) {

   }
   ACTION exchange::claim(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, account_name exc_acc, account_name fee_acc) {

   }
   ACTION exchange::freeze(uint32_t id) {

   }
   ACTION exchange::unfreeze(uint32_t id) {

   }

      /*  alter trade pair
   */
   void exchange::alter_pair_fee_rate(symbol base, symbol quote, uint32_t fee_rate)
   {
      trading_pairs   trading_pairs_table(_self,_self.value);
      auto idx_pair = trading_pairs_table.template get_index<"idxkey"_n>();
      uint128_t idxkey = compute_pair_index(base, quote);
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      require_auth( itr1->exc_acc );
      
      eosio_assert(itr1->fee_rate != fee_rate, "trade pair fee rate not changed!");
      
      idx_pair.modify(itr1, _self, [&]( auto& p ) {
         p.fee_rate = fee_rate;
      });
   }
   
   /*  alter trade pair
   */
   void exchange::alter_pair_exc_acc(symbol base, symbol quote, account_name exc_acc)
   {
      trading_pairs   trading_pairs_table(_self,_self.value);
      auto idx_pair = trading_pairs_table.template get_index<"idxkey"_n>();
      uint128_t idxkey = compute_pair_index(base, quote);
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      require_auth( itr1->exc_acc );
      
      eosio_assert(itr1->exc_acc != exc_acc, "trade pair exchange account not changed!");
      
      idx_pair.modify(itr1, _self, [&]( auto& p ) {
         p.exc_acc = exc_acc;
      });
   }

   /*  alter trade pair
   */
   void exchange::alter_pair_precision(symbol base, symbol quote)
   {
      trading_pairs   trading_pairs_table(_self,_self.value);
      auto idx_pair = trading_pairs_table.template get_index<"idxkey"_n>();
      uint128_t idxkey = compute_pair_index(base, quote);
      auto itr1 = idx_pair.find(idxkey);
      eosio_assert(itr1 != idx_pair.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      require_auth( itr1->exc_acc );
      
      eosio_assert(itr1->base.raw() == base.raw() && itr1->quote.raw() == quote.raw(), "can not change pair name!");
      eosio_assert(itr1->base.precision() != base.precision() || itr1->quote.precision() != quote.precision(), "trade pair precision not changed!");

      auto walk_table_range = [&]() {
         asset          quant_after_fee;   
         asset          quant_after_fee2;
         asset          converted_base;
         asset          converted_price;
         asset          diff;
         
         orderbook       orders(_self,_self.value);
         auto idx_orderbook = orders.template get_index<"idxkey"_n>();
         
         auto lower_key = compute_orderbook_lookupkey(itr1->id, 1, std::numeric_limits<uint64_t>::lowest());  
         auto lower = idx_orderbook.lower_bound( lower_key );
         auto upper_key = compute_orderbook_lookupkey(itr1->id, 0, std::numeric_limits<uint64_t>::max());
         auto upper = idx_orderbook.upper_bound( upper_key );

         for( auto itr = lower; itr != upper; ) {
            print("\n bid: order: id=", itr->id, ", pair_id=", itr->pair_id, ", bid_or_ask=", itr->bid_or_ask,", base=", itr->base,", price=", itr->price,", maker=", itr->maker);
            
            if (itr->bid_or_ask) { // refund quote
               if (itr->price.symbol.precision() >= itr->base.symbol.precision())
                  quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
               else
                  quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
               
               converted_base    = convert(base, itr->base);
               converted_price   = convert(quote, itr->price);
               
               if (converted_price.symbol.precision() >= converted_base.symbol.precision())
                  quant_after_fee2 = converted_price * converted_base.amount / precision(converted_base.symbol.precision());
               else
                  quant_after_fee2 = converted_base * converted_price.amount / precision(converted_price.symbol.precision());
                  
               diff = convert(itr->price.symbol, quant_after_fee) - convert(itr->price.symbol, quant_after_fee2);
               diff = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, diff);
               if (diff.amount > 0)
                  inline_transfer(escrow, itr->maker, itr1->quote_chain, diff, "");
            } else { // refund base
               if (itr->price.symbol.precision() >= itr->base.symbol.precision())
                  quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
               else
                  quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
               
               converted_base    = convert(base, itr->base);
               converted_price   = convert(quote, itr->price);
               
               if (converted_price.symbol.precision() >= converted_base.symbol.precision())
                  quant_after_fee2 = converted_price * converted_base.amount / precision(converted_base.symbol.precision());
               else
                  quant_after_fee2 = converted_base * converted_price.amount / precision(converted_price.symbol.precision());
                  
               diff = itr->base - convert(itr->base.symbol, quant_after_fee2);
               diff = to_asset(relay_token_acc, itr1->quote_chain, itr1->quote_sym, diff);
               if (diff.amount > 0)
                  inline_transfer(escrow, itr->maker, itr1->quote_chain, diff, "");
            }

            idx_orderbook.modify(itr, _self, [&]( auto& o ) {
               o.base   = converted_base;
               o.price  = converted_price;
            });
         }
      };

      if (base.precision() > itr1->base.precision()) {
         if (quote.precision() > itr1->quote.precision()) { // just modify base precision and quote precision
            idx_pair.modify(itr1, _self, [&]( auto& p ) {
               // p.base      = p.base.value | (base.value & 0xff);
               // p.base_sym  = p.base_sym.value | (base.value & 0xff);
               
               // p.quote     = p.quote.value | (quote.value & 0xff);
               // p.quote_sym = p.quote_sym.value | (quote.value & 0xff);
            });
         } else if (quote.precision() == itr1->quote.precision()) { // just modify base precision
            idx_pair.modify(itr1, _self, [&]( auto& p ) {
               // p.base      = p.base.value | (base.value & 0xff);
               // p.base_sym  = p.base_sym.value | (base.value & 0xff);
            });
         } else { // before modifying base precision and quote precision, refund the order maker and modify the orderbook
            
         }
      } else if (base.precision() == itr1->base.precision()) {
         if (quote.precision() > itr1->quote.precision()) {
            
         } else if (quote.precision() == itr1->quote.precision()) {
            
         } else {
            
         }
      } else {
         if (quote.precision() > itr1->quote.precision()) {
            
         } else if (quote.precision() == itr1->quote.precision()) {
            
         } else {
            
         }
      }
      
      
   }

   

   asset exchange::to_asset( account_name code, name chain, symbol sym, const asset& a ) {
      // asset b;
      // symbol expected_symbol;
      
      // if (chain.value == 0) {
      //    force::token t(token_account_name);
      //    //force::token::get_supply(token_account, core.code() )
      //    expected_symbol = t.get_supply(sym.raw()).symbol ;
      // } else {
      //    relay::token t(relay_token_acc);
      
      //    expected_symbol = t.get_supply(chain, sym.raw()).symbol ;
      // }

      // b = convert(expected_symbol, a);
      // return b;
      return asset(0,CORE_SYMBOL);
   }


   uint32_t exchange::get_pair( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) {
      raw_pairs   raw_pairs_table(_self, _self.value);

      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = raw_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = raw_pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym.raw() == base_sym.raw() && 
               itr->quote_chain == quote_chain && itr->quote_sym.raw() == quote_sym.raw()) {
            return itr->id;
         }
      }
      
      auto pk = raw_pairs_table.available_primary_key();
      raw_pairs_table.emplace( _self, [&]( auto& p ) {
         p.id = (uint32_t)pk;
         p.base_chain   = base_chain;
         p.base_sym     = base_sym;
         p.quote_chain  = quote_chain;
         p.quote_sym    = quote_sym;
      });

      return (uint32_t)pk;
   }
   
   uint32_t exchange::get_pair_id( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) const {
      raw_pairs   raw_pairs_table(_self, _self.value);

      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = raw_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = raw_pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym.raw() == base_sym.raw() && 
               itr->quote_chain == quote_chain && itr->quote_sym.raw() == quote_sym.raw()) {
            print("exchange::get_pair_id -- pair: id=", itr->id, "\n");
            return itr->id;   
         }
      }
          
      eosio_assert(false, "raw pair does not exist");

      return 0;
   }
   
   symbol exchange::get_pair_base( uint32_t pair_id ) const {
      trading_pairs   trading_pairs_table(_self, _self.value);
     
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      
      for( auto itr = lower; itr != upper; ++itr ) {
          if (itr->id == pair_id) return itr->base;
      }
      eosio_assert(false, "trading pair does not exist");
      return NULL_SYMBOL;
   }
   
   symbol exchange::get_pair_quote( uint32_t pair_id ) const {
      trading_pairs   trading_pairs_table(_self, _self.value);
     
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->id == pair_id) return itr->quote;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return NULL_SYMBOL;
   }
   
   account_name exchange::get_exchange_account( uint32_t pair_id ) const {
      trading_pairs   trading_pairs_table(_self, _self.value);
      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = trading_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = trading_pairs_table.upper_bound( upper_key );
      
      for ( auto itr = lower; itr != upper; ++itr ) {
         if (itr->id == pair_id) return itr->exc_acc;
      }
          
      eosio_assert(false, "trading pair does not exist");
      
      return NULL_NAME;
   }
   
   /*
   block_height: end block height
   */
   asset exchange::get_avg_price( uint32_t block_height, name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) const {
      deals   deals_table(_self, _self.value);
      asset   avg_price = asset(0, quote_sym);

      uint32_t pair_id = 0xFFFFFFFF;
      
      raw_pairs   raw_pairs_table(_self, _self.value);

      auto lower_key = std::numeric_limits<uint64_t>::lowest();
      auto lower = raw_pairs_table.lower_bound( lower_key );
      auto upper_key = std::numeric_limits<uint64_t>::max();
      auto upper = raw_pairs_table.upper_bound( upper_key );
      auto itr = lower;
      
      for ( itr = lower; itr != upper; ++itr ) {
         if (itr->base_chain == base_chain && itr->base_sym.raw() == base_sym.raw() && 
               itr->quote_chain == quote_chain && itr->quote_sym.raw() == quote_sym.raw()) {
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
 
   void exchange::upd_mark( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, asset sum, asset vol ) {
      deals   deals_table(_self, _self.value);
      
      auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
     
      auto lower_key = ((uint64_t)pair_id << 32) | 0;
      auto idx_deals = deals_table.template get_index<"idxkey"_n>();
      auto itr1 = idx_deals.lower_bound(lower_key);
      if (!( itr1 != idx_deals.end() && itr1->pair_id == pair_id )) {
         print("exchange::upd_mark trading pair not marked!\n");
         return;
      }
      
      uint32_t curr_block = current_block_num();
      lower_key = ((uint64_t)pair_id << 32) | curr_block;
      itr1 = idx_deals.lower_bound(lower_key);
      if (itr1 == idx_deals.cend()) itr1--;
      if ( curr_block <= itr1->block_height_end ) {
         idx_deals.modify( itr1, _self, [&]( auto& d ) {
            d.sum += sum;
            d.vol += vol;
         });
      } else {
         auto start_block =  itr1->reset_block_height + (curr_block - itr1->reset_block_height) / INTERVAL_BLOCKS * INTERVAL_BLOCKS;
         auto pk = deals_table.available_primary_key();
         deals_table.emplace( _self, [&]( auto& d ) {
            d.id                 = (uint32_t)pk;
            d.pair_id            = pair_id;
            d.sum                = sum;
            d.vol                = vol;
            d.reset_block_height = start_block;
            d.block_height_end   = start_block + INTERVAL_BLOCKS - 1;
         });   
      }
      
      return ;
   }
}

EOSIO_DISPATCH( relay::exchange, (create)(match)(cancel)(done)(mark)(claim)(freeze)(unfreeze) )