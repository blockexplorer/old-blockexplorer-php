<?php

require_once 'encode.inc';
require_once 'rpc.inc';
require_once 'sql.inc';
require_once 'cache.inc';
require_once 'Smarty.class.php' ;

function getblockcount()
{
	static $blockcount = null;

    if($blockcount)
        return $blockcount;

    if($cache = Cache::get("getblockcount")) {
        $blockcount = (integer)$cache;
        return $blockcount;
    }
    $blockcount = RPC::query("getblockcount");

    Cache::put("getblockcount", $blockcount, 5);
    return $blockcount;
}

function getdifficulty()
{
	$cache = Cache::get("getdifficulty");
	if($cache !== false)
		return $cache;
		
	$difficulty = RPC::query("getdifficulty");

	Cache::put("getdifficulty", $difficulty, 20);
	return $difficulty;
}

function getblockbynumber($num)
{
	$cache = Cache::get("getblock$num");
	if($cache !== false)
		return unserialize($cache);
	
	$block = RPC::query("getblock", array($num));

	Cache::put("getblock$num", serialize($block), 10);
	return $block;
}

function getdecimaltarget()
{
	$dtblock = getblockbynumber(getblockcount());
	$target = $dtblock->bits;
	return decodeCompact($target);	
}

function getprobability()
{
	return bcdiv(getdecimaltarget(), "115792089237316195423570985008687907853269984665640564039457584007913129639935", 55);
}

function getlastretarget()
{
	$blockcount = getblockcount();
	return ($blockcount-($blockcount%2016))-1;
}

class Error extends Exception { } 
class BadRequest extends Error { }

function app_stats($req) 
{
    $config = $req->testnet ? "TestnetConfig" : "Config";
    Address::$version = $config::$address_version;

    RPC::init($config::$rpc);
    SQL::init($config::$dbname);

    if($req->page == "home") 
        return page_home($req->path);

    if($req->page != "mytransactions")
        header("Cache-control: no-cache");

    header("Content-type: text/plain");
    $callback = "page_" . $req->page;

    $handle_error = function($code, $msg) {
        senderror($code);
        echo "ERROR: $msg";
    };

    if(function_exists($callback)) {

        if(!isset($req->params[0])) {
            $smarty = new Smarty();
            $tpl = 'stats/usage/' . $req-> page . '.tpl';

            if($smarty->templateExists($tpl))
                return $smarty->display($tpl);

            if(in_array($req->page, array("avgtxsize", 
                                          "avgtxvalue", 
                                          "avgtxnumber",
                                          "avgblocksize", 
                                          "interval", 
                                          "eta"))) 
                $req->params[0] = 1000;
        }
            
        
        try {
            return call_user_func($callback, $req->params);
        } catch (BadRequest $e) {
            return $handle_error(400, $e->getMessage());
        } catch (Error $e) {
            return $handle_error($e->getCode(), $e->getMessage());
        }
    }

    $handle_error(404, "invalid query");
}

function page_home($path) 
{
    $smarty = new Smarty();
    $smarty->assign('rootpath', $path);
    $smarty->display('stats/home.tpl');
}

function page_getdifficulty($params) 
{
    echo getdifficulty();
}

function page_getblockcount($params) 
{
    echo getblockcount();
}


function page_latesthash($params) 
{
    $block = getblockbynumber(getblockcount());
    echo strtoupper($block->hash);
}

function page_getblockhash($params) 
{
    $block = @(int)$params[0];
    if(!$block || $block > (int)getblockcount())
        throw new Error("block not found", 404);

    $block = getblockbynumber((int)$params[0]);
    echo strtoupper($block->hash);
}

function page_hextarget($params) 
{
    $target = encodeHex(getdecimaltarget());
    while(strlen($target)<64)
        $target = "0".$target;

    echo $target;
}

function page_decimaltarget($params)
{
    echo getdecimaltarget();
}

function page_probability($params)
{
    echo getprobability();
}

function page_hashestowin($params)
{
    echo bcdiv("1", getprobability(), 0);
}

function page_nextretarget($params)
{
    echo getlastretarget()+2016;
}

