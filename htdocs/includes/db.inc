<?php

global $_DB;
$_DB = NULL;

function db_init($dbname, $connect_timeout = 2)
{
    global $_DB;

	$db = @pg_pconnect("dbname = $dbname connect_timeout = $connect_timeout");

	if(!$db) {
		senderror(503);
		echo "ERROR: Couldn't connect to database '$dbname'.";
		error_log("Database down");
		die();
	}
	pg_query("set statement_timeout to 60000;");
    $_DB = $db;
}

function db_query($query, $params = false)
{
    global $_DB;
    if(!$_DB) 
        throw Exception("database not initialized");

	if($params !== false) {
		$retval = pg_query_params($_DB, $query, $params);
	} else {
		$retval = pg_query($_DB, $query);
	}
	if(!$retval) {
		senderror(500);
		echo "ERROR: Database problem. Try again in a few minutes";
		error_log("Failed pg_query");
		die();
	}
	return $retval;
}

function db_prepare($name, $query)
{
    global $_DB;
    if(!$_DB) 
        throw Exception("database not initialized");

    pg_prepare($_DB, $name, $query);
}

function db_execute($name, $params)
{
    global $_DB;
    if(!$_DB) 
        throw Exception("database not initialized");

    $retval = pg_execute($_DB, $name, $params);

	if(!$retval) {
		senderror(500);
		echo "ERROR: Database problem. Try again in a few minutes";
		error_log("Failed pg_execute");
		die();
	}
	return $retval;
}
