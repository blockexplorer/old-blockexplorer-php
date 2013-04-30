<?php

require_once 'encode.inc';
require_once 'rpc.inc';
require_once 'sql.inc';
require_once 'cache.inc';

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

function app_stats($req) 
{
    $config = $req->testnet ? "TestnetConfig" : "Config";
    Address::$version = $config::$address_version;

    RPC::init($config::$rpc);
    SQL::init($config::$dbname);

    if($req->page != "mytransactions")
        header("Cache-control: no-cache");

    if($req->page != "home") 
        header("Content-type: text/plain");

    $callback = "page_" . $req->page;

    if(function_exists($callback)) 
        return call_user_func($callback, $req);

    senderror(404);
    echo "ERROR: invalid query";
}

function page_home($req) 
{
    require_once('Smarty.class.php');

    $smarty = new Smarty();
    $smarty->assign('rootpath', $req->path);
    $smarty->display('stats/home.tpl');
}

function page_getdifficulty($req) 
{
    echo getdifficulty();
}

function page_getblockcount($req) 
{
    echo getblockcount();
}


function page_latesthash($req) 
{
    $block = getblockbynumber(getblockcount());
    echo strtoupper($block->hash);
}

function page_getblockhash($req) 
{
    if(isset($req->params[0]) && preg_match('/^[0-9]{1,9}$/', $req->params[0])) {
        $block = (int)$req->params[0];
        if($block <= (int)getblockcount())
        {
            $block = getblockbynumber((int)$req->params[0]);
            echo strtoupper($block->hash);
        }
        else
        {
            senderror(404);
            echo "ERROR: block not found";

            return;
        }
    }
    else
    {
        echo "Returns the hash of a block at a given height.\n\n/q/getblockhash/hash";
    }
}

function page_hextarget($req) 
{
    $target = encodeHex(getdecimaltarget());
    while(strlen($target)<64)
    {
        $target = "0".$target;
    }
    echo $target;
}

function page_decimaltarget($req)
{
    echo getdecimaltarget();
}

function page_probability($req)
{
    echo getprobability();
}

function page_hashestowin($req)
{
    echo bcdiv("1", getprobability(), 0);
}

function page_nextretarget($req)
{
    echo getlastretarget()+2016;
}

function page_estimate($req)
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

