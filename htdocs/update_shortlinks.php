#!/usr/bin/php
<?php

require 'includes/encode.inc';

function fatal($s) {
    echo "error: $s\n";
    exit(1);
}

function usage() {
    echo<<<"EOD"
Syntax: {$GLOBALS['argv'][0]} ( --tx | --address )

EOD;
    exit(1);
}

$args = $GLOBALS['argv'];
if(count($args) != 2 || !preg_match('/^--(tx|address)/', $args[1], $preg_m))
    usage();

$mode = $preg_m[1];

$fh = fopen("php://stdin","r");
while($line = trim(fgets($fh))) {
    $arr = explode(" ", $line);
    if($mode == "tx") {
        $shortcut_hex = decodeBase58($arr[0]);
        $tx_hex = $arr[1];

        echo "INSERT INTO t_shortlinks(shortcut, hash) VALUES (decode('$shortcut_hex', 'hex'), decode('$tx_hex', 'hex'));\n";
    } elseif($mode == "address") {
        
        $shortcut_hex = decodeBase58($arr[0]);
        $hash160_hex = addressToHash160($arr[1]);

        echo "INSERT INTO a_shortlinks(shortcut, hash160) VALUES (decode('$shortcut_hex', 'hex'), decode('$hash160_hex', 'hex'));\n";
        
    }
}
