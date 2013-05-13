{extends "explore.tpl"}
{block "description" prepend}{assign "description" "Largest transactions and strange transactions."}{/block}

{block "title"}Transaction stats (temporary){/block}
{block "body"}

<p>Because getting this data is currently very slow, this data has been moved here from the front page until I rewrite my database code.</p>

<h3>Largest transactions (last 300 blocks){help("Sorted by BTC moved")}</h3>
<table class="txtable">

<tr>
<th>Transaction{help("Truncated transaction hash.")}</th>
<th>Amount{help("BTC moved.")}</th>
<th>Block{help("The block this transaction appeared in.")}</th>
<th>Time{help("UTC network time of the block this appeared in (somewhat unreliable).")}</th>
</tr>

{while $tx=SQL::d($query_largest)}
<tr>
<td><a href="{$rootpath}tx/{$tx.hash}">{$tx.hash|truncate:23}</a></td>
<td>{$tx.totalvalue|rzerotrim}</td>
<td><a href="{$rootpath}block/{$tx.blockhash}">{$tx.blocknum}</a></td>
<td>{$tx.time}</td>
</tr>
{/while}

</table>
    
<h3>Latest strange transactions</h3>
<table class="txtable">

<tr>
<th>Transaction{help("Truncated transaction hash.")}</th>
<th>Block{help("The block this transaction appeared in.")}</th>
<th>Time{help("UTC network time of the block this appeared in (somewhat unreliable)")}</th>
</tr>

{while $tx=SQL::d($query_strange)}
<tr>
<td><a href="{$rootpath}tx/{$tx.txhash}">{$tx.txhash|truncate:23}</a></td>
<td><a href="{$rootpath}block/{$tx.blockhash}">{$tx.blocknum}</a></td>
<td>{$tx.time}</td>
</tr>
{/while}

</table>

{/block}
