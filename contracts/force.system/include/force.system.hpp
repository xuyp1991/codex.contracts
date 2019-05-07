#pragma once


#include <eosiolib/time.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/action.hpp>
#include <eosiolib/privileged.hpp>
#include <../../codexlib/config.hpp>

namespace eosiosystem {
   using eosio::asset;
   using eosio::print;
   using eosio::block_timestamp;
   using std::string;
   using eosio::permission_level;
   using std::vector;


#ifdef BEFORE_ONLINE_TEST   
   static constexpr uint32_t CYCLE_PREHOUR = 10;
   static constexpr uint32_t CYCLE_PREDAY = 50;//5;//275;
   static constexpr uint32_t STABLE_DAY = 10;//2;//60;
   static constexpr uint64_t PRE_BLOCK_REWARDS = 58.6*10000;
   static constexpr uint64_t STABLE_BLOCK_REWARDS = 126*10000;
#else
   static constexpr uint32_t CYCLE_PREHOUR = 12;
   static constexpr uint32_t CYCLE_PREDAY = 275;//5;//275;
   static constexpr uint32_t STABLE_DAY = 60;//2;//60;
   static constexpr uint64_t STABLE_BLOCK_REWARDS = 630*10000;
   static constexpr uint64_t PRE_BLOCK_REWARDS = 143*10000;
#endif
   static constexpr uint32_t STABLE_BLOCK_HEIGHT = config::UPDATE_CYCLE * CYCLE_PREDAY * STABLE_DAY;
   static constexpr uint32_t PRE_GRADIENT = 10250;
   static constexpr uint32_t STABLE_GRADIENT = 10010;
   static constexpr uint32_t REWARD_MODIFY_COUNT = config::UPDATE_CYCLE * CYCLE_PREDAY;

   static constexpr uint64_t REWARD_ID = 1;
   static constexpr uint64_t BLOCK_OUT_WEIGHT = 1000;
   static constexpr uint64_t MORTGAGE = 8228;
   static constexpr uint32_t PER_CYCLE_AMOUNT = config::UPDATE_CYCLE / config::NUM_OF_TOP_BPS; 

   static constexpr uint32_t REWARD_DEVELOP = 900;
   static constexpr uint32_t REWARD_BP = 300;
   static constexpr uint32_t REWARD_FUND = 100;
   static constexpr uint32_t REWARD_MINE = 10000 - REWARD_DEVELOP - REWARD_BP;

   #define LACKMORTGAGE_FREEZE config::UPDATE_CYCLE * CYCLE_PREHOUR
   #define PUNISH_BP_FEE   asset{100*10000,CORE_SYMBOL}
   #define CANCLE_PUNISH_BP_FEE  asset{10*10000,CORE_SYMBOL}
   #define APPROVE_PUNISH_BP_FEE   asset{10*10000,CORE_SYMBOL}
   #define UNAPPROVE_PUNISH_BP_FEE   asset{10*10000,CORE_SYMBOL}
   #define BAIL_PUNISH_FEE   asset{10*10000,CORE_SYMBOL}

   struct creation_producer {
      account_name bp_name;
      int64_t      total_staked    = 0;
      int64_t      mortgage = 0;
   };

   static constexpr creation_producer CREATION_BP[26] = {
      {name("biosbpa"_n),400000,40000*10000},
      {name("biosbpb"_n),400000,40000*10000},
      {name("biosbpc"_n),400000,40000*10000},
      {name("biosbpd"_n),400000,40000*10000},
      {name("biosbpe"_n),600000,40000*10000},
      {name("biosbpf"_n),600000,40000*10000},
      {name("biosbpg"_n),600000,40000*10000},
      {name("biosbph"_n),600000,40000*10000},
      {name("biosbpi"_n),1300000,40000*10000},
      {name("biosbpj"_n),1300000,40000*10000},
      {name("biosbpk"_n),1300000,40000*10000},
      {name("biosbpl"_n),2100000,40000*10000},
      {name("biosbpm"_n),2100000,40000*10000},
      {name("biosbpn"_n),10000000,40000*10000},
      {name("biosbpo"_n),10000000,40000*10000},
      {name("biosbpp"_n),10000000,40000*10000},
      {name("biosbpq"_n),10000000,40000*10000},
      {name("biosbpr"_n),10000000,40000*10000},
      {name("biosbps"_n),10000000,40000*10000},
      {name("biosbpt"_n),10000000,40000*10000},
      {name("biosbpu"_n),10000000,40000*10000},
      {name("biosbpv"_n),100000,40000*10000},
      {name("biosbpw"_n),100000,40000*10000},
      {name("biosbpx"_n),100000,40000*10000},
      {name("biosbpy"_n),100000,40000*10000},
      {name("biosbpz"_n),100000,40000*10000}
   };