function page_estimate($params)
{
    $currentcount = getblockcount(); // last one with the old difficulty
    $last = getlastretarget()+1; // first one with the "new" difficulty
    $targettime = 600*($currentcount-$last+1);
    // check for cases where we're comparing the same two blocks
    if($targettime == 0)
    {
        echo getdifficulty();
        return;
    }
    
    $oldblock = getblockbynumber($last);
    $newblock = getblockbynumber($currentcount);
    $oldtime = $oldblock->time;
    $oldtarget = decodeCompact($oldblock->bits);
    $newtime = $newblock->time;
    
    $actualtime = $newtime-$oldtime;
    
    if($actualtime<$targettime/4)
    {
        $actualtime = $targettime/4;
    }
    if($actualtime>$targettime*4)
    {
        $actualtime = $targettime*4;
    }
    
    $newtarget = bcmul($oldtarget, $actualtime);
    // check once more for safety
    if($newtarget == "0")
    {
        echo getdifficulty();
        return;
    }
    $newtarget = bcdiv($newtarget, $targettime, 0);
    $newtarget = decodeCompact(encodeCompact($newtarget));
    // we now have the real new target
    echo bcdiv("26959535291011309493156476344723991336010898738574164086137773096960", $newtarget, 8);
}

function page_bcperblock($params) 
{
    if(isset($params[0])) {

        $blockcount = (string)$params[0];
        if($blockcount>6929999)
            $blockcount = "6930000";

    } else
        $blockcount = getblockcount();

    $blockworth = "50";
    // for genesis block

    $totalbc = "50";
    bcscale(8);

    // $blockcount++; // genesis block
    while(bccomp($blockcount, "0") == 1) // while blockcount is larger than 0
    {
        if(bccomp($blockcount, "210000") == -1) // if blockcount is less than 210000
        {
            $totalbc = (string)bcadd($totalbc, bcmul($blockworth, $blockcount));
            $blockcount = "0";
        }
        else
        {
            $blockcount = bcsub($blockcount, "210000");
            $totalbc = (string)bcadd($totalbc, bcmul($blockworth, "210000"));
            $blockworth = bcdiv($blockworth, "2", 8);
        }
    }
    
    echo $blockworth;
}

function page_totalbc($params)
{
    if(isset($params[0])) {

        $blockcount = (string)$params[0];
        if($blockcount>6929999)
            $blockcount = "6930000";

    } else
        $blockcount = getblockcount();


    $blockworth = "50";
    // for genesis block
    $totalbc = "50";
    bcscale(8);
    // $blockcount++; // genesis block
    while(bccomp($blockcount, "0") == 1) // while blockcount is larger than 0
    {
        if(bccomp($blockcount, "210000") == -1) // if blockcount is less than 210000
        {
            $totalbc = (string)bcadd($totalbc, bcmul($blockworth, $blockcount));
            $blockcount = "0";
        }
        else
        {
            $blockcount = bcsub($blockcount, "210000");
            $totalbc = (string)bcadd($totalbc, bcmul($blockworth, "210000"));
            $blockworth = bcdiv($blockworth, "2", 8);
        }
    }
    
    echo $totalbc;
}

function page_changeparams($params)
{
    $blockcount = "10000000000";
    $origblockcount = $blockcount;
    $didsomething = 0;
    if(isset($_GET['subsidy'])) {
        $blockworth = (string)$_GET['subsidy'];
        $didsomething = 1;
    }
    else
        $blockworth = 50;

    if(isset($_GET['precision'])) {

        $precision = (integer)$_GET['precision'];
        if($precision > 9000)
            throw new BadRequest("Precision level over nine thousand! (Don't kill my server.)");

        $didsomething = 1;

    } else {
        $precision = 8;
    }

    if(isset($_GET['interval'])) {
        $interval = (string)$_GET['interval'];
        $didsomething = 1;
    } else {
        $interval = "210000";
    }

    if($didsomething != 1) {
        $smarty = new Smarty();
        
        echo "This gives you the end total BC and the time required to reach it after changing various parameters. \nSubsidy - Starting subsidy (generation reward)\nInterval - Subsidy is halved after this many blocks\nPrecision - Decimals of precision\nLeave a parameter out to use the Bitcoin default:\n";

        echo "/q/changeparams?interval=210000&precision=8&subsidy=50";

        return;
    }

    $totalbc = "0";
    bcscale($precision);

    while(bccomp($blockcount, "0") == 1 && $blockworth != "0") // while blockcount is larger than 0
    {
        if(bccomp($blockcount, $interval) == -1) // if blockcount is less than 210000
        {
            $totalbc = (string)bcadd($totalbc, bcmul($blockworth, $blockcount));
            $blockcount = "0";
            if($blockworth != 0)
            {
                echo "Could not complete calculation in 10,000,000,000 blocks. (This is an arbitrary limit of the calculator.)";
                return;
            }
        }
        else
        {
            $blockcount = bcsub($blockcount, $interval);
            $totalbc = (string)bcadd($totalbc, bcmul($blockworth, $interval));
            $blockworth = bcdiv($blockworth, "2");
        }
    }
    echo "Final BC in circulation: ".$totalbc;
    echo "\n";
    $realchange = bcsub($origblockcount, $blockcount, 0);
    echo "Took ".$realchange." blocks (".bcdiv($realchange, "52560", 3)." years).";
}

