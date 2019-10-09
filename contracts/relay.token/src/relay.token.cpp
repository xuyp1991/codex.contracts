#include <relay.token.hpp>
#include <../../force.token/include/force.token.hpp>
#include <../../sys.bridge/include/sys.bridge.hpp>
#include <../../sys.match/include/sys.match.hpp>

namespace relay {

   ACTION token::on( name chain, const checksum256 block_id, const force::relay::action& act ) {
      //require_auth(config::relay_account_name); // TODO use config

      // TODO this ACTION should no err

      const auto data = unpack<token::action>(act.data);
      print("map ", name{ data.from }, " ", data.quantity, " ", data.memo, "\n");

      auto sym = data.quantity.symbol;
      stats statstable(_self, chain.value);
      auto st = statstable.find(sym.raw());
      if( st == statstable.end() ){
         // TODO param err processing
         print("no token err");
         return;
      }

      if(    ( !sym.is_valid() )
         || ( data.memo.size() > 256 )
         || ( !data.quantity.is_valid() )
         || ( data.quantity.amount <= 0 )
         || ( data.quantity.symbol != st->supply.symbol )
         || ( data.quantity.amount > st->max_supply.amount - st->supply.amount )
         ) {
         // TODO param err processing
         print("token err");
         return;
      }

      if( data.memo.empty() || data.memo.size() >= 13 ){
         // TODO param err processing
         print("data.memo err");
         return;
      }
      const auto to = global_func::string_to_name(data.memo.c_str());
      if( !is_account(to) ) {
         // TODO param err processing
         print("to is no account");
         return;
      }
      relay::token::issue_action temp{"relay.token"_n, {chain, "active"_n}};
      temp.send(chain, to, data.quantity, "from chain");
   }

   ACTION token::create( account_name issuer,
               name chain,
               asset maximum_supply ) {
      require_auth("eosforce"_n);

      auto sym = maximum_supply.symbol;
      eosio_assert(sym.is_valid(), "invalid symbol name");
      eosio_assert(maximum_supply.is_valid(), "invalid supply");
      eosio_assert(maximum_supply.amount > 0, "max-supply must be positive");

      stats statstable(_self, chain.value);
      auto existing = statstable.find(sym.raw());
      eosio_assert(existing == statstable.end(), "token with symbol already exists");

      statstable.emplace(_self, [&]( auto& s ) {
         s.supply.symbol = maximum_supply.symbol;
         s.max_supply = maximum_supply;
         s.issuer = issuer;
         s.chain = chain;
         s.reward_pool = asset{0,CORE_SYMBOL};
         s.total_mineage = 0;
         s.total_mineage_update_height = current_block_num();
      });
   }

   /// @abi action
   ACTION token::issue( name chain, account_name to, asset quantity, string memo ) {
      auto sym = quantity.symbol;
      eosio_assert(sym.is_valid(), "invalid symbol name");
      eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

      auto sym_name = sym.raw();
      stats statstable(_self, chain.value);
      auto existing = statstable.find(sym_name);
      eosio_assert(existing != statstable.end(), "token with symbol does not exist, create token before issue");
      const auto& st = *existing;

      // TODO auth
      require_auth(st.issuer);

      eosio_assert(quantity.is_valid(), "invalid quantity");
      eosio_assert(quantity.amount > 0, "must issue positive quantity");

      eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
      eosio_assert(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

      auto current_block = current_block_num();
      auto last_devidend_num = current_block - current_block % config::UPDATE_CYCLE;

      statstable.modify(st, chain, [&]( auto& s ) {
         if (s.total_mineage_update_height < last_devidend_num) {
            s.total_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,last_devidend_num) + s.total_pending_mineage;
            s.total_pending_mineage = get_current_age(s.chain,s.supply,last_devidend_num,current_block);
         }
         else {
            s.total_pending_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,current_block);
         }
         s.total_mineage_update_height = current_block_num();
         s.supply += quantity;
      });

      add_balance(st.issuer, chain, quantity, st.issuer);

