<?php
header("Cache-control: no-cache");
require_once 'testnet/util.php';
require_once 'testnet/jsonrpc.php';

function decodeCompact($c)
{
	$nbytes = ($c >> 24) & 0xFF;
	return bcmul($c & 0xFFFFFF,bcpow(2,8 * ($nbytes - 3)));
}
function encodeCompact($in)
{
	return exec("/var/www/blockexplorer.com/bin/getcompact.py $in");
	//By ArtForz
	/*
	#!/usr/bin/python
import struct
import sys

def num2mpi(n):
        """convert number to MPI string"""
        if n == 0:
                return struct.pack(">I", 0)
        r = ""
        neg_flag = bool(n < 0)
        n = abs(n)
        while n:
                r = chr(n & 0xFF) + r
                n >>= 8
        if ord(r[0]) & 0x80:
                r = chr(0) + r
        if neg_flag:
                r = chr(ord(r[0]) | 0x80) + r[1:]
        datasize = len(r)
        return struct.pack(">I", datasize) + r

def GetCompact(n):
        """convert number to bc compact uint"""
        mpi = num2mpi(n)
		        nSize = len(mpi) - 4
        nCompact = (nSize & 0xFF) << 24
        if nSize >= 1:
                nCompact |= (ord(mpi[4]) << 16)
        if nSize >= 2:
                nCompact |= (ord(mpi[5]) << 8)
        if nSize >= 3:
                nCompact |= (ord(mpi[6]) << 0)
        return nCompact

print GetCompact(eval(sys.argv[1]))
	*/
}

function dbconnect()
{
	$db=pg_connect("dbname=testexplore");
	if(!$db)
	{
		senderror(503);
		echo "ERROR: Could not connect to database. Try again in a few minutes. Tell me if it still doesn't work in 30 minutes.";
		error_log("/testnet/q/ database down");
		die();
	}
	return $db;
}
function dbquery($db,$query,$params=false)
{
	if($params!==false)
	{
		$return=pg_query_params($db,$query,$params);
	}
	else
	{
		$return=pg_query($db,$query);
	}
	if(!$return)
	{
		senderror(500);
		echo "ERROR: Database problem. Try again in a few minutes. Tell me if it still doesn't work in 30 minutes.";
		error_log("/testnet/q/ invalid pg_query");
		die();
	}
	return $return;
	
}
$getblockcountcache=0;
function getblockcount()
{
	global $getblockcountcache;
	if($getblockcountcache==0)
	{
		$data=rpcQuery("getblockcount");
		if(!isset($data)||is_null($data)||is_null($data["r"])||!is_null($data["e"])||!is_int($data["r"]))
		{
			senderror(503);
			echo "ERROR: Could not connect to JSON-RPC. Try again in a few minutes. Tell me if it still doesn't work in 30 minutes.";
			error_log("/testnet/q/ getblockcount failure: {$data["e"]}");
			die();
		}
		$getblockcountcache=$data["r"];
		return $data["r"];
	}
	else
	{
		return $getblockcountcache;
	}
}

function getdifficulty()
{
	$dtblock=getblockbynumber(getblockcount());
	$target=$dtblock->bits;
	$target=decodeCompact($target);
	return bcdiv("26959535291011309493156476344723991336010898738574164086137773096960",$target,8);
}
function getblockbynumber($num)
{
	$data=rpcQuery("getblock",array($num));
	if(!isset($data)||is_null($data)||is_null($data["r"])||!is_null($data["e"]))
	{
		senderror(503);
		echo "ERROR: Could not connect to JSON-RPC. Try again in a few minutes. Tell me if it still doesn't work in 30 minutes.";
		error_log("/testnet/q/ getblockbynumber failure: {$data["e"]}");
		die();
	}
	return $data["r"];
}