function page_addresstohash($params) 
{
    $address = trim($params[0]);
    if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) || strlen($address) > 300)
        throw new BadRequest("the input is not base58 (or is too large).");

    echo Address::toHash160($address);

}

function page_hashtoaddress($params)
{
    $address_version = Address::$version;

    if(isset($params[1])) {
        $address_version = strtoupper(remove0x(trim((string)$params[1])));
        if(strlen($address_version) != 2 || !preg_match('/^[0-9A-F]+$/', $address_version))
            throw new BadRequest("address_version is a two-character hexadecimal byte. Like 1F or 03");
    }

    $hash160 = strtoupper(remove0x(trim($params[0])));
    if(!preg_match('/^[0-9A-F]+$/', $hash160) || strlen($hash160) > 400)
        throw new BadRequest("the input is not hex (or is too large).");

    if(strlen($hash160)%2 != 0)
        throw new BadRequest("it doesn't make sense to have an uneven number of hex characters. (Perhaps you can add or remove some leading zeros.)");

    echo Address::hash160ToAddress($hash160, $address_version);
}


function page_checkaddress($params)
{
    $address = trim($params[0]);
    if(preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address))
    {
        if(strlen($address)<300)
        {
            $address = decodeBase58($address);
            if(strlen($address) == 50)
            {
                $version = substr($address, 0, 2);
                $check = substr($address, 0, strlen($address)-8);
                $check = pack("H*" , $check);
                $check = strtoupper(hash("sha256", hash("sha256", $check, true)));
                $check = substr($check, 0, 8);
                if($check == substr($address, strlen($address)-8))
                {
                    echo $version;
                }
                else
                {
                    echo "CK";
                }
            }
            else
            {
                echo "SZ";
            }
        }
        else
        {
            echo "SZ";
        }
    }
    else
    {
        echo "X5";
    }
}

function page_hashpubkey($params)
{
    $pubkey = strtoupper(remove0x(trim($params[0])));

    if(!preg_match('/^[0-9A-F]+$/', $pubkey) || strlen($pubkey) > 300)
        throw new BadRequest("the input is not hex (or is too large).");

    echo hash160($pubkey);
}


