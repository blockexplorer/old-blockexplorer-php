{extends "explore.tpl"}
{block "description" prepend}
{assign "description" "List of transactions involving Bitcoin address $address."}
{/block}

{block "head" append}
<link rel="alternate" type="application/rss+xml" title="RSS" href="{$scheme}{$smarty.server.HTTP_HOST}{$rootpath}rssa/{$address}.xml">
<meta http-equiv="Content-type" content="text/html;charset=ISO-8859-1">
{/block}

{block "title"}Address {$address}{/block}
{block "body"}

<h1>Address {$address}</h1>

<ul class="infoList">

<li>First seen{help("The first block this address was used in.")}: 
{if $keyinfo}
<a href="{$rootpath}block/{$keyinfo.firstseen}">Block {$blockinfo.number}</a> ({$blockinfo.time})
{else}
Never used on the network (as far as I can tell)
{/if}
</li>

{***
<li>Balance: {$received_txs - $sent_txs}</li>
***}
<li>Received transactions: {$received_txs}</li>
<li>Received BTC: {$received_btc|rzerotrim}</li>
<li>Sent transactions: {$sent_txs}</li>
<li>Sent BTC: {$sent_btc|rzerotrim}</li>
<li>Hash160{help("The hash160 is a hash of the public key. Bitcoin uses these hashes internally - transactions don't contain Bitcoin addresses directly. Bitcoin addresses contain a base58-encoded hash160, along with a version and a check code.")}: {$hash160}</li>

<li>Public key{help("It's impossible to determine the public key from a Bitcoin address, but if the public key was ever used on the network, it is listed here.")}: 
<div class="hugeData">
{if $keyinfo.pubkey }
{$keyinfo.pubkey}
{else}
Unknown (not seen yet)
{/if}
</div>

</li>
</ul>

<h3>Ledger{help("A list of all transactions involving this address, with the oldest listed first.")}</h3>

<p>Note: While the last "balance" is the accurate number of bitcoins available to this address, it is likely not the balance available to this person. Every time a transaction is sent, some bitcoins are usually sent back to yourself <i>at a new address</i> (not included in the Bitcoin UI), which makes the balance of a single address misleading. See <a href="https://en.bitcoin.it/wiki/Transactions">the wiki</a> for more info on transactions.</p>

<table class="txtable">

<tr>

<th>Transaction{help("Truncated transaction hash")}</th>
<th>Block{help("Block this transaction appeared in")}</th>
<th>Amount{help("Number of BTC sent or received")}</th>
<th>Type{help("The type of the output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client. It's also possible for transactions to have several different types of outputs.")}</th>
<th>From/To{help("The addresses this was received from or sent to. When sending, Bitcoin usually sends some bitcoins back to a brand new address that you own.")}</th><th>Balance{help("Balance as of this transaction. The last balance is the current balance of this address.")}</th>

</tr>

{assign "balance" 0}

{while $tx = SQL::d($query_txs)}
<tr>
<td><a name="$anchorname" href="{$rootpath}tx/{$tx.tx}#{if $tx.type == 'credit'}o{else}i{/if}{$tx.id}">{$tx.tx|truncate:13}</a></td>
<td><a href="{$rootpath}block/{$tx.block}">Block {$tx.blocknum}</a> ({$tx.time})</td>
<td>{$tx.value|rzerotrim}</td>

{if $tx.type == "credit"}
    {assign "balance" $balance + $tx.value}

    {assign "address_list" array()}
    {assign "tx_inputs" SQLPrepare::execute("tx_inputs", $tx.tx)}
    {while $tx_input = SQL::d($tx_inputs)}
        {capture append="address_list"}
            {if $tx_input.address}
                {if $tx_input.address == $address}
                    {$address}
                {else}
                    <a href="{$rootpath}address/{$tx_input.address}">{$tx_input.address}</a>
                {/if}
            {else}
                {if $tx_input.type == "Generation"}
                    Generation
                {else}
                    Unknown
                {/if}
            {/if}
        {/capture}
    {/while}
    <td>Received: {$tx.txtype}</td>


{elseif $tx.type == "debit"}
    {assign "balance" $balance - $tx.value}

    {assign "sent_tx_type" ""}
    
    {assign "address_list" array()}
    {assign "tx_outputs" SQLPrepare::execute("tx_outputs", $tx.tx)}
    {while $tx_output = SQL::d($tx_outputs)}

        {if $sent_tx_type == ""}
            {assign "sent_tx_type" $tx_output.type}
        {else}
            {if $sent_tx_type != $tx_output.type}
                {assign "sent_tx_type" "Mixed types"}
            {/if}
        {/if}

        {capture append="address_list"}
            {if $tx_output.address}
                {if $tx_output.address == $address}
                    {$address}
                {else}
                    <a href="{$rootpath}address/{$tx_output.address}">{$tx_output.address}</a>
                {/if}
            {else}
                Unknown
            {/if}
        {/capture}
        
    {/while}
    <td>Sent: {$sent_tx_type}</td>

{/if}
<td>
<ul class="infoList">
{foreach $address_list as $address}
    <li>{$address}</li>
{/foreach}
</td>

<td>{$balance}</td>

</tr>
{/while}

</table>
{/block}