function getdecimaltarget()
{
	$dtblock=getblockbynumber(getblockcount());
	$target=$dtblock->bits;
	return decodeCompact($target);	
}
function getprobability()
{
	return bcdiv(getdecimaltarget(),"115792089237316195423570985008687907853269984665640564039457584007913129639935",55);
}
function getlastretarget()
{
	$blockcount=getblockcount();
	return ($blockcount-($blockcount%2016))-1;
}
if($page=="home")
{
echo '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
"http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<title>TEST Bitcoin real-time stats and tools</title>
</head>
<body>
<p>Usage: /testnet/q/query[/parameter]</p>
<p>This is data for the <span style="color:red">TESTnet</span>. Uptime and accuracy are not guaranteed.</p>
<p>Queries currently supported:</p>
<h4>Real-time stats</h4>
<p>While <a href="/">Bitcoin Block Explorer</a> can run at a delay of up to two minutes, these tools are all completely real-time.</p>
<ul>
<li><a href="/testnet/q/getdifficulty">getdifficulty</a> - shows the current difficulty as a multiple of the minimum difficulty (highest target) <b>on the main network</b>.</li>
<li><a href="/testnet/q/getblockcount">getblockcount</a> - shows the number of blocks in the longest block chain.</li>
<li><a href="/testnet/q/latesthash">latesthash</a> - shows the latest block hash.</li>
<li><a href="/testnet/q/hextarget">hextarget</a> - shows the current target as a hexadecimal number.</li>
<li><a href="/testnet/q/decimaltarget">decimaltarget</a> - shows the current target as a decimal number.</li>
<li><a href="/testnet/q/probability">probability</a> - shows the probability of a single hash solving a block with the current difficulty.</li>
<li><a href="/testnet/q/hashestowin">hashestowin</a> - shows the average number of hashes required to win a block with the current difficulty.</li>
<li><a href="/testnet/q/nextretarget">nextretarget</a> - shows the block count when the next retarget will take place.</li>
<li><a href="/testnet/q/estimate">estimate</a> - shows an estimate for the next mainline-equivalent difficulty.</li>
<li><a href="/testnet/q/totalbc">totalbc</a> - shows the total number of Bitcoins in circulation. You can also <a href="/testnet/q/totalbc/50000">see the circulation at a particular number of blocks</a>.</li>
<li><a href="/testnet/q/bcperblock">bcperblock</a> - shows the number of Bitcoins created per block. You can also <a href="/testnet/q/bcperblock/300000">see the BC per block at a particular number of blocks.</a></li>
</ul>
<h4>Delayed stats</h4>
<p>These use BBE data.</p>
<ul>
<li><a href="/testnet/q/avgtxsize">avgtxsize</a> - shows the average transaction size. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="/testnet/q/avgblocksize">avgblocksize</a> - shows the average block size. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="/testnet/q/interval">interval</a> - shows the average interval between blocks, in seconds. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="/testnet/q/eta">eta</a> - shows the estimated number of seconds until the next retarget. The parameter sets how many blocks to look back at (default 1000). Blocks before the last retarget are never taken into account, however.</li>
<li><a href="/testnet/q/avgtxnumber">avgtxnumber</a> - shows the average number of transactions per block. The parameter sets how many blocks to look back at (default 1000).</li>
<li><a href="/testnet/q/getreceivedbyaddress">getreceivedbyaddress</a> - shows the total BTC received by an address.</li>
<li><a href="/testnet/q/addressfirstseen">addressfirstseen</a> - shows the time at which an address was first seen on the network.</li>
<li><a href="/testnet/q/nethash">nethash</a> - produces CSV statistics about block difficulty. The parameter sets the interval between data points.</li>
</ul>
</body>
</html>
';
die();
}
else
{
	header("Content-type: text/plain");
}
//start main block - anything before this must die()
if($page=="getdifficulty")
{
	echo getdifficulty();
}
else if($page=="getblockcount")
{
	echo getblockcount();
}
else if($page=="latesthash")
{
	$block=getblockbynumber(getblockcount());
	echo strtoupper($block->hash);
}
else if($page=="hextarget")
{
	$target=encodeHex(getdecimaltarget());
	while(strlen($target)<64)
	{
		$target="0".$target;
	}
	echo $target;
}
else if($page=="decimaltarget")
{
	echo getdecimaltarget();
}
else if($page=="probability")
{
	echo getprobability();
}
else if($page=="hashestowin")
{
	echo bcdiv("1",getprobability(),0);
}
else if($page=="nextretarget")
{
	echo getlastretarget()+2016;
}
else if($page=="estimate")
{
	$currentcount=getblockcount(); //last one with the old difficulty
	$last=getlastretarget()+1; //first one with the "new" difficulty
	$targettime=600*($currentcount-$last+1);
	//check for cases where we're comparing the same two blocks
	if($targettime==0)
	{
		echo getdifficulty();
		die();
	}
	
	$oldblock=getblockbynumber($last);
	$newblock=getblockbynumber($currentcount);
	$oldtime=$oldblock->time;
	$oldtarget=decodeCompact($oldblock->bits);
	$newtime=$newblock->time;
	
	$actualtime=$newtime-$oldtime;
	
	if($actualtime<$targettime/4)
	{
		$actualtime=$targettime/4;
	}
	if($actualtime>$targettime*4)
	{
		$actualtime=$targettime*4;
	}
	
	$newtarget=bcmul($oldtarget,$actualtime);
	//check once more for safety
	if($newtarget=="0")
	{
		echo getdifficulty();
		die();
	}
	$newtarget=bcdiv($newtarget,$targettime,0);
	$newtarget=decodeCompact(encodeCompact($newtarget));
	//we now have the real new target
	echo bcdiv("26959535291011309493156476344723991336010898738574164086137773096960",$newtarget,8);
}
else if($page=="totalbc"||$page=="bcperblock")
{
	if(isset($param1)&&preg_match('/^[0-9]+$/',$param1)==1)
	{
		$blockcount=(string)$param1;
		if($blockcount>6929999)
		{
			$blockcount="6930000";
		}
	}
	else
	{
		$blockcount=getblockcount();
	}
	$blockworth="50";
	$totalbc="0";
	bcscale(8);
	while(bccomp($blockcount,"0")==1) //while blockcount is larger than 0
	{
		if(bccomp($blockcount,"210000")==-1) //if blockcount is less than 210000
		{
			$totalbc=(string)bcadd($totalbc,bcmul($blockworth,$blockcount));
			$blockcount="0";
		}
		else
		{
			$blockcount=bcsub($blockcount,"210000");
			$totalbc=(string)bcadd($totalbc,bcmul($blockworth,"210000"));
			$blockworth=bcdiv($blockworth,"2",8);
		}
	}
	
	if($page=="totalbc")
	{
		echo $totalbc;
	}
	else
	{
		echo $blockworth;
	}
}
else if($page=="avgtxsize")
{
	if(!isset($param1))
	{
		$param1=1000;
	}
	$param1=(int)$param1;
	if($param1>0)
	{
		$db=dbconnect();
		$result=dbquery($db,"SELECT round(avg(transactions.size),0) AS avg FROM transactions JOIN blocks ON (transactions.block=blocks.hash) WHERE blocks.number>(SELECT max(number) FROM blocks)-$1;",array($param1));
		$result=pg_fetch_assoc($result);
		$result=$result["avg"];
		echo $result;
	}
	else
	{
		senderror(400);
		echo "ERROR: the first parameter is the number of blocks to look back through.";
	}
}

