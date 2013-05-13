{extends "explore.tpl"}
{block "description" prepend}
{assign "description" "Information about Bitcoin transaction {$tx_hash|truncate:13}."}
{/block}

{block "title"}Tx {$tx_hash|truncate:13}{/block}
{block "body"}

<h1>Transaction</h1>

<ul class="infoList">


<li>Hash{help("Full hash of this transaction")}: {$tx_hash}</li>

<li>Appeared in <a href="{$rootpath}block/{$tx.block}">block {$tx.blocknumber}</a> ({$tx.time})</li>


<li>Number of inputs{help("Total number of previous outputs this transaction redeems")}: {SQL::count($query_inputs)} (<a href="#inputs">Jump to inputs</a>)</li>

<li>Total BTC in{help("Total BTC redeemed from previous transactions")}: {$inputs_total|rzerotrim|thousands}</li>


<li>Number of outputs: {SQL::count($query_outputs)} (<a href="#outputs">Jump to outputs</a>)</li>

<li>Total BTC out{help("Total BTC sent with this transaction.")}: {$outputs_total|rzerotrim|thousands}</li>

<li>Size{help("The data size of this transaction. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte=1000 bytes (this is how Bitcoin does it)")}: {$tx.size|fmt_bytesize}</li>

<li>Fee{help("The amount of BTC given to the person who generated the block this appeared in. It's the difference between total BTC in and total BTC out.")}: {$tx.fee|rzerotrim}</li>

<li><a href="{$rootpath}rawtx/{$tx_hash}">Raw transaction</a>{help("Almost the same as getblock.")}</li>
{if SQL::count($query_duplicates)}
<li>Duplicates{help("An exact copy of this transaction appeared in these blocks. These copies are not spendable.")}:
{while $duplicate = SQL::d($query_duplicates)}
<a href="{$rootpath}block/{$duplicate.block}">{$duplicate.block|rzerotrim|truncate:13}</a>
{/while}
</li>
{/if}
</ul>

<h3><a name="inputs">Inputs</a>{help("Each input redeems a previous output with a signature.")}</h3>


<table class="txtable">

<tr>
<th>Previous output (index){help("The truncated hash of a previous transaction and the index of the output that this input is redeeming (after the colon). The first output in a transaction has an index of 0.")}</th>
<th>Amount{help("Amount of BTC gotten from this output")}</th>
<th>From address{help("The addresses of the referenced outputs. Whoever sent this transaction owns all of these addresses.")}</th>
<th>Type{help("The type of the referenced output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")}</th>
<th>ScriptSig{help("This script is matched with the referenced output's scriptPubKey. It usually contains a signature, and possibly a public key. ScriptSigs of generation inputs are sometimes called the 'coinbase' parameter, and they contain the current compact target and the extraNonce variable")}</th></tr>

{while $input = SQL::d($query_inputs)}
<tr>
{if $input.prev}
<td><a name="i{$input.id}" href="{$rootpath}tx/{$input.prev}#o{$input.index}">{$input.prev|truncate:15}:{$input.index}</a></td>
{else}
<td><a name="i{$input.id}">N/A</a></td>
{/if}
<td>{$input.value|rzerotrim}{if $input.type == "Generation"} + fees{/if}</td>
<td>
{if $input.address}
    <a href="{$rootpath}address/{$input.address}">{$input.address}</a>
{else}
    {if $input.type == "Generation"}
    N/A
    {else}
    Unknown
    {/if}
{/if}
<td>{$input.type}</td>
<td><div class="hugeCell">{$input.scriptsig}</div></td>
</tr>
{/while}
</table>


<h3><a name="outputs">Outputs</a>{help("Each output sends BTC to some address. In the official client, usually one output sends coins to the destination, and one output sends coins back to a new address owned by the sender.")}</h3>

<table class="txtable">

<tr>
<th>Index{help("Starts at 0 and increments for each output.")}</th>
<th>Redeemed at input{help("If this output has ever been spent by the recipient, the transaction that did it is listed here. (If you look at these links, you will see that I assign a number to each input. This is an internal ID unrelated to Bitcoin.)")}</th>
<th>Amount{help("BTC sent by this output")}</th>
<th>To address{help("Addresses this output was sent to")}</th>
<th>Type{help("The type of the output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")}</th>
<th>ScriptPubKey{help("This script specifies the conditions that must be met by someone attempting to redeem this output. Usually it contains a hash160 (Bitcoin address) or a public key.")}</th>
</tr>


{while $output = SQL::d($query_outputs)}
<tr>
<td>{$output.index}</td>

<td>
{assign "redeemed" SQL::d(SQLPrepare::execute("redeemed", $output.index))}
{if $redeemed}
<a name="o{$output.index}" href="{$rootpath}tx/{$redeemed.tx}#i{$redeemed.id}">{$redeemed.tx|truncate:15}</a>
{else}
<a name="o{$output.index}">Not yet redeemed</a>
{/if}
</td>

<td>{$output.value|rzerotrim}</td>
<td>
    {if $output.address}
    <a href="{$rootpath}address/{$output.address}">{$output.address}</a>
    {else}
    Unknown
    {/if}
</td>
<td>{$output.type}</td>
<td><div class="hugeCell">{$output.scriptpubkey}</div></td>
</tr>

{/while}
</table>


{/block}