   struct permission_level_weight {
      permission_level  permission;
      weight_type       weight;
   };

   struct key_weight {
      public_key key;
      weight_type     weight;
   };

   struct wait_weight {
      uint32_t     wait_sec;
      weight_type  weight;
   };

   struct authority {
      uint32_t                          threshold = 0;
      vector<key_weight>                keys;
      vector<permission_level_weight>   accounts;
      vector<wait_weight>               waits;
   };

   enum  class active_type:int32_t {
      Normal=0,
      Punish,
      LackMortgage,
      Removed
   };

   class [[eosio::contract("force.system")]] system_contract : public contract {
      public:
         using contract::contract;
      private:
         TABLE vote_info {
            asset        vote                  = asset{0,CORE_SYMBOL};
            account_name bpname;
            int64_t      voteage               = 0;         // asset.amount * block height
            uint32_t     voteage_update_height = 0;

            uint64_t primary_key() const { return bpname.value; }

            EOSLIB_SERIALIZE(vote_info, (bpname)(vote)(voteage)(voteage_update_height))
         };

         TABLE votes_info {
            account_name                owner; /// the voter
            std::vector<account_name>   producers; /// the producers approved by this voter
            asset                       staked;    /// the staked to this producers

            uint64_t primary_key()const { return owner.value; }

            EOSLIB_SERIALIZE( votes_info, (owner)(producers)(staked) )
         };

         TABLE freeze_info {
            asset        staked         = asset{0,CORE_SYMBOL};
            asset        unstaking      = asset{0,CORE_SYMBOL};
            account_name voter;
            uint32_t     unstake_height = 0;

            uint64_t primary_key() const { return voter.value; }

            EOSLIB_SERIALIZE(freeze_info, (staked)(unstaking)(voter)(unstake_height))
         };

         TABLE vote4ram_info {
            account_name voter;
            asset        staked = asset{0,CORE_SYMBOL};

            uint64_t primary_key() const { return voter.value; }

            EOSLIB_SERIALIZE(vote4ram_info, (voter)(staked))
         };

         TABLE bp_info {
            account_name name;
            public_key   block_signing_key;
            uint32_t     commission_rate = 0; // 0 - 10000 for 0% - 100%
            int64_t      total_staked    = 0;
            asset        rewards_pool    = asset{0,CORE_SYMBOL};
            asset        rewards_block   = asset{0,CORE_SYMBOL};
            int64_t      total_voteage   = 0; // asset.amount * block height
            uint32_t     voteage_update_height = current_block_num();
            std::string  url;
            bool emergency = false;
            int32_t active_type = 0;

            int64_t      block_age = 0;
            uint32_t     last_block_amount = 0;
            int64_t      block_weight = BLOCK_OUT_WEIGHT;   
            asset        mortgage = asset{0,CORE_SYMBOL};

            int32_t     total_drain_block = 0;
            asset       remain_punish = asset{0,CORE_SYMBOL};
            int32_t     active_change_block_num = 0;

            uint64_t primary_key() const { return name.value; }

            void update( public_key key, uint32_t rate, std::string u ) {
               block_signing_key = key;
               commission_rate = rate;
               url = u;
            }
            void     deactivate()       {active_type = static_cast<int32_t>(active_type::Removed);}
            bool     isactive() const {
               if (active_type == static_cast<int32_t>(active_type::Removed)) return false;
               return true;
            }
            EOSLIB_SERIALIZE(bp_info, ( name )(block_signing_key)(commission_rate)(total_staked)
                  (rewards_pool)(rewards_block)(total_voteage)(voteage_update_height)(url)(emergency)(active_type)
                  (block_age)(last_block_amount)(block_weight)(mortgage)(total_drain_block)(remain_punish)(active_change_block_num))
         };

