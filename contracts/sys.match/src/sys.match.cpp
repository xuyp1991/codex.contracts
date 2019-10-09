#include <sys.match.hpp>
#include <../../force.token/include/force.token.hpp>
#include <../../relay.token/include/relay.token.hpp>
namespace relay{

   // uint128_t compute_orderbook_lookupkey(uint32_t pair_id, uint32_t bid_or_ask, uint64_t value) {
   //    auto lookup_key = (uint128_t(pair_id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | value;
   //    return lookup_key;
   // }

   // uint128_t compute_pair_index(symbol base, symbol quote)
   // {
   //    uint128_t idxkey = (uint128_t(base.raw()) << 64) | quote.raw();
   //    return idxkey;
   // }

   // void inline_transfer(account_name from, account_name to, name chain, asset quantity, string memo ) {
   //    // if (chain.value == 0) {
   //    //    action(
   //    //            permission_level{ from, N(active) },
   //    //            config::token_account_name, N(transfer),
   //    //            std::make_tuple(from, to, quantity, memo)
   //    //    ).send();
   //    // } else {
   //    //    action(
   //    //            permission_level{ from, N(active) },
   //    //            config::relay_token_account, N(transfer),
   //    //            std::make_tuple(from, to, chain, quantity, memo)
   //    //    ).send();
   //    // }
   // }

   void exchange::checkExcAcc(account_name exc_acc) {
      exchanges exc_tbl(_self,_self.value);
      auto itr1 = exc_tbl.find(exc_acc.value);
      eosio_assert(itr1 != exc_tbl.end(), "exechange account has not been registered!");
   }

   uint64_t exchange::get_order_scope(const uint64_t &a,const uint64_t &b) {
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey =  make_128_key(a,b);
      auto idx = order_scope_table.get_index<"idxkey"_n>();
      auto itr = idx.find(scopekey);
      
      eosio_assert(itr != idx.end(),"can not find scope");

      return itr->id;
   }

   void exchange::makeorder(account_name traders,coin_info base,coin_info quote,uint64_t trade_pair_id, account_name exc_acc) {
      require_auth( traders );
      checkExcAcc(exc_acc);//后续押金是否检查
      
      trading_pairs trading_pairs_table(_self,exc_acc.value);
      auto trade_pair =  trading_pairs_table.find(trade_pair_id);
      eosio_assert(trade_pair != trading_pairs_table.end(), "can not find the trade pair");
      //检查是否和交易对一样
      eosio_assert(( base.chain == trade_pair->base.chain && base.coin.symbol ==  trade_pair->base.coin.symbol 
         && quote.chain == trade_pair->quote.chain && quote.coin.symbol ==  trade_pair->quote.coin.symbol ) || 
         (quote.chain == trade_pair->base.chain && quote.coin.symbol ==  trade_pair->base.coin.symbol 
         && base.chain == trade_pair->quote.chain && base.coin.symbol ==  trade_pair->quote.coin.symbol),"the order do not match the trade pair");
      //查询scope
      auto order_scope = get_order_scope(base.coin.symbol.raw(),quote.coin.symbol.raw());
      //插表
      orderbook order_book_table(_self,order_scope);
      auto order_id = order_book_table.available_primary_key();
      order_book_table.emplace( traders, [&]( auto& p ) { 
         p.id = order_id;
         p.pair_id = trade_pair->id;
         p.maker = traders;
         p.receiver = traders;
         p.base = base;
         p.quote = quote;
         p.bid_or_ask = 0;
         p.exc_acc = exc_acc;
         p.timestamp = time_point_sec(uint32_t(current_time() / 1000000ll));
      });
      //去另外一个交易单列表上面匹配成交
   }
   //不做太多检查
   void exchange::match(const uint64_t &order_scope,const uint64_t &order_id,const account_name &exc_acc ) {
      orderbook order_book_table(_self,order_scope);
      auto order = order_book_table.find(order_id);
      eosio_assert( order != order_book_table.end(),"can not find the order" );
      //价格获取 小数点后6位
      auto price = order->base.coin.amount * 1000000 / order->quote.coin.amount;
      auto relative_price = order->quote.coin.amount * 1000000 / order->base.coin.amount;
      //选取相对的订单进行匹配
      auto relative_order_scope = get_order_scope(order->quote.coin.symbol.raw(),order->base.coin.symbol.raw()); 
      orderbook relative_order_book_table(_self,relative_order_scope);

      auto idx = relative_order_book_table.get_index<"pricekey"_n>();
      auto itr_low = idx.lower_bound( price );
      auto itr_up = idx.upper_bound( price );
      if ( itr_low != idx.end() ) {
         
      }
      else {

      }

   }

   ACTION exchange::regex(account_name exc_acc){
      require_auth( exc_acc );
      //押金以后再考虑
      //const asset min_staked(10000000,CORE_SYMBOL);
      
      // check if exc_acc has freezed 1000 SYS
      // eosiosystem::system_contract sys_contract(config::system_account_name);
      // eosio_assert(sys_contract.get_freezed(exc_acc) >= min_staked, "must freeze 1000 or more CDX!");
      
      exchanges exc_tbl(_self,_self.value);
      
      exc_tbl.emplace( exc_acc, [&]( auto& e ) {
         e.exc_acc      = exc_acc;
      });
   }

