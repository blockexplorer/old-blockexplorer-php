{extends "explore.tpl"}
{block "description" prepend}
{assign "description" "Example page description"}
{/block}

{block "title"}Scriptless help{/block}
{block "body"}

These question mark links produce help text when you hover your mouse
cursor over them. In case your client does not support displaying HTML
title attributes, here is the text for the link you clicked:</p>

<p><i>{$help}</i></p>

{/block}