         TABLE producer {
            account_name bpname;
            uint32_t amount = 0;
         };

         TABLE schedule_info {
            uint64_t version;
            uint32_t block_height;
            std::vector<producer>  producers;
            uint64_t primary_key() const { return version; }

            EOSLIB_SERIALIZE(schedule_info, ( version )(block_height)(producers))
         };

         TABLE reward_info {
            uint64_t     id;
            asset reward_block_out = asset{0,CORE_SYMBOL};
            asset reward_develop = asset{0,CORE_SYMBOL};
            int64_t total_block_out_age = 0;
            int64_t cycle_reward = 0;
            int32_t   gradient = 0;

            uint64_t primary_key() const { return id; }
            EOSLIB_SERIALIZE(reward_info, ( id )(reward_block_out)(reward_develop)(total_block_out_age)(cycle_reward)(gradient))
         };

         // /** from relay.token begin*/
         inline static uint128_t get_account_idx(const eosio::name& chain, const asset& a) {
            return (uint128_t(uint64_t(chain.value)) << 64) + uint128_t(a.symbol.raw());
         }

         TABLE currency_stats {
            asset        supply;
            asset        max_supply;
            account_name issuer;
            eosio::name         chain;

            asset        reward_pool;
            int64_t      total_mineage               = 0;         // asset.amount * block height
            uint32_t     total_mineage_update_height = 0;

            uint64_t primary_key() const { return supply.symbol.raw(); }
         };
         TABLE reward_currency {
            uint64_t     id;
            eosio::name         chain;
            asset        supply;

            uint64_t primary_key() const { return id; }
            uint128_t get_index_i128() const { return get_account_idx(chain, supply); }
         };

         TABLE creation_bp {
            account_name bpname;
            int64_t      total_staked    = 0;
            int64_t      mortgage = 0;
            uint64_t primary_key() const { return bpname.value; }

            EOSLIB_SERIALIZE(creation_bp, (bpname)(total_staked)(mortgage))
         };

         TABLE punish_bp_info {
            account_name initiator;
            account_name bpname;
            int32_t  drain_num = 0;
            int32_t  update_block_num = 0;
            uint64_t primary_key() const { return bpname.value; }

            EOSLIB_SERIALIZE(punish_bp_info, (initiator)(bpname)(drain_num)(update_block_num))
         };

         TABLE last_drain_block {
            account_name bpname;
            int32_t  drain_num = 0;
            int32_t  update_block_num = 0;
            uint64_t primary_key() const { return bpname.value; }

            EOSLIB_SERIALIZE(last_drain_block, (bpname)(drain_num)(update_block_num))
         };

         TABLE approve_punish_bp {
            account_name bpname;
            vector<account_name> approve_producer;
            uint64_t primary_key() const { return bpname.value; }
            EOSLIB_SERIALIZE(approve_punish_bp, (bpname)(approve_producer))
         };

         typedef eosio::multi_index<"stat"_n, currency_stats> stats;
         typedef eosio::multi_index<"rewardcoin"_n, reward_currency,
            eosio::indexed_by< "bychain"_n,
                        eosio::const_mem_fun<reward_currency, uint128_t, &reward_currency::get_index_i128 >>> rewards;
         typedef eosio::multi_index<"freezed"_n,     freeze_info>   freeze_table;
         typedef eosio::multi_index<"votes"_n,       vote_info>     votes_table;
         typedef eosio::multi_index<"mvotes"_n,      votes_info>    mvotes_table;
         typedef eosio::multi_index<"votes4ram"_n,   vote_info>     votes4ram_table;
         typedef eosio::multi_index<"vote4ramsum"_n, vote4ram_info> vote4ramsum_table;
         typedef eosio::multi_index<"bps"_n,         bp_info>       bps_table;
         typedef eosio::multi_index<"schedules"_n,   schedule_info> schedules_table;
         typedef eosio::multi_index<"reward"_n,   reward_info> reward_table;
         typedef eosio::multi_index<"creationbp"_n,   creation_bp> creation_producer;
         typedef eosio::multi_index<"lastdrainbp"_n,   last_drain_block> last_drain_bp;
         typedef eosio::multi_index<"punishbps"_n,   punish_bp_info> punish_bps;
         typedef eosio::multi_index<"apppunishbps"_n,   approve_punish_bp> approve_punish_bps;

