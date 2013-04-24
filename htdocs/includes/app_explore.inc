<?php

define("VERSION", 19);
define("CHECKPOINT", 128281);
define("MAINTMODE", false);
define("ADDRQUIT", true); // false will send broken HTML

// false = errors will be header errors; true = body errors
$error = false;
$title = "Error";

require_once 'encode.inc';
require_once 'db.inc';

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

function cacheput($key, $data, $secs, $etag = 0, $headerexpires = 86400)
{
	$shm = "/tmp/bbe/";
	if(!file_exists($shm))
	{
		if(!mkdir($shm))
			return false;
		chmod($shm, 0775);
	}
	
	$length = strlen($data);
	$chk = crc32($data);
	$expires = time()+$secs;
	$header = "$expires;$length;$chk;$etag;$headerexpires";
	$data = $header."\n\n".$data;
	
	$file = fopen($shm.$key, "c");
	if($file === false)
		return false;
	if(!flock($file, LOCK_EX))
		return false;
	ftruncate($file, 0);
	fwrite($file, $data);
	flock($file, LOCK_UN);
	fclose($file);
}

function cacheget($key)
{
    if(!ENABLE_CACHING)
        return false;

	$shm = "/tmp/bbe/";
	if(!file_exists($shm))
	{
		if(!mkdir($shm))
			return false;
		chmod($shm, 0775);
	}
	
	if(!file_exists($shm.$key))
	{
		return false;
	}
	
	$file = @fopen($shm.$key, "r");
	if($file === false)
		return false;
	if(!flock($file, LOCK_SH))
		return false;
	$header = explode(";", fgets($file));
	$time = $header[0];
	$length = $header[1];
	$chk = $header[2];
	if(isset($header[3])) // REMOVE THE CHECK LATER
	{
		$etag = $header[3];
		$headerexpires = $header[4];
	}
	else
	{
		$cache = 0;
		$headerexpires = 0;
	}
	
	if(empty($time) || empty($length) || empty($chk))
		return false;
	
	if($time<time())
	{
		flock($file, LOCK_UN);
		fclose($file);
		$file = fopen($shm.$key, "c");
		if($file === false)
			return false;
		flock($file, LOCK_EX);
		@unlink($shm.$key);
		flock($file, LOCK_UN);
		fclose($file);
		return false;
	}
	
	// cache
	if($etag != 0)
	{
		if(cache($etag, $headerexpires, false, false))
		{
			flock($file, LOCK_UN);
			fclose($file);
			die();
		}
	}
	
	fgets($file); // advance pointer
	$data = fread($file, $length);
	if(!empty($data) && strlen($data) == $length && crc32($data) == $chk)
	{
		flock($file, LOCK_UN);
		fclose($file);
		return $data;
	}
	else
	{
		error_log("Bad cache file: $key, length $length");
		flock($file, LOCK_UN);
		fclose($file);
		$file = fopen($shm.$key, "c");
		flock($file, LOCK_EX);
		unlink($shm.$key);
		flock($file, LOCK_UN);
		fclose($file);
		return false;
	}
}

function cache($etag = VERSION, $customcache = false, $override = false, $die = true)
{
    if(!ENABLE_CACHING) 
        return true;

	$etag = (string)$etag;
	$baseetag = $etag;
	if($etag != VERSION)
	{
		$etag = $etag."-".VERSION;
	}
	$etag = "W/\"$etag\"";
	header("ETag: $etag");
	if($override == true)
	{
		if($customcache === false)
		{
			if($baseetag == VERSION)
			{
				$cachetime = 86400;
			}
			else
			{
				$cachetime = -1500;
			}
		}
		else
		{
			$cachetime = $customcache;
		}
		if(is_int($cachetime))
		{
			header('Expires: '.gmdate('D, d M Y H:i:s \G\M\T', time() + $cachetime));
		}
	}
	else
	{
		header('Expires: '.gmdate('D, d M Y H:i:s \G\M\T', time() -1500));
	}
	
	if(isset($_SERVER['HTTP_IF_NONE_MATCH']) && !is_null($_SERVER['HTTP_IF_NONE_MATCH']) && !empty($_SERVER['HTTP_IF_NONE_MATCH']))
	{
		$tags = stripslashes($_SERVER['HTTP_IF_NONE_MATCH']);
		$tags = preg_split("/, /", $tags );
		foreach($tags as $tag)
		{
			if($tag == $etag)
			{
				header($_SERVER["SERVER_PROTOCOL"]." 304 Not Modified");
				if($die)
				{
					die();
				}
				else
				{
					return true;
				}
				
			}
		}
	}
	else
	{
		return false;
	}
}