else if($page=="avgblocksize")
{
	if(!isset($param1))
	{
		$param1=1000;
	}
	$param1=(int)$param1;
	if($param1>0)
	{
		$db=dbconnect();
		$result=dbquery($db,"SELECT round(avg(size),0) AS avg FROM blocks WHERE blocks.number>(SELECT max(number) FROM blocks)-$1;",array($param1));
		$result=pg_fetch_assoc($result);
		$result=$result['avg'];
		echo $result;
	}
	else
	{
		senderror(400);
		echo "ERROR: the first parameter is the number of blocks to look back through.";
	}
}

else if($page=="interval")
{
	if(!isset($param1))
	{
		//default lookback
		$param1=1000;
	}
	$param1=(int)$param1;
	if($param1<2)
	{
		senderror(400);
		echo "ERROR: invalid block count.";
		die();
	}
	$db=dbconnect();
	$result=pg_fetch_assoc(dbquery($db,"SELECT round((EXTRACT ('epoch' FROM avg(time.time)))::numeric,0) AS avg FROM (SELECT time-lag(time,1) OVER (ORDER BY time) AS time FROM blocks WHERE blocks.number>(SELECT max(number)-$1 FROM blocks)) AS time;",array($param1)));
	$result=$result['avg'];
	echo $result;
}

else if($page=="eta")
{
	if(!isset($param1))
	{
		//default lookback
		$param1=1000;
	}
	$param1=(int)$param1;
	if($param1<2)
	{
		senderror(400);
		echo "ERROR: invalid block count.";
		die();
	}
	$param1=min(getblockcount()-getlastretarget(),$param1);
	$param1=max($param1,2);
	$db=dbconnect();
	$result=pg_fetch_assoc(dbquery($db,"SELECT round((EXTRACT ('epoch' FROM avg(time.time)))::numeric,0) AS avg FROM (SELECT time-lag(time,1) OVER (ORDER BY time) AS time FROM blocks WHERE blocks.number>(SELECT max(number)-$1 FROM blocks)) AS time;",array($param1)));
	$result=$result['avg'];
	$blocksleft=(getlastretarget()+2016)-getblockcount();
	if($blocksleft==0)
	{
		$blocksleft=2016;
	}
	echo $blocksleft*$result;	
}

else if($page=="avgtxnumber")
{
	if(!isset($param1))
	{
		//default lookback
		$param1=1000;
	}
	$param1=(int)$param1;
	if($param1<1)
	{
		senderror(400);
		echo "ERROR: invalid block count.";
		die();
	}
	$db=dbconnect();
	$result=pg_fetch_assoc(dbquery($db,"SELECT round(avg(a.count),3) AS avg FROM (SELECT block,count(*) AS count FROM transactions GROUP BY block) AS a JOIN blocks ON blocks.hash=a.block WHERE blocks.number>(SELECT max(number)-$1 FROM blocks);",array($param1)));
	$result=$result['avg'];
	echo $result;
}

