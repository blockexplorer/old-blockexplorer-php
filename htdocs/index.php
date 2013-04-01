<?php

include_once 'includes/bootstrap.inc';
include_once 'includes/http.inc';

function _unknownerror($errno,$errstr,$errfile,$errline) {
	//suppressed errors
	if(error_reporting()==0)
		return;
	
    header("Content-type: text/html");

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
set_error_handler("_unknownerror",E_ALL^E_DEPRECATED^E_NOTICE);

function _parse_uri() {
    global $_SERVER;

    //set up variables for checking
    $fullpath=$_SERVER['REQUEST_URI'];
    $querystart=strpos($fullpath,"?");
    if($querystart === false) {
        $path=$fullpath;
        $query="";
    } else {
        $path=substr($fullpath,0,$querystart);
        $query="?".substr($fullpath,$querystart+1);
    }

    return array($path, $query);
}

function _redirect_canonical($path, $query) {
    //redirect odd link to canonical hostname 
    if(!isset($_SERVER['HTTP_HOST'])) {
        return;
    }

    $senthost=$_SERVER['HTTP_HOST'];

    if(preg_match_all("/[a-zA-Z]/",$senthost,$junk) > 6 && $senthost!=HOSTNAME) {
        redirect($path.$query, 301);
        die();
    }
}

function _redirect_trailing_slash($path, $query) {
    //trailing slash

    $last=strlen($path)-1;
    if($last!=0 && substr($path,$last,1) == "/") {
        redirect(substr($path,0,$last).$query,301);
    }
}

function _parse_path($path) {
    $path = trim($path, "/");
    $params=explode("/",$path,10);
    return $params;
}

list($path, $query) = _parse_uri();

REDIRECT_CANONICAL && _redirect_canonical();
_redirect_trailing_slash($path, $query);

$params = _parse_path($path);

//defaults
$page="home";

$testnet=false;
$rts=false;

function _empty($var) {
	return empty($var)&&$var!==0&&$var!=="0";
}

//tag and remove special views
$count=count($params);
for($i=0; $i<$count; $i++)
{
	if(_empty($params[$i])) {
		unset($params[$i]);
	} else if($params[$i]=="testnet") {
		$testnet=true;
		unset($params[$i]);
	} else if($params[$i]=="q") {
		$rts=true;
		unset($params[$i]);
	} else { 
        break;
	}
}

$number=0;
foreach($params as $item)
{
	if(_empty($item))
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
unset($matches,$path,$query,$junk,$params,$count,$i,$number,$item);

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
