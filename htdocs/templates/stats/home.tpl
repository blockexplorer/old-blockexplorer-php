<!DOCTYPE HTML PUBLIC "-//W3C// DTD HTML 4.01// EN"
"http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<title>Bitcoin real-time stats and tools</title>
</head>
<body>
<p>Usage: {$rootpath}/query[/parameter]</p>
<p>Queries currently supported:</p>
<h4>Real-time stats</h4>
<p>While <a href="/">Bitcoin Block Explorer</a> can run at a delay of up to two minutes, these tools are all completely real-time.</p>
<ul>
<li><a href="{$rootpath}/getdifficulty">getdifficulty</a> - shows the current difficulty as a multiple of the minimum difficulty (highest target).</li>
<li><a href="{$rootpath}/getblockcount">getblockcount</a> - shows the number of blocks in the longest block chain (not including the genesis block). Equivalent to Bitcoin\'s getblockcount.</li>
<li><a href="{$rootpath}/latesthash">latesthash</a> - shows the latest block hash.</li>
<li><a href="{$rootpath}/getblockhash">getblockhash</a> - returns the hash of a block at a given height.</li>
<li><a href="{$rootpath}/hextarget">hextarget</a> - shows the current target as a hexadecimal number.</li>
<li><a href="{$rootpath}/decimaltarget">decimaltarget</a> - shows the current target as a decimal number.</li>
<li><a href="{$rootpath}/probability">probability</a> - shows the probability of a single hash solving a block with the current difficulty.</li>
<li><a href="{$rootpath}/hashestowin">hashestowin</a> - shows the average number of hashes required to win a block with the current difficulty.</li>
<li><a href="{$rootpath}/nextretarget">nextretarget</a> - shows the block count when the next retarget will take place.</li>
<li><a href="{$rootpath}/estimate">estimate</a> - shows an estimate for the next difficulty.</li>
<li><a href="{$rootpath}/totalbc">totalbc</a> - shows the total number of Bitcoins in circulation. You can also <a href="{$rootpath}/totalbc/50000">see the circulation at a particular number of blocks</a>.</li>
<li><a href="{$rootpath}/bcperblock">bcperblock</a> - shows the number of Bitcoins created per block. You can also <a href="{$rootpath}/bcperblock/300000">see the BC per block at a particular number of blocks.</a></li>
</ul>
<h4>Delayed stats</h4>
<p>These use BBE data.</p>
<ul>
<li><a href="{$rootpath}/avgtxsize">avgtxsize</a> - shows the average transaction data size in bytes. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="{$rootpath}/avgtxvalue">avgtxvalue</a> - shows the average BTC input value per transaction, not counting generations. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="{$rootpath}/avgblocksize">avgblocksize</a> - shows the average block size. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="{$rootpath}/interval">interval</a> - shows the average interval between blocks, in seconds. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="{$rootpath}/eta">eta</a> - shows the estimated number of seconds until the next retarget. The parameter sets how many blocks to look back at (default 1000). Blocks before the last retarget are never taken into account, however.</li>
<li><a href="{$rootpath}/avgtxnumber">avgtxnumber</a> - shows the average number of transactions per block. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="{$rootpath}/getreceivedbyaddress">getreceivedbyaddress</a> - shows the total BTC received by an address.</li>
<li><a href="{$rootpath}/getsentbyaddress">getsentbyaddress</a> - shows the total BTC sent by an address. <i>Do not use this unless you know what you are doing: it does not do what you might expect.</i></li>
<li><a href="{$rootpath}/addressbalance">addressbalance</a> - shows received BTC minus sent BTC for an address. <i>Do not use this unless you know what you are doing: it does not do what you might expect.</i></li>
<li><a href="{$rootpath}/addressfirstseen">addressfirstseen</a> - shows the time at which an address was first seen on the network.</li>
<li><a href="{$rootpath}/nethash">nethash</a> - produces CSV statistics about block difficulty. The parameter sets the interval between data points.</li>
<li><a href="{$rootpath}/mytransactions">mytransactions</a> - dumps all transactions for given addresses</li>
</ul>

<h4>Tools</h4>
<ul>
<li><a href="{$rootpath}/addresstohash">addresstohash</a> - converts a Bitcoin address to a hash160.</li>
<li><a href="{$rootpath}/hashtoaddress">hashtoaddress</a> - converts a hash160 to a Bitcoin address.</li>
<li><a href="{$rootpath}/checkaddress">checkaddress</a> - checks a Bitcoin address for validity.</li>
<li><a href="{$rootpath}/hashpubkey">hashpubkey</a> - creates a hash160 from a public key.</li>
<li><a href="{$rootpath}/changeparams">changeparams</a> - calculates the end total number of bitcoins with different starting parameters.</li>
</ul>

<p>This server is up more than 99% of the time, but anything that pulls data from here should still be prepared for failure.</p>
</body>
</html>
