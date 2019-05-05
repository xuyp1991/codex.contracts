#pragma once
#include <eosiolib/transaction.hpp>
#include <../../codexlib/config.hpp>

namespace eosio {
   using std::vector;
   using std::string;
   class [[eosio::contract("force.msig")]] multisig : public contract {
      public:
         using contract::contract;

         ACTION propose();
         ACTION approve( account_name proposer, name proposal_name, permission_level level );
         ACTION unapprove( account_name proposer, name proposal_name, permission_level level );
         ACTION cancel( account_name proposer, name proposal_name, account_name canceler );
         ACTION exec( account_name proposer, name proposal_name, account_name executer );

         using propose_action = action_wrapper<"propose"_n, &multisig::propose>;
         using approve_action = action_wrapper<"approve"_n, &multisig::approve>;
         using unapprove_action = action_wrapper<"unapprove"_n, &multisig::unapprove>;
         using cancel_action = action_wrapper<"cancel"_n, &multisig::cancel>;
         using exec_action = action_wrapper<"exec"_n, &multisig::exec>;

      private:
         TABLE proposal {
            name                       proposal_name;
            vector<char>               packed_transaction;

            auto primary_key()const { return proposal_name.value; }
         };
         typedef eosio::multi_index<"proposal"_n,proposal> proposals;

         TABLE approvals_info {
            name                       proposal_name;
            vector<permission_level>   requested_approvals;
            vector<permission_level>   provided_approvals;

            auto primary_key()const { return proposal_name.value; }
         };
         typedef eosio::multi_index<"approvals"_n,approvals_info> approvals;
   };

} /// namespace eosio