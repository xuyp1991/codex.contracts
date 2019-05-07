#include "force.system.hpp"
// #include <relay.token/relay.token.hpp>
// #include <sys.match/sys.match.hpp>
#include <cmath>

namespace eosiosystem {
    const account_name SYS_MATCH = name("sys.match"_n);
    
    ACTION system_contract::onblock( const block_timestamp, const account_name bpname, const uint16_t, const block_id_type,
                                  const checksum256, const checksum256, const uint32_t schedule_version ) {
      bps_table bps_tbl(_self, _self.value);
      schedules_table schs_tbl(_self, _self.value);

      uint64_t block_producers[config::NUM_OF_TOP_BPS] = {};
      get_active_producers(block_producers, sizeof(account_name) * config::NUM_OF_TOP_BPS);
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      if( sch == schs_tbl.end()) {
         schs_tbl.emplace(bpname, [&]( schedule_info& s ) {
            s.version = schedule_version;
            s.block_height = current_block_num();          
            for( int i = 0; i < config::NUM_OF_TOP_BPS; i++ ) {
               s.producers[i].amount = block_producers[i] == bpname.value ? 1 : 0;
               s.producers[i].bpname = name(block_producers[i]);
            }
         });
         reset_block_weight(block_producers);
      } else {
         schs_tbl.modify(sch, _self, [&]( schedule_info& s ) {
            for( int i = 0; i < config::NUM_OF_TOP_BPS; i++ ) {
               if( s.producers[i].bpname == bpname ) {
                  s.producers[i].amount += 1;
                  break;
               }
            }
         });
      }
      if( current_block_num() % config::UPDATE_CYCLE == 0 ) {

         reward_table reward_inf(_self,_self.value);
         auto reward = reward_inf.find(REWARD_ID);
         if(reward == reward_inf.end()) {
            init_reward_info();
            reward = reward_inf.find(REWARD_ID);
         }

         int64_t block_rewards = reward->cycle_reward;

         if(current_block_num() % STABLE_BLOCK_HEIGHT == 0) {
            update_reward_stable();
         }
         else if(current_block_num() % REWARD_MODIFY_COUNT == 0) {
            reward_inf.modify(reward, _self, [&]( reward_info& s ) {
               s.cycle_reward = static_cast<int64_t>(s.cycle_reward * s.gradient / 10000);
            });
         }

         INLINE_ACTION_SENDER(force::token, issue)( config::token_account, {{config::system_account,config::active_permission}},
                                             { config::reward_account, 
                                             asset(block_rewards,CORE_SYMBOL), 
                                             "issue tokens for producer pay"} );
         
         uint64_t  vote_power = get_vote_power();
         uint64_t  coin_power = get_coin_power();
         uint64_t total_power = vote_power + coin_power;
         reward_develop(block_rewards * REWARD_DEVELOP / 10000);
         reward_block(schedule_version,block_rewards * REWARD_BP / 10000);
         if (total_power != 0) {
            reward_mines((block_rewards * REWARD_MINE / 10000) * coin_power / total_power);
            reward_bps((block_rewards * REWARD_MINE / 10000) * vote_power / total_power);
         }
         print("logs:",block_rewards,"---",block_rewards * REWARD_DEVELOP / 10000,"---",block_rewards * REWARD_BP / 10000,"---",(block_rewards * REWARD_MINE / 10000) * coin_power / total_power,"---",
         (block_rewards * REWARD_MINE / 10000) * vote_power / total_power,"\n");
         update_elected_bps();
      }
   }

