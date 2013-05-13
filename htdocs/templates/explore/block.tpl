{extends "explore.tpl"}
{block "description" prepend}
{assign "description" "List of transactions in Bitcoin block #{$block.number}."}
{/block}

{block "title"}Block {$block.number}{/block}
{block "body"}
<h1>Block {$block.number}{help("The number is a count of the number of blocks up to this one, with the genesis block being 0.")}</h1>

{assign "shortlink" "{$rootpath}b/{$block.number}"}
<div id="shortlink">Short link: <a href="{$shortlink}">{$scheme}{$smarty.server.HTTP_HOST}{$shortlink}</a></div>
<ul class="infoList">

<li>Hash{help("Full hash of this block. Sometimes this is expressed without the leading zeroes.")}: {$block_hash}</li>

{assign "prev" "{$block.prev}"}

{if $prev != "0000000000000000000000000000000000000000000000000000000000000000"}
<li>Previous block{help("Every block builds on another, forming a chain. This is the full hash of the previous block.")}: <a href="{$rootpath}block/{$prev}">{$prev}</a></li>
{/if}

{if $next}
<li>Next block{help("The full hash of the block that will build onto this one. This field is not included in real blocks.")}: <a href="{$rootpath}block/{$next}">{$next}</a></li>
{/if}

<li>Time{help("UTC time included in this block. The network's time must not be relied upon for precision, but it is generally accurate.")}: 
    {$block.time}</li>

<li>Difficulty{help("The difficulty of producing blocks at the time this block was created. Same as Bitcoin's getdifficulty.")}: 
    {$difficulty|rzerotrim|thousands} ("Bits"{help("This is the compact form of the 256-bit target used when generating. This is included in actual blocks. The difficulty number is derived from this.")}: {encodeHex($block.bits)|lower})</li>

<li>Transactions{help("Number of transactions in this block (listed below)")}: {$block.count}</li>

<li>Total BTC{help("Total BTC sent through this block, including fees")}: {$block.totalvalue|rzerotrim}</li>

<li>Size{help("The data size of this block. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte=1000 bytes (this is how Bitcoin does it)")}: {$block.size|fmt_bytesize}

</li>

<li>Merkle root{help("The root hash in a hash tree of all transactions.")}: {$block.root}</li>

<li>Nonce{help("When generating, Bitcoin starts this number at 1 and increments for each hash attempt.")}: {$block.nonce}</li>

<li><a href="{$rootpath}rawblock/{$block_hash}">Raw block</a>{help("Almost the same as getblock's output.")}</li>

</ul>


<h3>Transactions</h3>

<table class="txtable">

<tr>
<th>Transaction{help("Truncated hash of this transaction")}</th>
<th>Fee{help("Fee given - the difference between total input value and total output value. This goes to the generator of the block.")}</th>
<th>Size (kB){help("The data size of this transaction. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte=1000 bytes (this is how Bitcoin does it)")}</th>
<th>From (amount){help("List of all addresses that appear in an input. Whoever sent this transaction owns all of these addresses.")}</th>
<th>To (amount){help("A list of all addresses that have received bitcoins from this transaction")}</th>
</tr>

{assign "coinbase" 1}

{while $tx = SQL::d($query_special_tx)}
{assign "coinbase" 0}
<tr>
<td colspan="5">
This transaction is an exact copy of <a href="{$rootpath}tx/{$tx.hash}>{$tx.hash|truncate:13}</a>
This is usually caused by flawed custom miner code that rarely changes the keys used by generations, and is therefore likely to produce a generation transaction with the exact same data as a previous one by the same person. The network sees duplicate transactions as the same: only one can be redeemed.
</td>
</tr>
{/while}

{while $tx = SQL::d($query_tx)}
    <tr>

    <td><a href="{$rootpath}tx/{$tx.hash}">{$tx.hash|truncate:13}</a></td>
    <td>{if $coinbase }0{else}{$tx.fee|rzerotrim}{/if}</td>
    <td>{round($tx["size"]/1000, 3)}</td>
        
    {assign "tx_inputs" SQLPrepare::execute("tx_inputs", $tx.hash)}

    <td><ul class="infoList">
    {while $tx_input = SQL::d($tx_inputs)}
        <li>
        {if $tx_input.address}
        <a href="{$rootpath}address/{$tx_input.address}">{$tx_input.address}</a>
        {else}
            {if $coinbase}
            Generation
            {else}
            Unknown
            {/if}
        {/if}
        {if $coinbase}
        {$tx_input.value|rzerotrim} + {$tx.fee|rzerotrim} total fees
        {else}
        {$tx_input.value|rzerotrim}
        {/if}
        </li>
    {/while}
    </ul></td>

    {assign "tx_outputs"  SQLPrepare::execute("tx_outputs", $tx.hash)}

    <td><ul class="infoList">
    {while $tx_output = SQL::d($tx_outputs)}

        <li>
        {if $tx_output.address}
        <a href="{$rootpath}address/{$tx_output.address}">{$tx_output.address}</a>
        {else}
        Unknown
        {/if}
        {$tx_output.value|rzerotrim}
        </li>
    {/while}

    </ul></td>
    </tr>
    {assign "coinbase" 0}
{/while}
    
</table>

{/block}