function help($message)
{
	$encodemessage = urlencode($message);
	return "<sup><a href=\"/nojshelp/$encodemessage\"title = \"$message\" onClick = \"informHelp();return false\" class = \"help\">?</a></sup>";
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

function decodeCompact($c)
{
	$nbytes = ($c >> 24) & 0xFF;
	return bcmul($c & 0xFFFFFF, bcpow(2, 8 * ($nbytes - 3)));
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

function app_explore($req) {
    global $error;
    global $title;

    $config = $req->testnet ? "TestnetConfig" : "Config";
    db_init($config::$dbname);
    Address::$version = $config::$address_version;

    $rootpath = $req->testnet ? "/testnet/" : "/";

    if(MAINTMODE !== false && $_SERVER["REMOTE_ADDR"] != "192.168.1.1")
    {
        error("Bitcoin Block Explorer will be back shortly.", 503);
        $title = "Maintenance mode";
        goto headerend;
    }


    // /Homepage
    if($req->page == "home")
    {
        $cache = cacheget("home");
        if($cache !== false)
        {
            echo $cache;
            goto end;
        }
        ob_start();
        
        $title = "Home";
        $description = "Bitcoin Block Explorer is a web tool that provides detailed information about Bitcoin blocks, addresses, and transactions.";
        $keywords = "";
        $result = db_query("SELECT number AS number, encode(hash, 'hex') AS hash, time AT TIME ZONE 'UTC' AS time, transactions AS count, totalvalue AS sum, size FROM blocks ORDER BY number DESC LIMIT 20;");
        $oneblock = pg_fetch_assoc($result);
        $endingcache = $oneblock["number"];
        cache($endingcache);
    }
    if($req->page == "txstats")
    {
        $cache = cacheget("txstats");
        if($cache !== false)
        {
            echo $cache;
            goto end;
        }
        ob_start();
        
        $title = "Transaction stats (temporary)";
        $description = "Largest transactions and strange transactions.";
        $keywords = "";
        $latestblock = pg_fetch_array(db_query("SELECT max(number) AS latest FROM blocks;"));
        cache($latestblock['latest']);
    }
    // /Search
    if($req->page == "search") {

        // The form on / POST submits to /search/, but I want it to go to a static page (without ?q= stuff)
        if(isset($_POST["q"]))
        {
            redirect("{$rootpath}$req->page/{$_POST["q"]}");
        }
        $title = "Search";
        $input = $req->params[0];
        if($input === NULL)
        {
            redirect($rootpath);
        }
        $input = trim($input);
        if(!preg_match("/^[0-9A-HJ-NP-Za-km-z]+$/", $input))
        {
            error("Invalid characters.", 400);
            goto headerend;
        }

        // block number
        if(preg_match("/^[0-9]+$/", $input))
        {
            $hash = pg_fetch_result(db_query("SELECT encode(hash, 'hex') FROM blocks WHERE number = $1;", array($input)), 0);
            if($hash) 
                redirect("{$rootpath}block/$hash");

        }
        // size limits
        if(strlen($input)<6 || strlen($input)>130)
        {
            error("The number of characters you entered is either too small (must be 6+), or too large to ever return any results (130 hex characters is the size of a public key).", 400);
            goto headerend;
        }
        // address
        if(strlen($input)<36 && !preg_match("/0/", $input))
        {
            $exists = pg_fetch_result(db_query("SELECT 1 FROM keys WHERE address = $1;", array($input)), 0);
            if($exists)
                redirect("{$rootpath}address/$input");
        }
        // hex only from here
        $originput = $input;
        $input = strtolower(remove0x($input));

        // block hash
        $exists = pg_fetch_result(db_query("SELECT 1 FROM blocks WHERE hash = decode($1, 'hex');", array($input)), 0);
        if($exists)
            redirect("{$rootpath}block/$input");

        // tx hash
        $exists = pg_fetch_result(db_query("SELECT 1 FROM transactions WHERE hash = decode($1, 'hex');", array($input)), 0);
        if($exists)
            redirect("{$rootpath}tx/$input");

        // hash160

        $address = pg_fetch_result(db_query("SELECT address FROM keys WHERE hash160 = decode($1, 'hex');", array($input)), 0);
        if($address) 
            redirect("{$rootpath}address/$address");

        // unseen address/hash160
        if(Address::checkAddress($originput))
        {
            redirect("{$rootpath}address/$originput");
        }

        if(strlen($input) == 40 && preg_match("/[0-9a-f]{4,130}/", $input))
        {
            $input = Address::hash160ToAddress($input);
            redirect("{$rootpath}address/$input");
        }
    }
    // //short links
    if($req->page == "b")
    {
        if(preg_match("/^[0-9]{1,7}$/", $req->params[0]))
        {
            $result = pg_fetch_result(db_query("SELECT encode(hash, 'hex') FROM blocks WHERE number = $1;", array($req->params[0])), 0);

            if($result)
                redirect("{$rootpath}block/$result", 301);
        }
    }
    if($req->page == "t")
    {
        if(preg_match("/^[1-9A-HJ-NP-Za-km-z]+$/", $req->params[0]))
        {
            $shortcut = decodeBase58($req->params[0]);
            $hash = pg_fetch_result(db_query("SELECT encode(hash, 'hex') AS hash FROM t_shortlinks WHERE shortcut = decode($1, 'hex');", array($shortcut)), 0);
            if($hash)
                redirect("{$rootpath}tx/$hash", 301);
        }
    }
    if($req->page == "a")
    {
        if(preg_match("/^[1-9A-HJ-NP-Za-km-z]{7,20}$/", $req->params[0]))
        {
            $shortcut = decodeBase58($req->params[0]);
            $address = pg_fetch_result(db_query("SELECT address FROM a_shortlinks JOIN keys ON (keys.hash160 = a_shortlinks.hash160) WHERE shortcut = decode($1, 'hex');", array($shortcut)), 0);
            if($address)
                redirect("{$rootpath}address/$address", 301);
        }
    }

    // /Raw block/tx
    if($req->page == "rawtx")
    {
        header("Content-type: text/plain");
        $tx = trim(strtolower(remove0x($req->params[0])));
        if(!preg_match("/^[0-9a-f]{64}$/", $tx))
        {
            senderror(400);
            echo "ERROR: Not in correct format";
            die();
        }
        $raw = pg_fetch_result(db_query("SELECT raw FROM transactions WHERE hash = decode($1, 'hex');", array($tx)), 0);
        if(!$raw) {
            senderror(404);
            echo "ERROR: Transaction does not exist.";
            die();
        }
        else
        {
        cache();
        echo $raw;
        die();
        }
    }
    if($req->page == "rawblock")
    {
        header("Content-type: text/plain");
        $block = trim(strtolower(remove0x($req->params[0])));
        if(!preg_match("/^[0-9a-f]{64}$/", $block))
        {
            senderror(400);
            echo "ERROR: Not in correct format";
            die();
        }
        $raw = pg_fetch_result(db_query("SELECT raw FROM blocks WHERE hash = decode($1, 'hex');", array($block)), 0);
        if(!$raw) {
            senderror(404);
            echo "ERROR: Block does not exist.";
            die();
        }
        else
        {
        cache();
        echo $raw;
        die();
        }
    }
    // /Block
    if($req->page == "block")
    {
        $cache = cacheget($req->page.md5($req->params[0]));
        if($cache !== false)
        {
            echo $cache;
            goto end;
        }
        ob_start();
        
        // Get hash
        $block = trim(strtolower(remove0x($req->params[0])));
        if(!preg_match("/^[0-9a-f]{64}$/", $block))
        {
            error("Not in correct format", 400);
            goto headerend;
        }
        
        // Get block data
        $result = pg_fetch_assoc(db_query("SELECT encode(prev, 'hex') AS prev, number, encode(root, 'hex') AS root, bits, nonce, time AT TIME ZONE 'UTC' AS time, transactions AS count, totalvalue, size FROM blocks WHERE hash = decode($1, 'hex');", array($block)));
        if(!$result || is_null($result))
        {
            error("No such block", 404);
            goto headerend;
        }
        $next = pg_fetch_result(db_query("SELECT encode(hash, 'hex') FROM blocks WHERE prev = decode($1, 'hex');", array($block)), 0);

        if($next)
        {
            cache();
        }
        else
        {
            // Don't cache blocks that are the latest (we're waiting for the next block)
            $donotcache = true;
            cache(VERSION."o", 0);
        }
        
        
        $title = "Block {$result["number"]}";
        $description = "List of transactions in Bitcoin block #{$result["number"]}.";
        $keywords = "block, {$result["number"]}, $block";
    }
    // /Transaction
    if($req->page == "tx")
    {
        $cache = cacheget($req->page.md5($req->params[0]));
        if($cache !== false)
        {
            echo $cache;
            goto end;
        }
        ob_start();
        
        // get tx hash
        $hash = trim(strtolower(remove0x($req->params[0])));
        if(!preg_match("/^[0-9a-f]{64}$/", $hash))
        {
            error("Not in correct format", 400);
            goto headerend;
        }
        $hashtrunc = substr($hash, 0, 10)."...";

        $tx = pg_fetch_assoc(db_query("SELECT id, encode(transactions.block, 'hex') AS block, transactions.fee AS fee, transactions.size AS size, blocks.time AT TIME ZONE 'UTC' AS time, blocks.number AS blocknumber FROM transactions LEFT JOIN blocks ON transactions.block = blocks.hash WHERE transactions.hash = decode($1, 'hex');", array($hash)));
        if($tx === false || is_null($tx))
        {
            error("No such transaction", 404);
            goto headerend;
        }
        // cache();
        // this needs to be fixed
        
        $outputs = db_query("SELECT outputs.index AS index, outputs.value AS value, sum(outputs.value) OVER () AS totalvalue, keys.address AS address, outputs.type AS type, outputs.scriptpubkey AS scriptpubkey FROM outputs LEFT JOIN keys ON keys.hash160 = outputs.hash160 WHERE outputs.tx = decode($1, 'hex') ORDER BY outputs.index;", array($hash));
        $inputs = db_query("SELECT encode(inputs.prev, 'hex') AS prev, inputs.index AS index, inputs.value AS value, sum(inputs.value) OVER () AS totalvalue, keys.address AS address, inputs.type AS type, inputs.scriptsig AS scriptsig, inputs.id AS id FROM inputs LEFT JOIN keys ON keys.hash160 = inputs.hash160 WHERE inputs.tx = decode($1, 'hex') ORDER BY inputs.id;", array($hash));
        $oneoutput = pg_fetch_assoc($outputs);
        $oneinput = pg_fetch_assoc($inputs);
        
        $totalin = thousands(removeTrailingZeroes($oneinput["totalvalue"]));
        $totalout = thousands(removeTrailingZeroes($oneoutput["totalvalue"]));
        $fee = removeTrailingZeroes($tx["fee"]);
        $numin = pg_num_rows($inputs);
        $numout = pg_num_rows($outputs);
        
        $title = "Tx $hashtrunc";
        $description = "Information about Bitcoin transaction $hashtrunc.";
        $keywords = "transaction, $hash";
    }

    // /Address
    if($req->page == "address")
    {
        $cachename = $req->params[0];
        $cache = cacheget("addr".md5($cachename));
        if($cache !== false)
        {
            echo $cache;
            goto end;
        }
        ob_start();

        $knownaddress = false;
        // get address
        $address = $req->params[0];
        if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) || strlen($address)>36 || ! Address::checkAddress($address))
        {
            error("Invalid address", 400);
            goto headerend;
        }
        $insertrss = "{$req->scheme}".HOSTNAME."{$rootpath}rssa/$address.xml";
        $hash160 = strtolower(addressToHash160($address));
        $keyinfo = pg_fetch_assoc(db_query("SELECT encode(pubkey, 'hex') AS pubkey, encode(firstseen, 'hex') AS firstseen FROM keys WHERE hash160 = decode($1, 'hex');", array($hash160)));
        $pubkey = $keyinfo["pubkey"];
        if($pubkey === false || is_null($pubkey))
        {
            $pubkey = "Unknown (not seen yet)";
        }
        $firstseen = $keyinfo["firstseen"];
        if($firstseen !== false && !is_null($firstseen))
        {
            $knownaddress = true;
            $blockinfo = pg_fetch_assoc(db_query("SELECT number, time AT TIME ZONE 'UTC' AS time FROM blocks WHERE hash = decode($1, 'hex');", array($firstseen)));
            $blocknum = $blockinfo["number"];
            $blocktime = $blockinfo["time"];
            $blockstring = "<a href=\"{$rootpath}block/$firstseen\">Block $blocknum</a> ($blocktime)";
        }
        else
        {
            $blockstring = "Never used on the network (as far as I can tell)";
        }
        $title = "Address $address";
        $description = "List of transactions involving Bitcoin address $address.";
        $keywords = "address, $address, $hash160";
        
        $mytxs = db_query("SELECT inputs.type AS txtype, 'debit' AS type, encode(inputs.tx, 'hex') AS tx, inputs.value AS value, inputs.id AS id, encode(transactions.block, 'hex') AS block, blocks.number AS blocknum, transactions.id AS tid, inputs.index AS index, blocks.time AT TIME ZONE 'UTC' AS time FROM inputs, transactions, blocks WHERE inputs.hash160 = decode($1, 'hex') AND inputs.tx = transactions.hash AND transactions.block = blocks.hash UNION SELECT outputs.type AS txtype, 'credit' AS type, encode(outputs.tx, 'hex') AS tx, outputs.value AS value, outputs.index AS id, encode(transactions.block, 'hex') AS block, blocks.number AS blocknum, transactions.id AS tid, outputs.index AS index, blocks.time AT TIME ZONE 'UTC' AS time FROM outputs, transactions, blocks WHERE outputs.hash160 = decode($1, 'hex') AND outputs.tx = transactions.hash AND transactions.block = blocks.hash ORDER BY blocknum, type, tid, index;", array($hash160));
        $txlimit = pg_num_rows($mytxs)-1;
        $txcounter = 0;
        // cache
        if($knownaddress)
        {
            $latesttx = pg_fetch_assoc($mytxs, $txlimit);
            $latesttx = $latesttx["blocknum"];
        }
        else
        {
            $latesttx = -1;
        }
        cache($latesttx);
    }
    if($req->page == "nojshelp")
    {
        cache();
        $title = "Scriptless help";
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
                    $data = db_query("SELECT '/address/' || address AS url, id FROM keys ORDER BY id OFFSET $1 LIMIT $2;", array($start, $interval));
                }
                if($req->params[0] == "t")
                {
                    $data = db_query("SELECT '/tx/' || encode(hash, 'hex') AS url, id FROM transactions ORDER BY id OFFSET $1 LIMIT $2;", array($start, $interval));
                }
                if($req->params[0] == "b")
                {
                    $data = db_query("SELECT '/block/' || encode(hash, 'hex') AS url, number AS id FROM blocks ORDER BY id OFFSET $1 LIMIT $2;", array($start, $interval));
                }
                $returnedresults = pg_num_rows($data);
                $oneurl = pg_fetch_array($data);
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
                    $oneurl = pg_fetch_array($data);
                }
                $buffer.= '</urlset>';
            }
            if(!isset($req->params[0]) && !isset($req->params[1]))
            {
                $data = pg_fetch_assoc(db_query("SELECT (SELECT count(number) FROM blocks) AS blocks, (SELECT count(id) FROM transactions) AS transactions, (SELECT count(id) FROM keys) AS addresses;"));
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
            if($returnedresults == $interval)
            {
                $expires = 604800;
            }
            else
            {
                $expires = 600;
            }
            cache($bufferhash, $expires, true);
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
        $hash160 = addressToHash160($address);
        $query = db_query("SELECT to_char(blocks.time AT TIME ZONE 'UTC', 'Dy, DD Mon YYYY HH24:MI:SS +0000') AS time, outputs.value AS value, outputs.index AS oid, blocks.number AS number, encode(outputs.tx, 'hex') AS tx FROM outputs JOIN blocks ON outputs.block = blocks.hash WHERE outputs.hash160 = decode($1, 'hex') ORDER BY outputs.id DESC LIMIT 20;", array($hash160));
        $onetx = pg_fetch_array($query);
        if($onetx === false)
        {
            cache("e");
        }
        else
        {
            $builddate = $onetx["time"];
            $anchor = substr($onetx["tx"], 0, 16)."o{$onetx["oid"]}";
            cache($anchor);
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
            $onetx = pg_fetch_array($query);
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
        cache();
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

    if($req->page == "nojshelp") {
        $req->params[0] = htmlspecialchars(urldecode($req->params[0]));
        if(strlen($req->params[0])<4000) {
            echo "<p>These question mark links produce help text when you hover your mouse cursor over them. In case your client does not support displaying HTML title attributes, here is the text for the link you clicked:</p>\n";
            echo "<p><i>$req->params[0]</i></p>\n";
        }
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

    if($req->page == "home")
    {
        $help = help("Like this");

    echo <<<"EOD"

<h1>Bitcoin Block Explorer</h1>
<p>Bitcoin Block Explorer allows you to easily view information about the
<a href="https://en.bitcoin.it/wiki/Blocks">blocks</a>, <a href="https://en.bitcoin.it/wiki/Address">addresses</a>, and <a href="https://en.bitcoin.it/wiki/Transactions">transactions</a> created by <a href="http://bitcoin.org">Bitcoin</a>.
It uses the <a href="https://bitcointalk.org/index.php?topic = 724.0">getblock</a> patch by jgarzik, but also does a
ton of processing to make certain tasks, such as tracking transactions, easier. Help text is included in the tooltips produced by superscript question marks: $help. All times are UTC. Tell me (theymos) if you find any bugs.

<p>Some data from Bitcoin Block Explorer is available through the machine-readable <a href="{$rootpath}q">Real-Time Stats pages</a>.

<p>Thanks to <a href="https://www.privateinternetaccess.com/" title = "VPN Service">Private Internet Access</a> for running a mirror of Bitcoin Block Explorer. They've asked me to advertise their site: <a href="https://www.privateinternetaccess.com/" title = "VPN Service">VPN Service</a> (accepts Bitcoin ! ).</p>

EOD;
        
        $latestblock = pg_fetch_array(db_query("SELECT max(number) AS latest FROM blocks;"));
        $latestblock = $latestblock["latest"];
        if($latestblock<CHECKPOINT)
        {
            echo "<p><span style = \"color:red\">Notice:</span> The block database is currently being reloaded (probably to enable some cool new feature). You can refresh this page to see the progress.</p>";
        }
        
        echo "<h3>Search</h3>\n";
        echo '<p>You can enter a block number, address, block hash, transaction hash, hash160. Hashes are expressed in hexadecimal.</p>'."\n";
        echo '<form action = "/search" method = "post" ><p><input type = "text" name = "q" size = "50"><input type = "submit" value = "Search"></p></form>'."\n";
        
        // /latest blocks
        echo "<h3>Latest blocks".help("Up to two minutes delay.")."</h3>\n";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Number".help("A count of the number of blocks up to this one, with the genesis block being 0.")."</th><th>Hash".help("Truncated hash of this block.")."</th><th>Time".help("UTC time included in this block. The network's time must not be relied upon for precision, but it is generally accurate.")."</th><th>Transactions".help("Number of transactions in this block. All blocks have at least one generation transaction.")."</th><th>Total BTC".help("Total BTC moved by the transactions in this block.")."</th><th>Size (kB)".help("The data size of this block. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).")."</th></tr>\n";
        while($oneblock)
        {
            $hash = $oneblock["hash"];
            $hashtrunc = removeLeadingZeroes($hash);
            $hashtrunc = substr($hashtrunc, 0, 10)."...";
            
            $number = $oneblock["number"];
            $time = $oneblock["time"];
            $total = $oneblock["count"];
            $size = round($oneblock["size"]/1000, 3);
            $totalbtc = removeTrailingZeroes($oneblock["sum"]);
            echo "<tr>\n";
            echo "<td><a href=\"{$rootpath}block/$hash\">$number</a></td>\n";
            echo "<td><a href=\"{$rootpath}block/$hash\">$hashtrunc</a></td>\n";
            echo "<td>$time</td>\n";
            echo "<td>$total</td>\n";
            echo "<td>$totalbtc</td>\n";
            echo "<td>$size</td>\n";
            echo "</tr>\n";
            $oneblock = pg_fetch_assoc($result);
        }
        echo "</table>\n";
        
        // /largest transactions last 300 blocks
        /*$result = db_query("SELECT encode(inputs.tx, 'hex') AS hash, sum(inputs.value) AS totalvalue, encode(blocks.hash, 'hex') AS blockhash, blocks.number AS blocknum, blocks.time AT TIME ZONE 'UTC' AS time FROM blocks JOIN inputs ON (inputs.block = blocks.hash) WHERE blocks.number>(SELECT max(blocks.number)-300 FROM blocks) GROUP BY blocks.time, inputs.tx, blocks.hash, blocks.number ORDER BY totalvalue DESC LIMIT 20;");
        echo "<h3>Largest transactions (last 300 blocks)".help("Sorted by BTC moved")."</h3>\n";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Transaction".help("Truncated transaction hash.")."</th><th>Amount".help("BTC moved.")."</th><th>Block".help("The block this transaction appeared in.")."</th><th>Time".help("UTC network time of the block this appeared in (somewhat unreliable).")."</th></tr>\n";
        $onetx = pg_fetch_assoc($result);
        while($onetx)
        {
            $txhash = $onetx["hash"];
            $txhashtrunc = "<a href=\"{$rootpath}tx/$txhash\">".substr($txhash, 0, 10)."...</a>";
            $amount = removeTrailingZeroes($onetx["totalvalue"]);
            $blockhash = $onetx["blockhash"];
            $blockview = "<a href=\"{$rootpath}block/$blockhash\">".$onetx["blocknum"]."</a>";
            $time = $onetx["time"];
            echo "<tr><td>$txhashtrunc</td><td>$amount</td><td>$blockview</td><td>$time</td></tr>";
            $onetx = pg_fetch_assoc($result);
        }
        echo "</table>\n";
        
        // /latest strange transactions
        echo "<h3>Latest strange transactions</h3>";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Transaction".help("Truncated transaction hash.")."</th><th>Block".help("The block this transaction appeared in.")."</th><th>Time".help("UTC network time of the block this appeared in (somewhat unreliable).")."</th></tr>\n";
        $result = db_query("SELECT DISTINCT encode(outputs.tx, 'hex') AS txhash, encode(outputs.block, 'hex') AS blockhash, outputs.id, blocks.number AS blocknum, blocks.time AT TIME ZONE 'UTC' AS time FROM outputs JOIN blocks ON (blocks.hash = outputs.block) WHERE outputs.type = 'Strange' ORDER BY outputs.id DESC LIMIT 20;"); 
        $onetx = pg_fetch_assoc($result);
        while($onetx)
        {
            $txhash = $onetx["txhash"];
            $txhashtrunc = "<a href=\"{$rootpath}tx/$txhash\">".substr($txhash, 0, 10)."...</a>";
            $blockhash = $onetx["blockhash"];
            $blockview = "<a href=\"{$rootpath}block/$blockhash\">".$onetx["blocknum"]."</a>";
            $time = $onetx["time"];
            echo "<tr><td>$txhashtrunc</td><td>$blockview</td><td>$time</td></tr>";
            $onetx = pg_fetch_assoc($result);
        }
        echo "</table>";*/
        
        echo "<p>Largest transactions and strange transactions have been temporarily moved <a href=\"{$rootpath}txstats\">here</a>.</p>";
        
        cacheput("home", ob_get_flush(), 30, $endingcache, 0);
    }

    if($req->page == "txstats")
    {
        echo "<p>Because getting this data is currently very slow, this data has been moved here from the front page until I rewrite my database code.</p>";
        
        $result = db_query("SELECT encode(inputs.tx, 'hex') AS hash, sum(inputs.value) AS totalvalue, encode(blocks.hash, 'hex') AS blockhash, blocks.number AS blocknum, blocks.time AT TIME ZONE 'UTC' AS time FROM blocks JOIN inputs ON (inputs.block = blocks.hash) WHERE blocks.number>(SELECT max(blocks.number)-300 FROM blocks) GROUP BY blocks.time, inputs.tx, blocks.hash, blocks.number ORDER BY totalvalue DESC LIMIT 20;");
        echo "<h3>Largest transactions (last 300 blocks)".help("Sorted by BTC moved")."</h3>\n";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Transaction".help("Truncated transaction hash.")."</th><th>Amount".help("BTC moved.")."</th><th>Block".help("The block this transaction appeared in.")."</th><th>Time".help("UTC network time of the block this appeared in (somewhat unreliable).")."</th></tr>\n";
        $onetx = pg_fetch_assoc($result);
        while($onetx)
        {
            $txhash = $onetx["hash"];
            $txhashtrunc = "<a href=\"{$rootpath}tx/$txhash\">".substr($txhash, 0, 10)."...</a>";
            $amount = removeTrailingZeroes($onetx["totalvalue"]);
            $blockhash = $onetx["blockhash"];
            $blockview = "<a href=\"{$rootpath}block/$blockhash\">".$onetx["blocknum"]."</a>";
            $time = $onetx["time"];
            echo "<tr><td>$txhashtrunc</td><td>$amount</td><td>$blockview</td><td>$time</td></tr>";
            $onetx = pg_fetch_assoc($result);
        }
        echo "</table>\n";

        echo "<h3>Latest strange transactions</h3>";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Transaction".help("Truncated transaction hash.")."</th><th>Block".help("The block this transaction appeared in.")."</th><th>Time".help("UTC network time of the block this appeared in (somewhat unreliable).")."</th></tr>\n";
        $result = db_query("SELECT DISTINCT encode(outputs.tx, 'hex') AS txhash, encode(outputs.block, 'hex') AS blockhash, outputs.id, blocks.number AS blocknum, blocks.time AT TIME ZONE 'UTC' AS time FROM outputs JOIN blocks ON (blocks.hash = outputs.block) WHERE outputs.type = 'Strange' ORDER BY outputs.id DESC LIMIT 20;"); 
        $onetx = pg_fetch_assoc($result);
        while($onetx)
        {
            $txhash = $onetx["txhash"];
            $txhashtrunc = "<a href=\"{$rootpath}tx/$txhash\">".substr($txhash, 0, 10)."...</a>";
            $blockhash = $onetx["blockhash"];
            $blockview = "<a href=\"{$rootpath}block/$blockhash\">".$onetx["blocknum"]."</a>";
            $time = $onetx["time"];
            echo "<tr><td>$txhashtrunc</td><td>$blockview</td><td>$time</td></tr>";
            $onetx = pg_fetch_assoc($result);
        }
        echo "</table>";
        cacheput("txstats", ob_get_flush(), 60, $latestblock['latest'], 0);
    }

    if($req->page == "block")
    {
        // process data
        $target = (string)decodeCompact($result["bits"]);
        $difficulty = thousands(removeTrailingZeroes(bcdiv("26959535291011309493156476344723991336010898738574164086137773096960", $target, 6)));
        $prev = $result["prev"];
        $totalvalue = removeTrailingZeroes($result["totalvalue"]);
        $transactioncount = $result["count"];
        
        echo "<h1>Block {$result["number"]}".help("The number is a count of the number of blocks up to this one, with the genesis block being 0.")."</h1>\n";
        $shortlink = "{$rootpath}b/{$result['number']}";
        echo "<div id = \"shortlink\">Short link: <a href = \"$shortlink\">".$req->scheme.HOSTNAME."$shortlink</a></div>";
        echo "<ul class = \"infoList\">\n";
        echo "<li>Hash".help("Full hash of this block. Sometimes this is expressed without the leading zeroes.").": $block</li>\n";
        if($prev != "0000000000000000000000000000000000000000000000000000000000000000")
        {
            echo "<li>Previous block".help("Every block builds on another, forming a chain. This is the full hash of the previous block.").": <a href=\"{$rootpath}block/$prev\">$prev</a></li>\n";
        }
        if($next != false && !is_null($next))
        {
            echo "<li>Next block".help("The full hash of the block that will build onto this one. This field is not included in real blocks.").": <a href=\"{$rootpath}block/$next\">$next</a></li>\n";
        }
        echo "<li>Time".help("UTC time included in this block. The network's time must not be relied upon for precision, but it is generally accurate.").": {$result["time"]}</li>\n";
        echo "<li>Difficulty".help("The difficulty of producing blocks at the time this block was created. Same as Bitcoin's getdifficulty.").": $difficulty (\"Bits\"".help("This is the compact form of the 256-bit target used when generating. This is included in actual blocks. The difficulty number is derived from this.").": ".strtolower(encodeHex($result["bits"])).")</li>\n";
        echo "<li>Transactions".help("Number of transactions in this block (listed below)").": $transactioncount</li>\n";
        echo "<li>Total BTC".help("Total BTC sent through this block, including fees").": $totalvalue</li>\n";
        $properbytes = $result["size"];
        if($properbytes<1000)
        {
            $properbytes = "$properbytes bytes";
        }
        else
        {
            $properbytes = $properbytes/1000;
            $properbytes = "$properbytes kilobytes";
        }
        echo "<li>Size".help("The data size of this block. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).").": $properbytes</li>\n";
        echo "<li>Merkle root".help("The root hash in a hash tree of all transactions.").": {$result["root"]}</li>\n";
        echo "<li>Nonce".help("When generating, Bitcoin starts this number at 1 and increments for each hash attempt.").": {$result["nonce"]}</li>\n";
        echo "<li><a href=\"{$rootpath}rawblock/$block\">Raw block</a>".help("Almost the same as getblock's output.")."</li>\n";
        echo "</ul>";
        
        echo "<h3>Transactions</h3>\n";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Transaction".help("Truncated hash of this transaction")."</th><th>Fee".help("Fee given - the difference between total input value and total output value. This goes to the generator of the block.")."</th><th>Size (kB)".help("The data size of this transaction. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).")."</th><th>From (amount)".help("List of all addresses that appear in an input. Whoever sent this transaction owns all of these addresses.")."</th><th>To (amount)".help("A list of all addresses that have received bitcoins from this transaction")."</th></tr>\n";
        
        // prepare SQL

        db_prepare("transactions", "SELECT encode(hash, 'hex') AS hash, abs(fee) AS fee, size FROM transactions WHERE block = decode($1, 'hex') ORDER BY id;");

        db_prepare("outputs", "SELECT outputs.value AS value, keys.address AS address FROM outputs LEFT JOIN keys ON keys.hash160 = outputs.hash160 WHERE outputs.tx = decode($1, 'hex') ORDER BY outputs.id;");

        db_prepare("inputs", "SELECT inputs.value AS value, keys.address AS address FROM inputs LEFT JOIN keys ON keys.hash160 = inputs.hash160 WHERE inputs.tx = decode($1, 'hex') ORDER BY inputs.id;");
        
        $coinbase = true;
        
        // special transactions
        $result = db_query("SELECT encode(tx, 'hex') AS hash FROM special WHERE block = decode($1, 'hex')", array($block));
        $row = pg_fetch_assoc($result);
        while($row)
        {
            $hash = $row["hash"];
            $hashtrunc = "<a href=\"{$rootpath}tx/$hash\">".substr($hash, 0, 10)."...</a>";
            echo "<tr><td colspan = \"5\">This transaction is an exact copy of $hashtrunc. This is usually caused by flawed custom miner code that rarely changes the keys used by generations, and is therefore likely to produce a generation transaction with the exact same data as a previous one by the same person. The network sees duplicate transactions as the same: only one can be redeemed.</td></tr>\n";
            $coinbase = false;
            $row = pg_fetch_assoc($result);
        }
        
        // get list of transactions
        $result = db_execute("transactions", array($block));

        $row = pg_fetch_assoc($result);
        
        // go through each transaction
        while($row)
        {
        echo "<tr>\n";
            $hash = $row["hash"];
            // /// UPDATE
            $hashtrunc = "<a href=\"{$rootpath}tx/$hash\">".substr($hash, 0, 10)."...</a>";
            $fee = removeTrailingZeroes($row["fee"]);
            if($coinbase == true)
            {
                $totalfee = $fee;
                $fee = "0";
            }
            $size = round($row["size"]/1000, 3);
            echo "<td>$hashtrunc</td><td>$fee</td><td>$size</td>\n";
            
            // collect inputs
            $inputs = db_execute("inputs", array($hash));
            $oneinput = pg_fetch_assoc($inputs);
            echo "<td><ul class = \"infoList\">\n";
            // go through inputs
            while($oneinput)
            {
                // parse and linkify address
                $address = $oneinput["address"];
                if(is_null($address))
                {
                    $address = "Unknown";
                }
                else
                {
                    $address = "<a href=\"{$rootpath}address/$address\">$address</a>";
                }
                
                // parse value
                $value = removeTrailingZeroes($oneinput["value"]);
                if(is_null($value))
                {
                    error("No value found", 500);
                    goto end;
                }
                if($coinbase == true)
                {
                    $address = "Generation";
                    $value = "$value + $totalfee total fees";
                }
                
                echo "<li>$address: $value</li>\n";
                
                $oneinput = pg_fetch_assoc($inputs);
            }
            echo "</ul></td>\n";
            
            // collect outputs
            $outputs = db_execute("outputs", array($hash));
            $oneoutput = pg_fetch_assoc($outputs);
            echo "<td><ul class = \"infoList\">\n";
            // go through outputs
            while($oneoutput)
            {
                // parse and linkify addresses
                $address = $oneoutput["address"];
                if(is_null($address))
                {
                    $address = "Unknown";
                }
                else
                {
                    $address = "<a href=\"{$rootpath}address/$address\">$address</a>";
                }
                
                // parse value
                $value = removeTrailingZeroes($oneoutput["value"]);
                if(is_null($value))
                {
                    error("No value found", 500);
                    goto end;
                }
                
                echo "<li>$address: $value</li>\n";
                
                $oneoutput = pg_fetch_assoc($outputs);
            }
            echo "</ul></td>\n";
            
            $coinbase = false;
            
            echo "</tr>\n";
            
            $row = pg_fetch_assoc($result);		
        }
        
        echo "</table>\n";
        if(!isset($donotcache))
        {
            cacheput("block".md5($req->params[0]), ob_get_flush(), 300, VERSION);
        }
        else
        {
            cacheput("block".md5($req->params[0]), ob_get_flush(), 300, VERSION."o", 0);
        }
    }

    if($req->page == "tx")
    {
        echo "<h1>Transaction</h1>\n";
        echo "<ul class = \"infoList\">\n";
        
        echo "<li>Hash".help("Full hash of this transaction").": $hash</li>\n";
        echo "<li>Appeared in <a href=\"{$rootpath}block/{$tx["block"]}\">block {$tx["blocknumber"]}</a> ({$tx["time"]})</li>\n";
        echo "<li>Number of inputs".help("Total number of previous outputs this transaction redeems").": $numin (<a href=\"#inputs\">Jump to inputs</a>)</li>\n";
        echo "<li>Total BTC in".help("Total BTC redeemed from previous transactions").": $totalin</li>\n";
        echo "<li>Number of outputs: $numout (<a href=\"#outputs\">Jump to outputs</a>)</li>\n";
        echo "<li>Total BTC out".help("Total BTC sent with this transaction.").": $totalout</li>\n";
        $properbytes = $tx["size"];
        if($properbytes<1000)
        {
            $properbytes = "$properbytes bytes";
        }
        else
        {
            $properbytes = $properbytes/1000;
            $properbytes = "$properbytes kilobytes";
        }
        echo "<li>Size".help("The data size of this transaction. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).").": $properbytes</li>\n";
        echo "<li>Fee".help("The amount of BTC given to the person who generated the block this appeared in. It's the difference between total BTC in and total BTC out.").": $fee</li>\n";
        echo "<li><a href=\"{$rootpath}rawtx/$hash\">Raw transaction</a>".help("Almost the same as getblock.")."</li>\n";
        // duplicate transactions
        $duplicate = "";
        $duplicates = db_query("SELECT encode(block, 'hex') AS block FROM special WHERE tx = decode($1, 'hex')", array($hash));
        $onedup = pg_fetch_assoc($duplicates);
        $firstdup = true;
        if($onedup)
        {
            $duplicate .= "<li>Duplicates".help("An exact copy of this transaction appeared in these blocks. These copies are not spendable.").":";
            while($onedup)
            {
                $hashtrunc = substr($hashtrunc, 0, 10)."...";
                $blockhash = $onedup["block"];
                if(!$firstdup)
                {
                    $duplicate .= " , ";
                }
                $duplicate .= "<a href=\"{$rootpath}block/$blockhash\">".substr(removeLeadingZeroes($blockhash), 0, 10)."</a>";
                $onedup = pg_fetch_assoc($duplicates);
            }
            $duplicate .= "</li>";
        }
        echo $duplicate;
        // end duplicates
        echo "</ul>\n";
        // inputs
        echo "<h3><a name = \"inputs\">Inputs</a>".help("Each input redeems a previous output with a signature.")."</h3>\n";
        
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Previous output (index)".help("The truncated hash of a previous transaction and the index of the output that this input is redeeming (after the colon). The first output in a transaction has an index of 0.")."</th><th>Amount".help("Amount of BTC gotten from this output")."</th><th>From address".help("The addresses of the referenced outputs. Whoever sent this transaction owns all of these addresses.")."</th><th>Type".help("The type of the referenced output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")."</th><th>ScriptSig".help("This script is matched with the referenced output's scriptPubKey. It usually contains a signature, and possibly a public key. ScriptSigs of generation inputs are sometimes called the 'coinbase' parameter, and they contain the current compact target and the extraNonce variable")."</th></tr>\n";
        // go through inputs
        while($oneinput)
        {
            $type = $oneinput["type"];
            $value = removeTrailingZeroes($oneinput["value"]);
            if($type == "Generation")
            {
                $value = "$value + fees";
            }
            $myid = $oneinput["id"];
            echo "<tr>\n";
            $prev = $oneinput["prev"];
            $previndex = $oneinput["index"];
            $prevtrunc = substr($prev, 0, 12)."...:$previndex";
            if(!is_null($prev))
            {
            echo "<td><a name = \"i$myid\" href=\"{$rootpath}tx/$prev#o$previndex\">$prevtrunc</a></td>\n";
            }
            else
            {
                echo "<td><a name = \"i$myid\">N/A</a></td>\n";
            }
            echo "<td>$value</td>\n";
            $address = $oneinput["address"];
            if(is_null($address))
            {
                $address = "Unknown";
                if($type == "Generation")
                {
                    $address = "N/A";
                }
            }
            else
            {
                $address = "<a href=\"{$rootpath}address/$address\">$address</a>";
            }
            echo "<td>$address</td>\n";
            echo "<td>$type</td>\n";
            echo "<td><div class = \"hugeCell\">{$oneinput["scriptsig"]}</div></td>\n";
            echo "</tr>\n";
            $oneinput = pg_fetch_assoc($inputs);
        }
        
        echo "</table>\n";
        
        // outputs
        echo "<h3><a name = \"outputs\">Outputs</a>".help("Each output sends BTC to some address. In the official client, usually one output sends coins to the destination, and one output sends coins back to a new address owned by the sender.")."</h3>\n";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Index".help("Starts at 0 and increments for each output.")."</th><th>Redeemed at input".help("If this output has ever been spent by the recipient, the transaction that did it is listed here. (If you look at these links, you will see that I assign a number to each input. This is an internal ID unrelated to Bitcoin.)")."</th><th>Amount".help("BTC sent by this output")."</th><th>To address".help("Addresses this output was sent to")."</th><th>Type".help("The type of the output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")."</th><th>ScriptPubKey".help("This script specifies the conditions that must be met by someone attempting to redeem this output. Usually it contains a hash160 (Bitcoin address) or a public key.")."</th></tr>\n";
        // prepare query

        db_prepare("redeemed", "SELECT id, encode(tx, 'hex') AS tx FROM inputs WHERE prev = decode('$hash', 'hex') AND index = $1");
        // go through outputs
        while($oneoutput)
        {
            $index = $oneoutput["index"];
            echo "<tr>\n";
            $redeemed = pg_fetch_assoc(db_execute("redeemed", array($index)));
            if($redeemed !== false)
            {
                $rtx = $redeemed["tx"];
                $rid = $redeemed["id"];
                $rtxtrunc = "<a name = \"o$index\" href=\"{$rootpath}tx/$rtx#i$rid\">".substr($rtx, 0, 12)."...</a>";
            }
            else
            {
                $rtxtrunc = "<a name = \"o$index\">Not yet redeemed</a>";
            }
            echo "<td>$index</td>\n";
            echo "<td>$rtxtrunc</td>\n";
            $value = removeTrailingZeroes($oneoutput["value"]);
            echo "<td>$value</td>\n";
            $address = $oneoutput["address"];
            if(is_null($address))
            {
                $address = "Unknown";
            }
            else
            {
                $address = "<a href=\"{$rootpath}address/$address\">$address</a>";
            }
            echo "<td>$address</td>\n";
            echo "<td>{$oneoutput["type"]}</td>\n";
            echo "<td><div class = \"hugeCell\">{$oneoutput["scriptpubkey"]}</div></td>\n";
            $oneoutput = pg_fetch_assoc($outputs);
            echo "</tr>\n";
        }
        echo "</table>\n";
        
        cacheput("tx".md5($req->params[0]), ob_get_flush(), 300, VERSION);
    }

    if($req->page == "address")
    {
        $currentaddress = $address;
        echo "<h1>Address $address</h1>\n";
        echo "<ul class = \"infoList\">\n";
        echo "<li>First seen".help("The first block this address was used in.").": $blockstring</li>\n";
        
        // loop through our transactions. This is all echoed later.
        $balance = "0";
        db_prepare("outputcounter", "SELECT DISTINCT outputs.type AS type, outputs.value AS value, outputs.id, keys.address AS address FROM outputs LEFT JOIN keys ON outputs.hash160 = keys.hash160 WHERE outputs.tx = decode($1, 'hex') ORDER BY outputs.id"); 
        
        db_prepare("inputcounter", "SELECT DISTINCT inputs.value AS value, inputs.id, inputs.type AS type, keys.address AS address FROM inputs LEFT JOIN keys ON inputs.hash160 = keys.hash160 WHERE inputs.tx = decode($1, 'hex') ORDER BY inputs.id;"); 
        
        $echothis = "";
        $totalcredits = 0;
        $totalcredit = "0";
        $totaldebits = 0;
        $totaldebit = "0";
        $stopcaching = false;
        while($txcounter <= $txlimit)
        {
            // limits
            if($totalcredits+$totaldebits>5000 && !$stopcaching)
            {
                ob_end_clean();
                $stopcaching = true;
                if(ADDRQUIT)
                {
                    senderror(503);
                    header("Content-type: text/plain");
                    echo("ERROR: Address ledger is extremely large. Contact me if you really need the data.");
                    die();
                }
            }
            $onetx = pg_fetch_assoc($mytxs, $txcounter);
            $txcounter++;
            $echothis.= "<tr>\n";
            
            // get value, type, and whether Credit OR Debit (echoed a little later)
            $value = removeTrailingZeroes($onetx["value"]);
            $type = $onetx["txtype"];
            if($onetx["type"] == "credit")
            {
                $cord = "Received";
                $totalcredits++;
                $totalcredit = bcadd($totalcredit, $value, 8);
            }
            else
            {
                $cord = "Sent";
                $totaldebits++;
                $totaldebit = bcadd($totaldebit, $value, 8);
            }
            
            // get tx info
            $tx = $onetx["tx"];
            $index = $onetx["id"];
            if($cord == "Received")
            {
                $anchorname = substr($tx, 0, 16)."o$index";
                $txtrunc = "<a name = \"$anchorname\" href=\"{$rootpath}tx/$tx#o$index\">".substr($tx, 0, 10)."...</a>";
            }
            else
            {
                $anchorname = substr($tx, 0, 16)."i$index";
                $txtrunc = "<a name = \"$anchorname\" href=\"{$rootpath}tx/$tx#i$index\">".substr($tx, 0, 10)."...</a>";
            }
            $echothis.= "<td>$txtrunc</td>\n";
            
            // get block info
            $block = $onetx["block"];
            $txtime = $onetx["time"];
            $blocknum = $onetx["blocknum"];
            $blockstring = "<a href=\"{$rootpath}block/$block\">Block $blocknum</a> ($txtime)";
            $echothis.= "<td>$blockstring</td>\n";
            
            $echothis.= "<td>$value</td>\n";
            
            // /// /// /transaction loop. Echoed just a bit later
            // loop through counter ins if out.
            $superechothis = "";
            $superechothis.= "<td><ul class = \"infoList\">\n";
            if($cord == "Received")
            {
                $counter = db_execute("inputcounter", array($tx));
                $thisio = pg_fetch_assoc($counter);
                while($thisio)
                {
                    $address = $thisio["address"];
                    $addressstring = "<a href=\"{$rootpath}address/$address\">$address</a>";
                    if(is_null($address) || $address === false || $address == "Unknown")
                    {
                        $addressstring = "Unknown";
                        if($thisio["type"] == "Generation")
                        {
                            $addressstring = "Generation";
                        }
                    }
                    if($address == $currentaddress)
                    {
                    $addressstring = "$address";
                    }
                    $superechothis.= "<li>$addressstring</li>\n";
                    $thisio = pg_fetch_assoc($counter);
                }
            }
            else // loop through counter outs if in
            {
                $type = "";
                $counter = db_execute("outputcounter", array($tx));
                $thisio = pg_fetch_assoc($counter);
                while($thisio)
                {
                    // type determination
                    $statedtype = $thisio["type"];
                    if($type == "")
                    {
                        $type = $statedtype;
                    }
                    else
                    {
                        if($type != $statedtype)
                        {
                            $type = "Mixed types";
                        }
                    }
                    $address = $thisio["address"];
                    $addressstring = "<a href=\"{$rootpath}address/$address\">$address</a>";
                    if(is_null($address) || $address === false || $address == "Unknown")
                    {
                        $addressstring = "Unknown";
                    }
                    if($address == $currentaddress)
                    {
                    $addressstring = "$address";
                    }
                    $superechothis.= "<li>$addressstring</li>\n";
                    $thisio = pg_fetch_assoc($counter);
                }
            }
            $superechothis.= "</ul></td>\n";
            // /// /// /end tx loop
            
            // resume addr info
            $echothis.= "<td>$cord: $type</td>\n";
            // reintegrate ledger
            $echothis .= $superechothis;
            
            // calculate running balance
            if($cord == "Sent")
            {
                $balance = removeTrailingZeroes(bcsub($balance, $value, 8));
            }
            else
            {
                $balance = removeTrailingZeroes(bcadd($balance, $value, 8));
            }
            $echothis.= "<td>$balance</td>\n";
            $echothis.= "</tr>\n";
        }
        
        // addr info resumes
        $totalcredit = thousands(removeTrailingZeroes($totalcredit));
        $totaldebit = thousands(removeTrailingZeroes($totaldebit));
        echo "<li>Received transactions: $totalcredits</li>";
        echo "<li>Received BTC: $totalcredit</li>";
        echo "<li>Sent transactions: $totaldebits</li>";
        echo "<li>Sent BTC: $totaldebit</li>";
        echo "<li>Hash160".help("The hash160 is a hash of the public key. Bitcoin uses these hashes internally - transactions don't contain Bitcoin addresses directly. Bitcoin addresses contain a base58-encoded hash160, along with a version and a check code.").": $hash160</li>\n";
        echo "<li>Public key".help("It's impossible to determine the public key from a Bitcoin address, but if the public key was ever used on the network, it is listed here.").": <div class = \"hugeData\">$pubkey</div></li>\n";
        echo "</ul>\n";
        if($currentaddress == "1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28")
        {
            echo "<p><i>Thank you ! </i></p>";
        }
        
        // echo ledger
        echo "<h3>Ledger".help("A list of all transactions involving this address, with the oldest listed first.")."</h3>\n";
        echo '<p>Note: While the last "balance" is the accurate number of bitcoins available to this address, it is likely not the balance available to this person. Every time a transaction is sent, some bitcoins are usually sent back to yourself <i>at a new address</i> (not included in the Bitcoin UI), which makes the balance of a single address misleading. See <a href="https://en.bitcoin.it/wiki/Transactions">the wiki</a> for more info on transactions.</p>'."\n";
        echo "<table class = \"txtable\">\n";
        echo "<tr><th>Transaction".help("Truncated transaction hash")."</th><th>Block".help("Block this transaction appeared in")."</th><th>Amount".help("Number of BTC sent or received")."</th><th>Type".help("The type of the output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client. It's also possible for transactions to have several different types of outputs.")."</th><th>From/To".help("The addresses this was received from or sent to. When sending, Bitcoin usually sends some bitcoins back to a brand new address that you own.")."</th><th>Balance".help("Balance as of this transaction. The last balance is the current balance of this address.")."</th></tr>\n";
        echo $echothis;
        echo "</table>\n";
        
        if($stopcaching)
            cacheput("addr".md5($cachename), ob_get_flush(), 30, $latesttx, 0);
    }

    end:

    echo<<<'EOD'
<div id = "footer"><hr><a href="/">Bitcoin Block Explorer</a> (Mirror ad: <a href="https://www.privateinternetaccess.com/" title = "VPN Service">VPN Service</a>) - Donate: <a href="/address/1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28">1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28</a></div>
</body>
</html>
EOD;

}