   ACTION exchange::create(name base_chain, asset base_coin, name quote_chain, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      eosio_assert(base_coin.symbol.raw() != quote_coin.symbol.raw(), "base coin and quote coin must be different");
      trading_pairs trading_pairs_table(_self,exc_acc.value);

      uint128_t idxkey = make_trade_128_key(base_coin.symbol.raw(), quote_coin.symbol.raw());
      auto idx = trading_pairs_table.get_index<"idxkey"_n>();
      auto itr = idx.find(idxkey);
      eosio_assert(itr == idx.end(), "trading pair already created");

      auto pk = trading_pairs_table.available_primary_key();
      trading_pairs_table.emplace( exc_acc, [&]( auto& p ) {
         p.id = pk;
         p.base.chain = base_chain;
         p.base.coin  = base_coin;
         p.quote.chain = quote_chain;
         p.quote.coin  = quote_coin;
         p.frozen       = 0;
         p.fee_id       = -1;
      });

      //插入order scope  需要插入两个

      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw());
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      if ( itr_base == idx_scope.end() ) {
         auto orderscope = order_scope_table.available_primary_key();
         order_scope_table.emplace( exc_acc, [&]( auto& p ) {
            p.id = orderscope;
            p.base.chain = base_chain;
            p.base.coin = base_coin;
            p.quote.chain = quote_chain;
            p.quote.coin = quote_coin;
         });
      }

      uint128_t scopekey_quote =  make_128_key(quote_coin.symbol.raw(),base_coin.symbol.raw());
      auto itr_quote = idx_scope.find(scopekey_quote);
      if ( itr_quote == idx_scope.end() ) {
         auto orderscope = order_scope_table.available_primary_key();
         order_scope_table.emplace( exc_acc, [&]( auto& p ) {
            p.id = orderscope;
            p.base.chain = quote_chain;
            p.base.coin = quote_coin;
            p.quote.chain = base_chain;
            p.quote.coin = base_coin;
         });
      }
   }
