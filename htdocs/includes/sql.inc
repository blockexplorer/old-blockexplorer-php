<?php

class SQLException extends Exception {
}

class SQL {
    static $_DB = NULL;

    static function _query($query, $params = false)
    {
        if(!self::$_DB) 
            throw new SQLException("database not initialized");

        if($params !== false) {
            if(!is_array($params))
                $params = array($params);

            $retval = pg_query_params(self::$_DB, $query, $params);
        } else {
            $retval = pg_query(self::$_DB, $query);
        }

        if(!$retval)
            throw new SQLException("pg_query: " . pg_last_error());

        return $retval;
    }


    static function init($dbname, $connect_timeout = 2) {
        $db = @pg_connect("dbname = $dbname connect_timeout = $connect_timeout");

        if(!$db)
            throw new SQLException("pg_connect: " . pg_last_error());

        pg_query("set statement_timeout to 60000;");

        self::$_DB = $db;
    }


    // Get row as object
    static function row_object($arg, $params = false) {
        return pg_fetch_object(SQL($arg, $params));
    }
    // row_object alias
    static function o($arg, $params = false) { return self::row_object($arg, $params); }

    // Get all results as an array of dictionaries
    static function rows_dict($arg, $params = false) {
        return pg_fetch_all(SQL($arg, $params));
    }
    
    // Get row as dictionary
    static function row_dict($arg, $params = false) {
        return pg_fetch_assoc(SQL($arg, $params));
    }

    // row_dict alias
    static function d($arg, $params = false) { return self::row_dict($arg, $params); }
    
    // Get row as array
    static function row_array($arg, $params = false) {
        return pg_fetch_row(SQL($arg, $params));
    }

    // Get singleton (first value from row with single value)
    static function singleton($arg, $params = false) {
        return pg_fetch_row(SQL($arg, $params))[0];
    }

    // singleton alias
    static function s($arg, $params = false) { return self::singleton($arg, $params); }

    // Get number of rows
    static function count($arg, $params = false) {
        return pg_num_rows(SQL($arg, $params));
    }

    static function seek($result, $offset) {
        if(gettype($result) != "resource")
            throw new SQLException("can only seek on a resource");

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
        throw new SQLException("database not initialized");

    pg_prepare(SQL::$_DB, $name, $query);
}

class SQLPrepare {
    static function execute($name, $params)
    {
        if(!SQL::$_DB) 
            throw new SQLException("database not initialized");

        if($params !== false && !is_array($params)) 
            $params = array($params);

        $retval = pg_execute(SQL::$_DB, $name, $params);

        if(!$retval) {
            throw new SQLException("pg_execute: " . pg_last_error());
        }
        return $retval;
    }
}