      if( to != st.issuer ) {
         SEND_INLINE_ACTION(*this, transfer, { st.issuer, "active"_n }, { st.issuer, to, chain, quantity, memo });
      }
   }

   /// @abi action
   ACTION token::destroy( name chain, account_name from, asset quantity, string memo ) {
      require_auth(from);

      auto sym = quantity.symbol;
      eosio_assert(sym.is_valid(), "invalid symbol name");
      eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

      auto sym_name = sym.raw();
      stats statstable(_self, chain.value);
      auto existing = statstable.find(sym_name);
      eosio_assert(existing != statstable.end(), "token with symbol does not exist, create token before issue");
      const auto& st = *existing;

      eosio_assert(quantity.is_valid(), "invalid quantity");
      eosio_assert(quantity.amount > 0, "must issue positive quantity");

      eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
      eosio_assert(quantity.amount <= st.supply.amount, "quantity exceeds available supply");

      auto current_block = current_block_num();
      auto last_devidend_num = current_block - current_block % config::UPDATE_CYCLE;
      statstable.modify(st, chain, [&]( auto& s ) {
         if (s.total_mineage_update_height < last_devidend_num) {
            s.total_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,last_devidend_num) + s.total_pending_mineage;
            s.total_pending_mineage = get_current_age(s.chain,s.supply,last_devidend_num,current_block);
         }
         else {
            s.total_pending_mineage += get_current_age(s.chain,s.supply,s.total_mineage_update_height,current_block);
         }
         s.total_mineage_update_height = current_block;
         s.supply -= quantity;
      });

      sub_balance(from, chain, quantity);
   }

   /// @abi action
   ACTION token::transfer( account_name from,
                  account_name to,
                  name chain,
                  asset quantity,
                  string memo ) {
      eosio_assert(from != to, "cannot transfer to self");
      require_auth(from);
      eosio_assert(is_account(to), "to account does not exist");
      auto sym = quantity.symbol.raw();
      stats statstable(_self, chain.value);
      const auto& st = statstable.get(sym);

      require_recipient(from);
      require_recipient(to);

      eosio_assert(quantity.is_valid(), "invalid quantity");
      eosio_assert(quantity.amount > 0, "must transfer positive quantity");
      eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
      eosio_assert(chain == st.chain, "symbol chain mismatch");
      eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");


      sub_balance(from, chain, quantity);
      add_balance(to, chain, quantity, from);
   }
   
   /// @abi action
   ACTION token::addreward(name chain,asset supply) {
      require_auth(_self);

      auto sym = supply.symbol;
      eosio_assert(sym.is_valid(), "invalid symbol name");
      
      rewards rewardtable(_self, _self.value);
      auto idx = rewardtable.get_index<"bychain"_n>();
      auto con = idx.find(get_account_idx(chain, supply));

      eosio_assert(con == idx.end(), "token with symbol already exists");

         rewardtable.emplace(_self, [&]( auto& s ) {
            s.id = rewardtable.available_primary_key();
            s.supply = supply;
            s.chain = chain;
         });
   }
   /// @abi action
   ACTION token::rewardmine(asset quantity) {
      //require_auth(::config::system_account_name);
      rewards rewardtable(_self, _self.value);
      uint64_t total_power = 0;
      for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
         stats statstable(_self, it->chain.value);
         auto existing = statstable.find(it->supply.symbol.raw());
         eosio_assert(existing != statstable.end(), "token with symbol already exists");
         // auto price = relay::exchange::get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
         // total_power += existing->supply.amount * price ;
      }

      if (total_power == 0) return ;
      for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
         stats statstable(_self, it->chain.value);
         auto existing = statstable.find(it->supply.symbol.raw());
         eosio_assert(existing != statstable.end(), "token with symbol already exists");
         // auto price = relay::exchange::get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
         // uint64_t devide_amount = quantity.amount * existing->supply.amount * price / total_power;
         // statstable.modify(*existing, it->chain, [&]( auto& s ) {
         //    s.reward_pool += asset(devide_amount,s.reward_pool.symbol);
         // });
      }
   }
   /// @abi action
   ACTION token::claim(name chain,asset quantity,account_name receiver) {

      require_auth(receiver);
      auto sym = quantity.symbol;
      eosio_assert(sym.is_valid(), "invalid symbol name");
      
      rewards rewardtable(_self, _self.value);
      auto idx = rewardtable.get_index<"bychain"_n>();
      auto con = idx.find(get_account_idx(chain, quantity));
      eosio_assert(con != idx.end(), "token with symbol donot participate in dividends ");

      stats statstable(_self, chain.value);
      auto existing = statstable.find(quantity.symbol.raw());
      eosio_assert(existing != statstable.end(), "token with symbol already exists");

      auto reward_pool = existing->reward_pool;
      auto total_mineage = existing->total_mineage;
      auto total_mineage_update_height = existing->total_mineage_update_height;
      auto total_pending_mineage = existing->total_pending_mineage;
      auto supply = existing->supply;

      accounts to_acnts(_self, receiver.value);
      auto idxx = to_acnts.get_index<"bychain"_n>();
      const auto& to = idxx.get(get_account_idx(chain, quantity), "no balance object found");
      eosio_assert(to.chain == chain, "symbol chain mismatch");
      auto current_block = current_block_num();
      auto last_devidend_num = current_block - current_block % config::UPDATE_CYCLE;

      auto power = to.mineage;
      if (to.mineage_update_height < last_devidend_num) {
         power = to.mineage + get_current_age(to.chain,to.balance,to.mineage_update_height,last_devidend_num) + to.pending_mineage ;
      }

      eosio_assert(power != 0, "the devident is zreo");

      auto total_power = total_mineage;
      if (total_mineage_update_height < last_devidend_num) {
         total_power = total_mineage + get_current_age(existing->chain,supply,total_mineage_update_height,last_devidend_num) + total_pending_mineage;
      }

      auto total_power_64 = static_cast<int64_t>(total_power);

      int128_t reward_temp = static_cast<int128_t>(reward_pool.amount) * power;
      auto total_reward = asset(static_cast<int64_t>(reward_temp / total_power),CORE_SYMBOL);
      statstable.modify( existing,chain,[&](auto &st) {
         st.reward_pool -= total_reward;
         st.total_mineage = total_power - power;
         
         if (st.total_mineage_update_height < last_devidend_num) {
            st.total_mineage += st.total_pending_mineage;
            st.total_pending_mineage = 0;
         }
         st.total_mineage_update_height = last_devidend_num;
      });

      to_acnts.modify(to, receiver, [&]( auto& a ) {
         a.mineage = 0;
         if (a.mineage_update_height < last_devidend_num) {
            a.pending_mineage = get_current_age(a.chain,a.balance,last_devidend_num,current_block);
         }
         else {
            a.pending_mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,current_block);
         }
         a.mineage_update_height = current_block;
      });

      eosio_assert(total_reward.amount > 100000,"claim amount must > 10");
      INLINE_ACTION_SENDER(force::token, castcoin)( 
         config::reward_account, 
         {config::reward_account, "active"_n},
         { config::reward_account, 
         receiver,total_reward } );

   }

   void token::sub_balance( account_name owner, name chain, asset value ) {
      accounts from_acnts(_self, owner.value);

      auto idx = from_acnts.get_index<"bychain"_n>();

      const auto& from = idx.get(get_account_idx(chain, value), "no balance object found");
      eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");
      eosio_assert(from.chain == chain, "symbol chain mismatch");
      auto current_block = current_block_num();
      auto last_devidend_num = current_block - current_block % config::UPDATE_CYCLE;

      from_acnts.modify(from, owner, [&]( auto& a ) {
         if (a.mineage_update_height < last_devidend_num) {
            a.mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,last_devidend_num) + a.pending_mineage;
            a.pending_mineage = get_current_age(a.chain,a.balance,last_devidend_num,current_block);
         }
         else {
            a.pending_mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,current_block);
         }
         a.mineage_update_height = current_block;
         a.balance -= value;
      });
   }
   void token::add_balance( account_name owner, name chain, asset value, account_name ram_payer ) {
      accounts to_acnts(_self, owner.value);
      account_next_ids acntids(_self, owner.value);

      auto idx = to_acnts.get_index<"bychain"_n>();

      auto to = idx.find(get_account_idx(chain, value));
      auto current_block = current_block_num();
      auto last_devidend_num = current_block - current_block % config::UPDATE_CYCLE;
      if( to == idx.end() ) {
         uint64_t id = 1;
         auto ids = acntids.find(owner.value);
         if(ids == acntids.end()){
            acntids.emplace(ram_payer, [&]( auto& a ){
               a.id = 2;
               a.account = owner;
            });
         }else{
            id = ids->id;
            acntids.modify(ids, owner, [&]( auto& a ) {
               a.id++;
            });
         }

         to_acnts.emplace(ram_payer, [&]( auto& a ) {
            a.id = id;
            a.balance = value;
            a.chain = chain;
            a.mineage = 0;
            a.pending_mineage = 0;
            a.mineage_update_height = current_block_num();
         });
      } else { 
         idx.modify(to, owner, [&]( auto& a ) {
            if (a.mineage_update_height < last_devidend_num) {
               a.mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,last_devidend_num) + a.pending_mineage;
               a.pending_mineage = get_current_age(a.chain,a.balance,last_devidend_num,current_block);
            }
            else {
               a.pending_mineage += get_current_age(a.chain,a.balance,a.mineage_update_height,current_block);
            }
            a.mineage_update_height = current_block_num();
            a.balance += value;
         });
      }
   }

   int64_t token::get_current_age(name chain,asset balance,int64_t first,int64_t last) {return 1;}
}
//EOSIO_DISPATCH( relay::token,(create)(issue)(destroy)(transfer)(addreward)(rewardmine)(claim)(trade) )