   ACTION system_contract::updatebp( const account_name bpname, const public_key block_signing_key,
                                   const uint32_t commission_rate, const std::string& url ) {
      require_auth(bpname);
      eosio_assert(url.size() < 64, "url too long");
      eosio_assert(1 <= commission_rate && commission_rate <= 10000, "need 1 <= commission rate <= 10000");

      bps_table bps_tbl(_self, _self.value);
      auto bp = bps_tbl.find(bpname.value);
      if( bp == bps_tbl.end()) {
         bps_tbl.emplace(bpname, [&]( bp_info& b ) {
            b.name = bpname;
            b.update(block_signing_key, commission_rate, url);
         });
      } else {
         bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.update(block_signing_key, commission_rate, url);
         });
      }
   }

   ACTION system_contract::removebp( account_name bpname ) {
      require_auth(_self);

      bps_table bps_tbl(_self, _self.value);
      auto bp = bps_tbl.find(bpname.value);
      if( bp == bps_tbl.end()) {
        eosio_assert(false,"bpname is not registered");
      } else {
         bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.deactivate();
         });
      }
   }

   void system_contract::init_creation_bp() {
      creation_producer creation_bp_tbl(_self,_self.value);
      for (int i=0;i!=26;++i) {
         creation_bp_tbl.emplace(_self, [&]( creation_bp& b ) {
            b.bpname = CREATION_BP[i].bp_name;
            b.total_staked = CREATION_BP[i].total_staked;
            b.mortgage = CREATION_BP[i].mortgage;
         });
      }
   }

   void system_contract::update_elected_bps() {
      bps_table bps_tbl(_self, _self.value);
      
      std::vector<eosio::producer_key> vote_schedule;
      std::vector<int64_t> sorts(config::NUM_OF_TOP_BPS, 0);

      creation_producer creation_bp_tbl(_self,_self.value);
      auto create_bp = creation_bp_tbl.find(CREATION_BP[0].bp_name.value);
      if (create_bp == creation_bp_tbl.end()) {
         init_creation_bp();
      }

      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }

      int64_t reward_pre_block = reward->cycle_reward / config::UPDATE_CYCLE;
      auto block_mortgege = reward_pre_block * MORTGAGE;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         for( int i = 0; i < config::NUM_OF_TOP_BPS; ++i ) {
            auto total_shaked = it->total_staked;
            auto bp_mortgage = it->mortgage;
            if (total_shaked == 0) {
               create_bp = creation_bp_tbl.find(it->name.value);
               if (create_bp != creation_bp_tbl.end()) {
                  total_shaked = create_bp->total_staked;
                  bp_mortgage = asset{create_bp->mortgage,CORE_SYMBOL};
               }
            }
            if (it->active_type == static_cast<int32_t>(active_type::LackMortgage) && it->active_change_block_num < (current_block_num() - LACKMORTGAGE_FREEZE)) {
               bps_tbl.modify(it, _self, [&]( bp_info& b ) {
                  b.active_type = static_cast<int32_t>(active_type::Normal);
               });
            }

            if (bp_mortgage < asset(block_mortgege,CORE_SYMBOL) && is_super_bp(it->name) ) {
               bps_tbl.modify(it, _self, [&]( bp_info& b ) {
                  b.active_type = static_cast<int32_t>(active_type::LackMortgage);
                  b.active_change_block_num = current_block_num();
               });
            }
            //todo lackMortgage modify
            if( sorts[size_t(i)] <= total_shaked && it->active_type == static_cast<int32_t>(active_type::Normal)
             && bp_mortgage > asset(block_mortgege,CORE_SYMBOL)) {
               eosio::producer_key key;
               key.producer_name = it->name;
               key.block_signing_key = it->block_signing_key;
               vote_schedule.insert(vote_schedule.begin() + i, key);
               sorts.insert(sorts.begin() + i, total_shaked);
               break;
            }
         }
      }

      if( vote_schedule.size() > config::NUM_OF_TOP_BPS ) {
         vote_schedule.resize(config::NUM_OF_TOP_BPS);
      }

      std::sort(vote_schedule.begin(), vote_schedule.end());
      bytes packed_schedule = pack(vote_schedule);
      set_proposed_producers(packed_schedule.data(), packed_schedule.size());
   }

   void system_contract::reward_mines(const uint64_t reward_amount) {
      eosio::action(
         vector<eosio::permission_level>{{_self,"active"_n}},
         "relay.token"_n,
         "rewardmine"_n,
         std::make_tuple(
            asset(reward_amount,CORE_SYMBOL)
         )
      ).send();
   }

   void system_contract::reward_develop(const uint64_t reward_amount) {
      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }

      reward_inf.modify(reward, _self, [&]( reward_info& s ) {
         s.reward_develop += asset(reward_amount,CORE_SYMBOL);    
      });
   }

   // TODO it need change if no bonus to accounts

   void system_contract::reward_block(const uint32_t schedule_version,const uint64_t reward_amount ) {

      print("----------reward_block------------\n");
      schedules_table schs_tbl(_self, _self.value);
      bps_table bps_tbl(_self, _self.value);
      auto sch = schs_tbl.find(uint64_t(schedule_version));
      eosio_assert(sch != schs_tbl.end(),"cannot find schedule");
      int64_t total_block_out_age = 0;
      asset total_punish = asset(0,CORE_SYMBOL);
      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }
      auto current_block = current_block_num();
      last_drain_bp drain_bp_tbl(_self,_self.value);
      int64_t reward_pre_block = reward->cycle_reward / config::UPDATE_CYCLE;
      for( int i = 0; i < config::NUM_OF_TOP_BPS; i++ ) {
         auto bp = bps_tbl.find(sch->producers[i].bpname.value);
         eosio_assert(bp != bps_tbl.end(),"cannot find bpinfo");
         if(bp->mortgage < asset(MORTGAGE * reward_pre_block,CORE_SYMBOL)) {
            print(sch->producers[i].bpname," Insufficient mortgage, please replenish in time \n");
            continue;
         }
         auto drain_block_num = PER_CYCLE_AMOUNT + bp->last_block_amount - sch->producers[i].amount;

         bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.block_age +=  (sch->producers[i].amount > b.last_block_amount ? sch->producers[i].amount - b.last_block_amount : sch->producers[i].amount) * b.block_weight;
            total_block_out_age += (sch->producers[i].amount > b.last_block_amount ? sch->producers[i].amount - b.last_block_amount : sch->producers[i].amount) * b.block_weight;
               
            if(drain_block_num != 0) {
               b.block_weight = BLOCK_OUT_WEIGHT;
               b.mortgage -= asset(reward_pre_block * 2 * drain_block_num,CORE_SYMBOL);
               b.remain_punish += asset(reward_pre_block * 2 * drain_block_num,CORE_SYMBOL);
               b.total_drain_block += drain_block_num;
            } else {
              b.block_weight += 1;
            }
            b.last_block_amount = sch->producers[i].amount;
         });

         auto drainbp = drain_bp_tbl.find(sch->producers[i].bpname.value);
         if (drain_block_num != 0) {
            if (drainbp == drain_bp_tbl.end()) {
               drain_bp_tbl.emplace(_self, [&]( last_drain_block& s ) {
                  s.bpname = sch->producers[i].bpname;
                  s.drain_num = drain_block_num;
                  s.update_block_num = current_block;
               });
            }
            else {
               drain_bp_tbl.modify(drainbp, _self, [&]( last_drain_block& s ) {
                  s.drain_num += drain_block_num;
                  s.update_block_num = current_block;
               });
            }
         }
         else {
            if (drainbp != drain_bp_tbl.end()) {
               drain_bp_tbl.erase(drainbp);
            }
         }
      }

      reward_inf.modify(reward, _self, [&]( reward_info& s ) {
         s.reward_block_out += asset(reward_amount,CORE_SYMBOL);
         s.total_block_out_age += total_block_out_age;
      });


   }

   void system_contract::reward_bps(const uint64_t reward_amount) {
      bps_table bps_tbl(_self, _self.value);
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      if( staked_all_bps <= 0 ) {
         return;
      }
      const auto rewarding_bp_staked_threshold = staked_all_bps / 200;
      auto budget_reward = asset{0,CORE_SYMBOL};
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
      //todo active
         if( it->total_staked <= rewarding_bp_staked_threshold || it->commission_rate < 1 ||
             it->commission_rate > 10000 ) {
            continue;
         }
         auto vote_reward = static_cast<int64_t>( reward_amount  * double(it->total_staked) / double(staked_all_bps));
         if (it->active_type == static_cast<int32_t>(active_type::Punish) || it->active_type == static_cast<int32_t>(active_type::Removed)) {
            budget_reward += asset(vote_reward,CORE_SYMBOL);
            continue;
         }
         
         const auto& bp = bps_tbl.get(it->name.value, "bpname is not registered");
         bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.rewards_pool += asset(vote_reward * (10000 - b.commission_rate) / 10000,CORE_SYMBOL);
            b.rewards_block += asset(vote_reward * b.commission_rate / 10000,CORE_SYMBOL);
         });
      }
      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      if(reward == reward_inf.end()) {
         init_reward_info();
         reward = reward_inf.find(REWARD_ID);
      }
      reward_inf.modify(reward, _self, [&]( reward_info& s ) {
         s.reward_budget += budget_reward;
      });
   }

   void system_contract::init_reward_info() {
      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      if (reward == reward_inf.end()) {
         reward_inf.emplace(_self, [&]( reward_info& s ) {
            s.id = REWARD_ID;
            s.reward_block_out = asset(0,CORE_SYMBOL);
            s.reward_develop = asset(0,CORE_SYMBOL);
            s.total_block_out_age = 0;
            s.cycle_reward = PRE_BLOCK_REWARDS;
            s.gradient = PRE_GRADIENT;
         });
      }
   }

   void system_contract::update_reward_stable() {
      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      if (reward != reward_inf.end()) { 
         reward_inf.modify(reward, _self, [&]( reward_info& s ) {
            s.cycle_reward = STABLE_BLOCK_REWARDS;
            s.gradient = STABLE_GRADIENT;
         });
      }
   }

   void system_contract::reset_block_weight(uint64_t block_producers[]) {
      bps_table bps_tbl(_self, _self.value);
      for( int i = 0; i < config::NUM_OF_TOP_BPS; i++ ) {
         const auto& bp = bps_tbl.get(block_producers[i], "bpname is not registered");
         bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.block_weight = BLOCK_OUT_WEIGHT;
            b.last_block_amount = 0;
         });
      }
   }

   int64_t system_contract::get_coin_power() {
      // int64_t total_power = 0; 
      // rewards coin_reward(name("relay.token"_n),name("relay.token"_n).value);
      // exchange::exchange t(SYS_MATCH);
      // auto interval_block = exchange::INTERVAL_BLOCKS;

      // for( auto it = coin_reward.cbegin(); it != coin_reward.cend(); ++it ) {
      //    stats statstable("relay.token"_n, it->chain.value);
      //    auto existing = statstable.find(it->supply.symbol.raw());
      //    eosio_assert(existing != statstable.end(), "token with symbol already exists");
      //    auto price = t.get_avg_price(current_block_num(),existing->chain,existing->supply.symbol).amount;
      //    auto power = (existing->supply.amount / 10000) * OTHER_COIN_WEIGHT / 10000 * price;
      //    total_power += power;
      // }
      // return total_power ;
   }

   int64_t system_contract::get_vote_power() {
      bps_table bps_tbl(_self, _self.value);
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      return staked_all_bps* 10000;
   }

   ACTION system_contract::addmortgage(const account_name bpname,const account_name payer,asset quantity) {
      require_auth(payer);
      bps_table bps_tbl(_self, _self.value);
      auto bp = bps_tbl.find(bpname.value);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.mortgage += quantity;
         });

      INLINE_ACTION_SENDER(force::token, transfer)(
               config::token_account,
               { payer, config::active_permission },
               { payer, config::reward_account, quantity, "add mortgage" });
   }

   ACTION system_contract::claimmortgage(const account_name bpname,const account_name receiver,asset quantity) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self.value);
      auto bp = bps_tbl.find(bpname.value);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");
      eosio_assert(bp->mortgage < quantity,"the quantity is bigger then bp mortgage");
      
      bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.mortgage -= quantity;
         });
      
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { config::reward_account, config::active_permission },
         { config::reward_account, receiver, quantity, "claim bp mortgage" });
   }

   ACTION system_contract::claimdevelop(const account_name develop) {
      require_auth(develop);
      eosio_assert (develop == name("fosdevelop"_n),"invaild develop account");
      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      eosio_assert(reward != reward_inf.end(),"reward info do not find");

      auto reward_develop = reward->reward_develop;
      reward_inf.modify(reward, _self, [&]( reward_info& s ) {
         s.reward_develop = asset(0,CORE_SYMBOL);
      });
      eosio_assert(reward_develop > asset(100000,CORE_SYMBOL),"claim amount must > 10");
      INLINE_ACTION_SENDER(force::token, castcoin)(
         config::token_account,
         { config::reward_account, "active"_n },
         { config::reward_account, develop, reward_develop });
   }

   ACTION system_contract::claimbp(const account_name bpname,const account_name receiver) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self.value);
      auto bp = bps_tbl.find(bpname.value);
      eosio_assert(bp != bps_tbl.end(),"can not find the bp");

      reward_table reward_inf(_self,_self.value);
      auto reward = reward_inf.find(REWARD_ID);
      eosio_assert(reward != reward_inf.end(),"reward info do not find");

      asset reward_block_out = reward->reward_block_out;
      int64_t total_block_out_age = reward->total_block_out_age;
      int64_t block_age = bp->block_age;

      auto claim_block = reward_block_out * block_age/total_block_out_age;

      bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.block_age = 0;
         });

      reward_inf.modify(reward, _self, [&]( reward_info& s ) {
            s.reward_block_out -= claim_block;
            s.total_block_out_age -= block_age;
         });
      eosio_assert(claim_block > asset(100000,CORE_SYMBOL),"claim amount must > 10");
      INLINE_ACTION_SENDER(force::token, castcoin)(
         config::token_account,
         { config::reward_account, config::active_permission },
         { config::reward_account, receiver, claim_block });
   }

   ACTION system_contract::claimvote(const account_name bpname,const account_name receiver) {
      require_auth(receiver);

      bps_table bps_tbl(_self, _self.value);
      const auto& bp = bps_tbl.get(bpname.value, "bpname is not registered");

      votes_table votes_tbl(_self, receiver.value);
      const auto& vts = votes_tbl.get(bpname.value, "voter have not add votes to the the producer yet");

      const auto curr_block_num = current_block_num();
      const auto last_devide_num = curr_block_num - (curr_block_num % config::UPDATE_CYCLE);

      const auto newest_voteage =
            static_cast<int128_t>(vts.voteage + vts.vote.amount / 10000 * (last_devide_num - vts.voteage_update_height));
      const auto newest_total_voteage =
            static_cast<int128_t>(bp.total_voteage + bp.total_staked * (last_devide_num - bp.voteage_update_height));

      eosio_assert(0 < newest_total_voteage, "claim is not available yet");
      const auto amount_voteage = static_cast<int128_t>(bp.rewards_pool.amount) * newest_voteage;
      asset reward = asset(static_cast<int64_t>( amount_voteage / newest_total_voteage ),CORE_SYMBOL);
      eosio_assert(asset{} <= reward && reward <= bp.rewards_pool,
                   "need 0 <= claim reward quantity <= rewards_pool");

      auto reward_all = reward;
      if( receiver == bpname ) {
         reward_all += bp.rewards_block;
      }

      eosio_assert(reward_all> asset(100000,CORE_SYMBOL),"claim amount must > 10");
      INLINE_ACTION_SENDER(force::token, castcoin)(
            config::token_account,
            { config::reward_account, config::active_permission },
            { config::reward_account, receiver, reward_all});

      votes_tbl.modify(vts, _self, [&]( vote_info& v ) {
         v.voteage = 0;
         v.voteage_update_height = last_devide_num;
      });
      
      bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
         b.rewards_pool -= reward_all;
         b.total_voteage = static_cast<int64_t>(newest_total_voteage - newest_voteage);
         b.voteage_update_height = last_devide_num;
         if( receiver == bpname ) {
            b.rewards_block = asset(0,CORE_SYMBOL);
         }
      });
   }

   bool system_contract::is_super_bp( account_name block_producers[], account_name name ) {
      for( int i = 0; i < config::NUM_OF_TOP_BPS; i++ ) {
         if( name == block_producers[i] ) {
            return true;
         }
      }
      return false;
   }
