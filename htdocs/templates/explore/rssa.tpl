<?xml version="1.0" encoding="ISO-8859-1" ?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
<channel>
<description>Latest received transactions for {$address}</description>
<link>{$scheme}{$smarty.server.HTTP_HOST}{$rootpath}address/{$address}</link>
<title>BBE - {$address}</title>
<atom:link href="{$scheme}{$smarty.server.HTTP_HOST}{$rootpath}rssa/{$address}.xml" rel="self" type="application/xml" />
{if $builddate }
<lastBuildDate>{$builddate}</lastBuildDate>
{/if}
{while $tx = SQL::d($query)}
{assign "anchor" sprintf("%so%d", substr($tx.tx, 0, 16), $tx.oid)}
<item>
<description>{$address} received {$tx.value|rzerotrim} BTC at {$tx.time} in block number {$tx.number}.</description>
<guid>{$scheme}{$smarty.server.HTTP_HOST}{$rootpath}address/{$address}#{$anchor}</guid>
<pubDate>{$tx.time}</pubDate>
<title>Received {$tx.value|rzerotrim} BTC</title>
<link>{$scheme}{$smarty.server.HTTP_HOST}{$rootpath}address/{$address}#{$anchor}</link>
</item>
{/while}
</channel>
</rss>
