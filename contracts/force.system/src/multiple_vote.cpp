#include "force.system.hpp"

namespace eosiosystem {
   void system_contract::voteproducer( const account_name voter_name, const std::vector<account_name>& producers ) {
      require_auth(voter_name);
      update_votes(voter_name, producers, true);
   }

   struct producer_delta {
      asset delta = asset{};
      bool is_old = false;
   };

   void system_contract::update_votes( const account_name voter_name, const std::vector<account_name>& producers, bool voting ) {
      //validate input
      eosio_assert(producers.size() <= 30, "attempt to vote for too many producers");
      for( size_t i = 1; i < producers.size(); ++i ) {
         eosio_assert(producers[i - 1] < producers[i], "producer votes must be unique and sorted");
      }

      freeze_table freeze_tbl(_self, _self.value);
      auto fts = freeze_tbl.find(voter_name.value);
      eosio_assert(fts != freeze_tbl.end(), "voter need freeze first");
      const auto staked = fts->staked;

      mvotes_table _voters(_self,_self.value);
      auto voter = _voters.find(voter_name.value);
      boost::container::flat_map<account_name, producer_delta> producer_deltas;

      if( voter == _voters.end() ) {
         _voters.emplace(voter_name, [&]( votes_info& v ) {
            v.owner = voter_name;
            v.producers = producers;
            v.staked = staked;
         });
      } else {
         // fisrt, mark all old producers
         for( const auto& p : voter->producers ) {
            auto& d = producer_deltas[p];
            d.delta -= voter->staked;
            d.is_old = true;
         }

         _voters.modify(voter, _self, [&]( auto& av ) {
            av.producers = producers;
            av.staked = staked;
         });
      }

      // second, mark all new producers
      for( const auto& p : producers ) {
         auto& d = producer_deltas[p];
         d.delta += staked;
      }

      bps_table bps_tbl(_self, _self.value);
      votes_table votes_tbl(_self, voter_name.value);

      const auto curr_block_num = current_block_num();

      // apply all changes
      for( const auto& pd : producer_deltas ) {
         print("deltas ", pd.first, " ", pd.second.delta, " - ", pd.second.is_old, " \n");
         if( pd.second.delta == asset{} ) {
            continue;
         }

         auto bp = bps_tbl.find(pd.first.value);
         eosio_assert(bp != bps_tbl.end(), "bp is not found!");

         const auto change = pd.second.delta;
         auto vts = votes_tbl.find(pd.first.value);
         if( vts == votes_tbl.end() ) {
            // First vote the bp, it will cause ram add
            eosio_assert(change > asset{}, "for no vote bp, cannot del vote");
            votes_tbl.emplace(voter_name, [&]( vote_info& v ) {
               v.bpname = pd.first;
               v.vote = change;
               v.voteage_update_height = curr_block_num;
            });
         } else {
            eosio_assert((vts->vote + change) >= asset{}, "canot change vote < 0");
            votes_tbl.modify(vts, _self, [&]( vote_info& v ) {
               v.voteage += (v.vote.amount / 10000) * (curr_block_num - v.voteage_update_height);
               v.voteage_update_height = curr_block_num;
               v.vote += change;
            });
         }

         eosio_assert(bp->isactive() || (!bp->isactive() && change < asset{ 0,CORE_SYMBOL }), "bp is not active");

         bps_tbl.modify(bp, _self, [&]( bp_info& b ) {
            b.total_voteage += b.total_staked * (curr_block_num - b.voteage_update_height);
            b.voteage_update_height = curr_block_num;
            b.total_staked += (change.amount / 10000);
         });
      }
   }
}
