<?php

function redirect($path, $type = 302)
{
    global $_SERVER;

    if(isset($_SERVER['HTTPS'])) {
    	$scheme = "https://";
    } else {
    	$scheme = "http://";
    }

    $server = $scheme . HOSTNAME;

	if($type == 301) {
		header ('HTTP/1.1 301 Moved Permanently');
	}
	header("Location: ".$server.$path);
	die();
}

function senderror($error)
{
	if($error == 404)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found");
	}
	if($error == 400)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 400 Bad Request");
	}
	if($error == 503)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 503 Service Unavailable");
		header("Retry-After: 7200");
	}
	if($error == 500)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 500 Internal Server Error");
	}
}
