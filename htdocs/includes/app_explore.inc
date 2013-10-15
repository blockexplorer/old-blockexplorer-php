<?php

require_once 'encode.inc';
require_once 'sql.inc';
require_once 'cache.inc';
require_once 'Smarty.class.php' ;

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
        $handle_error(404, "No such page");

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

        if($etag == CACHE_ETAG_HASH)
            $etag = md5($view);

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
        return array(CACHE_NOEXPIRE, true);
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
        if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) || strlen($address)>36 || ! Address::check($address))
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
    if(Address::check($input))
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
    header("Access-Control-Allow-Origin: *");
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
    header("Access-Control-Allow-Origin: *");
    echo $raw;
}

function model_sitemap(&$vars, $params) {

    if($vars['scheme'] != 'http://') 
        throw new BadRequest("sitemap not supported via https");

    $cache_ttl = 600;
    $urls_per_section = SITEMAP_URLS_PER_SECTION;

    if(count($params) == 2) {
        $vars['sitemapindex'] = false;
        
        if(!in_array($params[0], array("a", "t", "b")))
            throw new BadRequest("no such section");

        $section = $params[0];
        $start = $params[1] * $urls_per_section;

        if($section == "a") {
            $sql = "SELECT '/address/' || address AS url,
                           id
                    FROM keys";

            $priority = "0.7";
            $changefreq = "hourly";
        }
        if($section == "t") {
            $sql = "SELECT '/tx/' || encode(hash, 'hex') AS url,
                           id
                    FROM transactions";
            $priority = "0.5";
            $changefreq = "monthly";
        }
        if($section == "b") {
            $sql = "SELECT '/block/' || encode(hash, 'hex') AS url,
                           number AS id
                    FROM blocks";
            $priority = "0.6";
            $changefreq = "monthly";
        }

        $sql .= " ORDER BY id 
                  OFFSET $1 
                  LIMIT $2";

        $query = SQL($sql, array($start, $urls_per_section));
        if(SQL::count($query) == $urls_per_section)
            $cache_ttl = CACHE_NOEXPIRE;

        $vars['query'] = $query;
        $vars['priority'] = $priority;
        $vars['changefreq'] = $changefreq;

    } else if (count($params) == 0) {
        $vars['sitemapindex'] = true;

        $stats = SQL::d("SELECT (SELECT count(number) FROM blocks) AS blocks, 
                                (SELECT count(id) FROM transactions) AS transactions,                                          
                                (SELECT count(id) FROM keys) AS addresses;"); 

        $vars['tx_sections'] = ceil($stats["transactions"] / $urls_per_section) - 1;
        $vars['block_sections'] = ceil($stats["blocks"] / $urls_per_section) - 1;
        $vars['address_sections'] = ceil($stats["addresses"] / $urls_per_section) - 1;


    } else {
        throw new BadRequest("invalid sitemap url");
    }

    header("Content-type: text/xml");
    return array($cache_ttl, CACHE_ETAG_HASH);
}

function model_rssa(&$vars, $params) {

    $address = preg_replace('/\.xml$/', '', $params[0]);

    if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) || !Address::check($address))
        throw new BadRequest("Invalid address");

    $hash160 = Address::toHash160($address);
    $query = SQL("SELECT to_char(blocks.time AT TIME ZONE 'UTC', 'Dy, DD Mon YYYY HH24:MI:SS +0000') AS time, 
                         outputs.value AS value, 
                         outputs.index AS oid, 
                         blocks.number AS number, 
                         encode(outputs.tx, 'hex') AS tx 

                  FROM outputs JOIN blocks ON 
                       (outputs.block = blocks.hash)

                  WHERE outputs.hash160 = decode($1, 'hex') 
                  ORDER BY outputs.id DESC LIMIT 20", 

                  $hash160);

    $vars['address' ] = $address;
    $vars['query'] = $query;

    $last_tx = SQL::d($query);
    SQL::seek($query, 0);

    $vars['builddate'] = $last_tx ? $last_tx['time'] : null;

    header("Content-type: application/xml; charset=iso-8859-1");

    return array(30, $last_tx['number']);
}
