#!/usr/bin/php
<?php

require_once 'oldcode/jsonrpc.php';
require_once 'oldcode/util.php';

$CONFLICT_LOG = "/var/www/blockexplorer.com/logs/conflict.log";

$lockfile=fopen("/tmp/blockupdate.lock","w+");
if(!flock($lockfile,LOCK_EX | LOCK_NB))
{
        die(); //other instance running
}

$db=pg_connect("dbname=explore") or die();

date_default_timezone_set('UTC');

//check to see if the system has been shut down
$tail=`tail -n 1 $CONFLICT_LOG`;
if(preg_match("/shutdown/",$tail)!=0)
{
	die("Stop: system has been shut down");
}

function logconflict($log)
{
	$file=fopen($CONFLICT_LOG,"a");
	fwrite($file,$log."\n");
	fclose($file);
}

function simplifyscript($script)
{
	$script=preg_replace("/[0-9a-f]+ OP_DROP ?/","",$script);
	$script=preg_replace("/OP_NOP ?/","",$script);
	return trim($script);
}

function getblockbynumber($num)
{
	do
	{
		set_time_limit(2);
		$data=rpcQuery("getblock",array($num));
		set_time_limit(0);
		if(!isset($data)||is_null($data)||is_null($data["r"])||!is_null($data["e"]))
		{
			echo "Error: retrying...\r\n".var_dump($data);
			sleep(5);
		}
	}
	while(!isset($data)||is_null($data)||is_null($data["r"])||!is_null($data["e"]));
	
	return $data["r"];
}
function getblockcount()
{
	set_time_limit(2);
	$data=rpcQuery("getblockcount");
	set_time_limit(0);
	if(!isset($data)||is_null($data)||is_null($data["r"])||!is_null($data["e"])||!is_int($data["r"]))
	{
		echo "Can't get block count\r\n";
		die();
	}
	return $data["r"];
}

function processblock($num)
{
	global $db;
	$block=getblockbynumber($num);
	$blockhash=$block->hash;
	$prevblock=$block->prev_block;
	$root=$block->mrkl_root;
	$timestamp=$block->time;
	$bits=$block->bits;
	$nonce=$block->nonce; //float
	$txcount=$block->n_tx;
	$rawblock=indent(json_encode($block));
	$transactions=$block->tx;
	$blocksize=$block->size;
	echo "\nBLOCK\n";
	echo "Num: ".$num."$\n";
	echo "Hash: ".$blockhash."$\n";
	echo "Prev: ".$prevblock."$\n";
	echo "Root: ".$root."$\n";
	echo "Bits: ".$bits."$\n";
	echo "Nonce: ".$nonce."$\n";
	echo "Timestamp: ".$timestamp."$\n";
	echo "Size: ".$blocksize."$\n";
	
	$oldhash=pg_fetch_assoc(pg_query_params($db,"SELECT encode(hash,'hex') AS oldhash FROM blocks WHERE number=$1",array($num)));
	$oldhash=$oldhash["oldhash"];
	if(!$oldhash)
	{
		pg_query($db,"BEGIN;");
		$totalvalue="0";
		$transactioncount=0;
		pg_query_params($db, "INSERT INTO blocks(hash,prev,number,root,bits,nonce,raw,time,size) VALUES (decode($1,'hex'),decode($2,'hex'),$3,decode($4,'hex'),$5,$6,$7,$8,$9);",array($blockhash,$prevblock,$num,$root,$bits,$nonce,$rawblock,date("Y-m-d H:i:s \U\T\C",$timestamp),$blocksize));
		foreach($transactions as $tx)
		{
			$transactioncount++;
			$txvalue=processtransaction($tx,$blockhash,$num);
			$totalvalue=bcadd($txvalue,$totalvalue,8);
		}
		pg_query_params($db,"UPDATE blocks SET transactions=$1,totalvalue=$2 WHERE hash=decode($3,'hex');",array($transactioncount,$totalvalue,$blockhash));
		echo "Total value: $totalvalue$\n";
		echo "Transactions: $transactioncount$\n";
		pg_query($db,"COMMIT;");
		return 1;
	}
	else if($oldhash==$blockhash)
	{
		echo "Already have this block";
		return 2;
	}
	else
	{
		pg_query($db,"BEGIN;");
		echo "***Deleting conflicting block***";
		logconflict(date("r").": block $num replaced");
		sleep(10);
		pg_query_params($db,"DELETE FROM blocks WHERE number=$1",array($num));
		$totalvalue="0";
		$transactioncount=0;
		pg_query_params($db, "INSERT INTO blocks(hash,prev,number,root,bits,nonce,raw,time,size) VALUES (decode($1,'hex'),decode($2,'hex'),$3,decode($4,'hex'),$5,$6,$7,$8,$9);",array($blockhash,$prevblock,$num,$root,$bits,$nonce,$rawblock,date("Y-m-d H:i:s \U\T\C",$timestamp),$blocksize));
		foreach($transactions as $tx)
		{
			$transactioncount++;
			$txvalue=processtransaction($tx,$blockhash,$num);
			$totalvalue=bcadd($txvalue,$totalvalue,8);
		}
		pg_query_params($db,"UPDATE blocks SET transactions=$1,totalvalue=$2 WHERE hash=decode($3,'hex');",array($transactioncount,$totalvalue,$blockhash));
		echo "Total value: $totalvalue$\n";
		echo "Transactions: $transactioncount$\n";
		pg_query($db,"COMMIT;");
		return 3;
	}
}

