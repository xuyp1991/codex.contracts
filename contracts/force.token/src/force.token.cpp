#include <force.token.hpp>
#include <eosiolib/system.hpp>
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
      print("-------token::create-----------\n");
      statstable.emplace( _self, [&]( auto& s ) {
         s.supply.symbol = maximum_supply.symbol;
         s.max_supply    = maximum_supply;
         s.issuer        = issuer;
      });
   }

   ACTION token::issue( account_name to, asset quantity, string memo ) {
      print("-------token::issue-----------\n");
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
      print("-------token::transfer-----------\n");
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

   asset token::get_supply( symbol_code sym ) const { return asset{0,CORE_SYMBOL};}
   asset token::get_balance( account_name owner, symbol_code sym ) const {return asset{0,CORE_SYMBOL};}

}
//
EOSIO_DISPATCH( force::token, (create)(issue)(transfer)(fee)(trade)(castcoin)(takecoin) )