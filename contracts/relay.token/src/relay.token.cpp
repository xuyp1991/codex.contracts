#include <relay.token.hpp>
#include <../../force.token/include/force.token.hpp>
#include <../../sys.bridge/include/sys.bridge.hpp>
#include <../../sys.match/include/sys.match.hpp>

namespace relay {

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
   ACTION token::trade( account_name from,
               account_name to,
               name chain,
               asset quantity,
               uint64_t type,
               string memo) {
      //eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");
      if (type == static_cast<uint64_t>(func_type::bridge_addmortgage) && to == config::bridge_account) {
         transfer(from, to, chain, quantity, memo);
         
         sys_bridge_addmort bri_add;
         bri_add.parse(memo);
         
         INLINE_ACTION_SENDER(relay::bridge, addmortgage)( 
            config::bridge_account, 
            {config::bridge_account, "active"_n},
            { bri_add.trade_name, 
            bri_add.trade_maker, 
            from,
            chain,
            quantity, 
            bri_add.type } );
      }
      else if (type == static_cast<uint64_t>(func_type::bridge_exchange) && to == config::bridge_account) {
         transfer(from, to, chain, quantity, memo);

         sys_bridge_exchange bri_exchange;
         bri_exchange.parse(memo);
         
         INLINE_ACTION_SENDER(relay::bridge, exchange)( 
            config::bridge_account, 
            {config::bridge_account, "active"_n},
            { bri_exchange.trade_name, 
            bri_exchange.trade_maker, 
            from,
            bri_exchange.recv,
            chain,
            quantity, 
            bri_exchange.type } );
      }
      else if(type == static_cast<uint64_t>(func_type::match) && to == config::match_account) {
         transfer(from, to, chain, quantity, memo);
         sys_match_match smm;
         smm.parse(memo);
         auto trade_imp = [smm](account_name payer, account_name receiver, uint32_t pair_id, asset quantity, asset price2, uint32_t bid_or_ask, account_name exc_acc, string referer) {
            asset           quant_after_fee;
            asset           base;
            asset           price;
            
            // relay::exchange e(config::match_account);
            // auto base_sym = e.get_pair_base(pair_id);
            // auto quote_sym = e.get_pair_quote(pair_id);
            // //auto exc_acc = e.get_exchange_account(pair_id);
            
            // if (bid_or_ask) {
            //    // to preserve precision
            //    quant_after_fee = convert(base_sym, quantity);
            //    //base = quant_after_fee * precision(price2.symbol.precision()) / price2.amount;
            //    base = quant_after_fee;
            //    print("after convert: quant_after_fee=", quant_after_fee, ", base=", base, "\n");
            // } else {
            //    base = convert(base_sym, quantity);
            // }
            // price = convert(quote_sym, price2);
            
            // print("\n before inline call sys.match --payer=",payer,", receiver=",receiver,", pair_id=",pair_id,", quantity=",quantity,", price=",price2,", bid_or_ask=",bid_or_ask, ", base=",quantity);
            
            // eosio::action(
            //       permission_level{ exc_acc, N(active) },
            //       N(sys.match), N(match),
            //       std::make_tuple(pair_id, payer, receiver, base, price, bid_or_ask, exc_acc, referer)
            // ).send();
         };
         trade_imp(from, smm.receiver, smm.pair_id, quantity, smm.price, smm.bid_or_ask, smm.exc_acc, smm.referer);
      }
      else {
         eosio_assert(false,"invalid trade type");
      }
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
         auto price = relay::exchange::get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
         total_power += existing->supply.amount * price ;
      }

      if (total_power == 0) return ;
      for( auto it = rewardtable.cbegin(); it != rewardtable.cend(); ++it ) {
         stats statstable(_self, it->chain.value);
         auto existing = statstable.find(it->supply.symbol.raw());
         eosio_assert(existing != statstable.end(), "token with symbol already exists");
         auto price = relay::exchange::get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
         uint64_t devide_amount = quantity.amount * existing->supply.amount * price / total_power;
         statstable.modify(*existing, it->chain, [&]( auto& s ) {
            s.reward_pool += asset(devide_amount,s.reward_pool.symbol);
         });
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
         name |= (char_to_symbol(str[i]) & 0x1f) << (64 - 5 * (i + 1));
      }
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

