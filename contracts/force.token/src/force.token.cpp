#include <force.token.hpp>
#include <eosiolib/system.hpp>
#include <../../sys.bridge/include/sys.bridge.hpp>
#include <../../sys.match/include/sys.match.hpp>

namespace force {

   ACTION token::create( account_name issuer,
               asset        maximum_supply) {
      require_auth( issuer );

      auto sym = maximum_supply.symbol;
      eosio_assert( sym.is_valid(), "invalid symbol name" );
      eosio_assert( maximum_supply.is_valid(), "invalid supply");
      eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");
      eosio_assert( sym != CORE_SYMBOL, "not allow create core symbol token by token contract");

      stats statstable( _self, sym.code().raw() );
      auto existing = statstable.find( sym.code().raw() );
      eosio_assert( existing == statstable.end(), "token with symbol already exists" );
      statstable.emplace( _self, [&]( auto& s ) {
         s.supply.symbol = maximum_supply.symbol;
         s.max_supply    = maximum_supply;
         s.issuer        = issuer;
      });
   }

   ACTION token::issue( account_name to, asset quantity, string memo ) {
      auto sym = quantity.symbol;
      eosio_assert( sym.is_valid(), "invalid symbol name" );
      eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

      auto sym_name = sym.code().raw();
      stats statstable( _self, sym_name );
      auto existing = statstable.find( sym_name );
      eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
      const auto& st = *existing;

      require_auth( st.issuer );
      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must issue positive quantity" );

      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
      eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

      statstable.modify( st, st.issuer, [&]( auto& s ) {
         s.supply += quantity;
      });

      add_balance( st.issuer, quantity, st.issuer );

      if( to != st.issuer ) {
         SEND_INLINE_ACTION( *this, transfer, {st.issuer,"active"_n}, {st.issuer, to, quantity, memo} );
      }
   }

   ACTION token::transfer( account_name from,
                  account_name to,
                  asset        quantity,
                  string       memo ) {
      eosio_assert( from != to, "cannot transfer to self" );
      require_auth( from );
      eosio_assert( is_account( to ), "to account does not exist");
      auto sym = quantity.symbol.code().raw();
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );

      require_recipient( from );
      require_recipient( to );

      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
      eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


