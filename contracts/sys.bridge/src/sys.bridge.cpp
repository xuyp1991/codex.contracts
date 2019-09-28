#include <sys.bridge.hpp>
#include <eosiolib/system.hpp>
#include <math.h>
//#include <forceio.token/forceio.token.hpp>
#include <../../relay.token/include/relay.token.hpp>
#include <../../force.token/include/force.token.hpp>

namespace relay{
   ACTION bridge::addmarket(name trade,name trade_maker,int64_t type,name base_chain,asset base_amount,uint64_t base_weight,
                  name market_chain,asset market_amount,uint64_t market_weight){
      require_auth(trade_maker);
      
      auto coinbase_sym = base_amount.symbol;
      check( coinbase_sym.is_valid(), "invalid symbol name" );
      check( base_amount.is_valid(), "invalid supply");
      check( base_amount.amount >= 0, "max-supply must be positive");
      
      auto coinmarket_sym = market_amount.symbol;
      check( coinmarket_sym.is_valid(), "invalid symbol name" );
      check( market_amount.is_valid(), "invalid supply");
      check( market_amount.amount >= 0, "max-supply must be positive");

      check(coinbase_sym != coinmarket_sym || base_chain != market_chain,"a market must on two coin");
      
      check( type == static_cast<int64_t>(trade_type::equal_ratio), "invalid trade type");
      check( market_weight > 0,"invalid market_weight");
      check( base_weight > 0,"invalid base_weight");
      tradepairs tradepair( _self,trade_maker.value);
      
      trade_pair trademarket;
      trademarket.trade_name = trade;
      trademarket.trade_maker = trade_maker;

      trademarket.base.chain = base_chain;
      trademarket.base.amount = asset(0,coinbase_sym);
      trademarket.base.weight = base_weight;
      trademarket.base.fee_amount = asset(0,coinbase_sym);

      trademarket.market.chain = market_chain;
      trademarket.market.amount = asset(0,coinmarket_sym);
      trademarket.market.weight = market_weight;
      trademarket.market.fee_amount = asset(0,coinmarket_sym);

      trademarket.type = type;
      trademarket.isactive = true;

      trademarket.fee.base = asset(0,coinbase_sym);
      trademarket.fee.market = asset(0,coinmarket_sym);
      trademarket.fee.fee_type = static_cast<int64_t>( fee_type::fixed);
      //no transfer here
      tradepair.emplace(trade_maker, [&]( trade_pair& s ) {
         s = trademarket;
      });
   }
   ACTION bridge::addmortgage(name trade,name trade_maker,name recharge_account,name coin_chain,asset recharge_amount,int64_t type){
      require_auth(_self);

      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );

      auto coinrecharge_sym = recharge_amount.symbol;
      check( coinrecharge_sym.is_valid(), "invalid symbol name" );
      check( recharge_amount.is_valid(), "invalid supply");
      check( recharge_amount.amount > 0, "max-supply must be positive");
      name chain_name;
      if (type == static_cast<int64_t>( coin_type::coin_base)) {
         check(coinrecharge_sym == existing->base.amount.symbol,"recharge coin is not the same coin on the market");
         check(coin_chain == existing->base.chain,"recharge chain is not the same chain on the market");
         
         chain_name = existing->base.chain;
      }
      else {
         check(coinrecharge_sym == existing->market.amount.symbol,"recharge coin is not the same coin on the market");
         check(coin_chain == existing->market.chain,"recharge chain is not the same chain on the market");
         
         chain_name = existing->market.chain;
      }
      // recharge_account transfer to self
      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         if (type == static_cast<int64_t>( coin_type::coin_base)) {
            s.base.amount = s.base.amount + recharge_amount;
         }
         else {
            s.market.amount = s.market.amount + recharge_amount;
         }
      });
   }
   ACTION bridge::claimmortgage(name trade,name trade_maker,name recv_account,asset claim_amount,int64_t type){
      require_auth(trade_maker);
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );

      auto coinclaim_sym = claim_amount.symbol;
      check( coinclaim_sym.is_valid(), "invalid symbol name" );
      check( claim_amount.is_valid(), "invalid supply");
      check( claim_amount.amount > 0, "max-supply must be positive");
      name chain_name;
      if (type == static_cast<int64_t>( coin_type::coin_base)) {
         check(coinclaim_sym == existing->base.amount.symbol,"recharge coin is not the same coin on the market");
         check(claim_amount <= existing->base.amount,"overdrawn balance");
         chain_name = existing->base.chain;
      }
      else {
         check(coinclaim_sym == existing->market.amount.symbol,"recharge coin is not the same coin on the market");
         check(claim_amount <= existing->market.amount,"overdrawn balance");
         chain_name = existing->market.chain;
      }

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         if (type == static_cast<int64_t>( coin_type::coin_base)) {
            s.base.amount = s.base.amount - claim_amount;
         }
         else {
            s.market.amount = s.market.amount - claim_amount;
         }
      });

      send_transfer_action(chain_name,recv_account,claim_amount,
         std::string("claim market transfer coin market")); 
   }
   ACTION bridge::exchange(name trade,name trade_maker,name account_covert,name account_recv,name coin_chain,asset convert_amount,int64_t type){
      require_auth(_self);

      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );
      check( existing->isactive == true, "the market is not active" );

      auto coinconvert_sym = convert_amount.symbol;
      check( coinconvert_sym.is_valid(), "invalid symbol name" );
      check( convert_amount.is_valid(), "invalid supply");
      check( convert_amount.amount > 0, "max-supply must be positive");

      if (type == static_cast<int64_t>( coin_type::coin_base)) {
         check(coinconvert_sym == existing->base.amount.symbol,"covert coin is not the same coin on the base");
         check(coin_chain == existing->base.chain,"covert chain is not the same chain on the base");
      }
      else {
         check(coinconvert_sym == existing->market.amount.symbol,"covert coin is not the same coin on the market");
         check(coin_chain == existing->market.chain,"covert chain is not the same chain on the market");
      }

      asset market_recv_amount = type != static_cast<int64_t>( coin_type::coin_base) ? existing->base.amount : existing->market.amount;
      uint64_t recv_amount;
      if (existing->type == static_cast<int64_t>(trade_type::equal_ratio)) {
         recv_amount = type != static_cast<int64_t>( coin_type::coin_base)? (convert_amount.amount * existing->base.weight / existing->market.weight) :
         (convert_amount.amount * existing->market.weight / existing->base.weight);
      }
      else if(existing->type == static_cast<int64_t>(trade_type::bancor)) 
      {
         if (type != static_cast<int64_t>( coin_type::coin_base)) { 
            auto tempa = 1 + (double)convert_amount.amount/existing->market.amount.amount;
            auto cw = (double)existing->market.weight/existing->base.weight;
            recv_amount = existing->base.amount.amount * (pow(tempa,cw) - 1);
         }
         else {
            auto tempa = 1 + (double)convert_amount.amount/existing->base.amount.amount;
            auto cw = (double)existing->base.weight/existing->market.weight;
            recv_amount = existing->market.amount.amount * (pow(tempa,cw) - 1);
         }
      }
      auto fee_amout = recv_amount;
      //fee
      if (type != static_cast<int64_t>( coin_type::coin_base)) { 
         if(existing->fee.fee_type == static_cast<int64_t>( fee_type::fixed)) {
            fee_amout = existing->fee.base.amount;
         }
         else if(existing->fee.fee_type == static_cast<int64_t>( fee_type::proportion)) {
            fee_amout = recv_amount*existing->fee.base_ratio/PROPORTION_CARD;
         }
         else if(existing->fee.fee_type == static_cast<int64_t>( fee_type::proportion_mincost)){
            if (recv_amount*existing->fee.base_ratio/PROPORTION_CARD > existing->fee.base.amount)
               fee_amout = recv_amount*existing->fee.base_ratio/PROPORTION_CARD;
            else
            {
               fee_amout = existing->fee.base.amount;
            }
            
         }
      }
      else
      {
         if(existing->fee.fee_type == static_cast<int64_t>( fee_type::fixed)) {
            fee_amout = existing->fee.market.amount;
         }
         else if(existing->fee.fee_type == static_cast<int64_t>( fee_type::proportion)) {
            fee_amout = recv_amount*existing->fee.market_ratio/PROPORTION_CARD;
         }
         else if(existing->fee.fee_type == static_cast<int64_t>( fee_type::proportion_mincost)){
            if (recv_amount*existing->fee.market_ratio/PROPORTION_CARD > existing->fee.market.amount)
               fee_amout = recv_amount*existing->fee.market_ratio/PROPORTION_CARD;
            else
            {
               fee_amout = existing->fee.market.amount;
            }
            
         }
      }
      
      recv_amount -= fee_amout;
      check(recv_amount < market_recv_amount.amount,
      "the market do not has enough dest coin");

      auto recv_asset = asset(recv_amount,market_recv_amount.symbol);
      auto fee_asset = asset(fee_amout,market_recv_amount.symbol);
      // account_covert transfer to self
      
      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         if (type == static_cast<int64_t>( coin_type::coin_base)) {
            s.base.amount = s.base.amount + convert_amount;
            s.market.amount = s.market.amount - recv_asset - fee_asset;
            s.market.fee_amount += fee_asset;
         }
         else {
            s.market.amount = s.market.amount + convert_amount;
            s.base.amount = s.base.amount - recv_asset - fee_asset;
            s.base.fee_amount += fee_asset;
         }
      });
   
      send_transfer_action(type == static_cast<int64_t>( coin_type::coin_market) ? existing->base.chain:existing->market.chain,account_recv,recv_asset,
         std::string("exchange market transfer coin market"));
   }
   ACTION bridge::frozenmarket(name trade,name trade_maker){
      require_auth(trade_maker);

      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );
      check( existing->isactive == true, "the market is not active" );

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.isactive = false;
      });
   }
   ACTION bridge::trawmarket(name trade,name trade_maker){
      require_auth(trade_maker);

      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );
      check( existing->isactive == false, "the market is already active" );

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.isactive = true;
      });
   }
   ACTION bridge::setfixedfee(name trade,name trade_maker,asset base,asset market){
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );
      
      auto base_sym = base.symbol;
      check( base_sym.is_valid(), "invalid symbol name" );
      check( base.is_valid(), "invalid supply");
      check( base.amount >= 0, "max-supply must be positive");

      auto market_sym = market.symbol;
      check( market_sym.is_valid(), "invalid symbol name" );
      check( market.is_valid(), "invalid supply");
      check( market.amount >= 0, "max-supply must be positive");

      check(existing->fee.base.symbol == base_sym,"base asset is different coin with fee base");
      check(existing->fee.market.symbol == market_sym,"base asset is different coin with fee base");

      check(existing->isactive == false,"only frozen market can set fixedfee");

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.fee.base = base;
         s.fee.market = market;
         s.fee.fee_type = static_cast<int64_t>( fee_type::fixed);
      });
   }
   ACTION bridge::setprofee(name trade,name trade_maker,uint64_t base_ratio,uint64_t market_ratio){
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );

      check(base_ratio >= 0 && base_ratio < PROPORTION_CARD,"base_ratio must between 0 and 10000");
      check(market_ratio >= 0 && market_ratio < PROPORTION_CARD,"market_ratio must between 0 and 10000");

      check(existing->isactive == false,"only frozen market can set proportion fee");
      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.fee.base_ratio = base_ratio;
         s.fee.market_ratio = market_ratio;
         s.fee.fee_type = static_cast<int64_t>( fee_type::proportion);
      });
   }
   ACTION bridge::setprominfee(name trade,name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market){
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );
      
      auto base_sym = base.symbol;
      check( base_sym.is_valid(), "invalid symbol name" );
      check( base.is_valid(), "invalid supply");
      check( base.amount >= 0, "max-supply must be positive");

      auto market_sym = market.symbol;
      check( market_sym.is_valid(), "invalid symbol name" );
      check( market.is_valid(), "invalid supply");
      check( market.amount >= 0, "max-supply must be positive");

      check(existing->fee.base.symbol == base_sym,"base asset is different coin with fee base");
      check(existing->fee.market.symbol == market_sym,"base asset is different coin with fee base");

      check(base_ratio >= 0 && base_ratio < PROPORTION_CARD,"base_ratio must between 0 and 10000");
      check(market_ratio >= 0 && market_ratio < PROPORTION_CARD,"market_ratio must between 0 and 10000");

      check(existing->isactive == false,"only frozen market can set proportion and min fee");

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.fee.base_ratio = base_ratio;
         s.fee.market_ratio = market_ratio;
         s.fee.base = base;
         s.fee.market = market;
         s.fee.fee_type = static_cast<int64_t>( fee_type::proportion_mincost);
      });
   }
   ACTION bridge::setweight(name trade,name trade_maker,uint64_t  base_weight,uint64_t  market_weight){
      require_auth(trade_maker); 
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );

      check( market_weight > 0,"invalid market_weight");
      check( base_weight > 0,"invalid base_weight");

      check(existing->isactive == false,"only frozen market can set weight");
      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.base.weight = base_weight;
         s.market.weight = market_weight;
      });
   }
   ACTION bridge::settranscon(name chain,asset quantity,name contract_name){
      require_auth(_self);
      transcon trans(_self,_self.value);
      auto idx = trans.get_index<"bychain"_n>();
      auto con = idx.find(global_func::make_int128_index(chain, quantity));

      if( con == idx.end() ) {
         trans.emplace(_self, [&]( auto& a ) {
            a.id = trans.available_primary_key();
            a.quantity = quantity;
            a.contract_name = contract_name;
            a.chain = chain;
         });
      }
      else {
         idx.modify(con, _self, [&]( auto& a ) {
            a.contract_name = contract_name;
         });
      }
   }
   ACTION bridge::removemarket(name trade,name trade_maker,name base_recv,name maker_recv){
      require_auth(trade_maker);
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );
      if(existing->base.amount.amount > 0) {
         send_transfer_action(existing->base.chain,base_recv,existing->base.amount,std::string("remove market return the base coins"));
      }
      if(existing->market.amount.amount > 0) {
         send_transfer_action(existing->market.chain,maker_recv,existing->market.amount,std::string("remove market return the market coins"));
      }
      if(existing->base.fee_amount.amount > 0) {
         send_transfer_action(existing->base.chain,base_recv,existing->base.fee_amount,std::string("remove market return the base fee coins"));
      }
      if(existing->market.fee_amount.amount > 0) {
         send_transfer_action(existing->market.chain,maker_recv,existing->market.fee_amount,std::string("remove market return the market fee coins"));
      }

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         s.base.amount -= s.base.amount;
         s.market.amount -= s.market.amount;
         s.base.fee_amount -= s.base.amount;
         s.market.fee_amount -= s.market.amount;
      });
      tradepair.erase(existing);
   }
   ACTION bridge::claimfee(name trade,name trade_maker,name recv_account,int64_t type){
      require_auth(trade_maker);
      tradepairs tradepair( _self,trade_maker.value);
      auto existing = tradepair.find( trade.value );
      check( existing != tradepair.end(), "the market is not exist" );

      send_transfer_action(type == static_cast<int64_t>( coin_type::coin_base) ? existing->base.chain:existing->market.chain,
                           recv_account,
                           type == static_cast<int64_t>( coin_type::coin_base) ? existing->base.fee_amount:existing->market.fee_amount,
                           std::string("claim fee transfer coin market")); 

      tradepair.modify( existing, trade_maker, [&]( auto& s ) {
         if (type == static_cast<int64_t>( coin_type::coin_base)) {
            s.base.fee_amount = asset(0,s.base.fee_amount.symbol);
         }
         else {
            s.market.fee_amount = asset(0,s.market.fee_amount.symbol);
         }
      });
   }

   void bridge::send_transfer_action(name chain,name recv,asset quantity,std::string memo) {
      transcon transfer_contract(_self, _self.value);
      auto idx = transfer_contract.get_index<"bychain"_n>();
      const auto& contract = idx.get(global_func::make_int128_index(chain, quantity), "contract object not found");
      if (contract.contract_name == name("relay.token"_n)) {
         INLINE_ACTION_SENDER(relay::token, transfer)( 
            contract.contract_name, 
            {_self, "active"_n},
            { _self, 
            recv, 
            chain,
            quantity, 
            memo } );
      }
      else if(contract.contract_name == name("force.token"_n)) {
         INLINE_ACTION_SENDER(force::token, transfer)( 
            contract.contract_name, 
            {_self, "active"_n},
            { _self, 
            recv, 
            quantity, 
            memo } );
      }
      else {
         eosio_assert(false,
            "the contract is invalid");
      }
   }
}
EOSIO_DISPATCH(relay::bridge, (addmarket)(addmortgage)(claimmortgage)(exchange)(frozenmarket)(trawmarket)(setfixedfee)(setprofee)(setprominfee)(setweight)
         (settranscon)(removemarket)(claimfee) )