function page_avgtxsize($params)
{
    $lastblocks = (int)$params[0];
    if($lastblocks <= 0)
        throw new BadRequest("the first parameter is the number of blocks to look back through.");

    $avg = SQL::s("SELECT round(avg(transactions.size), 0) 
                   FROM transactions JOIN blocks ON 
                        (transactions.block = blocks.hash) 
                   WHERE blocks.number > (SELECT max(number) 
                   FROM blocks)-$1;", $lastblocks);
    echo $avg;
}

function page_avgtxvalue($params)
{
    $lastblocks = (int)$params[0];
    if($lastblocks <= 0)
        throw new BadRequest("the first parameter is the number of blocks to look back through.");

    $avg = SQL::s("SELECT coalesce(round(avg(sum), 8), '0') 
                   FROM (SELECT sum(inputs.value), inputs.tx AS avg 
                         FROM inputs JOIN blocks ON 
                              (inputs.block = blocks.hash) 

                         WHERE blocks.number > (SELECT max(number) FROM blocks)-$1 AND
                               inputs.type <> 'Generation' 
                         GROUP BY inputs.tx) AS a;", 

                   $lastblocks);
    echo $avg;
}

function page_avgblocksize($params)
{
    $lastblocks = (int)$params[0];
    if($lastblocks <= 0)
        throw new BadRequest("the first parameter is the number of blocks to look back through.");

    $avg = SQL::s("SELECT round(avg(size), 0) AS avg 
                   FROM blocks 
                   WHERE blocks.number > (SELECT max(number) 
                                          FROM blocks) - $1;", $lastblocks);
    echo $avg;
}

function page_interval($params)
{
    $lastblocks = (int)$params[0];
    if($lastblocks < 2)
        throw new BadRequest("invalid block count");
    
    if($cache = Cache::get("interval{$lastblocks}")) {
        echo (integer)$cache;
        return;
    }
    
    $avg = SQL::s("SELECT round((EXTRACT ('epoch' FROM avg(time.time)))::numeric, 0) AS avg 

                   FROM (SELECT time - lag(time, 1) OVER (ORDER BY time) AS time 
                         FROM blocks 
                         WHERE blocks.number > (SELECT max(number) - $1 FROM blocks)) AS time;", 

                   $lastblocks);

    Cache::put("interval{$lastblocks}", $avg, 30);
    echo $avg;
}

function page_eta($params)
{
    $lastblocks = (int)$params[0];
    if($lastblocks < 2)
        throw new BadRequest("invalid block count");

    $avg = SQL::s("SELECT round((EXTRACT ('epoch' FROM avg(time.time)))::numeric, 0) AS avg    
                                 FROM (SELECT time-lag(time, 1) OVER (ORDER BY time) AS time 
                                       FROM blocks WHERE blocks.number > (SELECT max(number)-$1 
                                                                          FROM blocks)
                                      ) AS time;", 

                  max(min(getblockcount() - getlastretarget(), $lastblocks), 2));

    $blocksleft = (getlastretarget() + 2016) - getblockcount();
    if($blocksleft == 0)
        $blocksleft = 2016;

    echo $blocksleft * $avg;	
}

function page_avgtxnumber($params)
{
    $lastblocks = (int)$params[0];
    if($lastblocks < 1)
        throw new BadRequest("invalid block count");

    $avg = SQL::s("SELECT round(avg(a.count), 3) AS avg 
                   FROM (SELECT block, count(*) AS count 
                         FROM transactions GROUP BY block) AS a JOIN blocks ON 
                              (blocks.hash = a.block) 
                         WHERE blocks.number > (SELECT max(number) - $1 
                                                FROM blocks);",
                  $lastblocks);
    echo $avg;
}

function page_getreceivedbyaddress($params)
{
    $minconf = (int)(isset($params[1]) ? $params[1] : 1);
    if($minconf <= 0)
        throw new BadRequest("you must use an integer above 0 for minconf");

    $address = $params[0];
    if(!Address::check($address))
        throw new BadRequest("invalid address");

    $hash160 = Address::toHash160($address);
    $sum = SQL::s("SELECT sum(value) AS sum 
                   FROM outputs 
                   WHERE hash160 = decode($1, 'hex') AND 
                         block NOT IN (SELECT hash 
                                       FROM blocks ORDER BY number DESC LIMIT $2);", 

                   array($hash160, $minconf - 1));

    if(!$sum)
        $sum = 0;

    echo $sum;
}

function page_getsentbyaddress($params)
{
    $address = $params[0];

    if(!Address::check($address)) 
        throw new BadRequest("invalid address");

    $hash160 = Address::toHash160($params[0]);
    $sum = SQL::s("SELECT sum(value) AS sum 
                   FROM inputs 
                   WHERE hash160 = decode($1, 'hex');", $hash160);

    if(!$sum)
        $sum = 0;

    echo $sum;
}

function page_addressbalance($params)
{
    $address = $params[0];

    if(!Address::check($address))
        throw new BadRequest("invalid address");

    $hash160 = Address::toHash160($params[0]);
    $sent = SQL::s("SELECT sum(value) 
                    FROM inputs 
                    WHERE hash160 = decode($1, 'hex');", $hash160);
    if(!$sent)
        $sent = 0;
    
    $received = SQL::s("SELECT sum(value) 
                        FROM outputs 
                        WHERE hash160 = decode($1, 'hex');", $hash160);

    if(!$received)
        $received = 0;

    echo $received - $sent;
}

function page_addressfirstseen($params)
{
    $address = $params[0];
    if(!Address::check($address))
        throw new BadRequest("invalid address");

    $result = SQL::s("SELECT time AT TIME ZONE 'UTC'
                      FROM keys JOIN blocks ON 
                           (keys.firstseen = blocks.hash) 
                      WHERE address = $1;", $params[0]);
    if(!$result)
        $result = "Never seen";

    echo $result;
}

function page_nethash($params)
{
    $step = (int)(isset($params[0]) ? $params[0] : 144);
    if(!$step || ! ($step >= 5 && $step <= 10000))
        throw new BadRequest("invalid step (must be between 5 - 10000");

    $smarty = new Smarty();
    $smarty->setCaching(Smarty::CACHING_LIFETIME_SAVED);
    $smarty->setCacheLifetime(600);

    $tpl = 'stats/nethash.tpl';

    if($smarty->isCached($tpl))
        return $smarty->display($tpl);

    $query = SQL("SELECT number, 
                         EXTRACT ('epoch' FROM time) AS time, 
                         bits, 
                         round(EXTRACT ('epoch' 
                                         FROM (SELECT avg(a.time) 
                                               FROM (SELECT time-lag(time, 1) OVER (ORDER BY number) AS time 
                                                     FROM blocks 
                                                     WHERE number > series AND number < series+($1+1)) 
                                                     
                                                     AS a))::numeric, 0) 
                                AS avg 
                  FROM blocks, generate_series(0, (SELECT max(number) 
                                                   FROM blocks), $1) AS series(series) 
                  WHERE number = series+$1;", 
                  
                  $step);

    $rows = array();
    while($onerow = SQL::d($query)) {

        $number = $onerow["number"];
        $time = $onerow["time"];
        $target = decodeCompact($onerow["bits"]);
        
        if(!$target)
            throw new Error("divide by zero", 500);
        
        // average targets to get accurate estimates
        $avgtarget = isset($prevtarget) ? bcdiv(bcadd($target, $prevtarget), "2", 0) : $target;
        $prevtarget = $target;
        
        $difficulty = bcdiv("26959535291011309493156476344723991336010898738574164086137773096960", $target, 2);
        $hashestowin = bcdiv("1", bcdiv($target, "115792089237316195423570985008687907853269984665640564039457584007913129639935", 55), 0);
        $avginterval = $onerow['avg'];
        $avghashestowin = bcdiv("1", bcdiv($avgtarget, "115792089237316195423570985008687907853269984665640564039457584007913129639935", 55), 0);
        if(!$avginterval)
            continue;

        $nethash = bcdiv($avghashestowin, $avginterval, 0);
        $rows[] = array($number, $time, $target, $avgtarget, $difficulty, $hashestowin, $avginterval, $nethash);
    }

    $smarty->assign('rows', $rows);
    $smarty->display('stats/nethash.tpl');
}

function page_mytransactions($params)
{
    // This RELIES on the fact that only address transactions will be sent/received
    $blocklimit = 0;
    if(!empty($params[1])) {
        $blockhash = remove0x(trim(strtolower($params[1])));
        if(!preg_match("/[0-9a-f]{64}/", $blockhash))
            throw new BadRequest("block limit is in invalid format");

        $blocklimit = (int)SQL::s("SELECT number FROM blocks WHERE hash = decode($1, 'hex');", $blockhash);
    }

    // gather addresses
    $addresses = explode('.', trim($params[0]));
    foreach($addresses as &$address)
    {
        if(!Address::check($address))
            throw new BadRequest("One or more addresses are invalid");

        $address = "decode('".Address::toHash160($address)."', 'hex')";
    }
    // this is safe because addresses were checked above
    $addresses = implode(", ", $addresses);
    $result = SQL("SELECT encode(blocks.hash, 'hex') AS block, 
                          encode(transactions.hash, 'hex') AS tx, 
                          blocks.number AS blocknum, 
                          blocks.time AT TIME ZONE 'UTC' AS time, 
                          transactions.id AS tid, 
                          transactions.raw AS rawtx

                   FROM inputs JOIN transactions ON (inputs.tx = transactions.hash) 
                               JOIN blocks ON (inputs.block = blocks.hash)

                   WHERE inputs.type = 'Address' AND 
                         blocks.number>$1 AND inputs.hash160 IN ($addresses)

                   UNION 
                   
                   SELECT encode(blocks.hash, 'hex') AS block, 
                          encode(transactions.hash, 'hex') AS tx, 
                          blocks.number AS blocknum, 
                          blocks.time AT TIME ZONE 'UTC' AS time, 
                          transactions.id AS tid, 
                          transactions.raw AS rawtx

                   FROM outputs JOIN transactions ON (outputs.tx = transactions.hash) 
                                JOIN blocks ON (outputs.block = blocks.hash)

                   WHERE outputs.type = 'Address' AND 
                         blocks.number>$1 AND 
                         outputs.hash160 IN ($addresses) 
                         
                   ORDER BY tid;", $blocklimit);

    $return = (object)array();
    $maxrow = SQL::count($result);
    $counter = 0;
    while($row = SQL::d($result))
    {
        $rowtx = json_decode($row["rawtx"]);
        $block = $row["block"];
        $blocknum = $row["blocknum"];
        $time = $row["time"];
        $tx = $row["tx"];
        
        // caching
        $counter++;
        if($counter == $maxrow)
        {
            Cache::handle_client_side_etag($block);
        }
        
        // add additional info
        $rowtx->block = $row["block"];
        $rowtx->blocknumber = $row["blocknum"];
        $rowtx->time = $row["time"];
        
        // add addresses
        foreach($rowtx->in as &$i)
        {
            if(isset($i->scriptSig))
            {
                $scriptsig = $i->scriptSig;
                $simplescriptsig = preg_replace("/[0-9a-f]+ OP_DROP ?/", "", $scriptsig);
                if(preg_match("/^[0-9a-f]+ [0-9a-f]{130}$/", $simplescriptsig))
                {
                    $pubkey = preg_replace("/^[0-9a-f]+ ([0-9a-f]{130})$/", "$1", $simplescriptsig);
                    $hash160 = strtolower(hash160($pubkey));
                    $address = Address::hash160ToAddress($hash160);
                    $i->address = $address;
                }
            }
        }
        foreach($rowtx->out as &$i)
        {
            $scriptpubkey = $i->scriptPubKey;
            $simplescriptpk = preg_replace("/[0-9a-f]+ OP_DROP ?/", "", $scriptpubkey);
            if(preg_match("/^OP_DUP OP_HASH160 [0-9a-f]{40} OP_EQUALVERIFY OP_CHECKSIG$/", $simplescriptpk))
            {
                $hash160 = preg_replace("/^OP_DUP OP_HASH160 ([0-9a-f]{40}) OP_EQUALVERIFY OP_CHECKSIG$/", "$1", $simplescriptpk);
                $address = Address::hash160ToAddress($hash160);
                $i->address = $address;
            }
        }
        
        $return = (array)$return;
        $return[$tx] = $rowtx;
    }

    $json_indent = function($json) {
        $result    = '';
        $pos       = 0;
        $strLen    = strlen($json);
        $indentStr = '  ';
        $newLine   = "\n";

        for($i = 0; $i <= $strLen; $i++) {
            
            // Grab the next character in the string
            $char = substr($json, $i, 1);
            
            // If this character is the end of an element, 
            // output a new line and indent the next line
            if($char == '}' || $char == ']') {
                $result .= $newLine;
                $pos --;
                for ($j = 0; $j<$pos; $j++) {
                    $result .= $indentStr;
                }
            }
            
            // Add the character to the result string
            $result .= $char;

            // If the last character was the beginning of an element, 
            // output a new line and indent the next line
            if ($char == ',' || $char == '{' || $char == '[') {
                $result .= $newLine;
                if ($char == '{' || $char == '[') {
                    $pos ++;
                }
                for ($j = 0; $j < $pos; $j++) {
                    $result .= $indentStr;
                }
            }
        }
        
        return $result;
    };

    echo $json_indent(json_encode($return));
}
