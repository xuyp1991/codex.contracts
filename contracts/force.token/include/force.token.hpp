#include <../../codexlib/config.hpp>

enum  class func_type:uint64_t {
   match=1,
   bridge_addmortgage,
   bridge_exchange,
   trade_type_count
};

namespace force {
   using std::string;
   static constexpr uint32_t PRE_CAST_NUM = 5184000;
   static constexpr uint32_t STABLE_CAST_NUM = 1209600;
   static constexpr double WEAKEN_CAST_NUM = 2.5;

   struct sys_bridge_addmort {
      name trade_name;
      account_name trade_maker;
      uint64_t type;
      void parse(const string memo);
   };

   struct sys_bridge_exchange {
      name trade_name;
      account_name trade_maker;
      account_name recv;
      uint64_t type;
      void parse(const string memo);
   };
   //CONTRACT token : public contract {
   class [[eosio::contract("force.token")]] token : public contract {
      public:
         using contract::contract;

         ACTION create( account_name issuer,
                     asset        maximum_supply);

         ACTION issue( account_name to, asset quantity, string memo );

         ACTION transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         ACTION fee( account_name payer, asset quantity );
      
      
         inline asset get_supply( symbol_code sym )const;
         
         inline asset get_balance( account_name owner, symbol_code sym )const;

         ACTION trade( account_name   from,
                     account_name   to,
                     asset          quantity,
                     uint64_t      type,
                     string           memo);
         ACTION castcoin(account_name from,account_name to,asset quantity);
         ACTION takecoin(account_name to);

         using create_action = action_wrapper<"create"_n, &token::create>;
         using issue_action = action_wrapper<"issue"_n, &token::issue>;
         using transfer_action = action_wrapper<"transfer"_n, &token::transfer>;
         using fee_action = action_wrapper<"fee"_n, &token::fee>;
         using trade_action = action_wrapper<"trade"_n, &token::trade>;
         using castcoin_action = action_wrapper<"castcoin"_n, &token::castcoin>;
         using takecoin_action = action_wrapper<"takecoin"_n, &token::takecoin>;
      private:

         TABLE account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.raw(); }
         };

         TABLE currency_stats {
            asset          supply;
            asset          max_supply;
            account_name   issuer;

            uint64_t primary_key()const { return supply.symbol.raw(); }
         };

         TABLE coin_cast {
            asset    balance ;
            uint32_t   finish_block = 0;

            uint64_t primary_key()const { return static_cast<uint64_t>(finish_block); }
         };

         typedef eosio::multi_index<"accounts"_n, account> accounts;
         typedef eosio::multi_index<"stat"_n, currency_stats> stats;
         typedef eosio::multi_index<"coincast"_n, coin_cast> coincasts;

         void sub_balance( account_name owner, asset value );
         void add_balance( account_name owner, asset value, account_name ram_payer );
   };

}