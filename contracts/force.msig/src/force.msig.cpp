#include <force.msig.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/permission.hpp>
#include <eosiolib/crypto.hpp>

namespace eosio {
   ACTION multisig::propose() {
      constexpr size_t max_stack_buffer_size = 512;
      size_t size = action_data_size();
      char* buffer = (char*)( max_stack_buffer_size < size ? malloc(size) : alloca(size) );
      read_action_data( buffer, size );

      account_name proposer;
      name proposal_name;
      vector<permission_level> requested;
      transaction_header trx_header;

      datastream<const char*> ds( buffer, size );
      ds >> proposer >> proposal_name >> requested;

      size_t trx_pos = ds.tellp();
      ds >> trx_header;

      require_auth( proposer );
      eosio_assert( trx_header.expiration >= eosio::time_point_sec(now()), "transaction expired" );
      //eosio_assert( trx_header.actions.size() > 0, "transaction must have at least one action" );

      proposals proptable( _self, proposer.value );
      eosio_assert( proptable.find( proposal_name.value ) == proptable.end(), "proposal with the same name exists" );

      auto packed_requested = pack(requested);
      auto res = ::check_transaction_authorization( buffer+trx_pos, size-trx_pos,
                                                   (const char*)0, 0,
                                                   packed_requested.data(), packed_requested.size()
                                                );

      eosio_assert( res > 0, "transaction authorization failed" );
      std::vector<char> pkd_trans;
      pkd_trans.resize(size);
      memcpy((char*)pkd_trans.data(), buffer, size);

      proptable.emplace( proposer, [&]( auto& prop ) {
         prop.proposal_name       = proposal_name;
         prop.packed_transaction  = pkd_trans;
      });

      approvals apptable(  _self, proposer.value );
      apptable.emplace( proposer, [&]( auto& a ) {
         a.proposal_name       = proposal_name;
         a.requested_approvals = std::move(requested);
      });
   }
   ACTION multisig::approve( account_name proposer, name proposal_name, permission_level level ) {
      require_auth( level );

      approvals apptable(  _self, proposer.value );
      auto& apps = apptable.get( proposal_name.value, "proposal not found" );

      auto itr = std::find( apps.requested_approvals.begin(), apps.requested_approvals.end(), level );
      eosio_assert( itr != apps.requested_approvals.end(), "approval is not on the list of requested approvals" );

      apptable.modify( apps, proposer, [&]( auto& a ) {
         a.provided_approvals.push_back( level );
         a.requested_approvals.erase( itr );
      });
   }
   ACTION multisig::unapprove( account_name proposer, name proposal_name, permission_level level ) {
      require_auth( level );

      approvals apptable(  _self, proposer.value );
      auto& apps = apptable.get( proposal_name.value, "proposal not found" );
      auto itr = std::find( apps.provided_approvals.begin(), apps.provided_approvals.end(), level );
      eosio_assert( itr != apps.provided_approvals.end(), "no approval previously granted" );

      apptable.modify( apps, proposer, [&]( auto& a ) {
         a.requested_approvals.push_back(level);
         a.provided_approvals.erase(itr);
      });
   }
   ACTION multisig::cancel( account_name proposer, name proposal_name, account_name canceler ) {
      require_auth( canceler );

      proposals proptable( _self, proposer.value );
      auto& prop = proptable.get( proposal_name.value, "proposal not found" );

      if( canceler != proposer ) {
         eosio_assert( unpack<transaction_header>( prop.packed_transaction ).expiration < eosio::time_point_sec(now()), "cannot cancel until expiration" );
      }

      approvals apptable(  _self, proposer.value );
      auto& apps = apptable.get( proposal_name.value, "proposal not found" );

      proptable.erase(prop);
      apptable.erase(apps);
   }
   ACTION multisig::exec( account_name proposer, name proposal_name, account_name executer ) {
      require_auth( executer );

      proposals proptable( _self, proposer.value );
      auto& prop = proptable.get( proposal_name.value, "proposal not found" );

      approvals apptable(  _self, proposer.value );
      auto& apps = apptable.get( proposal_name.value, "proposal not found" );

      transaction_header trx_header;
      datastream<const char*> ds( prop.packed_transaction.data(), prop.packed_transaction.size() );
      ds >> trx_header;
      eosio_assert( trx_header.expiration >= eosio::time_point_sec(now()), "transaction expired" );

      auto packed_provided_approvals = pack(apps.provided_approvals);
      auto res = ::check_transaction_authorization( prop.packed_transaction.data(), prop.packed_transaction.size(),
                                                   (const char*)0, 0,
                                                   packed_provided_approvals.data(), packed_provided_approvals.size()
                                                );
      eosio_assert( res > 0, "transaction authorization failed" );

      send_deferred( (uint128_t(proposer.value) << 64) | proposal_name.value, executer.value, prop.packed_transaction.data(), prop.packed_transaction.size() );

      proptable.erase(prop);
      apptable.erase(apps);
   }
}

//EOSIO_DISPATCH( eosio::multisig,(propose)(approve)(unapprove)(cancel)(exec) )