<?php

include_once 'includes/config.inc';
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

class Request {
    public $app = "explore";

    public $page = "home";
    public $params = array();

    public $testnet = false; // testnet mode

    public $scheme = null;
    public $path = null;
    private $query = null;

    const redirect_canonical = REDIRECT_CANONICAL;

    private function parse_uri() {
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

        $this->path = $path;
        $this->query = $query;
    }

    private function redirect_canonical() {
        //redirect odd link to canonical hostname 
        global $_SERVER;

        if(!isset($_SERVER['HTTP_HOST'])) {
            return;
        }

        $senthost=$_SERVER['HTTP_HOST'];

        if(preg_match_all("/[a-zA-Z]/",$senthost,$junk) > 6 && $senthost != HOSTNAME) {
            redirect($this->path.$this->query, 301);
            die();
        }
    }

    private function redirect_trailing_slash() {
        //trailing slash

        $last=strlen($this->path)-1;
        if($last!=0 && substr($this->path,$last,1) == "/") {
            redirect(substr($this->path,0,$last).$this->query,301);
        }
    }

    private function fix_url() {
        if(self::redirect_canonical) {
            $this->redirect_canonical();
        }
        $this->redirect_trailing_slash();
    }

    function __construct() {
        $this->scheme = isset($_SERVER['HTTPS']) ? "https://" : "http://";

        $this->parse_uri();
        $this->fix_url();

        $path = trim($this->path, "/");

        function _notempty($var) {
            return !(empty($var) && $var !== 0 && $var !== "0");
        }
        $params = array_filter(explode("/", $path, 10), "_notempty");

        function _array_remove_item(&$array, $item) {
            $index = array_search($item, $array);
            if($index === false) {
                return false;
            }
            array_splice($array, $index, 1);
            return true;
        }

        if(_array_remove_item($params, "testnet")) {
            $this->testnet = true;
        }
        if(_array_remove_item($params, "q")) {
            $this->app = "stats";
        }

        if(!empty($params)) {
            $this->page = array_shift($params);
            $this->params = array_map("urldecode", $params);
        }

        // sitemap special case
        if($this->page == "sitemap.xml") {

            $this->page = "sitemap";

        } else if(preg_match("/^sitemap.+\.xml$/", $page)) {

            $matches=array();
            preg_match("/^sitemap-([tab])-([0-9]+)\.xml$/", $page, $matches);

            if(isset($matches[1])&&isset($matches[2]))
            {
                $this->params = array($matches[1], $matches[2]);
                $this->page="sitemap";
            }
        }
    }
}

$req = new Request();
//header("Content-type: text/plain"); print_r($req); die();


if($req->app == "stats") {

	ini_set("zlib.output_compression","Off");
	require "includes/app_stats.inc";

    app_stats($req);

} else if ($req->app == "explore") {

	require "includes/app_explore.inc";
    app_explore($req);

} else {

    die("unknown app: " . $req->app);

}
