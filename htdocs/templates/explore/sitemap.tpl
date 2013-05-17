<?xml version="1.0" encoding="ISO-8859-1"?>
{if $sitemapindex}
<sitemapindex xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
{for $i=0 to $tx_sections}
<sitemap>
<loc>http://{$smarty.server.HTTP_HOST}/sitemap-t-{$i}.xml</loc>
</sitemap>
{/for}
{for $i=0 to $address_sections}
<sitemap>
<loc>http://{$smarty.server.HTTP_HOST}/sitemap-a-{$i}.xml</loc>
</sitemap>
{/for}
{for $i=0 to $block_sections}
<sitemap>
<loc>http://{$smarty.server.HTTP_HOST}/sitemap-b-{$i}.xml</loc>
</sitemap>
{/for}
</sitemapindex>
{else}
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
{while $loc = SQL::d($query)}
<url>
<loc>http://{$smarty.server.HTTP_HOST}{$loc.url}</loc>
<changefreq>{$changefreq}</changefreq>
<priority>{$priority}</priority>
</url>
{/while}
</urlset>
{/if}
