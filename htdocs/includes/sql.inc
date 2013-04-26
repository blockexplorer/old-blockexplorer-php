<?php

class SQL {
    static $_DB = NULL;

    static function _query($query, $params = false)
    {
        if(!self::$_DB) 
            throw new Exception("database not initialized");

        if($params !== false) {
            $retval = pg_query_params(self::$_DB, $query, $params);
        } else {
            $retval = pg_query(self::$_DB, $query);
        }
        if(!$retval) {
            senderror(500);
            echo "ERROR: Database problem. Try again in a few minutes";
            error_log("Failed pg_query: " . pg_last_error());
            die();
        }
        return $retval;
    }


    static function init($dbname, $connect_timeout = 2) {
        $db = @pg_connect("dbname = $dbname connect_timeout = $connect_timeout");

        if(!$db) {
            senderror(503);
            echo "ERROR: Couldn't connect to database '$dbname'.";
            error_log("Database down: " . pg_last_error());
            die();
        }
        pg_query("set statement_timeout to 60000;");

        self::$_DB = $db;
    }

    // Get dictionary
    static function d($arg, $params = false) {
        return pg_fetch_assoc(SQL($arg, $params));
    }
    
    // Get array
    static function a($arg, $params = false) {
        return pg_fetch_array(SQL($arg, $params));
    }

    // Get singleton
    static function s($arg, $params = false) {
        return pg_fetch_row(SQL($arg, $params))[0];
    }

    // Get number of rows
    static function count($arg, $params = false) {
        return pg_num_rows(SQL($arg, $params));
    }

    static function seek($result, $offset) {
        if(gettype($result) != "resource")
            throw new Exception("can only seek on a resource");

        return pg_result_seek($result, $offset);
    }
}

// Get raw query Resource
function SQL($arg, $params = false) {
    if(gettype($arg) == "resource")
        return $arg;

    return SQL::_query($arg, $params);
}

function SQLPrepare($name, $query)
{
    if(!SQL::$_DB) 
        throw new Exception("database not initialized");

    pg_prepare(SQL::$_DB, $name, $query);
}

class SQLPrepare {
    static function execute($name, $params)
    {
        if(!SQL::$_DB) 
            throw new Exception("database not initialized");

        $retval = pg_execute(SQL::$_DB, $name, $params);

        if(!$retval) {
            senderror(500);
            echo "ERROR: Database problem. Try again in a few minutes";
            error_log("Failed pg_execute: " . pg_last_error());

            die();
        }
        return $retval;
    }
}

