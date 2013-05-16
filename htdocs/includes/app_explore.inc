<?php

define("CHECKPOINT", 128281);
define("MAINTMODE", false);
define("ADDRQUIT", true); // false will send broken HTML

// false = errors will be header errors; true = body errors
$error = false;
$title = "Error";

require_once 'encode.inc';
require_once 'sql.inc';
require_once 'cache.inc';
require_once 'Smarty.class.php' ;

date_default_timezone_set('UTC');
function error($message, $status = false)
{
    global $error;
    // also run goto end
	if($status !== false)
	{
		senderror($status);
	}
	if($error === false)
	{
		$error = $message;
	}
	else
	{
		echo "<p>".$message."</p>\n";
		echo "<p>Tell me (theymos) if this is a bug.</p>\n";
	}
}

function help($message)
{
	$encodemessage = urlencode($message);
	return "<sup><a href=\"/nojshelp/$encodemessage\" title=\"$message\" onClick=\"informHelp(); return false\" class=\"help\">?</a></sup>";
}

function removeTrailingZeroes($value)
{
	$end = strlen($value)-1;
	$i = $end;
	$target = 0;
	if(strpos($value, ".") != false)
	{
		while($i>0 && ($value[$i] == "0" || $value[$i] == "."))
		{
			$target++;
			if($value[$i] == ".")
			{
			break;
			}
			$i--;
		}
	}
	return $value = substr($value, 0, $end-$target+1);
}
function removeLeadingZeroes($value)
{
		while($value[0] == "0")
		{
			$value = substr($value, 1);
		}
		return $value;
}

function smarty_modifier_rzerotrim($value) {
    return removeTrailingZeroes($value);
}

function smarty_modifier_lzerotrim($value) {
    return removeLeadingZeroes($value);
}

function thousands($num)
{
	$start = strpos($num, ".");
	if($start === false)
	{
		$start = strlen($num)-1;
		$return = "";
	}
	else
	{
		$return = substr($num, $start);
		$start = $start-1;
	}
	$count = 0;
	for($i = $start;$i>-1;$i--)
	{
		$count++;
		$return = $num[$i].$return;
		if($count == 3 && $i != 0)
		{
			$return = " ".$return;
			$count = 0;
		}
	}
	return $return;
}

function smarty_modifier_thousands($value) {
    return thousands($value);
}

function smarty_modifier_fmt_bytesize($value) {
    if($value < 1000)
        return "$value bytes";

    $value /= 1000;
    return "$value kilobytes";
}

class Error extends Exception { } 
class BadRequest extends Error { }

function app_explore($req) 
{
    // try to get request from Cache first
    $cache_key = $req->path;
    if($cached = Cache::get($cache_key)) {
        echo $cached;
        return;
    }
        
    $config = $req->testnet ? "TestnetConfig" : "Config";
    SQL::init($config::$dbname);
    Address::$version = $config::$address_version;

    $handle_error = function($code, $msg) {
        senderror($code);

        $smarty = new Smarty();
        $smarty->assign('message', $msg);
        $smarty->display('error.tpl');

        die;
    };

    $vars = array('rootpath' => ($req->testnet ? "/testnet/" : "/"),
                  'scheme' => $req->scheme,
                  'page' => $req->page);

    $invoke_handler = function($handler) use (&$vars, $req, $handle_error) {
        try {
            return $handler($vars, $req->params);
        } catch (BadRequest $e) {
            $handle_error(400, $e->getMessage());
        } catch (Error $e) {
            $handle_error($e->getCode(), $e->getMessage());
        }
    };

    // get page function
    $page_func = "page_" . $req->page;
    if(function_exists($page_func))
        return $invoke_handler($page_func);

    // get redirect function
    $redirect_func = "redirect_" . $req->page;
    if(function_exists($redirect_func)) {
        $redirect = $invoke_handler($redirect_func);
        if($redirect) {
            if(is_array($redirect))
                redirect($redirect[0], $redirect[1]);
            else
                redirect($redirect);
        } else {
            $handle_error(404, "No such page");
        }
    }

    // get model function
    $model_func = "model_" . $req->page;

    if(!function_exists($model_func))
        return old_app_explore($req);
        //$handle_error(404, "No such page");

    $cache_conf = $invoke_handler($model_func);

    // render model into view
    $view_func = "view_" . $req->page;
    $view = (function_exists($view_func) ?  $view_func($vars, $req->page) :
                                            view_default($vars, $req->page));

    if(!$view)
        return;

    if($cache_conf) {
        $etag = null;

        if(is_array($cache_conf)) {
            $ttl = $cache_conf[0];
            $etag = $cache_conf[1];
        } else {
            $ttl = $cache_conf;
        }

        Cache::put($cache_key, $view, $ttl, $etag);
        if($etag)
            Cache::handle_client_side_etag($etag);
    }

    echo $view;
}

