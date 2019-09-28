<h1 class="contract">close</h1>

---
spec_version: "0.2.0"
title: Close Token Balance
summary: 'Close {{nowrap owner}}’s zero quantity balance'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

{{owner}} agrees to close their zero quantity balance for the {{symbol_to_symbol_code symbol}} token.

RAM will be refunded to the RAM payer of the {{symbol_to_symbol_code symbol}} token balance for {{owner}}.

<h1 class="contract">create</h1>

---
spec_version: "0.2.0"
title: Create New Token
summary: 'Create a new token'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

{{$action.account}} agrees to create a new token with symbol {{asset_to_symbol_code maximum_supply}} to be managed by {{issuer}}.

This action will not result any any tokens being issued into circulation.

{{issuer}} will be allowed to issue tokens into circulation, up to a maximum supply of {{maximum_supply}}.

RAM will deducted from {{$action.account}}’s resources to create the necessary records.

<h1 class="contract">issue</h1>

---
spec_version: "0.2.0"
title: Issue Tokens into Circulation
summary: 'Issue {{nowrap quantity}} into circulation and transfer into {{nowrap to}}’s account'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

The token manager agrees to issue {{quantity}} into circulation, and transfer it into {{to}}’s account.

{{#if memo}}There is a memo attached to the transfer stating:
{{memo}}
{{/if}}

If {{to}} does not have a balance for {{asset_to_symbol_code quantity}}, or the token manager does not have a balance for {{asset_to_symbol_code quantity}}, the token manager will be designated as the RAM payer of the {{asset_to_symbol_code quantity}} token balance for {{to}}. As a result, RAM will be deducted from the token manager’s resources to create the necessary records.

This action does not allow the total quantity to exceed the max allowed supply of the token.

<h1 class="contract">open</h1>

---
spec_version: "0.2.0"
title: Open Token Balance
summary: 'Open a zero quantity balance for {{nowrap owner}}'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

{{ram_payer}} agrees to establish a zero quantity balance for {{owner}} for the {{symbol_to_symbol_code symbol}} token.

If {{owner}} does not have a balance for {{symbol_to_symbol_code symbol}}, {{ram_payer}} will be designated as the RAM payer of the {{symbol_to_symbol_code symbol}} token balance for {{owner}}. As a result, RAM will be deducted from {{ram_payer}}’s resources to create the necessary records.


<h1 class="contract">transfer</h1>

---
spec_version: "0.2.0"
title: Transfer Tokens
summary: 'Send {{nowrap quantity}} from {{nowrap from}} to {{nowrap to}}'
icon: @ICON_BASE_URL@/@TRANSFER_ICON_URI@
---

{{from}} agrees to send {{quantity}} to {{to}}.

{{#if memo}}There is a memo attached to the transfer stating:
{{memo}}
{{/if}}

If {{from}} is not already the RAM payer of their {{asset_to_symbol_code quantity}} token balance, {{from}} will be designated as such. As a result, RAM will be deducted from {{from}}’s resources to refund the original RAM payer.

If {{to}} does not have a balance for {{asset_to_symbol_code quantity}}, {{from}} will be designated as the RAM payer of the {{asset_to_symbol_code quantity}} token balance for {{to}}. As a result, RAM will be deducted from {{from}}’s resources to create the necessary records.

<h1 class="contract">addreward</h1>

---
spec_version: "0.2.0"
title: add a coin to reward table
summary: 'add a coin to reward table,the user who hold the coin can get some reward everyday'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

This action must be called by the account relay.token

<h1 class="contract">rewardmine</h1>

---
spec_version: "0.2.0"
title: reward some token to the coin in the reward table
summary: 'reward some token to the coin in the reward table, the reward token should be the core token'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

This action must be called by the account force

<h1 class="contract">claim</h1>

---
spec_version: "0.2.0"
title: Take the reward token 
summary: 'take the reward token to the coin cast'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

<h1 class="contract">destroy</h1>

---
spec_version: "0.2.0"
title: destroy some token on this chain
summary: 'destroy some token on this chain,the coin will be transfer to your account on another chain'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

<h1 class="contract">on</h1>

---
spec_version: "0.2.0"
title: other chain push some coin to this coin ,on action send some coin to user
summary: 'Called by contract force.relay'
icon: @ICON_BASE_URL@/@TOKEN_ICON_URI@
---