         void init_creation_bp();
         void update_elected_bps();

         void reward_bps(const uint64_t reward_amount);
         void reward_block(const uint32_t schedule_version,const uint64_t reward_amount);
         void reward_mines(const uint64_t reward_amount);
         void reward_develop(const uint64_t reward_amount);

         bool is_super_bp( account_name block_producers[], account_name name );
         bool is_super_bp( account_name bpname);
         int32_t effective_approve_num(account_name punishbpname); 
         void exec_punish_bp(account_name punishbpname);

         //defind in delegate_bandwidth.cpp
         void changebw( account_name from, account_name receiver,
                        asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );

         void update_votes( const account_name voter, const std::vector<account_name>& producers, bool voting );

         void reset_block_weight(uint64_t block_producers[]);
         int64_t get_coin_power();
         int64_t get_vote_power();

         void init_reward_info();
         void update_reward_stable();
      public:
         // @abi action
         ACTION updatebp( const account_name bpname, const public_key producer_key,
                        const uint32_t commission_rate, const std::string& url );
         // @abi action
         ACTION freeze( const account_name voter, const asset stake );
         // @abi action
         ACTION unfreeze( const account_name voter );
         // @abi action
         ACTION vote( const account_name voter, const account_name bpname, const asset stake );
         // @abi action
         ACTION vote4ram( const account_name voter, const account_name bpname, const asset stake );
         // @abi action
         ACTION onblock( const block_timestamp, const account_name bpname, const uint16_t,
                     const block_id_type, const checksum256, const checksum256, const uint32_t schedule_version );
         // @abi action
         ACTION setparams( const eosio::blockchain_parameters& params );
         // @abi action
         ACTION removebp( account_name producer );
         // @abi action
         ACTION newaccount(account_name creator,account_name name,authority owner,authority active);
         // @abi action
         ACTION updateauth(account_name account,permission_name permission,permission_name parent,authority auth);
         // @abi action
         ACTION deleteauth(account_name account,permission_level permission);
         // @abi action
         ACTION linkauth(account_name account,account_name code,action_name type,permission_level requirement);
         // @abi action
         ACTION unlinkauth(account_name account,account_name code,action_name type);
         // @abi action
         ACTION canceldelay(permission_level canceling_auth,checksum256 trx_id);
         // @abi action
         ACTION onerror(uint128_t sender_id,bytes sent_trx);
         // @abi action
         ACTION setconfig(account_name typ,int64_t num,account_name key,asset fee);
         // @abi action
         ACTION setcode(account_name account,uint8_t vmtype,uint8_t vmversion,bytes code);
         // @abi action
         ACTION setfee(account_name account,action_name action,asset fee,uint32_t cpu_limit,uint32_t net_limit,uint32_t ram_limit);
         // @abi action
         ACTION setabi(account_name account,bytes abi);
         // @abi action
         ACTION addmortgage(const account_name bpname,const account_name payer,asset quantity);
         // @abi action
         ACTION claimmortgage(const account_name bpname,const account_name receiver,asset quantity);
         // @abi action
         ACTION claimvote(const account_name bpname,const account_name receiver);
         // @abi action
         ACTION claimbp(const account_name bpname,const account_name receiver);
         // @abi action
         ACTION claimdevelop(const account_name develop);
   //#if CONTRACT_RESOURCE_MODEL == RESOURCE_MODEL_DELEGATE
         // @abi action
         ACTION delegatebw( account_name from, account_name receiver,
                        asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );
         // @abi action
         ACTION undelegatebw( account_name from, account_name receiver,
                        asset unstake_net_quantity, asset unstake_cpu_quantity );
         // @abi action
         ACTION refund( account_name owner );
         // @abi action
         ACTION voteproducer( const account_name voter, const std::vector<account_name>& producers );
         // @abi action
         ACTION fee( const account_name payer, const account_name bpname, int64_t voteage );
         // @abi action
         ACTION punishbp(const account_name initiator,const account_name bpname);
         // @abi action
         ACTION canclepunish(const account_name initiator,const account_name bpname);
         // @abi action
         ACTION apppunish(const account_name bpname,const account_name punishbpname);
         // @abi action
         ACTION unapppunish(const account_name bpname,const account_name punishbpname);
         // @abi action
         ACTION bailpunish(const account_name bpname);