function updateKeys($hash160,$pubkey,$blockhash)
{
	global $db;
	$address=hash160ToAddress($hash160);
	$result=pg_fetch_assoc(pg_query_params($db,"SELECT pubkey,encode(hash160,'hex') AS hash160 FROM keys WHERE hash160=decode($1,'hex')",array($hash160)));
	if(!$result && !is_null($pubkey))
	{
		pg_query_params($db, "INSERT INTO keys VALUES (decode($1,'hex'),$2,decode($3,'hex'),decode($4,'hex'));",array($hash160,$address,$pubkey,$blockhash));
	}
	else if(!$result)
	{
		pg_query_params($db, "INSERT INTO keys(hash160,address,firstseen) VALUES (decode($1,'hex'),$2,decode($3,'hex'));",array($hash160,$address,$blockhash));
	}
	else if($result && !is_null($pubkey) && is_null($result["pubkey"]))
	{
		if($result["hash160"]!=strtolower(hash160($pubkey)))
		{
			sleep(10);
			die("Hashes don't match");
		}
		pg_query_params($db, "UPDATE keys SET pubkey = decode($1,'hex') WHERE hash160=decode($2,'hex');",array($pubkey,$hash160));
	}
}

function processtransaction($tx,$blockhash,$blocknum)
{
	//returns tx value
	global $db;
	$txhash=$tx->hash;
	$txsize=$tx->size;
	$rawtx=indent(json_encode($tx));
	if(pg_num_rows(pg_query_params($db, "SELECT hash FROM transactions WHERE hash=decode($1,'hex');",array($txhash)))===0)
	{
		pg_query_params($db, "INSERT INTO transactions(hash,block,raw,size) VALUES (decode($1,'hex'),decode($2,'hex'),$3,$4);",array($txhash,$blockhash,$rawtx,$txsize));
	}
	else
	{
		if(pg_num_rows(pg_query_params($db,"SELECT hash FROM transactions WHERE hash=decode($1,'hex') AND block<>decode($2,'hex');",array($txhash,$blockhash)))==1)
		{
			echo "***Duplicate transaction: adding special record***";
			sleep(30);
			pg_query_params($db, "INSERT INTO special VALUES (decode($1,'hex'),decode($2,'hex'),'Duplicate');",array($txhash,$blockhash));
			return "0";
		}
		else
		{
			die("Can't insert tx");
		}
	}
	foreach($tx->in as $input)
	{
		$type=NULL;
		$prev=NULL;
		$previndex=NULL;
		$hash160=NULL;
		$scriptsig=NULL;
		$index=NULL;
		$value=NULL;
		
		echo "INPUT\n";
		if(isset($input->coinbase))
		{
			$type="Generation";
			$value=bcdiv("50",floor(pow(2,floor($blocknum/210000))),8);
			$scriptsig=$input->coinbase;
			
		}
		else
		{
			$prev=$input->prev_out->hash;
			$index=$input->prev_out->n;
			$scriptsig=$input->scriptSig;
			$simplescriptsig=simplifyscript($scriptsig);
			echo "Simplescriptsig: ".$simplescriptsig."$\n";
			
			$prevtx=pg_fetch_assoc(pg_query_params($db, "SELECT value,type,encode(hash160,'hex') AS hash160 FROM outputs WHERE index=$1 AND tx=decode($2,'hex');",array($index,$prev)));
			if(!$prevtx)
			{
				var_dump(shell_exec("crontab -r"));
				die("Error: Failed getting prev tx...");
			}
			$value=$prevtx["value"];
			$type=$prevtx["type"];
			$hash160=$prevtx["hash160"];
			if($type=="Address")
			{
				if(preg_match("/^[0-9a-f]+ [0-9a-f]{66,130}$/",$simplescriptsig))
				{
					$pubkey=preg_replace("/^[0-9a-f]+ ([0-9a-f]{66,130})$/","$1",$simplescriptsig);
					$hash160=strtolower(hash160($pubkey));
					updateKeys($hash160,$pubkey,$blockhash);
				}
			}
			if(is_null($type))
			{
				var_dump(shell_exec("fcrontab -r"));
				die("Error: No input type");
			}
		}
		pg_query_params($db, "INSERT INTO inputs (tx,prev,index,value,scriptsig,hash160,type,block) VALUES (decode($1,'hex'),decode($2,'hex'),$3,$4,$5,decode($6,'hex'),$7,decode($8,'hex'))",array($txhash,$prev,$index,$value,$scriptsig,$hash160,$type,$blockhash));
		echo "Type: ".$type."$\n";
		echo "Value: ".$value."$\n";
		echo "Prev: ".$prev."$\n";
		echo "TxHash: ".$txhash."$\n";
		echo "Index: ".$index."$\n";
		echo "ScriptSig: ".$scriptsig."$\n";
		echo "Hash160: ".$hash160."$\n";
	}
	$index=-1;
	$txvalue="0";
	foreach($tx->out as $output)
	{
		$hash160=NULL;
		$type=NULL;
		$index++;
		echo "OUTPUT\n";
		$value=$output->value;
		$txvalue=bcadd($txvalue,$value,8);
		$scriptpubkey=$output->scriptPubKey;
		$simplescriptpk=simplifyscript($scriptpubkey);
		echo "Simplescriptpubkey: ".$simplescriptpk."$\n";
		
		//To pubkey
		if(preg_match("/^[0-9a-f]{66,130} OP_CHECKSIG$/",$simplescriptpk))
		{
			$type="Pubkey";
			$pubkey=preg_replace("/^([0-9a-f]{66,130}) OP_CHECKSIG$/","$1",$simplescriptpk);
			$hash160=strtolower(hash160($pubkey));
			updateKeys($hash160,$pubkey,$blockhash);
		}
		
		//To BC address
		if(preg_match("/^OP_DUP OP_HASH160 [0-9a-f]{40} OP_EQUALVERIFY OP_CHECKSIG$/",$simplescriptpk))
		{
			$type="Address";
			$hash160=preg_replace("/^OP_DUP OP_HASH160 ([0-9a-f]{40}) OP_EQUALVERIFY OP_CHECKSIG$/","$1",$simplescriptpk);
			updateKeys($hash160,NULL,$blockhash);
		}
		
		if(is_null($type))
		{
			$type="Strange";
		}
		pg_query_params($db, "INSERT INTO outputs (tx,index,value,scriptpubkey,hash160,type,block) VALUES (decode($1,'hex'),$2,$3,$4,decode($5,'hex'),$6,decode($7,'hex'));",array($txhash,$index,$value,$scriptpubkey,$hash160,$type,$blockhash));
		echo "Hash160: ".$hash160."$\n";
		echo "Type: ".$type."$\n";
		echo "Index: ".$index."$\n";
		echo "Value: ".$value."$\n";
		echo "Scriptpubkey: ".$scriptpubkey."$\n";
	}
	pg_query_params($db, "UPDATE transactions SET fee=(SELECT (SELECT sum(value) FROM inputs WHERE tx=decode($1,'hex'))-(SELECT sum(value) from outputs WHERE tx=decode($1,'hex'))) WHERE hash=decode($1,'hex');",array($txhash));
	return $txvalue;
}
$blockcount=getblockcount();
$wehave=pg_fetch_assoc(pg_query($db,"SELECT max(number) AS max FROM blocks;"));
$wehave=(int)$wehave["max"];
echo "\r\n\r\n".date("r")."\r\n\r\n";
if($wehave<5)
{
	$wehave=5;
}
if($blockcount<=$wehave)
{
	echo "No update necessary\n";
	die();
}
$wehave=$wehave-5;
echo "Starting block update: $wehave to $blockcount\r\n";
sleep(5);

$earliest=true;
for($current=$wehave;$blockcount>=$current;$current++)
{
	$blockstatus=processblock($current);
	if($blockstatus==3&&$earliest==true)
	{
		logconflict("Reorg limit: system shutdown");
		var_dump(shell_exec("crontab -r"));
		die("Error: Updating blocks too far back");
	}
	$earliest=false;
	
}
//processblock((int)$argv[1]);
echo "\n";
?>

