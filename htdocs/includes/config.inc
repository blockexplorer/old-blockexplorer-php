<?php

define('HOSTNAME', 'blockexplorer.com');
define('REDIRECT_CANONICAL', false);

define('CONFIG_SHOW_ERRORS', true);

class Config {
    static $dbname = "testexplore";
    static $addressversion = "00";
    static $rpc = array("user" => "bitcoin",
                        "password" => "7AvathEBracheCra",
                        "target" => "127.0.0.1",
                        "port" => 8331);
}

class TestnetConfig {
    static $dbname = "testexplore";
    static $addressversion = "6F";
    static $rpc = array("user" => "bitcoin",
                        "password" => "7AvathEBracheCra",
                        "target" => "127.0.0.1",
                        "port" => 8331);
}
?>