         using updatebp_action = action_wrapper<"updatebp"_n, &system_contract::updatebp>;
         using freeze_action = action_wrapper<"freeze"_n, &system_contract::freeze>;
         using unfreeze_action = action_wrapper<"unfreeze"_n, &system_contract::unfreeze>;
         using vote_action = action_wrapper<"vote"_n, &system_contract::vote>;
         using vote4ram_action = action_wrapper<"vote4ram"_n, &system_contract::vote4ram>;
         using voteproducer_action = action_wrapper<"voteproducer"_n, &system_contract::voteproducer>;
         using fee_action = action_wrapper<"fee"_n, &system_contract::fee>;
         using onblock_action = action_wrapper<"onblock"_n, &system_contract::onblock>;
         using setparams_action = action_wrapper<"setparams"_n, &system_contract::setparams>;
         using removebp_action = action_wrapper<"removebp"_n, &system_contract::removebp>;
         using newaccount_action = action_wrapper<"newaccount"_n, &system_contract::newaccount>;
         using updateauth_action = action_wrapper<"updateauth"_n, &system_contract::updateauth>;
         using deleteauth_action = action_wrapper<"deleteauth"_n, &system_contract::deleteauth>;
         using linkauth_action = action_wrapper<"linkauth"_n, &system_contract::linkauth>;
         using unlinkauth_action = action_wrapper<"unlinkauth"_n, &system_contract::unlinkauth>;
         using canceldelay_action = action_wrapper<"canceldelay"_n, &system_contract::canceldelay>;
         using onerror_action = action_wrapper<"onerror"_n, &system_contract::onerror>;
         using addmortgage_action = action_wrapper<"addmortgage"_n, &system_contract::addmortgage>;
         using claimmortgage_action = action_wrapper<"claimortgage"_n, &system_contract::claimmortgage>;
         using claimbp_action = action_wrapper<"claimbp"_n, &system_contract::claimbp>;
         using claimvote_action = action_wrapper<"claimvote"_n, &system_contract::claimvote>;
         using claimdevelop_action = action_wrapper<"claimdevelop"_n, &system_contract::claimdevelop>;
         using setconfig_action = action_wrapper<"setconfig"_n, &system_contract::setconfig>;
         using setcode_action = action_wrapper<"setcode"_n, &system_contract::setcode>;
         using setfee_action = action_wrapper<"setfee"_n, &system_contract::setfee>;
         using setabi_action = action_wrapper<"setabi"_n, &system_contract::setabi>;
         using delegatebw_action = action_wrapper<"delegatebw"_n, &system_contract::delegatebw>;
         using undelegatebw_action = action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;
         using refund_action = action_wrapper<"refund"_n, &system_contract::refund>;
         using punishbp_action = action_wrapper<"punishbp"_n, &system_contract::punishbp>;
         using canclepunish_action = action_wrapper<"canclepunish"_n, &system_contract::canclepunish>;
         using apppunish_action = action_wrapper<"apppunish"_n, &system_contract::apppunish>;
         using unapppunish_action = action_wrapper<"unapppunish"_n, &system_contract::unapppunish>;
         
   };

}