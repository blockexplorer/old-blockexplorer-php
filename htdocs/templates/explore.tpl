<!DOCTYPE HTML PUBLIC "-//W3C// DTD HTML 4.01// EN"
   "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
{block "head"}
<script type="text/javascript">
{literal}
  var _gaq=_gaq || [];
  _gaq.push(['_setAccount', 'UA-38773634-1']);
  _gaq.push(['_trackPageview']);

  (function() {
    var ga=document.createElement('script'); ga.type='text/javascript'; ga.async=true;
    ga.src=('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
    var s=document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
  })();

  function highlightNamedAnchor() {
      if(location.hash != "")
          document.getElementsByName(location.hash.substr(1, location.hash.length))[0].parentNode.parentNode.style.backgroundColor="#FFFDD0";
  }

  function informHelp() {
      alert("These question mark links produce help text when you hover your mouse cursor over them.");
  }
{/literal}
</script>
<link rel="shortcut icon" href="/favicon.ico">

{if isset($keywords)}
{if $keywords}
<meta name="keywords" content="bitcoin, search, data, {implode(", ", $keywords)}">
{else}
<meta name="keywords" content="bitcoin, search, data">
{/if}
{/if}

{block "description"}
{if isset($description)}
<meta name="description" content="{$description}">
{/if}
{/block}

<title>{block "title"}{$title}{/block} - Bitcoin Block Explorer</title>

<style type="text/css">
    .infoList {
        list-style-type: none;
        margin-left: 0;
        padding-left: 0;
    }
    table{
        border-collapse: collapse;
    }
    table, td, th {
        border: 1px solid black;
        padding: 4px;
    }
    div.hugeCell {
        width: 300px;
        overflow: auto;
    }
    div.hugeData {
        width: 700px;
        overflow: auto;
    }
    #footer {
        text-align: center;
        font-size: smaller;
        margin-top: 2em;
    }
    div#shortlink {
        font-size: smaller;
        margin-top: -1.5em;
        margin-bottom: -1em;
        margin-left: 0.5em;
    }
    .help {
        cursor: help;
    }
</style>
{/block}
</head>
<body onLoad="highlightNamedAnchor()">
{block name="body"}{/block}
<div id="footer"><hr><a href="/">Bitcoin Block Explorer</a> (Mirror ad: <a href="https://www.privateinternetaccess.com/" title="VPN Service">VPN Service</a>) - Donate: <a href="/address/1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28">1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28</a></div>
</body>
</html>