      sub_balance( from, quantity );
      add_balance( to, quantity, from );
   }

   ACTION token::fee( account_name payer, asset quantity ) {
      const auto fee_account = "force.fee"_n;

      require_auth( payer );

      auto sym = quantity.symbol.code().raw();
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );

      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

      sub_balance( payer, quantity );
      add_balance( fee_account, quantity, payer );
   }

   ACTION token::trade( account_name   from,
               account_name   to,
               asset          quantity,
               uint64_t      type,
               string           memo) {
      if (type == static_cast<uint64_t>(func_type::bridge_addmortgage) && to == config::bridge_account) {
         sys_bridge_addmort bri_add;
         bri_add.parse(memo);

         INLINE_ACTION_SENDER(relay::bridge, addmortgage)( 
            config::bridge_account, 
            {config::bridge_account, "active"_n},
            { bri_add.trade_name, 
            bri_add.trade_maker, 
            from,
            name("self"_n),
            quantity, 
            bri_add.type } );
         
         // eosio::action(
         //    vector<eosio::permission_level>{{SYS_BRIDGE,N(active)}},
         //    SYS_BRIDGE,
         //    N(addmortgage),
         //    std::make_tuple(
         //          bri_add.trade_name.value,bri_add.trade_maker,from,N(self),quantity,bri_add.type
         //    )
         // ).send();
         transfer(from, to,  quantity, memo);
      }
      else if (type == static_cast<uint64_t>(func_type::bridge_exchange) && to == config::bridge_account) {

         sys_bridge_exchange bri_exchange;
         bri_exchange.parse(memo);

         INLINE_ACTION_SENDER(relay::bridge, exchange)( 
            config::bridge_account, 
            {config::bridge_account, "active"_n},
            { bri_exchange.trade_name, 
            bri_exchange.trade_maker, 
            from,
            bri_exchange.recv,
            name("self"_n),
            quantity, 
            bri_exchange.type } );

         // eosio::action(
         //    vector<eosio::permission_level>{{SYS_BRIDGE,N(active)}},
         //    SYS_BRIDGE,
         //    N(exchange),
         //    std::make_tuple(
         //       bri_exchange.trade_name.value,bri_exchange.trade_maker,from,bri_exchange.recv,N(self),quantity,bri_exchange.type
         //    )
         // ).send();
         transfer(from, to,  quantity, memo);
      }
      else if(type == static_cast<uint64_t>(func_type::match) && to == config::match_account) {
         transfer(from, to, quantity, memo);
         // sys_match_match smm;
         // smm.parse(memo);
         //trade_imp(smm.payer, smm.receiver, smm.pair_id, quantity, smm.price, smm.bid_or_ask);
      }
      else {
         eosio_assert(false,"invalid type");
      }
   }
   ACTION token::castcoin(account_name from,account_name to,asset quantity) {
      //eosio_assert( from == ::config::reward_account_name, "only the account force.reward can cast coin to others" );
      require_auth( from );

      eosio_assert( is_account( to ), "to account does not exist");
      coincasts coincast_table( _self, to.value );
      auto current_block = current_block_num();
      int32_t cast_num = PRE_CAST_NUM - static_cast<int32_t>(current_block / WEAKEN_CAST_NUM);
      if (cast_num < static_cast<int32_t>(STABLE_CAST_NUM)) cast_num = STABLE_CAST_NUM;
      auto finish_block = current_block;// + cast_num;
      const auto cc = coincast_table.find( static_cast<uint64_t>(finish_block) );

      require_recipient( from );
      require_recipient( to );

      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
      if (cc != coincast_table.end()) {
         eosio_assert( quantity.symbol == cc->balance.symbol, "symbol precision mismatch" );
      }

      sub_balance( from, quantity );
      if (cc == coincast_table.end()) {
         coincast_table.emplace( from, [&]( auto& a ){
            a.balance = quantity;
            a.finish_block = finish_block;
         });
      }
      else {
         coincast_table.modify( cc, to, [&]( auto& a ) {
            a.balance += quantity;
         });
      }
   }
   ACTION token::takecoin(account_name to) {
      require_auth( to );
      coincasts coincast_table( _self, to.value );
      auto current_block = current_block_num();
      std::vector<uint32_t>  finish_block;
      asset finish_coin = asset{0,CORE_SYMBOL};
      finish_block.clear();
      for( auto it = coincast_table.cbegin(); it != coincast_table.cend(); ++it ) {
         if(it->finish_block < current_block) {
            finish_block.push_back(it->finish_block);
            finish_coin += it->balance;
         }
      }
      add_balance( to, finish_coin, to );
      for (auto val : finish_block)
      {
         const auto cc = coincast_table.find( static_cast<uint64_t>(val) );
         if (cc != coincast_table.end()) {
            coincast_table.erase(cc);
         }
      }
   }

   void token::sub_balance( account_name owner, asset value ) {
      accounts from_acnts( _self, owner.value );

      const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
      eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

      if( from.balance.amount == value.amount ) {
         from_acnts.erase( from );
      } else {
         from_acnts.modify( from, owner, [&]( auto& a ) {
            a.balance -= value;
         });
      }
   }
   void token::add_balance( account_name owner, asset value, account_name ram_payer ){
      accounts to_acnts( _self, owner.value );
      
      auto to = to_acnts.find( value.symbol.code().raw() );
      if( to == to_acnts.end() ) {
         to_acnts.emplace( ram_payer, [&]( auto& a ){
         a.balance = value;
         });
      } else {
         to_acnts.modify( to, owner, [&]( auto& a ) {
         a.balance += value;
         });
      }
   }

   void splitMemo(std::vector<std::string>& results, const std::string& memo,char separator) {
      auto start = memo.cbegin();
      auto end = memo.cend();

      for (auto it = start; it != end; ++it) {
      if (*it == separator) {
            results.emplace_back(start, it);
            start = it + 1;
      }
      }
      if (start != end) results.emplace_back(start, end);
   }

   uint64_t char_to_symbol( char c ) {
      if( c >= 'a' && c <= 'z' )
         return (c - 'a') + 6;
      if( c >= '1' && c <= '5' )
         return (c - '1') + 1;
      return 0;
   }

   uint64_t string_to_name( const char* str )
   {
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

   void sys_bridge_addmort::parse(const string memo) {
      std::vector<std::string> memoParts;
      splitMemo(memoParts, memo, ';');
      eosio_assert(memoParts.size() == 3,"memo is not adapted with bridge_addmortgage");
      this->trade_name = name(string_to_name(memoParts[0].c_str()));
      this->trade_maker = name(string_to_name(memoParts[1].c_str()));
      this->type = atoi(memoParts[2].c_str());
      eosio_assert(this->type == 1 || this->type == 2,"type is not adapted with bridge_addmortgage");
   }

   void sys_bridge_exchange::parse(const string memo) {
      std::vector<std::string> memoParts;
      splitMemo(memoParts, memo, ';');
      eosio_assert(memoParts.size() == 4,"memo is not adapted with bridge_addmortgage");
      this->trade_name = name(string_to_name(memoParts[0].c_str()));
      this->trade_maker = name(string_to_name(memoParts[1].c_str()));
      this->recv = name(string_to_name(memoParts[2].c_str()));
      this->type = atoi(memoParts[3].c_str());
      eosio_assert(this->type == 1 || this->type == 2,"type is not adapted with bridge_addmortgage");
   }

   asset token::get_supply( symbol_code sym ) const { return asset{0,CORE_SYMBOL};}
   asset token::get_balance( account_name owner, symbol_code sym ) const {return asset{0,CORE_SYMBOL};}

}
//
EOSIO_DISPATCH( force::token, (create)(issue)(transfer)(fee)(trade)(castcoin)(takecoin) )