//惩罚BP的机制
   ACTION system_contract::punishbp(const account_name initiator,const account_name bpname) {
      require_auth(initiator);
      //押金最低值的判断
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { initiator, config::active_permission },
         { initiator, config::system_account, PUNISH_BP_FEE, "punishbp deposit" });

      last_drain_bp drain_bp_tbl(_self,_self.value);
      auto drainbp = drain_bp_tbl.find(bpname.value); 
      if (drainbp == drain_bp_tbl.end()) {
         print("--------------\n");
         return ;
      }
      //插表   发起议案       BP多签进行同意    一次性可以有
      punish_bps punish_bp(_self,_self.value);
      auto bp_punish = punish_bp.find(bpname.value);
      eosio_assert(bp_punish == punish_bp.end(), "the bp is being punishing");
      //不是出块BP是否可以被惩罚有待商榷
      punish_bp.emplace(bpname,[&]( punish_bp_info& s) {
         s.bpname = bpname;
         s.initiator = initiator;
         s.drain_num = drainbp->drain_num;
         s.update_block_num = current_block_num();
      });
   }

   ACTION system_contract::canclepunish(const account_name initiator,const account_name bpname) {
      require_auth(initiator);
      //取消押金最低值的判断
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { initiator, config::active_permission },
         { initiator, config::system_account, CANCLE_PUNISH_BP_FEE, "cancle punishbp deposit" });

      punish_bps punish_bp(_self,_self.value);
      auto bp_punish = punish_bp.find(bpname.value);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");
      eosio_assert(initiator == bp_punish->initiator,"only the initiator can cancle the punish proposal");
      
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { _self, config::active_permission },
         { _self, initiator, PUNISH_BP_FEE, "cancle punishbp return deposit" });
      
      punish_bp.erase(bp_punish);

      //对于同意的人也需要进行删除
      approve_punish_bps app_punish_bp(_self,_self.value);
      auto app_punish = app_punish_bp.find(bpname.value);
      if (app_punish != app_punish_bp.end()) {
         app_punish_bp.erase(app_punish);
      }

   }
   ACTION system_contract::apppunish(const account_name bpname,const account_name punishbpname) {
      require_auth(bpname);

      //验证是否是出块BP
      eosio_assert(is_super_bp(bpname),"only super bp can approve to punish bp");
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { bpname, config::active_permission },
         { bpname, config::system_account, APPROVE_PUNISH_BP_FEE, "cancle punishbp deposit" });

      punish_bps punish_bp(_self,_self.value);
      auto bp_punish = punish_bp.find(punishbpname.value);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");

      approve_punish_bps app_punish_bp(_self,_self.value);
      auto app_punish = app_punish_bp.find(punishbpname.value);
      if (app_punish == app_punish_bp.end()) {
         app_punish_bp.emplace(bpname,[&]( approve_punish_bp& s) {
            s.bpname = bpname;
            s.approve_producer.push_back(bpname);
         });
      }
      else {
         auto iter = std::find(app_punish->approve_producer.begin(),app_punish->approve_producer.end(),bpname);
         eosio_assert(iter != app_punish->approve_producer.end(),"the bp has approved");
         app_punish_bp.modify(app_punish, _self, [&]( approve_punish_bp& s ) {
               s.approve_producer.push_back(bpname);
            });
      }

      app_punish = app_punish_bp.find(punishbpname.value);
      if (app_punish->approve_producer.size() > config::NUM_OF_TOP_BPS * 2 / 3 ) {
         //执行提议
         exec_punish_bp(punishbpname);
      }
   }

   void system_contract::exec_punish_bp(account_name punishbpname) {
      auto effective_approve = effective_approve_num(punishbpname);
      if (effective_approve < config::NUM_OF_TOP_BPS * 2 / 3) {
         print("Need confirmation from other producers to punish the bp");
         return ;
      }
      //开始惩罚
      bps_table bps_tbl(_self, _self.value);
      auto punishbp = bps_tbl.find(punishbpname.value);
      if (punishbp == bps_tbl.end()) {
         print("can not find punish bp");
         return ;
      }

      auto total_reward = punishbp->remain_punish;
      bps_tbl.modify(punishbp, _self, [&]( bp_info& b ) {
         b.remain_punish = asset{0,CORE_SYMBOL};
         b.active_type = static_cast<int32_t>(active_type::Punish);
         b.active_change_block_num = current_block_num();
      });
      //开始奖励
      auto reward_initiator = total_reward / 2;//提出人奖励一半
      //提出人奖励
      punish_bps punish_bp(_self,_self.value);
      auto bp_punish = punish_bp.find(punishbpname.value);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");
      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { config::system_account, config::active_permission },
         { config::system_account, bp_punish->initiator, reward_initiator + PUNISH_BP_FEE, "cancle punishbp deposit" });
      punish_bp.erase(bp_punish);
      //通过的BP奖励
      auto reward_approve = total_reward / 2 / effective_approve;
      approve_punish_bps app_punish_bp(_self,_self.value);
      auto app_punish = app_punish_bp.find(punishbpname.value);
      eosio_assert(app_punish != app_punish_bp.end(), "no producer approve to punish the bp");
      auto iSize = app_punish->approve_producer.size();
      for(int i=0;i!=iSize;++i) {
         if (is_super_bp(app_punish->approve_producer[i])) {
            INLINE_ACTION_SENDER(force::token, transfer)(
               config::token_account,
               { config::system_account, config::active_permission },
               { config::system_account, app_punish->approve_producer[i], reward_approve + APPROVE_PUNISH_BP_FEE, "cancle punishbp deposit" });
         }
      }
      app_punish_bp.erase(app_punish);
   }

   ACTION system_contract::unapppunish(const account_name bpname,const account_name punishbpname) {
      require_auth(bpname);
     
      approve_punish_bps app_punish_bp(_self,_self.value);
      auto app_punish = app_punish_bp.find(punishbpname.value);
      eosio_assert(app_punish != app_punish_bp.end(), "can find the approve punish record");
      app_punish_bp.erase(app_punish);
   }

   ACTION system_contract::bailpunish(const account_name punishbpname) {
      require_auth(punishbpname);

      INLINE_ACTION_SENDER(force::token, transfer)(
         config::token_account,
         { punishbpname, config::active_permission },
         { punishbpname, config::system_account, BAIL_PUNISH_FEE, "bail punish fee" });

      bps_table bps_tbl(_self, _self.value);
      auto punishbp = bps_tbl.find(punishbpname.value);
      eosio_assert(punishbp != bps_tbl.end(),"can not find bp");

      if (punishbp->active_type == static_cast<int32_t>(active_type::Punish) 
         && punishbp->active_change_block_num + config::UPDATE_CYCLE*CYCLE_PREDAY*2 < current_block_num()) {
         bps_tbl.modify(punishbp, _self, [&]( bp_info& b ) {
            b.active_type = static_cast<int32_t>(active_type::Normal);
         });
      }
      else {
         eosio_assert(false,"the bp can not to be bailed");
      }
   }

   bool system_contract::is_super_bp( account_name bpname) {
      schedules_table schs_tbl(_self, _self.value);
      auto producer_schs = schs_tbl.crbegin();
      int32_t i = 0;
      for ( i = 0;i!= config::NUM_OF_TOP_BPS; ++i) {
         if (producer_schs->producers[i].bpname == bpname) {
            return true;
         }
      }
      return false;
   }

   int32_t system_contract::effective_approve_num(account_name punishbpname) {
      punish_bps punish_bp(_self,_self.value);
      auto bp_punish = punish_bp.find(punishbpname.value);
      eosio_assert(bp_punish != punish_bp.end(), "the bp is not be punish");

      approve_punish_bps app_punish_bp(_self,_self.value);
      auto app_punish = app_punish_bp.find(punishbpname.value);
      eosio_assert(app_punish != app_punish_bp.end(), "no producer approve to punish the bp");

      schedules_table schs_tbl(_self, _self.value);
      auto producer_schs = schs_tbl.crbegin();
      int32_t i = 0,j = 0,result = 0;
      for (i=0;i!=app_punish->approve_producer.size();++i) {
         for (j=0;j!=config::NUM_OF_TOP_BPS;++j) {
            if (producer_schs->producers[j].bpname == app_punish->approve_producer[i]) {
               result++;
               break;
            }
         }
      }

      return result;
   } 
}
