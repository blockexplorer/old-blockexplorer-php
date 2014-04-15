{extends "explore.tpl"}
{block "description" prepend}
{assign "description" "Bitcoin Block Explorer is a web tool that provides detailed information about Bitcoin blocks, addresses, and transactions."}
{/block}

{block "title"}Home{/block}
{block "body"}

<h1>Bitcoin Block Explorer</h1>
<p>Bitcoin Block Explorer allows you to easily view information about the
<a href="https://en.bitcoin.it/wiki/Blocks">blocks</a>, <a href="https://en.bitcoin.it/wiki/Address">addresses</a>, and <a href="https://en.bitcoin.it/wiki/Transactions">transactions</a> created by <a href="http://bitcoin.org">Bitcoin</a>.
It uses the <a href="https://bitcointalk.org/index.php?topic=724.0">getblock</a> patch by jgarzik, but also does a
ton of processing to make certain tasks, such as tracking transactions, easier. Help text is included in the tooltips produced by superscript question marks: {help("Like this")}. All times are UTC. Tell me (Liraz Siri) if you find any bugs.

<p>Some data from Bitcoin Block Explorer is available through the machine-readable <a href="{$rootpath}q">Real-Time Stats pages</a>.

<p>Thanks to <a href="https://www.privateinternetaccess.com/" title = "VPN Service">Private Internet Access</a> for running a mirror of Bitcoin Block Explorer. They've asked me to advertise their site: <a href="https://www.privateinternetaccess.com/" title = "VPN Service">VPN Service</a> (accepts Bitcoin!).</p>

<h3>Search</h3>
<p>You can enter a block number, address, block hash, transaction hash, hash160. Hashes are expressed in hexadecimal.</p>
<form action="/search" method="post" ><p><input type="text" name="q" size="50"><input type="submit" value="Search"></p></form>

<h3>Latest blocks{help("Up to two minutes delay.")}</h3>
<table class="txtable">

<tr>
    <th>Number{help("A count of the number of blocks up to this one, with the genesis block being 0.")}</th>
    <th>Hash{help("Truncated hash of this block.")}</th>
    <th>Time{help("UTC time included in this block. The network's time must not be relied upon for precision, but it is generally accurate.")}</th>
    <th>Transactions{help("Number of transactions in this block. All blocks have at least one generation transaction.")}</th>
    <th>Total BTC{help("Total BTC moved by the transactions in this block.")}</th>
    <th>Size (kB){help("The data size of this block. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).")}</th>
</tr>
{while $row = SQL::d($query)}
<tr>
<td><a href="{$rootpath}block/{$row.hash}">{$row.number}</a></td>
<td><a href="{$rootpath}block/{$row.hash}">{$row.hash|lzerotrim|truncate:13}</a></td>
<td>{$row.time}</td>
<td>{$row.count}</td>
<td>{$row.sum|rzerotrim}</td>
<td>{round($row.size/1000, 3)}</td>
</tr>

{/while}
</table>


<p>Largest transactions and strange transactions have been temporarily moved <a href="{$rootpath}txstats">here</a>.</p>
{/block}
