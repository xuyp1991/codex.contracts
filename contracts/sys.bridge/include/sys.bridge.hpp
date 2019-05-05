#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
using namespace eosio;
namespace relay{
   enum  class trade_type:int64_t {
      equal_ratio=1,
      bancor,
      trade_type_count
   };

   enum  class fee_type:int64_t {
      fixed=1,                //fixed
      proportion,             // proportion
      proportion_mincost,      // proportion and have a Minimum cost
      fee_type_count
   };

   enum  class coin_type:int64_t {
      coin_base=1,
      coin_market,
      trade_type_count
   };
   const int64_t PROPORTION_CARD = 10000; 
   //const name TOKEN = N(relay.token);
   // CONTRACT bridge : public contract {
   class [[eosio::contract("sys.bridge")]] bridge : public contract {
      public:
         using contract::contract;

         /**
          * add a trade market 
          * trade : the name of the trade market
          * trade_maker : the account make the trade market
          * type : the trade type 1 for Fixed ratio  2 for bancor(Not yet implemented)
          * base_amount : the base coin amount 
          * base_account : the account to recv the basecoin when you claim from the trade market (when you add a trade market you should pay some base coin from this account)
          * base_weight : the base coin weight to calculate exchange rate
          * market_amount : the market coin amount 
          * market_account : the account to recv the marketcoin when you claim from the trade market (when you add a trade market you should pay some market coin from this account)
          * market_weight : the market coin weight to calculate exchange rate
          */
         ACTION addmarket(name trade,name trade_maker,int64_t type,name base_chain,asset base_amount,uint64_t base_weight,
               name market_chain,asset market_amount,uint64_t market_weight);
         /**
          * add mortgage
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          * recharge_account : the account to pay the coin 
          * recharge_amount ： the amount you add mortgage
          * type : to distinguish the base coin or the market coin 1 for base coin 2 for market coin
          */
         ACTION addmortgage(name trade,name trade_maker,name recharge_account,name coin_chain,asset recharge_amount,int64_t type);
         /**
          * claim mortgage
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          * claim_amount ： the amount you claim the mortgage
          * type : to distinguish the base coin or the market coin 1 for base coin 2 for market coin
          */
         ACTION claimmortgage(name trade,name trade_maker,name recv_account,asset claim_amount,int64_t type);
         /**
          * exchange the client use this function for exchange two coins
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          * account_covert ： the account to pay the coin
          * account_recv : the account to recv the coin
          * amount : the amount you pay the coin 
          * type : to distinguish the exchange 1 for you pay basecoin and recv the market coin  AND 2 for you pay the market coin and recv the base coin
          */
         ACTION exchange(name trade,name trade_maker,name account_covert,name account_recv,name coin_chain,asset amount,int64_t type);
         /**
          * forzen market : for the trade maker to frozen the trade market . if the market is frozened it could not use exchage function
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          */
         ACTION frozenmarket(name trade,name trade_maker);
         /**
          * thaw market : for the trade maker to thaw the trade market 
          * trade : the name of the trade market
          * trade_maker : the account make the trade market 
          */
         ACTION trawmarket(name trade,name trade_maker);
         /**
          * set fixed fee
          */
         ACTION setfixedfee(name trade,name trade_maker,asset base,asset market);
         /**
          * set proportion fee
          */
         ACTION setprofee(name trade,name trade_maker,uint64_t base_ratio,uint64_t market_ratio);
         /**
          * set proportion with a Minimum fee
          */
         ACTION setprominfee(name trade,name trade_maker,uint64_t base_ratio,uint64_t market_ratio,asset base,asset market);

         ACTION setweight(name trade,name trade_maker,uint64_t  base_weight,uint64_t  market_weight);

         ACTION settranscon(name chain,asset quantity,name contract_name);

         ACTION removemarket(name trade,name trade_maker,name base_recv,name maker_recv);

         ACTION claimfee(name trade,name trade_maker,name recv_account,int64_t type);
         
         using addmarket_action = action_wrapper<"addmarket"_n, &bridge::addmarket>;
         using addmortgage_action = action_wrapper<"addmortgage"_n, &bridge::addmortgage>;
         using claimmortgage_action = action_wrapper<"claimmortgage"_n, &bridge::claimmortgage>;
         using exchange_action = action_wrapper<"exchange"_n, &bridge::exchange>;
         using frozenmarket_action = action_wrapper<"frozenmarket"_n, &bridge::frozenmarket>;
         using trawmarket_action = action_wrapper<"trawmarket"_n, &bridge::trawmarket>;
         using setfixedfee_action = action_wrapper<"setfixedfee"_n, &bridge::setfixedfee>;
         using setprofee_action = action_wrapper<"setprofee"_n, &bridge::setprofee>;
         using setprominfee_action = action_wrapper<"setprominfee"_n, &bridge::setprominfee>;
         using setweight_action = action_wrapper<"setweight"_n, &bridge::setweight>;
         using settranscon_action = action_wrapper<"settranscon"_n, &bridge::settranscon>;
         using removemarket_action = action_wrapper<"removemarket"_n, &bridge::removemarket>;
         using claimfee_action = action_wrapper<"claimfee"_n, &bridge::claimfee>;
      private:
         void send_transfer_action(name chain,name recv,asset quantity,std::string memo);
         inline static uint128_t get_contract_idx(const name& chain, const asset  &a) {
            return (uint128_t(uint64_t(chain.value)) << 64) + uint128_t(a.symbol.raw());
         }
         
         TABLE trans_contract {
            uint64_t     id;
            name chain;
            asset  quantity;
            name contract_name;

            uint64_t primary_key()const { return id; }
            uint128_t get_index_i128() const { return get_contract_idx(chain, quantity); }
         };
            //fixed cost      think about the Proportionate fee
         struct trade_fee {
            asset base;             //fixed
            asset market;
            
            uint64_t    base_ratio;    //Proportionate
            uint64_t    market_ratio;

            int64_t  fee_type;
            
         };

         struct coin {
            name chain;                //the name of chain
            asset  amount;             //the coin amont
            uint64_t  weight;          //coin_base weight 
            asset fee_amount;          //the fee of the coin
         };

         TABLE trade_pair {          
            name trade_name;          //the name of the trade market
            int64_t type;          // the trade type 1 for Fixed ratio  2 for bancor(Not yet implemented)
            coin  base;               //the base coin 
            coin  market;             //the market coin
            name   trade_maker;//the account to pay for the trade market
            bool  isactive =true;      //the sattus of the trade market when isactive is false ,it can exchange
            trade_fee fee;

            uint64_t primary_key()const { return trade_name.value; }
         };

         typedef eosio::multi_index<"tradepairs"_n, trade_pair> tradepairs;
         typedef eosio::multi_index<"transcon"_n, trans_contract, eosio::indexed_by<"bychain"_n, eosio::const_mem_fun<trans_contract, uint128_t, &trans_contract::get_index_i128>>> transcon;
   };
}