   inline std::string trim(const std::string str) {
      auto s = str;
      s.erase(s.find_last_not_of(" ") + 1);
      s.erase(0, s.find_first_not_of(" "));
      
      return s;
   }

   static constexpr uint64_t string_to_symbol( uint8_t precision, const char* str ) {
      uint32_t len = 0;
      while( str[len] ) ++len;

      uint64_t result = 0;
      for( uint32_t i = 0; i < len; ++i ) {
         if( str[i] < 'A' || str[i] > 'Z' ) {
            /// ERRORS?
         } else {
            result |= (uint64_t(str[i]) << (8*(1+i)));
         }
      }

      result |= uint64_t(precision);
      return result;
   }

   int64_t precision(uint64_t decimals)
   {
      int64_t p10 = 1;
      int64_t p = (int64_t)decimals;
      while( p > 0  ) {
         p10 *= 10; --p;
      }
      return p10;
   }

   asset asset_from_string(const std::string& from) {
      std::string s = trim(from);
      const char * str1 = s.c_str();

      // Find space in order to split amount and symbol
      const char * pos = strchr(str1, ' ');
      eosio_assert((pos != NULL), "Asset's amount and symbol should be separated with space");
      auto space_pos = pos - str1;
      auto symbol_str = trim(s.substr(space_pos + 1));
      auto amount_str = s.substr(0, space_pos);
      eosio_assert((amount_str[0] != '-'), "now do not support negetive asset");

      // Ensure that if decimal point is used (.), decimal fraction is specified
      const char * str2 = amount_str.c_str();
      const char *dot_pos = strchr(str2, '.');
      if (dot_pos != NULL) {
         eosio_assert((dot_pos - str2 != amount_str.size() - 1), "Missing decimal fraction after decimal point");
      }
      print("------asset_from_string: symbol_str=",symbol_str, ", amount_str=",amount_str, "\n");
      // Parse symbol
      uint32_t precision_digits;
      if (dot_pos != NULL) {
         precision_digits = amount_str.size() - (dot_pos - str2 + 1);
      } else {
         precision_digits = 0;
      }

      symbol sym(string_to_symbol((uint8_t)precision_digits, symbol_str.c_str()));
      // Parse amount
      int64_t int_part, fract_part;
      if (dot_pos != NULL) {
         int_part = ::atoll(amount_str.substr(0, dot_pos - str2).c_str());
         fract_part = ::atoll(amount_str.substr(dot_pos - str2 + 1).c_str());
      } else {
         int_part = ::atoll(amount_str.c_str());
         fract_part = 0;
      }
      int64_t amount = int_part * precision(precision_digits);
      amount += fract_part;

      return asset(amount, sym);
   }

   void sys_match_match::parse(const string memo) {
      using namespace boost;

      std::vector<std::string> memoParts;
      splitMemo( memoParts, memo,  ';' );
      eosio_assert(memoParts.size() == 6,"memo is not adapted with sys_match_match");
      receiver    = name(string_to_name(memoParts[0].c_str()));
      pair_id     = (uint32_t)atoi(memoParts[1].c_str());
      price       = asset_from_string(memoParts[2]);
      bid_or_ask  = (uint32_t)atoi(memoParts[3].c_str());
      exc_acc     = name(string_to_name(memoParts[4].c_str()));
      referer     = memoParts[5];
      eosio_assert(bid_or_ask == 0 || bid_or_ask == 1,"type is not adapted with sys_match_match");
   }

   

   int64_t token::get_current_age(name chain,asset balance,int64_t first,int64_t last) {return 1;}
}
EOSIO_DISPATCH( relay::token,(create)(issue)(destroy)(transfer)(addreward)(rewardmine)(claim)(trade) )