<?php

define('HOSTNAME', 'dev.blockexplorer.com');
define('REDIRECT_CANONICAL', false);
define('ENABLE_CACHING', false);

class Config {
    static $dbname = "testexplore";
    //static $address_version = "00";
    static $address_version = "6F";
    static $rpc = array("user" => "bitcoin",
                        "password" => "7AvathEBracheCra",
                        "target" => "127.0.0.1",
                        "port" => 8331);
}

class TestnetConfig {
    static $dbname = "testexplore";
    static $address_version = "6F";
    static $rpc = array("user" => "bitcoin",
                        "password" => "7AvathEBracheCra",
                        "target" => "127.0.0.1",
                        "port" => 8331);
}