function view_default($vars, $page) {
    $smarty = new Smarty();

    $tpl = "explore/{$page}.tpl";
    if(!$smarty->templateExists($tpl))
        return var_dump($vars);

    foreach($vars as $key => $value)
        $smarty->assign($key, $value);

    return $smarty->fetch($tpl);
}

function model_home(&$vars, $params) {
    $vars['checkpoint'] = CHECKPOINT;
    $vars['keywords'] = array();
    
    $latestblock = SQL::s("SELECT max(number) FROM blocks;");
    $vars['latestblock'] = $latestblock;

    $query = SQL("SELECT number AS number, 
                         encode(hash, 'hex') AS hash, 
                         time AT TIME ZONE 'UTC' AS time, 
                         transactions AS count, 
                         totalvalue AS sum, size 
                         
                  FROM blocks 
                  
                  ORDER BY number DESC LIMIT 20;");

    $vars['query'] = $query;

    return array(30, $latestblock);
}

function model_txstats(&$vars, $params) {
    $vars['keywords'] = array();

    $latestblock = SQL::s("SELECT max(number) AS latest FROM blocks;");

    $vars['query_largest'] = SQL("SELECT encode(inputs.tx, 'hex') AS hash, 
                                         sum(inputs.value) AS totalvalue, 
                                         encode(blocks.hash, 'hex') AS blockhash, 
                                         blocks.number AS blocknum, 
                                         blocks.time AT TIME ZONE 'UTC' AS time 

                                  FROM blocks JOIN inputs ON 
                                       (inputs.block = blocks.hash) 

                                  WHERE blocks.number > (SELECT max(blocks.number)-300 FROM blocks) 
                                  GROUP BY blocks.time, inputs.tx, blocks.hash, blocks.number 
                                  ORDER BY totalvalue DESC LIMIT 20;");


    $vars['query_strange'] = SQL("SELECT DISTINCT encode(outputs.tx, 'hex') AS txhash, 
                                                  encode(outputs.block, 'hex') AS blockhash, 
                                                  outputs.id, blocks.number AS blocknum, 
                                                  blocks.time AT TIME ZONE 'UTC' AS time 
                                  FROM outputs JOIN blocks ON 
                                       (blocks.hash = outputs.block) 
                                  WHERE outputs.type = 'Strange' 
                                  ORDER BY outputs.id DESC LIMIT 20;"); 

    return array(60, $latestblock);
}

function model_block(&$vars, $params) {
    $block_hash = trim(strtolower(remove0x($params[0])));
    if(!preg_match("/^[0-9a-f]{64}$/", $block_hash))
        throw new BadRequest("Not in correct format");
    
    // Get block data
    $block = SQL::d("SELECT encode(prev, 'hex') AS prev, 
                                   number, 
                                   encode(root, 'hex') AS root, 
                                   bits, 
                                   nonce, 
                                   time AT TIME ZONE 'UTC' AS time, 
                                   transactions AS count, 
                                   totalvalue, 
                                   size 

                            FROM blocks 

                            WHERE hash = decode($1, 'hex');", $block_hash);
    if(!$block) 
        throw new Error("No such block", 404);

    $next = SQL::s("SELECT encode(hash, 'hex') 
                    FROM blocks 
                    WHERE prev = decode($1, 'hex');", array($block_hash));

    if($next)
        Cache::handle_client_side_etag();

    $vars['keywords'] = array("block", $block["number"], $block_hash);
    $vars['block'] = $block;
    $vars['block_hash'] = $block_hash;
    $vars['next'] = $next;

    // process data
    $vars['difficulty'] = bcdiv("26959535291011309493156476344723991336010898738574164086137773096960", 
                                (string)decodeCompact($block["bits"]), 6);


    // special transactions
    $vars['query_special_tx'] = SQL("SELECT encode(tx, 'hex') AS hash 
                                     FROM special 
                                     WHERE block = decode($1, 'hex')", $block_hash);
   

    // prepare SQL
    $vars['query_tx'] = SQL("SELECT encode(hash, 'hex') AS hash, 
                                    abs(fee) AS fee, size 
                             FROM transactions 
                             WHERE block = decode($1, 'hex') 
                             ORDER BY id;", $block_hash);

    SQLPrepare("tx_outputs", "SELECT outputs.value AS value, 
                                     keys.address AS address 
                              FROM outputs LEFT JOIN keys ON 
                                   (keys.hash160 = outputs.hash160) 
                              WHERE outputs.tx = decode($1, 'hex') 
                              ORDER BY outputs.id;");

    SQLPrepare("tx_inputs", "SELECT inputs.value AS value, 
                                    keys.address AS address 
                             FROM inputs LEFT JOIN keys ON 
                                  (keys.hash160 = inputs.hash160) 
                             WHERE inputs.tx = decode($1, 'hex') 
                             ORDER BY inputs.id;");

    if($next)
        return array(300, true);
    else
        return 300;
}

function model_tx(&$vars, $params) {
    // get tx hash
    $tx_hash = trim(strtolower(remove0x($params[0])));
    if(!preg_match("/^[0-9a-f]{64}$/", $tx_hash))
        throw new BadRequest("Not in correct format");

    $tx = SQL::d("SELECT id, 
                         encode(transactions.block, 'hex') AS block, 
                         transactions.fee AS fee, 
                         transactions.size AS size, 
                         blocks.time AT TIME ZONE 'UTC' AS time,
                         blocks.number AS blocknumber 

                  FROM transactions LEFT JOIN blocks ON 
                       (transactions.block = blocks.hash)

                  WHERE transactions.hash = decode($1, 'hex');", 

                 $tx_hash);

    if(!$tx)
        throw new Error("No such transaction", 404);

    $vars['tx'] = $tx;

    $vars['tx_hash'] = $tx_hash;
    
    $vars['outputs_total'] = SQL::s("SELECT sum(value)
                                     FROM outputs 
                                     WHERE tx = decode($1, 'hex')",
                                     $tx_hash);

    $vars['inputs_total'] = SQL::s("SELECT sum(value)
                                    FROM inputs
                                    WHERE tx = decode($1, 'hex')",
                                    $tx_hash);

    $vars['query_outputs'] = SQL("SELECT outputs.index AS index, 
                                         outputs.value AS value,
                                         keys.address AS address,
                                         outputs.type AS type,
                                         outputs.scriptpubkey AS scriptpubkey 

                                  FROM outputs LEFT JOIN keys ON 
                                       (keys.hash160 = outputs.hash160)

                                  WHERE outputs.tx = decode($1, 'hex') 

                                  ORDER BY outputs.index;", 

                                 $tx_hash);

    $vars['query_inputs'] = SQL("SELECT encode(inputs.prev, 'hex') AS prev, 
                                        inputs.index AS index,
                                        inputs.value AS value,
                                        keys.address AS address,
                                        inputs.type AS type,
                                        inputs.scriptsig AS scriptsig,
                                        inputs.id AS id 

                                 FROM inputs LEFT JOIN keys ON 
                                      (keys.hash160 = inputs.hash160)

                                 WHERE inputs.tx = decode($1, 'hex') 

                                 ORDER BY inputs.id;", 

                                 $tx_hash);
    
    $vars['query_duplicates'] = SQL("SELECT encode(block, 'hex') AS block 
                                     FROM special 
                                     WHERE tx=decode($1, 'hex')", $tx_hash);

    $vars['keywords'] = array("transaction", $tx_hash);

    SQLPrepare("redeemed", "SELECT id, 
                                   encode(tx, 'hex') AS tx 
                            FROM inputs 
                            WHERE prev=decode('$tx_hash', 'hex') AND 
                                  index=$1");

    return array(300, true);
}

function model_address(&$vars, $params) {
        // get address
        $address = $params[0];
        if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) || strlen($address)>36 || ! Address::checkAddress($address))
            throw new BadRequest("Invalid address");

        $hash160 = strtolower(Address::toHash160($address));

        $vars['address'] = $address;
        $vars['hash160'] = $hash160;

        $vars['keywords'] = array("address", $address, $hash160);

        $keyinfo = SQL::d("SELECT encode(pubkey, 'hex') AS pubkey, 
                                  encode(firstseen, 'hex') AS firstseen 
                           FROM keys 
                           WHERE hash160 = decode($1, 'hex');", $hash160);

        if($keyinfo)
            $blockinfo = SQL::d("SELECT number, 
                                        time AT TIME ZONE 'UTC' AS time 
                                 FROM blocks 
                                 WHERE hash=decode($1, 'hex');", $keyinfo['firstseen']);
        else
            $blockinfo = null;

        $vars['keyinfo'] = $keyinfo;
        $vars['blockinfo'] = $blockinfo;

        $vars['query_txs'] = SQL("SELECT inputs.type AS txtype, 
                                         'debit' AS type, 
                                         encode(inputs.tx, 'hex') AS tx, 
                                         inputs.value AS value, inputs.id AS id, 
                                         encode(transactions.block, 'hex') AS block, 
                                         blocks.number AS blocknum, 
                                         transactions.id AS tid, 
                                         inputs.index AS index, 
                                         blocks.time AT TIME ZONE 'UTC' AS time 
                                         
                                  FROM inputs, transactions, blocks 

                                  WHERE inputs.hash160 = decode($1, 'hex') AND 
                                        inputs.tx = transactions.hash AND 
                                        transactions.block = blocks.hash 
                                        
                                  UNION 
                                  
                                  SELECT outputs.type AS txtype, 
                                         'credit' AS type, 
                                         encode(outputs.tx, 'hex') AS tx, 
                                         outputs.value AS value, outputs.index AS id, 
                                         encode(transactions.block, 'hex') AS block, 
                                         blocks.number AS blocknum, 
                                         transactions.id AS tid, 
                                         outputs.index AS index, 
                                         blocks.time AT TIME ZONE 'UTC' AS time 
                                               
                                  FROM outputs, transactions, blocks 
                                  
                                  WHERE outputs.hash160 = decode($1, 'hex') AND 
                                        outputs.tx = transactions.hash AND 
                                        transactions.block = blocks.hash 
                                        
                                  ORDER BY blocknum, type, tid, index;", 
                                  
                                  $hash160);

        SQLPrepare("tx_outputs", "SELECT DISTINCT outputs.type AS type, 
                                         outputs.value AS value, 
                                         outputs.id, 
                                         keys.address AS address 

                                  FROM outputs LEFT JOIN keys ON 
                                       (outputs.hash160 = keys.hash160) 

                                  WHERE outputs.tx = decode($1, 'hex') 
                                  ORDER BY outputs.id"); 
        
        SQLPrepare("tx_inputs", "SELECT DISTINCT inputs.value AS value, 
                                        inputs.id, 
                                        inputs.type AS type, 
                                        keys.address AS address 

                                 FROM inputs LEFT JOIN keys ON 
                                      (inputs.hash160 = keys.hash160) 

                                 WHERE inputs.tx = decode($1, 'hex') 
                                 ORDER BY inputs.id;"); 
        
        // etag token
        $latest_tx_block = null;

        $query_txs = $vars['query_txs'];
        if($keyinfo) {

            SQL::seek($query_txs, SQL::count($query_txs) - 1);
            $latest_tx_block = SQL::d($query_txs)["blocknum"];
            SQL::seek($query_txs, 0);
        }

        $received_txs=0;
        $received_btc=0;

        $sent_txs=0;
        $sent_btc=0;

        while($tx = SQL::d($query_txs)) {

            if($tx["type"] == "credit") {
                $received_txs++;
                $received_btc = bcadd($received_btc, $tx['value'], 8);
            }
            elseif($tx['type'] == 'debit') {
                $sent_txs++;
                $sent_btc = bcadd($sent_btc, $tx['value'], 8);
            }

        }

        $vars['received_txs'] = $received_txs;
        $vars['received_btc'] = $received_btc;

        $vars['sent_txs'] = $sent_txs;
        $vars['sent_btc'] = $sent_btc;
            
        SQL::seek($query_txs, 0);

        return array(30, $latest_tx_block);
}

function model_nojshelp(&$vars, $params) {
    
        $title = "Scriptless help";
        $vars['help'] = htmlspecialchars(urldecode($params[0]));

        return array(0, true);
}

function redirect_search($vars, $params) {

    $rootpath = $vars['rootpath'];

    // The form on / POST submits to /search/, but I want it to go to a static page (without ?q= stuff)
    if(isset($_POST["q"]))
        return "{$rootpath}{$vars['page']}/{$_POST["q"]}";

    $input = $params[0];
    if(!$input)
        return $rootpath;

    $input = trim($input);
    if(!preg_match("/^[0-9A-HJ-NP-Za-km-z]+$/", $input))
        throw new BadRequest("Invalid characters");

    // block number
    if(preg_match("/^[0-9]+$/", $input))
    {
        $hash = SQL::s("SELECT encode(hash, 'hex') FROM blocks WHERE number = $1;", $input);
        if($hash) 
            return "{$rootpath}block/$hash";

    }
    // size limits
    if(strlen($input) < 6 || strlen($input) > 130)
        throw new BadRequest("The number of characters you entered is either too small (must be 6+), or too large to ever return any results (130 hex characters is the size of a public key).");

    // address
    if(strlen($input) < 36 && !preg_match("/0/", $input)) {
        $exists = SQL::s("SELECT 1 FROM keys WHERE address = $1;", $input);
        if($exists)
            return "{$rootpath}address/$input";
    }

    // hex only from here
    $input = strtolower(remove0x($input));

    // block hash
    $exists = SQL::s("SELECT 1 FROM blocks WHERE hash = decode($1, 'hex');", $input);
    if($exists)
        return "{$rootpath}block/$input";

    // tx hash
    $exists = SQL::s("SELECT 1 FROM transactions WHERE hash = decode($1, 'hex');", $input);
    if($exists)
        return "{$rootpath}tx/$input";

    // hash160
    $address = SQL::s("SELECT address FROM keys WHERE hash160 = decode($1, 'hex');", $input);
    if($address) 
        return "{$rootpath}address/$address";

    // unseen address/hash160
    if(Address::checkAddress($input))
        return "{$rootpath}address/$input";

    if(strlen($input) == 40 && preg_match("/[0-9a-f]{4,130}/", $input))
        return "{$rootpath}address/" .  $input;
}

function redirect_b($vars, $params) {
    if(!preg_match("/^[0-9]{1,7}$/", $params[0]))
        return;

    $result = SQL::s("SELECT encode(hash, 'hex') FROM blocks WHERE number = $1;", $params[0]);
    if($result)
        return array("{$vars['rootpath']}block/$result", 301);
}

function redirect_t($vars, $params) {
    if(!preg_match("/^[1-9A-HJ-NP-Za-km-z]+$/", $params[0]))
        return;

    $shortcut = decodeBase58($params[0]);
    $hash = SQL::s("SELECT encode(hash, 'hex') AS hash 
                    FROM t_shortlinks 
                    WHERE shortcut = decode($1, 'hex');", $shortcut);
    if($hash)
        return array("{$vars['rootpath']}tx/$hash", 301);
}

function redirect_a(&$vars, $params) {
    if(!preg_match("/^[1-9A-HJ-NP-Za-km-z]{7,20}$/", $params[0]))
        return;

    $shortcut = decodeBase58($params[0]);
    $address = SQL::s("SELECT address 

                       FROM a_shortlinks JOIN keys ON 
                            (keys.hash160 = a_shortlinks.hash160) 

                       WHERE shortcut = decode($1, 'hex');", $shortcut);
    if($address)
        return array("{$vars['rootpath']}address/$address", 301);
}

function page_rawtx($vars, $params) {
    $tx = trim(strtolower(remove0x($params[0])));
    if(!preg_match("/^[0-9a-f]{64}$/", $tx))
        throw new BadRequest("Not in correct format");

    $raw = SQL::s("SELECT raw 
                   FROM transactions 
                   WHERE hash = decode($1, 'hex');", $tx);
    if(!$raw)
        throw new Error("Transaction does not exist.", 404);

    Cache::handle_client_side_etag();
    header("Content-type: text/plain");
    echo $raw;
}

function page_rawblock($vars, $params) {
    $block = trim(strtolower(remove0x($params[0])));
    if(!preg_match("/^[0-9a-f]{64}$/", $block))
        throw new BadRequest("Not in correct format");

    $raw = SQL::s("SELECT raw 
                   FROM blocks 
                   WHERE hash = decode($1, 'hex');", array($block));
    if(!$raw)
        throw new Error("Block does not exist.", 404);

    Cache::handle_client_side_etag();
    header("Content-type: text/plain");
    echo $raw;
}

function old_app_explore($req) {
    global $error;
    global $title;

    $config = $req->testnet ? "TestnetConfig" : "Config";
    SQL::init($config::$dbname);
    Address::$version = $config::$address_version;

    $rootpath = $req->testnet ? "/testnet/" : "/";

    if(MAINTMODE !== false && $_SERVER["REMOTE_ADDR"] != "192.168.1.1")
    {
        error("Bitcoin Block Explorer will be back shortly.", 503);
        $title = "Maintenance mode";
        goto headerend;
    }

    if($req->page == "sitemap")
    {
        if($req->scheme == "http://")
        {
            $buffer = "";
            $returnedresults = 0;
            $interval = 10000;
            if(isset($req->params[0]) && isset($req->params[1]) && ($req->params[0] == "a" || $req->params[0] == "t" || $req->params[0] == "b") && $req->params[1] >= 0 && $req->params[1]<500)
            {
                $start = $req->params[1]*$interval;
                if($req->params[0] == "a")
                {
                    $data = SQL("SELECT '/address/' || address AS url, 
                                        id 
                                 FROM keys 
                                 ORDER BY id 

                                 OFFSET $1 
                                 LIMIT $2;", array($start, $interval));
                }
                if($req->params[0] == "t")
                {
                    $data = SQL("SELECT '/tx/' || encode(hash, 'hex') AS url, 
                                        id 
                                 FROM transactions 
                                 ORDER BY id 
                                 
                                 OFFSET $1 
                                 LIMIT $2;", array($start, $interval));
                }
                if($req->params[0] == "b")
                {
                    $data = SQL("SELECT '/block/' || encode(hash, 'hex') AS url, 
                                        number AS id 
                                 FROM blocks 
                                 ORDER BY id 
                                 
                                 OFFSET $1 
                                 LIMIT $2;", array($start, $interval));
                }
                $returnedresults = SQL::count($data);
                $oneurl = SQL::a($data);
                $buffer.= '<?xml version = "1.0" encoding = "ISO-8859-1"?>'."\n";
                $buffer.= '<urlset xmlns = "http://www.sitemaps.org/schemas/sitemap/0.9">'."\n";
                while($oneurl)
                {
                    $buffer.= '<url>'."\n";
                    $buffer.= "<loc>http://".HOSTNAME."{$oneurl["url"]}</loc>"."\n";
                    if($req->params[0] == "a")
                    {
                        $priority = "0.7";
                        $changefreq = "hourly";
                    }
                    if($req->params[0] == "t")
                    {
                        $priority = "0.5";
                        $changefreq = "monthly";
                    }
                    if($req->params[0] == "b")
                    {
                        $priority = "0.6";
                        $changefreq = "monthly";
                    }
                    if(isset($priority) && isset($changefreq))
                    {
                        $buffer.= "<changefreq>$changefreq</changefreq>"."\n";
                        $buffer.= "<priority>$priority</priority>"."\n";
                    }
                    unset($priority, $changefreq);
                    $buffer.= '</url>'."\n";
                    $oneurl = SQL::a($data);
                }
                $buffer.= '</urlset>';
            }
            if(!isset($req->params[0]) && !isset($req->params[1]))
            {
                $data = SQL::d("SELECT (SELECT count(number) FROM blocks) AS blocks, 
                                       (SELECT count(id) FROM transactions) AS transactions,                                          
                                       (SELECT count(id) FROM keys) AS addresses;"); 
                $totaltx = ceil($data["transactions"]/$interval)-1;
                $totalblk = ceil($data["blocks"]/$interval)-1;
                $totaladdr = ceil($data["addresses"]/$interval)-1;
                
                $buffer.= '<?xml version = "1.0" encoding = "ISO-8859-1"?>'."\n";
                $buffer.= '<sitemapindex xmlns = "http://www.sitemaps.org/schemas/sitemap/0.9">'."\n";
                for($i = 0;$i <= $totaltx;$i++)
                {
                    $buffer.= "<sitemap>"."\n";
                    $buffer.= "<loc>http://".HOSTNAME."/sitemap-t-$i.xml</loc>"."\n";
                    $buffer.= "</sitemap>"."\n";
                }
                for($i = 0;$i <= $totaladdr;$i++)
                {
                    $buffer.= "<sitemap>"."\n";
                    $buffer.= "<loc>http://".HOSTNAME."/sitemap-a-$i.xml</loc>"."\n";
                    $buffer.= "</sitemap>"."\n";
                }
                for($i = 0;$i <= $totalblk;$i++)
                {
                    $buffer.= "<sitemap>"."\n";
                    $buffer.= "<loc>http://".HOSTNAME."/sitemap-b-$i.xml</loc>"."\n";
                    $buffer.= "</sitemap>"."\n";
                }
                $buffer.= '</sitemapindex>';
            }
            $bufferhash = hash("md4", $buffer);

            Cache::handle_client_side_etag($bufferhash, ($returnedresults == $interval) ? 604800 : 600);

            header("Content-type: text/xml");
            echo $buffer;
            die();
        }
    }

    if($req->page == "rssa")
    {
        header("Content-type: application/xml; charset = iso-8859-1");
        $address = substr($req->params[0], 0, strlen($req->params[0])-4);
        if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) ||strlen($address)>36 || ! Address::checkAddress($address))
        {
            senderror(400);
            header("Content-type: text/plain");
            echo "ERROR: invalid address";
            die();
        }
        $hash160 = Address::toHash160($address);
        $query = SQL("SELECT to_char(blocks.time AT TIME ZONE 'UTC', 'Dy, DD Mon YYYY HH24:MI:SS +0000') AS time, 
                             outputs.value AS value, 
                             outputs.index AS oid, 
                             blocks.number AS number, 
                             encode(outputs.tx, 'hex') AS tx 

                      FROM outputs JOIN blocks ON 
                           (outputs.block = blocks.hash)

                      WHERE outputs.hash160 = decode($1, 'hex') 
                      ORDER BY outputs.id DESC LIMIT 20;", 

                      $hash160);

        $onetx = SQL::a($query);
        if($onetx === false)
        {
            Cache::handle_client_side_etag("e");
        }
        else
        {
            $builddate = $onetx["time"];
            $anchor = substr($onetx["tx"], 0, 16)."o{$onetx["oid"]}";
            Cache::handle_client_side_etag($anchor);
        }
        echo '<?xml version = "1.0" encoding = "ISO-8859-1" ?>'."\n";
        // echo ""."\n";
        echo '<rss version = "2.0" xmlns:atom = "http://www.w3.org/2005/Atom">'."\n";
        echo '<channel>'."\n";
        echo "<description>Latest received transactions for $address</description>"."\n";
        echo "<link>{$req->scheme}".HOSTNAME."${rootpath}address/$address</link>"."\n";
        echo "<title>BBE - $address</title>"."\n";
        echo '<atom:link href="'."{$req->scheme}".HOSTNAME."{$rootpath}rssa/$address.xml".'" rel = "self" type = "application/xml" />'."\n";
        if(isset($builddate))
        {
            echo "<lastBuildDate>$builddate</lastBuildDate>"."\n";
        }
        
        // content
        while($onetx)
        {
            $anchor = substr($onetx["tx"], 0, 16)."o{$onetx["oid"]}";
            $number = $onetx["number"];
            $time = $onetx["time"];
            $amount = removeTrailingZeroes($onetx["value"]);
            echo "<item>\n";
            echo "<description>$address received $amount BTC at $time in block number $number.</description>\n";
            echo "<guid>{$req->scheme}".HOSTNAME."${rootpath}address/$address#$anchor</guid>\n";
            echo "<pubDate>$time</pubDate>\n";
            echo "<title>Received $amount BTC</title>\n";
            echo "<link>{$req->scheme}".HOSTNAME."${rootpath}address/$address#$anchor</link>"."\n";
            echo "</item>\n";
            $onetx = SQL::a($query);
        }
        // end content
        echo '</channel>'."\n";
        echo '</rss>'."\n";
        
        die();
    }

    // do not index pages that are not blockexplorer.com
    if($req->page == "robots.txt" && ( ! isset($senthost) || $senthost != $my_name))
    {
        error_log("Deny search engine using odd host $senthost");
        Cache::handle_client_side_etag();
        header("Content-type: text/plain");
        echo "User-agent: *
    Disallow: /";
        die();
    }

    // This must be set
    if($error === false)
    {
        $error = true;
    }
    // Haven't done anything? Page doesn't exist.
    if($title == "Error" && is_bool($error))
    {
    error("No such page", 404);
    }
    headerend:

    echo<<<'EOD'

<!DOCTYPE HTML PUBLIC "-//W3C// DTD HTML 4.01// EN"
   "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<script type = "text/javascript">

  var _gaq = _gaq || [];
  _gaq.push(['_setAccount', 'UA-38773634-1']);
  _gaq.push(['_trackPageview']);

  (function() {
    var ga = document.createElement('script'); ga.type = 'text/javascript'; ga.async = true;
    ga.src = ('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
    var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
  })();

</script>
<link rel = "shortcut icon" href="/favicon.ico">
EOD;

    if(isset($insertrss)) {
        echo '<link rel = "alternate" type = "application/rss+xml" title = "RSS" href="'.$insertrss.'">'."\n";
    }
    echo '<meta http-equiv = "Content-type" content = "text/html;charset = ISO-8859-1">';

    if(isset($keywords)) {
        if($keywords != "") {
            $keywords = ", ".$keywords;
        }
        echo "<meta name = \"keywords\" content = \"bitcoin, search, data$keywords\">\n";
    }
    if(isset($description))
        echo "<meta name = \"description\" content = \"$description\">\n";

    echo<<<"EOD"
<title>$title - Bitcoin Block Explorer</title>
<style type = "text/css">
.infoList{list-style-type:none;margin-left:0;padding-left:0}
table{border-collapse:collapse}
table, td, th{border:1px solid black;padding:4px}
div.hugeCell{width:300px;overflow:auto}
div.hugeData{width:700px;overflow:auto}
#footer{text-align:center;font-size:smaller;margin-top:2em}
div#shortlink{font-size:smaller;margin-top:-1.5em;margin-bottom:-1em;margin-left:0.5em}
.help{cursor:help}
</style>
<script type = "text/javascript">
function highlightNamedAnchor()
{
    if(location.hash != "")
    {
        document.getElementsByName(location.hash.substr(1, location.hash.length))[0].parentNode.parentNode.style.backgroundColor = "#FFFDD0";
    }
}
function informHelp()
{
    alert("These question mark links produce help text when you hover your mouse cursor over them.");
}
</script>
</head>
<body onLoad = "highlightNamedAnchor()">
EOD;

    // error handling for header errors
    if(!is_bool($error)) {
        echo "<h1>Error</h1>";
        echo $error;
        goto end;
    }

    // /search
    if($req->page == "search")
    {
        $donesomething = false;
        if(!empty($transactions))
        {
            echo "<h3>Transactions</h3>\n";
            echo "<ul>\n";
            foreach ($transactions as $i)
            {
                echo "<li><a href=\"{$rootpath}tx/$i\">$i</a></li>\n";
                $donesomething = true;
            }
            echo "</ul>\n";
        }
        if(!empty($addresses))
        {
            echo "<h3>Addresses</h3>\n";
            echo "<ul>\n";
            foreach ($addresses as $i)
            {
                echo "<li><a href=\"{$rootpath}address/$i\">$i</a></li>\n";
                $donesomething = true;
            }
            echo "</ul>\n";
        }
        if(!empty($blocks))
        {
            echo "<h3>Blocks</h3>\n";
            echo "<ul>\n";
            foreach ($blocks as $i)
            {
                echo "<li><a href=\"{$rootpath}block/$i\">$i</a></li>\n";
                $donesomething = true;
            }
            echo "</ul>\n";
        }
        if($donesomething === false)
        {
            error("No results.");
            goto end;
        }
        else
        {
            echo "<p>Note: results may not be complete if the search would return more than 100 items.</p>\n";
        }
    }

    end:

    echo<<<'EOD'
<div id = "footer"><hr><a href="/">Bitcoin Block Explorer</a> (Mirror ad: <a href="https://www.privateinternetaccess.com/" title = "VPN Service">VPN Service</a>) - Donate: <a href="/address/1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28">1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28</a></div>
</body>
</html>
EOD;

}
