#include "force.system.hpp"

namespace eosiosystem {
    
    ACTION system_contract::updateauth(account_name account,permission_name permission,permission_name parent,authority auth) {
        
    }

    ACTION system_contract::deleteauth(account_name account,permission_level permission) {
        
    }

    ACTION system_contract::linkauth(account_name account,account_name code,action_name type,permission_level requirement) {
        
    }

    ACTION system_contract::unlinkauth(account_name account,account_name code,action_name type) {
        
    }

    ACTION system_contract::canceldelay(permission_level canceling_auth,transaction_id_type trx_id) {
    }

    ACTION system_contract::onerror(uint128_t sender_id,bytes sent_trx) {
       
    }

    ACTION system_contract::setconfig(account_name typ,int64_t num,account_name key,asset fee){
        
    }

    ACTION system_contract::setcode(account_name account,uint8_t vmtype,uint8_t vmversion,bytes code){
        
    }
    
    ACTION system_contract::setfee(account_name account,action_name action,asset fee,uint32_t cpu_limit,uint32_t net_limit,uint32_t ram_limit){
        
    }

    ACTION system_contract::setabi(account_name account,bytes abi){
       
    }
}