else if($page=="getreceivedbyaddress")
{
	if(isset($param1))
	{
		$param1=trim($param1);
	}
	else
	{
		echo "Returns total BTC received by an address. Sends are not taken into account.\n/testnet/q/getreceivedbyaddress/address";
		die();
	}
	if(isset($param1)&&strlen($param1)>24&&strlen($param1)<36&&checkAddress($param1,"6F"))
	{
		$hash160=addressToHash160($param1);
		$db=dbconnect();
		$result=pg_fetch_assoc(dbquery($db,"SELECT sum(value) AS sum FROM outputs WHERE hash160=decode($1,'hex');",array($hash160)));
		$result=$result["sum"];
		if(is_null($result))
		{
			$result=0;
		}
		echo $result;
		
	}
	else
	{
		senderror(400);
		echo "ERROR: invalid address";
	}
}

else if($page=="addressfirstseen")
{
	if(isset($param1))
	{
		$param1=trim($param1);
	}
	else
	{
		echo "Returns the block time at which an address was first seen.\n/testnet/q/addressfirstseen/address";
		die();
	}
	if(isset($param1)&&strlen($param1)>24&&strlen($param1)<36&&checkAddress($param1,"6F"))
	{
		$db=dbconnect();
		$result=pg_fetch_assoc(dbquery($db,"SELECT time AT TIME ZONE 'UTC' AS time FROM keys JOIN blocks ON keys.firstseen=blocks.hash WHERE address=$1;",array($param1)));
		$result=$result["time"];
		if(is_null($result))
		{
			$result="Never seen";
		}
		echo $result;
		
	}
	else
	{
		senderror(400);
		echo "ERROR: invalid address";
	}
}
else if($page=="nethash")
{
	$db=dbconnect();
	if(!isset($param1))
	{
		$param1=144;
	}
	$param1=(int)$param1;
	if(empty($param1)||!($param1>4&&$param1<10001))
	{
		senderror(400);
		echo "ERROR: invalid stepping (must be 5-10,000)";
		die();
	}
	
	echo "blockNumber,time,target,difficulty,hashesToWin,avgIntervalSinceLast,netHashPerSecond\n";
	$query=dbquery($db,"SELECT number, EXTRACT ('epoch' FROM time) AS time, bits, round(EXTRACT ('epoch' FROM (SELECT avg(a.time) FROM (SELECT time-lag(time,1) OVER (ORDER BY time) AS time FROM blocks WHERE number>series AND number<series+($1+1)) AS a))::numeric,0) AS avg FROM blocks, generate_series(0,(SELECT max(number) FROM blocks),$1) AS series(series) WHERE number=series+$1;",array($param1));
	$onerow=pg_fetch_assoc($query);
	
	while($onerow)
	{
		$number=$onerow["number"];
		$time=$onerow["time"];
		$target=decodeCompact($onerow["bits"]);
		if(empty($target))
		{
			senderror(500);
			echo "ERROR: divide by zero";
			die();
		}
		$difficulty=bcdiv("26959535291011309493156476344723991336010898738574164086137773096960",$target,2);
		$hashestowin=bcdiv("1",bcdiv($target,"115792089237316195423570985008687907853269984665640564039457584007913129639935",55),0);
		$avginterval=$onerow['avg'];
		$nethash=bcdiv($hashestowin,$avginterval,0);
		echo "$number,$time,$target,$difficulty,$hashestowin,$avginterval,$nethash\n";
		$onerow=pg_fetch_assoc($query);
	}
	
	/*for($i=0;$i<getblockcount();$i+=144)
	{
		$start=$i;
		$stop=$start+144;
		$onerow=pg_fetch_array(dbquery($db,"SELECT bstat.number AS number,bstat.time AS time,bstat.bits AS bits,round((EXTRACT ('epoch' FROM bavg.time))::numeric,0) AS avg FROM (SELECT avg(time.time)::interval AS time FROM (SELECT time-lag(time,1) OVER (ORDER BY time) AS time FROM blocks WHERE blocks.number>$1 AND blocks.number<($2+1)) AS time) AS bavg, (SELECT number,EXTRACT ('epoch' FROM time) AS time,bits FROM blocks WHERE number=$2) AS bstat;",array($start,$stop)));
		$number=$onerow['number'];
		$time=$onerow['time'];
		$target=decodeCompact($onerow['bits']);
		if(empty($target)||$target==0)
		{
			die();
		}
		$hashesToWin=bcdiv("1",bcdiv($target,"115792089237316195423570985008687907853269984665640564039457584007913129639935",55),0);
		$intervalSinceLast=$onerow['avg'];
		$nethash=bcdiv($hashesToWin,$intervalSinceLast,0);
		echo "$number,$time,$target,$hashesToWin,$intervalSinceLast,$nethash\n";
		
	}*/
	die();
}
else //no matching page
{
	senderror(404);
	echo "ERROR: invalid query";
}
