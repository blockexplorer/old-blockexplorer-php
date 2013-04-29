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
    switch ($error) {
        case 404:
            header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found");
            break;

        case 400:
            header($_SERVER["SERVER_PROTOCOL"]." 400 Bad Request");
            break;

        case 503:
            header($_SERVER["SERVER_PROTOCOL"]." 503 Service Unavailable");
            header("Retry-After: 7200");
            break;

        case 500:
            header($_SERVER["SERVER_PROTOCOL"]." 500 Internal Server Error");
            break;
    }
}