function page_bcperblock($req) 
{
    if(isset($req->params[0]) && preg_match('/^[0-9]+$/', $req->params[0]))
    {
        $blockcount = (string)$req->params[0];
        if($blockcount>6929999)
        {
            $blockcount = "6930000";
        }
    }
    else
    {
        $blockcount = getblockcount();
    }

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

function page_totalbc($req)
{
    if(isset($req->params[0]) && preg_match('/^[0-9]+$/', $req->params[0]))
    {
        $blockcount = (string)$req->params[0];
        if($blockcount>6929999)
        {
            $blockcount = "6930000";
        }
    }
    else
    {
        $blockcount = getblockcount();
    }
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

function page_changeparams($req)
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
        if($precision > 9000) {
            echo "Precision level over nine thousand! (Don't kill my server.)";
            return;
        }
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
        echo "This gives you the end total BC and the time required to reach it after changing various parameters. \nSubsidy - Starting subsidy (generation reward)\nInterval - Subsidy is halved after this many blocks\nPrecision - Decimals of precision\nLeave a parameter out to use the Bitcoin default:\n";
        echo "/q/changeparams?interval = 210000&precision = 8&subsidy = 50";

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
                echo "Could not complete calculation in 10, 000, 000, 000 blocks. (This is an arbitrary limit of the calculator.)";
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

function page_addresstohash($req) 
{
    if(isset($req->params[0]))
    {
        $address = trim($req->params[0]);
        if(preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/', $address) && strlen($address)<300)
        {
            echo addressToHash160($address);
        }
        else
        {
            senderror(400);
            echo "ERROR: the input is not base58 (or is too large).";
        }
    }
    else
    {
        echo "Converts a BC address to the hash160 format used internally by Bitcoin.\nNote: the address is not checked for validity.\n/q/addresstohash/address";
    }
}

function page_hashtoaddress($req)
{
    $address_version = Address::$version;

    if(isset($req->params[1]))
    {
        $address_version = strtoupper(remove0x(trim((string)$req->params[1])));
        if(strlen($address_version) != 2 || !preg_match('/^[0-9A-F]+$/', $address_version))
        {
            echo "ERROR: address_version is a two-character hexadecimal byte. Like 1F or 03. Using default.\n";
        }
    }
    if(isset($req->params[0]))
    {
        $hash160 = strtoupper(remove0x(trim($req->params[0])));
        if(preg_match('/^[0-9A-F]+$/', $hash160) && strlen($hash160)<400)
        {
            if(strlen($hash160)%2 == 0)
            {
                echo Address::hash160ToAddress($hash160, $address_version);
            }
            else
            {
                senderror(400);
                echo "ERROR: it doesn't make sense to have an uneven number of hex characters.
(Perhaps you can add or remove some leading zeros.)";
            }
        }
        else
        {
            senderror(400);
            echo "ERROR: the input is not hex (or is too large).";
        }
    }
    else
    {
        echo "Converts a Bitcoin hash160 (in hex) to a valid BC address.\n/q/hashtoaddress/hexHash[/address_version]";
    }
}


function page_checkaddress($req)
{
    if(isset($req->params[0]))
    {
        $address = trim($req->params[0]);
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
    else
    {
    echo "Returns 00 if the address is valid, something else otherwise. Note that it
is impossible to determine whether someone actually *owns* the address. Someone
could easily give 20 random bytes to /q/hashtoaddress and get a valid address.
X5 - Address not base58
SZ - Address not the correct size
CK - Failed hash check
Anything else - the encoded AddressVersion (always 00 in valid addresses)

/q/checkaddress/address";

    }
}


function page_hashpubkey($req)
{
    if(isset($req->params[0]))
    {
        $pubkey = strtoupper(remove0x(trim($req->params[0])));
        if(preg_match('/^[0-9A-F]+$/', $pubkey) && strlen($pubkey)<300)
        {
            echo hash160($pubkey);
        }
        else
        {
            senderror(400);
            echo "ERROR: the input is not hex (or is too large).";
        }
    }
    else
    {
        echo "Generates a hash160 from a Bitcoin public key. In the current implementation,
public keys are either the first 65 bytes (130 hex characters) of a scriptPubKey
or the last 65 bytes of a scriptSig, depending on the type of transaction. They
always seem to start with 04 (this must be included).

/q/hashpubkey/hexPubKey";
    }
}


function page_avgtxsize($req)
{
    if(!isset($req->params[0]))
    {
        $req->params[0] = 1000;
    }
    $req->params[0] = (int)$req->params[0];
    if($req->params[0]>0)
    {
        $avg = SQL::s("SELECT round(avg(transactions.size), 0) 
                       FROM transactions JOIN blocks ON 
                            (transactions.block = blocks.hash) 
                       WHERE blocks.number > (SELECT max(number) 
                       FROM blocks)-$1;", array($req->params[0]));
        echo $avg;
    }
    else
    {
        senderror(400);
        echo "ERROR: the first parameter is the number of blocks to look back through.";
    }
}

function page_avgtxvalue($req)
{
    if(!isset($req->params[0]))
    {
        $req->params[0] = 1000;
    }
    $req->params[0] = (int)$req->params[0];
    if($req->params[0]>0)
    {
        $avg = SQL::s("SELECT coalesce(round(avg(sum), 8), '0') 
                       FROM (SELECT sum(inputs.value), inputs.tx AS avg 
                             FROM inputs JOIN blocks ON 
                                  (inputs.block = blocks.hash) 

                             WHERE blocks.number > (SELECT max(number) FROM blocks)-$1 AND
                                   inputs.type <> 'Generation' 
                             GROUP BY inputs.tx) AS a;", 

                       array($req->params[0]));
        echo $avg;
    }
    else
    {
        senderror(400);
        echo "ERROR: the first parameter is the number of blocks to look back through.";
    }
}

function page_avgblocksize($req)
{
    if(!isset($req->params[0]))
    {
        $req->params[0] = 1000;
    }
    $req->params[0] = (int)$req->params[0];
    if($req->params[0]>0)
    {
        $avg = SQL::s("SELECT round(avg(size), 0) AS avg 
                       FROM blocks 
                       WHERE blocks.number > (SELECT max(number) 
                                              FROM blocks) - $1;", array($req->params[0]));
        echo $avg;
    }
    else
    {
        senderror(400);
        echo "ERROR: the first parameter is the number of blocks to look back through.";
    }
}

function page_interval($req)
{
    if(!isset($req->params[0]))
    {
        // default lookback
        $req->params[0] = 1000;
    }
    $req->params[0] = (int)$req->params[0];
    if($req->params[0]<2)
    {
        senderror(400);
        echo "ERROR: invalid block count.";
        return;
    }
    
    $cache = Cache::get("interval{$req->params[0]}");
    if($cache !== false)
    {
        echo (integer)$cache;
        return;
    }
    
    $avg = SQL::s("SELECT round((EXTRACT ('epoch' FROM avg(time.time)))::numeric, 0) AS avg 

                   FROM (SELECT time - lag(time, 1) OVER (ORDER BY time) AS time 
                         FROM blocks 
                         WHERE blocks.number > (SELECT max(number) - $1 FROM blocks)) AS time;", 

                   array($req->params[0]));

    Cache::put("interval{$req->params[0]}", $avg, 30);
    echo $avg;
}

function page_eta($req)
{
    if(!isset($req->params[0]))
    {
        // default lookback
        $req->params[0] = 1000;
    }
    $req->params[0] = (int)$req->params[0];
    if($req->params[0]<2)
    {
        senderror(400);
        echo "ERROR: invalid block count.";
        return;
    }
    $req->params[0] = min(getblockcount()-getlastretarget(), $req->params[0]);
    $req->params[0] = max($req->params[0], 2);
    $avg = SQL::s("SELECT round((EXTRACT ('epoch' FROM avg(time.time)))::numeric, 0) AS avg    
                                 FROM (SELECT time-lag(time, 1) OVER (ORDER BY time) AS time 
                                       FROM blocks WHERE blocks.number > (SELECT max(number)-$1 
                                                                          FROM blocks)
                                      ) AS time;", 

                  array($req->params[0]));

    $blocksleft = (getlastretarget()+2016)-getblockcount();
    if($blocksleft == 0)
    {
        $blocksleft = 2016;
    }
    echo $blocksleft * $avg;	
}

function page_avgtxnumber($req)
{
    if(!isset($req->params[0]))
    {
        // default lookback
        $req->params[0] = 1000;
    }
    $req->params[0] = (int)$req->params[0];
    if($req->params[0]<1)
    {
        senderror(400);
        echo "ERROR: invalid block count.";
        return;
    }
    $avg = SQL::s("SELECT round(avg(a.count), 3) AS avg 
                   FROM (SELECT block, count(*) AS count 
                         FROM transactions GROUP BY block) AS a JOIN blocks ON 
                              (blocks.hash = a.block) 
                         WHERE blocks.number > (SELECT max(number) - $1 
                                                FROM blocks);",
                  array($req->params[0]));
    echo $avg;
}


function page_getreceivedbyaddress($req)
{
    if(isset($req->params[0]))
    {
        $req->params[0] = trim($req->params[0]);
    }
    else
    {
        echo "Returns total BTC received by an address. Sends are not taken into account.\nThe optional second parameter specifies the required number of confirmations for\ntransactions comprising the balance.\n/q/getreceivedbyaddress/address[/minconf]";
        return;
    }
    
    $minconf = 1;
    if(isset($req->params[1]))
    {
        $req->params[1] = (int)$req->params[1];
        if(empty($req->params[1]) || ! is_int($req->params[1]) || $req->params[1]<0)
        {
            senderror(400);
            echo "ERROR: you must use an integer above 0 for minconf";
            return;
        }
        if($req->params[1] == 0) // I don't think this can happen currently
        {
            senderror(400); 
            echo "ERROR: this page never counts 0-confirmation transactions";
            return;
        }
        $minconf = (int)$req->params[1];
    }
    
    if(isset($req->params[0]) && strlen($req->params[0])>24 && strlen($req->params[0])<36 && Address::checkAddress($req->params[0]))
    {
        $hash160 = addressToHash160($req->params[0]);
        $sum = SQL::s("SELECT sum(value) AS sum 
                       FROM outputs 
                       WHERE hash160 = decode($1, 'hex') AND 
                             block NOT IN (SELECT hash 
                                           FROM blocks ORDER BY number DESC LIMIT $2);", 

                       array($hash160, $minconf-1));

        if(!$sum)
            $sum = 0;

        echo $sum;
        
    }
    else
    {
        senderror(400);
        echo "ERROR: invalid address";
    }
}

function page_getsentbyaddress($req)
{
    if(isset($req->params[0]))
    {
        $req->params[0] = trim($req->params[0]);
    }
    else
    {
        echo "Returns total BTC sent by an address. Using this data is almost always a very\nbad idea, as the amount of BTC sent by an address is usually very different\nfrom the amount of BTC sent by the person owning the address.\n/q/getsentbyaddress/address";
        return;
    }
    
    if(isset($req->params[0]) && strlen($req->params[0])>24 && strlen($req->params[0])<36 && Address::checkAddress($req->params[0]))
    {
        $hash160 = addressToHash160($req->params[0]);
        $sum = SQL::s("SELECT sum(value) AS sum 
                       FROM inputs 
                       WHERE hash160 = decode($1, 'hex');", array($hash160));

        if(!$sum)
            $sum = 0;

        echo $sum;
        
    }
    else
    {
        senderror(400);
        echo "ERROR: invalid address";
    }
}

function page_addressbalance($req)
{
    if(isset($req->params[0]))
    {
        $req->params[0] = trim($req->params[0]);
    }
    else
    {
        echo "This is the same as subtracting /q/getsentbyaddress from /q/getreceivedbyaddress.\nUsing this data is almost always a very bad idea, as the amount of BTC sent\nby an address is usually very different from the amount of BTC sent by the\nperson owning the address.\n/q/addressbalance/address";
        return;
    }
    
    if(isset($req->params[0]) && strlen($req->params[0])>24 && strlen($req->params[0])<36 && Address::checkAddress($req->params[0]))
    {
        $hash160 = addressToHash160($req->params[0]);
        $sent = SQL::s("SELECT sum(value) 
                        FROM inputs 
                        WHERE hash160 = decode($1, 'hex');", array($hash160));
        if(!$sent)
            $sent = 0;
        
        $received = SQL::s("SELECT sum(value) 
                            FROM outputs 
                            WHERE hash160 = decode($1, 'hex');", array($hash160));

        if(!$received)
            $received = 0;

        echo $received-$sent;
    }
    else
    {
        senderror(400);
        echo "ERROR: invalid address";
    }
}

function page_addressfirstseen($req)
{
    if(isset($req->params[0])) {

        $req->params[0] = trim($req->params[0]);

    } else {

        echo "Returns the block time at which an address was first seen.\n/q/addressfirstseen/address";
        return;
    }

    if(isset($req->params[0]) && 
       strlen($req->params[0]) > 24 && 
       strlen($req->params[0]) < 36 && 
       Address::checkAddress($req->params[0])) {

        $result = SQL::s("SELECT time AT TIME ZONE 'UTC'
                          FROM keys JOIN blocks ON 
                               (keys.firstseen = blocks.hash) 
                          WHERE address = $1;", array($req->params[0]));
        if(!$result)
            $result = "Never seen";

        echo $result;
    } else {
        senderror(400);
        echo "ERROR: invalid address";
    }
}

function page_nethash($req)
{
    if(!isset($req->params[0]))
    {
        $req->params[0] = 144;
    }
    $req->params[0] = (int)$req->params[0];
    if(empty($req->params[0]) || ! ($req->params[0]>4 && $req->params[0]<10001))
    {
        senderror(400);
        echo "ERROR: invalid stepping (must be 5-10, 000)";
        return;
    }
    echo "Each row contains some info about the single block at height blockNumber:
- Time when the block was created (UTC)
- Decimal target
- Difficulty
- The average number of hashes it takes to solve a block at this difficulty

Each row also contains stats that apply to the set of blocks between blockNumber and the previous blockNumber:
- Average interval between blocks.
- Average target over these blocks. This is only different from the block target when
a retarget occurred in this section. (I'm not totally sure I'm doing this correctly.)
- The estimated number of network-wide hashes per second during
this time, calculated from the average interval and average target.\n";

    echo "blockNumber, time, target, avgTargetSinceLast, difficulty, hashesToWin, avgIntervalSinceLast, netHashPerSecond\n";
    echo "START DATA\n";

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
                       
                       array($req->params[0]));

    $onerow = SQL::d($query);
    
    while($onerow)
    {
        $number = $onerow["number"];
        $time = $onerow["time"];
        $target = decodeCompact($onerow["bits"]);
        
        if(empty($target))
        {
            senderror(500);
            echo "ERROR: divide by zero";
            return;
        }
        
        // average targets to get accurate estimates
        if(isset($prevtarget))
        {
            $avgtarget = bcdiv(bcadd($target, $prevtarget), "2", 0);
        }
        else
        {
            $avgtarget = $target;
        }
        $prevtarget = $target;
        
        $difficulty = bcdiv("26959535291011309493156476344723991336010898738574164086137773096960", $target, 2);
        $hashestowin = bcdiv("1", bcdiv($target, "115792089237316195423570985008687907853269984665640564039457584007913129639935", 55), 0);
        $avginterval = $onerow['avg'];
        $avghashestowin = bcdiv("1", bcdiv($avgtarget, "115792089237316195423570985008687907853269984665640564039457584007913129639935", 55), 0);
        $nethash = bcdiv($avghashestowin, $avginterval, 0);
        echo "$number, $time, $target, $avgtarget, $difficulty, $hashestowin, $avginterval, $nethash\n";
        $onerow = SQL::d($query);
    }
    
    /*for($i = 0;$i<getblockcount();$i += 144)
    {
        $start = $i;
        $stop = $start+144;
        $onerow = SQL::a(SQL("SELECT bstat.number AS number, bstat.time AS time, bstat.bits AS bits, round((EXTRACT ('epoch' FROM bavg.time))::numeric, 0) AS avg FROM (SELECT avg(time.time)::interval AS time FROM (SELECT time-lag(time, 1) OVER (ORDER BY time) AS time FROM blocks WHERE blocks.number>$1 AND blocks.number<($2+1)) AS time) AS bavg, (SELECT number, EXTRACT ('epoch' FROM time) AS time, bits FROM blocks WHERE number = $2) AS bstat;", array($start, $stop)));
        $number = $onerow['number'];
        $time = $onerow['time'];
        $target = decodeCompact($onerow['bits']);
        if(empty($target) || $target == 0)
        {
            return;
        }
        $hashesToWin = bcdiv("1", bcdiv($target, "115792089237316195423570985008687907853269984665640564039457584007913129639935", 55), 0);
        $intervalSinceLast = $onerow['avg'];
        $nethash = bcdiv($hashesToWin, $intervalSinceLast, 0);
        echo "$number, $time, $target, $hashesToWin, $intervalSinceLast, $nethash\n";
        
    }*/
    return;
}

function page_mytransactions($req)
{
    // This RELIES on the fact that only address transactions will be sent/received
    ini_set("zlib.output_compression", "On");
    
    if(empty($req->params[0])) {
        echo "Returns all transactions sent or received by the period-separated Bitcoin
addresses in parameter 1. The optional parameter 2 contains a hexadecimal block
hash: transactions in blocks up to and including this block will not be returned.

The transactions are returned as a JSON object. The object's \"keys\" are transaction
hashes. The structure is like this (mostly the same as jgarzik's getblock):
root
transaction hash
hash (same as above)
version
number of inputs
number of outputs
lock time
size (bytes)
inputs
previous output
hash of previous transaction
index of previous output
scriptsig (replaced by \"coinbase\" on generation inputs)
sequence (only when the sequence is non-default)
address (on address transactions only!)
outputs
value
scriptpubkey
address (on address transactions only!)
block hash
block number
block time

Only transactions to or from the listed *addresses* will be shown. Public key transactions
will not be included.

When encountering an error, the response will start with \"ERROR:\", followed by
the error. An appropriate HTTP response code will also be sent. A response with no body
must also be considered to be an error.

/q/mytransactions/address1.address2/blockHash";
        return;
    }
    if(!empty($req->params[1]))
    {
        $req->params[1] = remove0x(trim(strtolower($req->params[1])));
        if(!preg_match("/[0-9a-f]{64}/", $req->params[1]))
        {
            echo "ERROR: block limit is in invalid format";
            senderror(400);
            return;
        }
        $blocklimit = (int)SQL::s("SELECT number FROM blocks WHERE hash = decode($1, 'hex');", array($req->params[1]));
        if(!$blocklimit)
            $blocklimit = 0;
    }
    else
    {
        $blocklimit = 0;
    }
    // gather addresses
    $addresses = explode('.', trim($req->params[0]));
    foreach($addresses as &$address)
    {
        if(!preg_match("/^[1-9A-HJ-NP-Za-km-z]{25,44}$/", $address) || ! Address::checkAddress($address))
        {
            echo "ERROR: One or more addresses are invalid";
            senderror(400);
            return;
        }
        $address = "decode('".addressToHash160($address)."', 'hex')";
    }
    // this is safe because addresses were checked above
    $addresses = implode(", ", $addresses);
    $sql = <<<EOF
SELECT encode(blocks.hash, 'hex') AS block, encode(transactions.hash, 'hex') AS tx, blocks.number AS blocknum, blocks.time AT TIME ZONE 'UTC' AS time, transactions.id AS tid, transactions.raw AS rawtx
FROM inputs JOIN transactions ON (inputs.tx = transactions.hash) JOIN blocks ON (inputs.block = blocks.hash)
WHERE inputs.type = 'Address' AND blocks.number>$1 AND inputs.hash160 IN ($addresses)
UNION
SELECT encode(blocks.hash, 'hex') AS block, encode(transactions.hash, 'hex') AS tx, blocks.number AS blocknum, blocks.time AT TIME ZONE 'UTC' AS time, transactions.id AS tid, transactions.raw AS rawtx
FROM outputs JOIN transactions ON (outputs.tx = transactions.hash) JOIN blocks ON (outputs.block = blocks.hash)
WHERE outputs.type = 'Address' AND blocks.number>$1 AND outputs.hash160 IN ($addresses)
ORDER BY tid;
EOF;
    $result = SQL($sql, array($blocklimit));
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