//目前仅支持按比例收取
   ACTION exchange::feecreate(uint32_t type,uint32_t rate,name base_chain, asset base_coin, name quote_chain, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      trade_fees trade_fee_tbl(_self,exc_acc.value);
      auto pk = trade_fee_tbl.available_primary_key();
      trade_fee_tbl.emplace( exc_acc, [&]( auto& p ) {
         p.id = pk;
         p.fees_base.chain = base_chain;
         p.fees_base.coin  = base_coin;
         p.fees_quote.chain = quote_chain;
         p.fees_quote.coin  = quote_coin;
         p.type = type;
         p.rate = rate;
      });
   }

   ACTION exchange::setfee(uint64_t trade_pair_id, uint64_t trade_fee_id, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      trading_pairs trading_pairs_table(_self,exc_acc.value);
      auto trade_pair =  trading_pairs_table.find(trade_pair_id);
      eosio_assert(trade_pair != trading_pairs_table.end(), "can not find the trade pair");

      trade_fees trade_fee_tbl(_self,exc_acc.value);
      auto trade_fee =  trade_fee_tbl.find(trade_pair_id);
      eosio_assert(trade_fee != trade_fee_tbl.end(), "can not find the trade fee info");

      trading_pairs_table.modify(trade_pair, exc_acc, [&]( auto& s ) {
         s.fee_id = trade_fee_id;
      });

   }

   void exchange::onforcetrans( const account_name& from,
                                 const account_name& to,
                                 const asset& quantity,
                                 const string& memo ) {
      if ( from == _self ) {
         return ;
      }
   }

   void exchange::onrelaytrans( const account_name from,
                                 const account_name to,
                                 const name chain,
                                 const asset quantity,
                                 const string memo ) {
      if ( from == _self ) {
         return ;
      }
   }

   

   // ACTION exchange::match( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, uint32_t bid_or_ask, account_name exc_acc, string referer ){
   //    if (bid_or_ask) {
   //       match_for_bid(pair_id, payer, receiver, quantity, price, exc_acc, referer);
   //    } else {
   //       match_for_ask(pair_id, payer, receiver, quantity, price, exc_acc, referer);
   //    }

   //    return;
   // }
   // ACTION exchange::cancel(account_name maker, uint32_t type, uint64_t order_or_pair_id){
   //    eosio_assert(type == 0 || type == 1 || type == 2, "invalid cancel type");
   //    trading_pairs   trading_pairs_table(_self,_self.value);
   //    orderbook       orders(_self,_self.value);
   //    asset           quant_after_fee;
      
   //    require_auth( maker );
      
   //    if (type == 0) {
   //       auto itr = orders.find(order_or_pair_id);
   //       eosio_assert(itr != orders.cend(), "order record not found");
   //       eosio_assert(maker == itr->maker, "not the maker");
         
   //       uint128_t idxkey = (uint128_t(itr->base.symbol.raw()) << 64) | itr->price.symbol.raw();
         
   //       auto idx_pair = trading_pairs_table.template get_index<"idxkey"_n>();
   //       auto itr1 = idx_pair.find(idxkey);
   //       eosio_assert(itr1 != idx_pair.end(), "trading pair does not exist");
         
   //       // refund the config::match_account
   //       if (itr->bid_or_ask) {
   //          if (itr->price.symbol.precision() >= itr->base.symbol.precision())
   //             quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
   //          else
   //             quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
   //          quant_after_fee = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //          inline_transfer(config::match_account, itr->maker, itr1->quote_chain, quant_after_fee, "");
   //       } else {
   //          quant_after_fee = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, itr->base);
   //          inline_transfer(config::match_account, itr->maker, itr1->base_chain, quant_after_fee, "");
   //       }
         
   //       orders.erase(itr);
   //    } else {
   //       auto idx_pair = trading_pairs_table.template get_index<"idxkey"_n>();

   //       auto lower_key = std::numeric_limits<uint64_t>::lowest();
   //       auto lower = orders.lower_bound( lower_key );
   //       auto upper_key = std::numeric_limits<uint64_t>::max();
   //       auto upper = orders.lower_bound( upper_key );
         
   //       if ( lower == orders.cend() ) {
   //          eosio_assert(false, "orderbook empty");
   //       }
         
   //       for ( ; lower != upper; ) {
   //          if (maker != lower->maker) {
   //             lower++;
   //             continue;
   //          }
   //          if (type == 1 && static_cast<uint32_t>(order_or_pair_id) != lower->pair_id) {
   //             lower++;
   //             continue;
   //          }
            
   //          auto itr = lower++;
   //          uint128_t idxkey = compute_pair_index(itr->base.symbol, itr->price.symbol);
   //          auto itr1 = idx_pair.find(idxkey);
   //          // refund the config::match_account
   //          if (itr->bid_or_ask) {
   //             if (itr->price.symbol.precision() >= itr->base.symbol.precision())
   //                quant_after_fee = itr->price * itr->base.amount / precision(itr->base.symbol.precision());
   //             else
   //                quant_after_fee = itr->base * itr->price.amount / precision(itr->price.symbol.precision());
   //             quant_after_fee = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //             inline_transfer(config::match_account, itr->maker, itr1->quote_chain, quant_after_fee, "");
   //          } else {
   //             quant_after_fee = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, itr->base);
   //             inline_transfer(config::match_account, itr->maker, itr1->base_chain, quant_after_fee, "");
   //          }
            
   //          orders.erase(itr);
   //       }
   //    }

   //    return;
   // }
   // ACTION exchange::done(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask, time_point_sec timestamp){
   //    require_auth(taker_exc_acc);
      
   //    asset quant_after_fee;
   //    if (price.symbol.precision() >= quantity.symbol.precision())
   //       quant_after_fee = price * quantity.amount / precision(quantity.symbol.precision());
   //    else
   //       quant_after_fee = quantity * price.amount / precision(price.symbol.precision());
   //    quant_after_fee = to_asset(config::relay_token_account, quote_chain, price.symbol, quant_after_fee);
   //    upd_mark( base_chain, quantity.symbol, quote_chain, price.symbol, quant_after_fee, quantity );
   // }
   // ACTION exchange::mark(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym){
   //    require_auth(_self);
      
   //    deals   deals_table(_self, _self.value);
     
   //    auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
   //    auto lower_key = ((uint64_t)pair_id << 32) | 0;
   //    auto idx_deals = deals_table.template get_index<"idxkey"_n>();
   //    auto itr1 = idx_deals.lower_bound(lower_key);
   //    eosio_assert(!(itr1 != idx_deals.end() && itr1->pair_id == pair_id), "trading pair already marked");
      
   //    auto start_block = (current_block_num() - 1) / INTERVAL_BLOCKS * INTERVAL_BLOCKS + 1;
   //    auto pk = deals_table.available_primary_key();
   //    deals_table.emplace( _self, [&]( auto& d ) {
   //       d.id = (uint32_t)pk;
   //       d.pair_id      = pair_id;
   //       d.sum          = to_asset(config::relay_token_account, quote_chain, quote_sym, asset(0, quote_sym));
   //       d.vol          = to_asset(config::relay_token_account, base_chain, base_sym, asset(0, base_sym));
   //       d.reset_block_height = start_block;
   //       d.block_height_end = start_block + INTERVAL_BLOCKS - 1;
   //    });
   // }
   // ACTION exchange::claimrelay(name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, account_name exc_acc, account_name fee_acc) {
   //    require_auth(exc_acc);
   //    auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
   //    fees fees_tbl(_self,_self.value);
   //    auto idx_fees = fees_tbl.template get_index<"idxkey"_n>();
   //    auto idxkey = (uint128_t(exc_acc.value) << 64) | pair_id;
   //    auto itr = idx_fees.find(idxkey);
   //    eosio_assert(itr != idx_fees.cend(), "no fees in fees table");

   //    bool claimed = false;
     
   //    if (config::match_account != fee_acc && itr->fees_base.amount > 0)
   //    {
   //       inline_transfer(config::match_account, fee_acc, base_chain, itr->fees_base, "");
   //       idx_fees.modify(itr, exc_acc, [&]( auto& f ) {
   //          f.fees_base = to_asset(config::relay_token_account, base_chain, base_sym, asset(0, base_sym));
   //       });
   //       claimed = true;
   //    }
     
   //    if (config::match_account != fee_acc && itr->fees_quote.amount > 0)
   //    {
   //       inline_transfer(config::match_account, fee_acc, quote_chain, itr->fees_quote, "");
   //       idx_fees.modify(itr, exc_acc, [&]( auto& f ) {
   //          f.fees_quote = to_asset(config::relay_token_account, quote_chain, quote_sym, asset(0, quote_sym));
   //       });
   //       claimed = true;
   //    }   
     
   //    eosio_assert(claimed, "no fees or fee_acc is config::match_account account");
     
   //    return;
   // } 

   // ACTION exchange::freeze(uint32_t id) {
   //    trading_pairs   trading_pairs_table(_self, _self.value);
      
   //    auto itr = trading_pairs_table.find(id);
   //    eosio_assert(itr != trading_pairs_table.cend(), "trading pair not found");
      
   //    require_auth(itr->exc_acc);
      
   //    trading_pairs_table.modify(itr, _self, [&]( auto& p ) {
   //      p.frozen = 1;
   //    });
   // }
   // ACTION exchange::unfreeze(uint32_t id) {
   //    trading_pairs   trading_pairs_table(_self, _self.value);
      
   //    auto itr = trading_pairs_table.find(id);
   //    eosio_assert(itr != trading_pairs_table.cend(), "trading pair not found");
      
   //    require_auth(itr->exc_acc);
      
   //    trading_pairs_table.modify(itr, _self, [&]( auto& p ) {
   //      p.frozen = 0;
   //    });
   // }
   // ACTION exchange::setfee(account_name exc_acc, uint32_t pair_id, uint32_t type, uint32_t rate, name chain, asset fee) {
   //    require_auth(exc_acc);
   //    fees   fees_tbl(_self, _self.value);
      
   //    auto idx_fee = fees_tbl.template get_index<"idxkey"_n>();
      
   //    auto idxkey = (uint128_t(exc_acc.value) << 64) | pair_id;
   //    auto itr = idx_fee.find(idxkey);
   //    eosio_assert(type == 1, "now only suppert type 1");
      
   //    if (itr == idx_fee.cend()) {
   //       name        base_chain;
   //       symbol base_sym;
   //       name        quote_chain;
   //       symbol quote_sym;
         
   //       get_pair(pair_id, base_chain, base_sym, quote_chain, quote_sym);
         
   //       auto pk = fees_tbl.available_primary_key();
   //       fees_tbl.emplace( exc_acc, [&]( auto& f ) {
   //          f.id           = (uint32_t)pk;
   //          f.exc_acc      = exc_acc;
   //          f.pair_id      = pair_id;
   //          f.type         = type;
   //          f.rate         = rate;
   //          f.chain        = chain;
   //          f.fee          = fee;
   //          f.fees_base    = to_asset(config::relay_token_account, base_chain, base_sym, asset(0, base_sym));
   //          f.fees_quote   = to_asset(config::relay_token_account, quote_chain, quote_sym, asset(0, quote_sym));
   //       });
   //    } else {
   //       idx_fee.modify(itr, exc_acc, [&]( auto& f ) {
   //          f.type         = type;
   //          f.rate         = rate;
   //          f.chain        = chain;
   //          f.fee          = fee;
   //       });
   //    }
   // }

   // void exchange::done_helper(account_name taker_exc_acc, account_name maker_exc_acc, name quote_chain, asset price, name base_chain, asset quantity, uint32_t bid_or_ask) {
   //    // auto timestamp = time_point_sec(uint32_t(current_time() / 1000000ll));
   //    // action(
   //    //       permission_level{ taker_exc_acc, N(active) },
   //    //       N(sys.match), N(done),
   //    //       std::make_tuple(taker_exc_acc, maker_exc_acc, quote_chain, price, base_chain, quantity, bid_or_ask, timestamp)
   //    // ).send();
   // }
   // void exchange::match_for_bid( uint32_t pair_id, account_name payer, account_name receiver, asset quantity, asset price, account_name exc_acc, string referer) {
   //    require_auth( exc_acc );

   //    trading_pairs  trading_pairs_table(_self,_self.value);
   //    orderbook      orders(_self,_self.value);
   //    asset          quant_after_fee;
   //    asset          converted_price;
   //    asset          cumulated_refund_quote;
   //    asset          fee;
   //    uint32_t       bid_or_ask = 1;

   //    auto base = quantity * precision(price.symbol.precision()) / price.amount;

   //    auto itr1 = trading_pairs_table.find(pair_id);
   //    eosio_assert(itr1 != trading_pairs_table.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");
      
   //    fees fees_tbl(_self,_self.value);
   //    auto idx_fees = fees_tbl.template get_index<"idxkey"_n>();
   //    auto idxkey_taker = (uint128_t(exc_acc.value) << 64) | pair_id;
   //    auto itr_fee_taker = idx_fees.find(idxkey_taker);
   //    auto itr_fee_maker = idx_fees.find(idxkey_taker);

   //    base    = convert(itr1->base, base);
   //    price   = convert(itr1->quote, price);
      
   //    if (price.symbol.precision() >= base.symbol.precision())
   //       quant_after_fee = price * base.amount / precision(base.symbol.precision());
   //    else
   //       quant_after_fee = base * price.amount / precision(price.symbol.precision());
      
   //    cumulated_refund_quote = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quantity) -
   //                             to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //    auto idx_orderbook = orders.template get_index<"idxkey"_n>();
   //    auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
      
   //    // traverse ask orders
   //    auto walk_table_range = [&]( auto itr, auto end_itr ) {
   //       bool  full_match;
   //       asset deal_base;
   //       asset cumulated_base          = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, asset(0,CORE_SYMBOL));

   //       auto send_cumulated = [&]() {
   //          if (cumulated_base.amount > 0) {
   //             inline_transfer(config::match_account, receiver, itr1->base_chain, cumulated_base, "");
   //             cumulated_base = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, asset(0,CORE_SYMBOL));
   //          }
               
   //          if (cumulated_refund_quote.amount > 0) {
   //             inline_transfer(config::match_account, payer, itr1->quote_chain, cumulated_refund_quote, "");
   //             cumulated_refund_quote  = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, asset(0,CORE_SYMBOL));
   //          }
   //       };

   //       for( ; itr != end_itr; ) {
            
   //          if (base <= itr->base) {
   //             full_match  = true;
   //             deal_base = base;
   //          } else {
   //             full_match  = false;
   //             deal_base = itr->base;
   //          }
            
   //          quant_after_fee = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, deal_base);
   //          converted_price = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, itr->price);   
   //          done_helper(exc_acc, itr->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, quant_after_fee, bid_or_ask);
   //          if (itr_fee_taker != idx_fees.cend()) {
   //             if (itr_fee_taker->type == 1) {
   //                fee = calcfee(quant_after_fee, itr_fee_taker->rate);
   //                quant_after_fee -= fee;
   //             } else {
   //                // now no other type
   //             }
   //          } else {
   //             fee = calcfee(quant_after_fee, 0);
   //             quant_after_fee -= fee;
   //          }
            
   //          cumulated_base += quant_after_fee;
   //          if (full_match)
   //             inline_transfer(config::match_account, receiver, itr1->base_chain, cumulated_base, "");
               
   //          // transfer fee to exchange
   //          if (fee.amount > 0 && itr_fee_taker != idx_fees.cend()) {
   //             idx_fees.modify(itr_fee_taker, exc_acc, [&]( auto& f ) {
   //                f.fees_base += fee;
   //             });
   //          }
               
   //          //quant_after_fee = convert(itr1->quote_sym, itr->price) * base.amount / precision(base.symbol.precision());
   //          if (itr->price.symbol.precision() >= deal_base.symbol.precision())
   //             quant_after_fee = itr->price * deal_base.amount / precision(deal_base.symbol.precision());
   //          else
   //             quant_after_fee = deal_base * itr->price.amount / precision(itr->price.symbol.precision());
   //          quant_after_fee = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //          if (itr->exc_acc == exc_acc) {
   //             if (itr_fee_taker != idx_fees.cend()) {
   //                fee = calcfee(quant_after_fee, itr_fee_taker->rate);
   //             } else {
   //                fee = calcfee(quant_after_fee, 0);
   //             }
   //          } else {
   //             auto idxkey_maker = (uint128_t(itr->exc_acc.value) << 64) | pair_id;
   //             itr_fee_maker = idx_fees.find(idxkey_maker);
   //             if (itr_fee_maker != idx_fees.cend()) {
   //                fee = calcfee(quant_after_fee, itr_fee_maker->rate);
   //             } else {
   //                fee = calcfee(quant_after_fee, 0);
   //             }
   //          }
            
   //          quant_after_fee -= fee;
   //          inline_transfer(config::match_account, itr->receiver, itr1->quote_chain, quant_after_fee, "");
   //          // transfer fee to exchange
   //          if (fee.amount > 0) {
   //             if (itr->exc_acc == exc_acc) {
   //                idx_fees.modify(itr_fee_taker, exc_acc, [&]( auto& f ) {
   //                   f.fees_quote += fee;
   //                });
   //             } else if (itr_fee_maker != idx_fees.cend()) {
   //                idx_fees.modify(itr_fee_maker, itr->exc_acc, [&]( auto& f ) {
   //                   f.fees_quote += fee;
   //                });
   //             }
               
   //          }
   //          // refund the difference to payer
   //          if ( price > itr->price) {
   //             auto diff = price - itr->price;
   //             if (itr->price.symbol.precision() >= deal_base.symbol.precision())
   //                quant_after_fee = diff * deal_base.amount / precision(deal_base.symbol.precision());
   //             else
   //                quant_after_fee = deal_base * diff.amount / precision(diff.symbol.precision());
   //             quant_after_fee = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //             cumulated_refund_quote += quant_after_fee;
   //             if (full_match)
   //                inline_transfer(config::match_account, payer, itr1->quote_chain, cumulated_refund_quote, "");
   //          } else if (cumulated_refund_quote.amount > 0)
   //             send_cumulated();

   //          if (full_match) {
   //             if (base < itr->base) {
   //                idx_orderbook.modify(itr, _self, [&]( auto& o ) {
   //                   o.base -= deal_base;
   //                });
   //             } else {
   //                idx_orderbook.erase(itr);
   //             }
   //             return;
   //          } else {
   //             base -= itr->base;
   //             idx_orderbook.erase(itr++);
   //             if (itr == end_itr) {
   //                send_cumulated();
   //                insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
   //             }
   //          }
   //       }
   //    };
      
   //    //auto lower_key = (uint128_t(itr1->id) << 96) | ((uint128_t)(bid_or_ask ? 0 : 1)) << 64 | std::numeric_limits<uint64_t>::lowest();
   //    auto lower_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::lowest());    
   //    auto lower = idx_orderbook.lower_bound( lower_key );
   //    auto upper = idx_orderbook.upper_bound( lookup_key );
 
   //    if (lower == idx_orderbook.cend() // orderbook empty
   //       || lower->pair_id != itr1->id || lower->bid_or_ask != 0 || lower->price > price
   //       ) {
   //       prints("\n buy: qualified ask orderbook empty");
   //       if (cumulated_refund_quote.amount > 0) {
   //          inline_transfer(config::match_account, payer, itr1->quote_chain, cumulated_refund_quote, "");
   //       }
   //       insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
   //    } else {
   //       walk_table_range(lower, upper);
   //    }
   // }
   // void exchange::match_for_ask( uint32_t pair_id, account_name payer, account_name receiver, asset base, asset price, account_name exc_acc, string referer) {
   //    require_auth( exc_acc );

   //    trading_pairs  trading_pairs_table(_self,_self.value);
   //    orderbook      orders(_self,_self.value);
   //    asset          quant_after_fee;
   //    asset          converted_price;
   //    asset          converted_base;
   //    asset          fee;
   //    uint32_t       bid_or_ask = 0;

   //    auto itr1 = trading_pairs_table.find(pair_id);
   //    eosio_assert(itr1 != trading_pairs_table.end() && itr1->frozen == 0, "trading pair does not exist or be frozen");

   //    fees fees_tbl(_self,_self.value);
   //    auto idx_fees = fees_tbl.template get_index<"idxkey"_n>();
   //    auto idxkey_taker = (uint128_t(exc_acc.value) << 64) | pair_id;
   //    auto itr_fee_taker = idx_fees.find(idxkey_taker);
   //    auto itr_fee_maker = idx_fees.find(idxkey_taker);

   //    base    = convert(itr1->base, base);
   //    price   = convert(itr1->quote, price);
      
   //    auto idx_orderbook = orders.template get_index<"idxkey"_n>();
   //    auto lookup_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, (uint32_t)price.amount);
      
   //    auto walk_table_range = [&]( auto itr, auto end_itr ) {
   //       bool  full_match;
   //       asset deal_base;
   //       asset cumulated_quote         = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, asset(0,CORE_SYMBOL));
         
   //       auto send_cumulated = [&]() {
   //          if (cumulated_quote.amount > 0) {
   //             inline_transfer(config::match_account, receiver, itr1->quote_chain, cumulated_quote, "");
   //             cumulated_quote         = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, asset(0,CORE_SYMBOL));
   //          }
   //       };
         
   //       for ( auto exit_on_done = false; !exit_on_done; ) {
   //          // only traverse bid orders
   //          if (end_itr == itr) exit_on_done = true;
            
   //          if ( base <= end_itr->base ) {// full match
   //             full_match = true;
   //             deal_base = base;
   //          } else {
   //             full_match = false;
   //             deal_base = end_itr->base;
   //          }
            
   //          // eat the order
   //          if (price.symbol.precision() >= deal_base.symbol.precision())
   //             quant_after_fee = price * deal_base.amount / precision(deal_base.symbol.precision()) ;
   //          else
   //             quant_after_fee = deal_base * price.amount / precision(price.symbol.precision()) ;
   //          quant_after_fee = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //          converted_price = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, price);
   //          converted_base = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, deal_base);
   //          done_helper(exc_acc, itr->exc_acc, itr1->quote_chain, converted_price, itr1->base_chain, converted_base, bid_or_ask);
   //          if (itr_fee_taker != idx_fees.cend()) {
   //             if (itr_fee_taker->type == 1) {
   //                fee = calcfee(quant_after_fee, itr_fee_taker->rate);
   //             }
   //             else {
   //                // now not support other tpes
   //             }
   //          } else {
   //             fee = calcfee(quant_after_fee, 0);
   //          }
            
   //          quant_after_fee -= fee;
   //          cumulated_quote += quant_after_fee;
   //          if (full_match) 
   //             inline_transfer(config::match_account, receiver, itr1->quote_chain, cumulated_quote, "");
               
   //          // transfer fee to exchange
   //          if (fee.amount > 0 && itr_fee_taker != idx_fees.cend()) {
   //             idx_fees.modify(itr_fee_taker, _self, [&]( auto& f ) {
   //                f.fees_quote += fee;
   //             });
   //          }
               
   //          quant_after_fee = to_asset(config::relay_token_account, itr1->base_chain, itr1->base_sym, deal_base);
   //          if (itr->exc_acc == exc_acc) {
   //             if (itr_fee_taker != idx_fees.cend()) {
   //                fee = calcfee(quant_after_fee, itr_fee_taker->rate);
   //             } else {
   //                fee = calcfee(quant_after_fee, 0);
   //             }
   //          } else {
   //             auto idxkey_maker = (uint128_t(itr->exc_acc.value) << 64) | pair_id;
   //             itr_fee_maker = idx_fees.find(idxkey_maker);
   //             if (itr_fee_maker != idx_fees.cend()) {
   //                fee = calcfee(quant_after_fee, itr_fee_maker->rate);
   //             } else {
   //                fee = calcfee(quant_after_fee, 0);
   //             }
   //          }
   //          quant_after_fee -= fee;
   //          inline_transfer(config::match_account, end_itr->receiver, itr1->base_chain, quant_after_fee, "");
   //          // transfer fee to exchange
   //          if (fee.amount > 0) {
   //             if (itr->exc_acc == exc_acc) {
   //                idx_fees.modify(itr_fee_taker, exc_acc, [&]( auto& f ) {
   //                   f.fees_base += fee;
   //                });
   //             } else if (itr_fee_maker != idx_fees.cend()) {
   //                idx_fees.modify(itr_fee_maker, itr->exc_acc, [&]( auto& f ) {
   //                   f.fees_base += fee;
   //                });
   //             }
   //          }
               
   //          // refund the difference
   //          if ( end_itr->price > price) {
   //             auto diff = end_itr->price - price;
   //             if (end_itr->price.symbol.precision() >= deal_base.symbol.precision())
   //                quant_after_fee = diff * deal_base.amount / precision(deal_base.symbol.precision());
   //             else
   //                quant_after_fee = deal_base * diff.amount / precision(diff.symbol.precision());
   //             //print("bid step1: quant_after_fee=",quant_after_fee);
   //             quant_after_fee = to_asset(config::relay_token_account, itr1->quote_chain, itr1->quote_sym, quant_after_fee);
   //             //print("bid step2: quant_after_fee=",quant_after_fee);
   //             inline_transfer(config::match_account, end_itr->maker, itr1->quote_chain, quant_after_fee, "");
   //          }
   //          if (full_match) {
   //             if( deal_base < end_itr->base ) {
   //                idx_orderbook.modify(end_itr, _self, [&]( auto& o ) {
   //                   o.base -= deal_base;
   //                });
   //             } else {
   //                idx_orderbook.erase(end_itr);
   //             }
               
   //             return;
   //          } else {
   //             base -= end_itr->base;
               
   //             if (exit_on_done) {
   //                send_cumulated();
   //                idx_orderbook.erase(end_itr);
   //                insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
   //             } else
   //                idx_orderbook.erase(end_itr--);
   //          }
   //       }
   //    };
      
   //    auto lower = idx_orderbook.lower_bound( lookup_key );
   //    auto upper_key = compute_orderbook_lookupkey(itr1->id, bid_or_ask, std::numeric_limits<uint64_t>::max());
   //    auto upper = idx_orderbook.lower_bound( upper_key );
      
   //    if (lower == idx_orderbook.cend() // orderbook empty
   //       || lower->pair_id != itr1->id // not desired pair /* || lower->bid_or_ask != 1 || lower->price < price */ // not at all
   //       ) {
   //       insert_order(orders, itr1->id, exc_acc, bid_or_ask, base, price, payer, receiver);
   //    } else {
   //       if (upper == idx_orderbook.cend() || upper->pair_id != itr1->id) {
   //          upper--;
   //       }
             
   //       walk_table_range(lower, upper);
   //    }
   // }
   // asset exchange::calcfee(asset quant, uint64_t fee_rate) {
   //    asset fee = quant * fee_rate / MAX_FEE_RATE;
   //    if(fee_rate > 0 && fee.amount < 1) {
   //        fee.amount = 1;
   //    }

   //    return fee;
   // }

   // void exchange::get_pair(uint32_t pair_id, name &base_chain, symbol &base_sym, name &quote_chain, symbol &quote_sym) const {
   //    trading_pairs   pairs_table(_self, _self.value);

   //    auto itr = pairs_table.find(pair_id);
   //    eosio_assert(itr != pairs_table.cend(), "pair does not exist");
      
   //    base_chain  = itr->base_chain;
   //    base_sym    = itr->base_sym;
   //    quote_chain = itr->quote_chain;
   //    quote_sym   = itr->quote_sym;

   //    return;
   // }
   // symbol exchange::get_pair_base( uint32_t pair_id ) const {
   //    trading_pairs   trading_pairs_table(_self, _self.value);

   //    auto lower_key = std::numeric_limits<uint64_t>::lowest();
   //    auto lower = trading_pairs_table.lower_bound( lower_key );
   //    auto upper_key = std::numeric_limits<uint64_t>::max();
   //    auto upper = trading_pairs_table.upper_bound( upper_key );
      
   //    for( auto itr = lower; itr != upper; ++itr ) {
   //        if (itr->id == pair_id) return itr->base;
   //    }
          
   //    eosio_assert(false, "trading pair does not exist");
      
   //    return NULL_SYMBOL;
   // }
   // symbol exchange::get_pair_quote( uint32_t pair_id ) const {
   //    trading_pairs   trading_pairs_table(_self, _self.value);
     
   //    auto lower_key = std::numeric_limits<uint64_t>::lowest();
   //    auto lower = trading_pairs_table.lower_bound( lower_key );
   //    auto upper_key = std::numeric_limits<uint64_t>::max();
   //    auto upper = trading_pairs_table.upper_bound( upper_key );
      
   //    for ( auto itr = lower; itr != upper; ++itr ) {
   //       if (itr->id == pair_id) return itr->quote;
   //    }
          
   //    eosio_assert(false, "trading pair does not exist");
   //    return NULL_SYMBOL;
   // }
   // void exchange::check_pair( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) {
   //    trading_pairs   pairs_table(_self, _self.value);

   //    auto lower_key = std::numeric_limits<uint64_t>::lowest();
   //    auto lower = pairs_table.lower_bound( lower_key );
   //    auto upper_key = std::numeric_limits<uint64_t>::max();
   //    auto upper = pairs_table.upper_bound( upper_key );
      
   //    for ( auto itr = lower; itr != upper; ++itr ) {
   //       if (itr->base_chain == base_chain && itr->base_sym == base_sym && 
   //             itr->quote_chain == quote_chain && itr->quote_sym == quote_sym) {
   //          eosio_assert(false, "trading pair already exist");
   //          return;
   //       }
   //    }

   //    return;
   // }

   // uint32_t exchange::get_pair_id( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) const {
   //    trading_pairs   pairs_table(_self, _self.value);
   //    auto lower_key = std::numeric_limits<uint64_t>::lowest();
   //    auto lower = pairs_table.lower_bound( lower_key );
   //    auto upper_key = std::numeric_limits<uint64_t>::max();
   //    auto upper = pairs_table.upper_bound( upper_key );
      
   //    for ( auto itr = lower; itr != upper; ++itr ) {
   //       if (itr->base_chain == base_chain && itr->base_sym == base_sym && 
   //             itr->quote_chain == quote_chain && itr->quote_sym == quote_sym) {
   //          return itr->id;   
   //       }
   //    }
          
   //    eosio_assert(false, "pair does not exist");

   //    return 0;
   // }
   // // asset exchange::get_avg_price( uint32_t block_height, name base_chain, symbol base_sym, name quote_chain, symbol quote_sym ) {

   // // }
   // void exchange::upd_mark( name base_chain, symbol base_sym, name quote_chain, symbol quote_sym, asset sum, asset vol ) {
   //    deals   deals_table(_self, _self.value);
   //    auto pair_id = get_pair_id(base_chain, base_sym, quote_chain, quote_sym);
   //    auto lower_key = ((uint64_t)pair_id << 32) | 0;
   //    auto idx_deals = deals_table.template get_index<"idxkey"_n>();
   //    auto itr1 = idx_deals.lower_bound(lower_key);
   //    if (!( itr1 != idx_deals.end() && itr1->pair_id == pair_id )) {
   //       print("exchange::upd_mark trading pair not marked!\n");
   //       return;
   //    }
      
   //    uint32_t curr_block = current_block_num();
   //    lower_key = ((uint64_t)pair_id << 32) | curr_block;
   //    itr1 = idx_deals.lower_bound(lower_key);
   //    if (itr1 == idx_deals.cend()) itr1--;
   //    if ( curr_block <= itr1->block_height_end ) {
   //       idx_deals.modify( itr1, _self, [&]( auto& d ) {
   //          d.sum += sum;
   //          d.vol += vol;
   //       });
   //    } else {
   //       auto start_block =  itr1->reset_block_height + (curr_block - itr1->reset_block_height) / INTERVAL_BLOCKS * INTERVAL_BLOCKS;
   //       auto pk = deals_table.available_primary_key();
   //       deals_table.emplace( _self, [&]( auto& d ) {
   //          d.id                 = (uint32_t)pk;
   //          d.pair_id            = pair_id;
   //          d.sum                = sum;
   //          d.vol                = vol;
   //          d.reset_block_height = start_block;
   //          d.block_height_end   = start_block + INTERVAL_BLOCKS - 1;
   //       });   
   //    }
      
   //    return ;
   // }

   // void exchange::insert_order(
   //                orderbook &orders, 
   //                uint32_t pair_id, 
   //                account_name exc_acc,
   //                uint32_t bid_or_ask, 
   //                asset base, 
   //                asset price, 
   //                account_name payer, 
   //                account_name receiver) {
   //    auto pk = orders.available_primary_key();
   //    orders.emplace( exc_acc, [&]( auto& o ) {
   //       o.id            = pk;
   //       o.pair_id       = pair_id;
   //       o.exc_acc       = exc_acc;
   //       o.bid_or_ask    = bid_or_ask;
   //       o.base          = base;
   //       o.price         = price;
   //       o.maker         = payer;
   //       o.receiver      = receiver;
   //       o.timestamp     = time_point_sec(uint32_t(current_time() / 1000000ll));
   //    });
   // }

}

//EOSIO_DISPATCH( relay::exchange, (regex)(create)(match)(cancel)(done)(mark)(claimrelay)(freeze)(unfreeze)(setfee) )