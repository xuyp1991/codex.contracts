#include <force.system.hpp>
#include "../../force.token/include/force.token.hpp"
#include "producer.cpp"
#include "native.cpp"
#include "multiple_vote.cpp"
#include "voting.cpp"
#include "vote4ram.cpp"
#include "delegate_bandwidth.cpp"

namespace eosiosystem {
   ACTION system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( _self );
      eosio_assert( 3 <= params.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   ACTION system_contract::newaccount(account_name creator,account_name name,authority owner,authority active) {
      user_resources_table  userres( _self, name.value);
      userres.emplace( name, [&]( auto& res ) {
         res.owner = name;
      });
      set_resource_limits( name.value, 0, 0, 0 );
   }
}

EOSIO_DISPATCH( eosiosystem::system_contract,
(updatebp)
      (freeze)(unfreeze)
      (vote)(vote4ram)(voteproducer)(fee)
      //(claim)
      (onblock)
      (setparams)(removebp)
      (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)
      (onerror)(addmortgage)(claimmortgage)(claimbp)(claimvote)(claimdevelop)
      (setconfig)(setcode)(setfee)(setabi)
       (delegatebw)(undelegatebw)(refund)
       (punishbp)(canclepunish)(apppunish)(unapppunish)(bailpunish)
       (testaction)
        )