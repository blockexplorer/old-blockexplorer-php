<?php

include_once 'includes/bootstrap.inc';

$my_name="dev.blockexplorer.com";

if(isset($_SERVER['HTTPS']))
{
	$scheme="https://";
}
else
{
	$scheme="http://";
}

if(isset($_SERVER['HTTP_HOST']))
{
	$server=$scheme.$_SERVER['HTTP_HOST'];
}
else
{
	$server=$scheme.$my_name;
}

function emptym($var)
{
	return empty($var)&&$var!==0&&$var!=="0";
}

function redirect($path,$type=302)
{
	global $scheme;
	global $server;
	if($type==301)
	{
		header ('HTTP/1.1 301 Moved Permanently');
	}
	header("Location: ".$server.$path);
	die();
}

function mredirect($path)
{
	global $scheme;
	global $my_name;
	header ('HTTP/1.1 301 Moved Permanently');
	header("Location: ".$scheme.$my_name.$path);
	die();
}

function senderror($error)
{
	if($error==404)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found");
	}
	if($error==400)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 400 Bad Request");
	}
	if($error==503)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 503 Service Unavailable");
		header("Retry-After: 7200");
	}
	if($error==500)
	{
		header($_SERVER["SERVER_PROTOCOL"]." 500 Internal Server Error");
	}
}

function unknownerror($errno,$errstr,$errfile,$errline)
{
	//suppressed errors
	if(error_reporting()==0)
		return;
	
	//clear all buffers
	for($i=ob_get_level();$i>0;$i--)
	{
		ob_end_clean();
	}
	senderror(500);

    echo "<h1>Unknown error</h1>";
    if(CONFIG_SHOW_ERRORS) {
        echo "$errstr in $errfile on line $errline";
    }

	error_log("BBE unknown error: $errstr in $errfile on line $errline");
	die();
	return true;
}
set_error_handler("unknownerror",E_ALL^E_DEPRECATED^E_NOTICE);

//set up variables for checking
$fullpath=$_SERVER['REQUEST_URI'];
$querystart=strpos($fullpath,"?");
if($querystart===false)
{
	$path=$fullpath;
	$query="";
}
else
{
	$path=substr($fullpath,0,$querystart);
	$query="?".substr($fullpath,$querystart+1);
}

//Redirect old theymos.ath.cx links
if($_SERVER['SERVER_PORT']==64150)
{
	if(preg_match(",^/testnet/bbe,",$path))
	{
		$path=preg_replace(",^/testnet/bbe,","",$path);
		$path="/testnet".$path;
		mredirect($path.$query);
	}
	else if(preg_match(",^/bbe,",$path))
	{
		$path=preg_replace(",^/bbe,","",$path);
		mredirect($path.$query);
	}
	else
	{
		mredirect($path.$query);
	}
}

//odd hosts
if(isset($_SERVER['HTTP_HOST']))
{
	$senthost=$_SERVER['HTTP_HOST'];
}
#if(isset($senthost)&&preg_match_all("/[a-zA-Z]/",$senthost,$junk)>6&&$senthost!=$my_name)
#{
#	$path=preg_replace("/^\/bbe/","",$path);
#	mredirect($path.$query);
#	die();
#}

//trailing slash
$last=strlen($path)-1;
if($last!=0&&substr($path,$last,1)=="/")
{
	redirect(substr($path,0,$last).$query,301);
}

//set site-wide variables
//fix delimiters
if(isset($path[0])&&$path[0]=="/")
{
	$path=substr($path,1);
}

$params=explode("/",$path,10);

//defaults
$page="home";
$testnet=false;
$xml=false;
$rts=false;

//tag and remove special views
$count=count($params);
for($i=0;$i<$count;$i++)
{
	if(emptym($params[$i]))
	{
		unset($params[$i]);
	}
	else if($params[$i]=="testnet")
	{
		$testnet=true;
		unset($params[$i]);
	}
	else if($params[$i]=="xml"&&$testnet)
	{
		$xml=true;
		unset($params[$i]);
	}
	else if($params[$i]=="q"&&!$xml)
	{
		$rts=true;
		unset($params[$i]);
	}
	else
	{
		break;
	}
}

$number=0;
foreach($params as $item)
{
	if(emptym($item))
	{
		continue;
	}
	if($number==0)
	{
		$page=$item;
		$number++;
	}
	else
	{
		//creates variables like $param1,$param2, etc.
		${"param".$number}=urldecode($item);
		$number++;
	}
}

//sitemap special case
if($page=="sitemap.xml")
{
	$page="sitemap";
}
if(preg_match("/^sitemap.+\.xml$/",$page))
{
	$matches=array();
	preg_match("/^sitemap-([tab])-([0-9]+)\.xml$/",$page,$matches);
	if(isset($matches[1])&&isset($matches[2]))
	{
		$param1=$matches[1];
		$param2=$matches[2];
		$page="sitemap";
	}
}

//padding
for($i=1;$i<10;$i++)
{
	if(!isset(${"param".$i}))
	{
		${"param".$i}=null;
	}
}

//clear away junk variables
unset($matches,$fullpath,$querystart,$path,$query,$last,$junk,$params,$count,$i,$number,$item);

//routing
if($rts&&$testnet)
{
	ini_set("zlib.output_compression","Off");
	require "includes/statx-testnet.php";
}
else if($rts)
{
	ini_set("zlib.output_compression","Off");
	require "includes/statx.php";
}
else if($testnet)
{
	require "includes/explore-testnet.php";
}
else
{
	require "includes/explore.php";
}
?>
