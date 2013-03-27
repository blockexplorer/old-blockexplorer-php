<?php
define("VERSION",13);
define("ENABLECACHE",false);
define("CHECKPOINT",10673);
define("MAINTMODE",false);

//false=errors will be header errors; true=body errors
$error=false;
$title="Error";

require_once 'jsonrpc.php';
require_once 'util.php';
date_default_timezone_set('UTC');
function error($message,$status=false)
{
global $error;
//also run goto end
	if($status!==false)
	{
		senderror($status);
	}
	if($error===false)
	{
		$error=$message;
	}
	else
	{
		echo "<p>".$message."</p>\n";
		echo "<p>Tell me (theymos) if this is a bug.</p>\n";
	}
}

function cache($etag=VERSION,$customcache=false,$override=false)
{
	$etag=(string)$etag;
	$baseetag=$etag;
	if($etag!=VERSION)
	{
		$etag=$etag."-".VERSION;
	}
	$etag="W/\"$etag\"";
	header("ETag: $etag");
	if(ENABLECACHE===true||$override==true)
	{
		if($customcache===false)
		{
			if($baseetag==VERSION)
			{
				$cachetime=86400;
			}
			else
			{
				$cachetime=0;
			}
		}
		else
		{
			$cachetime=$customcache;
		}
		if($cachetime!=0&&is_int($cachetime))
		{
			header('Expires: '.gmdate('D, d M Y H:i:s \G\M\T', time() + $cachetime));
		}
	}
	
	if(isset($_SERVER['HTTP_IF_NONE_MATCH'])&&!is_null($_SERVER['HTTP_IF_NONE_MATCH'])&&!empty($_SERVER['HTTP_IF_NONE_MATCH']))
	{
		$tags=stripslashes($_SERVER['HTTP_IF_NONE_MATCH']);
		$tags=preg_split("/, /",$tags );
		foreach($tags as $tag)
		{
			if($tag==$etag)
			{
				header($_SERVER["SERVER_PROTOCOL"]." 304 Not Modified");
				die();
				
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
	$encodemessage=urlencode($message);
	return "<sup><a href=\"/testnet/nojshelp/$encodemessage\"title=\"$message\" onClick=\"informHelp();return false\" class=\"help\">?</a></sup>";
}
function removeTrailingZeroes($value)
{
	$end=strlen($value)-1;
	$i=$end;
	$target=0;
	if(strpos($value,".")!=false)
	{
		while($i>0&&($value[$i]=="0"||$value[$i]=="."))
		{
			$target++;
			if($value[$i]==".")
			{
			break;
			}
			$i--;
		}
	}
	return $value=substr($value,0,$end-$target+1);
}
function removeLeadingZeroes($value)
{
		while($value[0]=="0")
		{
			$value=substr($value,1);
		}
		return $value;
}

function decodeCompact($c)
{
	$nbytes = ($c >> 24) & 0xFF;
	return bcmul($c & 0xFFFFFF,bcpow(2,8 * ($nbytes - 3)));
}

function thousands($num)
{
	$start=strpos($num,".");
	if($start===false)
	{
		$start=strlen($num)-1;
		$return="";
	}
	else
	{
		$return=substr($num,$start);
		$start=$start-1;
	}
	$count=0;
	for($i=$start;$i>-1;$i--)
	{
		$count++;
		$return=$num[$i].$return;
		if($count==3&&$i!=0)
		{
			$return="&thinsp;".$return;
			$count=0;
		}
	}
	return $return;
}

if(MAINTMODE!==false&&$_SERVER["REMOTE_ADDR"]!="192.168.1.1")
{
	error("Bitcoin Block Explorer will be back shortly.",503);
	$title="Maintenance mode";
	goto headerend;
}

$db=pg_connect("dbname=testexplore") or die();

///Homepage
if($page=="home")
{
	$title="Home";
	$description="Testnet version of Bitcoin Block Explorer - a tool for viewing Bitcoin data.";
	$keywords="";
	$result=pg_query($db,"SELECT number AS number,encode(hash,'hex') AS hash,time AT TIME ZONE 'UTC' AS time,transactions AS count,totalvalue AS sum,size FROM blocks ORDER BY number DESC LIMIT 20;");
	$oneblock=pg_fetch_assoc($result);
	cache($oneblock["number"]);
}
///Search
if($page=="search")
{
	//The form on /testnet/ POST submits to /search/, but I want it to go to a static page (without ?q= stuff)
	if(isset($_POST["q"]))
	{
		redirect("/testnet/search/{$_POST["q"]}");
	}
	$title="Search";
	$input=$param1;
	if($input==NULL)
	{
		redirect("/testnet/");
	}
	$input=trim($input);
	$blocks=array();
	$addresses=array();
	$transactions=array();
	//$errortext="Incorrect search input. You can search for a Bitcoin address, a block number, a public key, a hash160, a transaction hash, or a block hash. Block numbers are expressed in decimal, and addresses are expressed in the normal base58; everything else is hexadecimal. You can also enter a 6+ character portion of any of these (except block numbers).";
	if(preg_match("/^[0-9A-HJ-NP-Za-km-z]+$/",$input)!=1)
	{
		error("Invalid characters.",400);
		goto headerend;
	}
	//block number
	if(preg_match("/^[0-9]{1,7}$/",$input)==1)
	{
		$result=pg_fetch_assoc(pg_query_params($db,"SELECT encode(hash,'hex') AS hash FROM blocks WHERE number=$1;",array($input)));
		$result=$result["hash"];
		if(!is_null($result)&&$result!=false)
		{
			redirect("/testnet/block/$result");
		}
	}
	//size limits
	if(strlen($input)<6||strlen($input)>130)
	{
		error("The number of characters you entered is either too small (must be 6+), or too large to ever return any results (130 hex characters is the size of a public key).",400);
		goto headerend;
	}
	//address
	if(strlen($input)<36&&preg_match("/0/",$input)!=1)
	{
		$result=pg_query_params($db,"SELECT address FROM keys WHERE address LIKE $1 LIMIT 100;",array("%".$input."%"));
		$oneaddr=pg_fetch_assoc($result);
		while($oneaddr)
		{
			array_push($addresses,$oneaddr["address"]);
			$oneaddr=pg_fetch_assoc($result);
		}
	}
	//hex only from here
	$originput=$input;
	$input=strtolower(remove0x($input));
	if(preg_match("/[0-9a-f]{4,130}/",$input)!=1)
	{
		goto jump;
	}
	//pubkey
	$result=pg_query_params($db,"SELECT address FROM keys WHERE encode(pubkey,'hex') LIKE $1 LIMIT 100;",array("%".$input."%"));
	$oneaddr=pg_fetch_assoc($result);
	while($oneaddr)
	{
		array_push($addresses,$oneaddr["address"]);
		$oneaddr=pg_fetch_assoc($result);
	}
	//new size limits
	if(strlen($input)>64)
	{
		goto jump;
	}
	//block hash
	$result=pg_query_params($db,"SELECT encode(hash,'hex') AS hash FROM blocks WHERE encode(hash,'hex') LIKE $1 LIMIT 100;",array("%".$input."%"));
	$oneblock=pg_fetch_assoc($result);
	while($oneblock)
	{
		array_push($blocks,$oneblock["hash"]);
		$oneblock=pg_fetch_assoc($result);
	}
	//tx hash
	$result=pg_query_params($db,"SELECT encode(hash,'hex') AS hash FROM transactions WHERE encode(hash,'hex') LIKE $1 LIMIT 100;",array("%".$input."%"));
	$onetx=pg_fetch_assoc($result);
	while($onetx)
	{
		array_push($transactions,$onetx["hash"]);
		$onetx=pg_fetch_assoc($result);
	}
	//new size limits
	if(strlen($input)>40)
	{
		goto jump;
	}
	//hash160
	$result=pg_query_params($db,"SELECT address FROM keys WHERE encode(hash160,'hex') LIKE $1 LIMIT 100;",array("%".$input."%"));
	$oneaddr=pg_fetch_assoc($result);
	while($oneaddr)
	{
		array_push($addresses,$oneaddr["address"]);
		$oneaddr=pg_fetch_assoc($result);
	}
	//jump to perfect results
	jump:
	if(count($transactions)+count($blocks)+count($addresses)==1)
	{
		if(count($transactions)==1)
		{
			$goto=$transactions[0];
			redirect("/testnet/tx/$goto");
		}
		if(count($blocks)==1)
		{
			$goto=$blocks[0];
			redirect("/testnet/block/$goto");
		}
		if(count($addresses)==1)
		{
			$goto=$addresses[0];
			redirect("/testnet/address/$goto");
		}
	}
	//jump to exact address/hash160/pubkey pages if no results
	if(count($transactions)+count($blocks)+count($addresses)==0)
	{
		if(strlen($input)==130&&preg_match("/[0-9a-f]{4,130}/",$input))
		{
			$input=pubKeyToAddress($input);
			redirect("/testnet/address/$input");
			die();
		}
		if(strlen($input)==40&&preg_match("/[0-9a-f]{4,130}/",$input))
		{
			$input=hash160ToAddress($input,"6F");
			redirect("/testnet/address/$input");
			die();
		}
		if(checkAddress($originput,"6F"))
		{
			redirect("/testnet/address/$originput");
			die();
		}
	}
}
////short links
if($page=="b")
{
	if(preg_match("/^[0-9]{1,7}$/",$param1)==1)
	{
		$result=pg_fetch_assoc(pg_query_params($db,"SELECT encode(hash,'hex') AS hash FROM blocks WHERE number=$1;",array($param1)));
		$result=$result["hash"];
		if(!is_null($result)&&$result!=false)
		{
			redirect("/testnet/block/$result",301);
		}
	}
}
if($page=="t")
{
	if(preg_match("/^[1-9A-HJ-NP-Za-km-z]{7,20}$/",$param1)==1)
	{
		$hash=strtolower(decodeBase58($param1));
		$hash=pg_query_params($db,"SELECT encode(hash,'hex') AS hash FROM transactions WHERE encode(hash,'hex') LIKE $1;",array($hash."%"));
		$num_rows=pg_num_rows($hash);
		if($num_rows==1)
		{
			$hash=pg_fetch_assoc($hash);
			$hash=$hash["hash"];
			redirect("/testnet/tx/$hash",301);
		}
		if($num_rows>1)
		{
			$title="Multiple choices";
			$error="<p>That ShortLink could apply to multiple pages. Maybe I need to increase the number of bytes in each address...</p>\n<ul>\n";
			error_log("ShortLink ambiguity: $param1");
			$onetx=pg_fetch_assoc($hash);
			while($onetx)
			{
				$thistx=$onetx["hash"];
				$error.="<li><a href=\"/testnet/tx/$thistx\">$thistx</a></li>\n";
				$onetx=pg_fetch_assoc($hash);
			}
			$error.="</ul>\n";
		}
	}
}
if($page=="a")
{
	if(preg_match("/^[1-9A-HJ-NP-Za-km-z]{7,20}$/",$param1)==1)
	{
		$hash=strtolower(decodeBase58($param1));
		$address=pg_query_params($db,"SELECT address AS address FROM keys WHERE encode(hash160,'hex') LIKE $1;",array($hash."%")); 
		$num_rows=pg_num_rows($address);
		if($num_rows==1)
		{
			$address=pg_fetch_assoc($address);
			$address=$address["address"];
			redirect("/testnet/address/$address",301);
		}
		if($num_rows>1)
		{
			$title="Multiple choices";
			$error="<p>That ShortLink could apply to multiple pages. Maybe I need to increase the number of bytes in each address...</p>\n<ul>\n";
			error_log("ShortLink ambiguity: $param1");
			$oneaddr=pg_fetch_assoc($address);
			while($oneaddr)
			{
				$thisaddr=$oneaddr["address"];
				$error.="<li><a href=\"/testnet/address/$thisaddr\">$thisaddr</a></li>\n";
				$oneaddr=pg_fetch_assoc($address);
			}
			$error.="</ul>\n";
		}
	}
}

///Raw block/tx
if($page=="rawtx")
{
	header("Content-type: text/plain");
	$tx=trim(strtolower(remove0x($param1)));
	if(preg_match("/^[0-9a-f]{64}$/",$tx)!=1)
	{
		echo "ERROR: Not in correct format";
		die();
	}
	$result=pg_fetch_assoc(pg_query_params($db,"SELECT raw FROM transactions WHERE hash=decode($1,'hex');",array($tx)));
	if($result==false||is_null($result))
	{
	echo "ERROR: Transaction does not exist.";
	die();
	}
	else
	{
	cache();
	echo $result["raw"];
	die();
	}
}
if($page=="rawblock")
{
	header("Content-type: text/plain");
	$block=trim(strtolower(remove0x($param1)));
	if(preg_match("/^[0-9a-f]{64}$/",$block)!=1)
	{
		echo "ERROR: Not in correct format";
		die();
	}
	$result=pg_fetch_assoc(pg_query_params($db,"SELECT raw FROM blocks WHERE hash=decode($1,'hex');",array($block)));
	if($result==false||is_null($result))
	{
	echo "ERROR: Block does not exist.";
	die();
	}
	else
	{
	cache();
	echo $result["raw"];
	die();
	}
}
///Block
if($page=="block")
{
	//Get hash
	$block=trim(strtolower(remove0x($param1)));
	if(preg_match("/^[0-9a-f]{64}$/",$block)!=1)
	{
		error("Not in correct format",400);
		goto headerend;
	}
	
	//Get block data
	$result=pg_fetch_assoc(pg_query_params($db,"SELECT encode(prev,'hex') AS prev,number,encode(root,'hex') AS root,bits,nonce,time AT TIME ZONE 'UTC' AS time,transactions AS count,totalvalue,size FROM blocks WHERE hash=decode($1,'hex');",array($block)));
	if(!$result || is_null($result))
	{
		error("No such block",404);
		goto headerend;
	}
	//Don't expire blocks that are the latest
	$next=pg_fetch_assoc(pg_query_params($db,"SELECT encode(hash,'hex') AS next FROM blocks WHERE prev=decode($1,'hex');",array($block)));
	$next=$next["next"];
	if($next!=false&&!is_null($next))
	{
		cache();
	}
	else
	{
		cache(VERSION."o",0);
	}
	
	
	$title="Block {$result["number"]}";
	$description="List of transactions in Bitcoin block #{$result["number"]}.";
	$keywords="block, {$result["number"]}, $block";
}
///Transaction
if($page=="tx")
{
	//get tx hash
	$hash=trim(strtolower(remove0x($param1)));
	if(preg_match("/^[0-9a-f]{64}$/",$hash)!=1)
	{
		error("Not in correct format",400);
		goto headerend;
	}
	$hashtrunc=substr($hash,0,10)."...";
	$tx=pg_fetch_assoc(pg_query_params($db,"SELECT encode(transactions.block,'hex') AS block,transactions.fee AS fee,transactions.size AS size,blocks.time AT TIME ZONE 'UTC' AS time,blocks.number AS blocknumber FROM transactions LEFT JOIN blocks ON transactions.block=blocks.hash WHERE transactions.hash=decode($1,'hex');",array($hash)));
	if($tx===false||is_null($tx))
	{
		error("No such transaction",404);
		goto headerend;
	}
	cache();
	
	$outputs=pg_query_params($db,"SELECT outputs.index AS index,outputs.value AS value,sum(outputs.value) OVER () AS totalvalue,keys.address AS address,outputs.type AS type,outputs.scriptpubkey AS scriptpubkey FROM outputs LEFT JOIN keys ON keys.hash160=outputs.hash160 WHERE outputs.tx=decode($1,'hex') ORDER BY outputs.index;",array($hash));
	$inputs=pg_query_params($db,"SELECT encode(inputs.prev,'hex') AS prev,inputs.index AS index,inputs.value AS value,sum(inputs.value) OVER () AS totalvalue,keys.address AS address,inputs.type AS type,inputs.scriptsig AS scriptsig,inputs.id AS id FROM inputs LEFT JOIN keys ON keys.hash160=inputs.hash160 WHERE inputs.tx=decode($1,'hex') ORDER BY inputs.id;",array($hash));
	$oneoutput=pg_fetch_assoc($outputs);
	$oneinput=pg_fetch_assoc($inputs);
	
	$totalin=thousands(removeTrailingZeroes($oneinput["totalvalue"]));
	$totalout=thousands(removeTrailingZeroes($oneoutput["totalvalue"]));
	$fee=removeTrailingZeroes($tx["fee"]);
	$numin=pg_num_rows($inputs);
	$numout=pg_num_rows($outputs);
	
	$title="Tx $hashtrunc";
	$description="Information about Bitcoin transaction $hashtrunc.";
	$keywords="transaction, $hash";
}

///Address
if($page=="address")
{
	$knownaddress=false;
	//get address
	$address=$param1;
	if(!preg_match('/^[1-9A-HJ-NP-Za-km-z]+$/',$address)==1 ||strlen($address)>36||!checkAddress($address,"6F"))
	{
		error("Invalid address",400);
		goto headerend;
	}
	$hash160=strtolower(addressToHash160($address));
	$keyinfo=pg_fetch_assoc(pg_query_params($db,"SELECT encode(pubkey,'hex') AS pubkey,encode(firstseen,'hex') AS firstseen FROM keys WHERE hash160=decode($1,'hex');",array($hash160)));
	$pubkey=$keyinfo["pubkey"];
	if($pubkey===false||is_null($pubkey))
	{
		$pubkey="Unknown (not seen yet)";
	}
	$firstseen=$keyinfo["firstseen"];
	if($firstseen!==false&&!is_null($firstseen))
	{
		$knownaddress=true;
		$blockinfo=pg_fetch_assoc(pg_query_params($db,"SELECT number,time AT TIME ZONE 'UTC' AS time FROM blocks WHERE hash=decode($1,'hex');",array($firstseen)));
		$blocknum=$blockinfo["number"];
		$blocktime=$blockinfo["time"];
		$blockstring="<a href=\"/testnet/block/$firstseen\">Block $blocknum</a> ($blocktime)";
	}
	else
	{
		$blockstring="Never used on the network (as far as I can tell)";
	}
	$title="Address $address";
	$description="List of transactions involving Bitcoin address $address.";
	$keywords="address, $address, $hash160";
	
	$mytxs=pg_query_params($db,"SELECT inputs.type AS txtype,'debit' AS type,encode(inputs.tx,'hex') AS tx,inputs.value AS value,inputs.id AS id,encode(transactions.block,'hex') AS block,blocks.number AS blocknum,transactions.id AS tid, inputs.index AS index, blocks.time AT TIME ZONE 'UTC' AS time FROM inputs,transactions,blocks WHERE inputs.hash160=decode($1,'hex') AND inputs.tx=transactions.hash AND transactions.block=blocks.hash UNION SELECT outputs.type AS txtype,'credit' AS type,encode(outputs.tx,'hex') AS tx,outputs.value AS value,outputs.index AS id,encode(transactions.block,'hex') AS block,blocks.number AS blocknum,transactions.id AS tid, outputs.index AS index, blocks.time AT TIME ZONE 'UTC' AS time FROM outputs,transactions,blocks WHERE outputs.hash160=decode($1,'hex') AND outputs.tx=transactions.hash AND transactions.block=blocks.hash ORDER BY blocknum,type,tid,index;",array($hash160));
	$txlimit=pg_num_rows($mytxs)-1;
	$txcounter=0;
	//cache
	if($knownaddress)
	{
		$latesttx=pg_fetch_assoc($mytxs,$txlimit);
		$latesttx=$latesttx["blocknum"];
	}
	else
	{
		$latesttx=-1;
	}
	cache($latesttx);
}
if($page=="nojshelp")
{
	cache();
	$title="Scriptless help";
}
/*if($page=="sitemap")
{
	if($scheme=="http://")
	{
		$buffer="";
		if(isset($param1))
		{
			$param1=(int)$param1;
			if($param1>=0&&$param1<500)
			{
				$start=$param1*10000;
				$end=$start+10000;
				$alldata=pg_query_params($db,"SELECT a.url AS url,a.id AS id,a.type AS type FROM (SELECT '/tx/'||encode(hash,'hex') AS url,id,'tx' AS type FROM transactions UNION ALL SELECT '/address/'||address AS url,id,'address' AS type FROM keys UNION ALL SELECT '/block/'||encode(hash,'hex') AS url,number AS id, 'block' AS type FROM blocks) AS a ORDER BY a.id DESC, a.type LIMIT $1 OFFSET $2;",array(10000,$start));
				$oneurl=pg_fetch_array($alldata);
				$buffer.= '<?xml version="1.0" encoding="ISO-8859-1"?>'."\n";
				$buffer.= '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'."\n";
				while($oneurl)
				{
					$buffer.= '<url>'."\n";
					$buffer.= "<loc>http://blockexplorer.com{$oneurl["url"]}</loc>"."\n";
					$type=$oneurl["type"];
					if($type=="address")
					{
						$priority="0.7";
						$changefreq="hourly";
					}
					if($type=="tx")
					{
						$priority="0.5";
						$changefreq="monthly";
					}
					if($type=="block")
					{
						$priority="0.6";
						$changefreq="monthly";
					}
					if(isset($priority)&&isset($changefreq))
					{
						$buffer.= "<changefreq>$changefreq</changefreq>"."\n";
						$buffer.= "<priority>$priority</priority>"."\n";
					}
					unset($priority,$changefreq);
					$buffer.= '</url>'."\n";
					$oneurl=pg_fetch_array($alldata);
				}
				$buffer.= '</urlset>';
			}
		}
		else
		{
			$datacount=pg_fetch_array(pg_query($db,"SELECT count(a.url) AS count FROM (SELECT '/tx/'||encode(hash,'hex') AS url,id,'tx' AS type FROM transactions UNION ALL SELECT '/address/'||address AS url,id,'address' AS type FROM keys UNION ALL SELECT '/block/'||number AS url,number AS id, 'block' AS type FROM blocks) AS a;"));
			$datacount=$datacount["count"];
			$current=0;
			$count=0;
			$buffer.= '<?xml version="1.0" encoding="ISO-8859-1"?>'."\n";
			$buffer.= '<sitemapindex xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'."\n";
			while($count<$datacount)
			{
				$buffer.= "<sitemap>"."\n";
				$buffer.= "<loc>http://blockexplorer.com/sitemap-$current.xml</loc>"."\n";
				$buffer.= "</sitemap>"."\n";
				$count+=10000;
				$current++;
			}
			$buffer.= '</sitemapindex>';
		}
		$bufferhash=hash("md4",$buffer);
		cache($bufferhash,86400);
		header("Content-type: text/xml");
		echo $buffer;
		die();
	}
}*/

if($page=="sitemap")
{
	if($scheme=="http://")
	{
		$buffer="";
		$returnedresults=0;
		$interval=10000;
		if(isset($param1)&&isset($param2)&&($param1=="a"||$param1=="t"||$param1=="b")&&$param2>=0&&$param2<500)
		{
			$start=$param2*$interval;
			if($param1=="a")
			{
				$data=pg_query_params($db,"SELECT '/address/'||address AS url,id FROM keys ORDER BY id OFFSET $1 LIMIT $2;",array($start,$interval));
			}
			if($param1=="t")
			{
				$data=pg_query_params($db,"SELECT '/tx/'||encode(hash,'hex') AS url,id FROM transactions ORDER BY id OFFSET $1 LIMIT $2;",array($start,$interval));
			}
			if($param1=="b")
			{
				$data=pg_query_params($db,"SELECT '/block/'||encode(hash,'hex') AS url,number AS id FROM blocks ORDER BY id OFFSET $1 LIMIT $2;",array($start,$interval));
			}
			$returnedresults=pg_num_rows($data);
			$oneurl=pg_fetch_array($data);
			$buffer.= '<?xml version="1.0" encoding="ISO-8859-1"?>'."\n";
			$buffer.= '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'."\n";
			while($oneurl)
			{
				$buffer.= '<url>'."\n";
				$buffer.= "<loc>http://blockexplorer.com{$oneurl["url"]}</loc>"."\n";
				if($param1=="a")
				{
					$priority="0.7";
					$changefreq="hourly";
				}
				if($param1=="t")
				{
					$priority="0.5";
					$changefreq="monthly";
				}
				if($param1=="b")
				{
					$priority="0.6";
					$changefreq="monthly";
				}
				if(isset($priority)&&isset($changefreq))
				{
					$buffer.= "<changefreq>$changefreq</changefreq>"."\n";
					$buffer.= "<priority>$priority</priority>"."\n";
				}
				unset($priority,$changefreq);
				$buffer.= '</url>'."\n";
				$oneurl=pg_fetch_array($data);
			}
			$buffer.= '</urlset>';
		}
		if(!isset($param1)&&!isset($param2))
		{
			$data=pg_fetch_assoc(pg_query($db,"SELECT (SELECT count(number) FROM blocks) AS blocks,(SELECT count(id) FROM transactions) AS transactions,(SELECT count(id) FROM keys) AS addresses;"));
			$totaltx=ceil($data["transactions"]/$interval)-1;
			$totalblk=ceil($data["blocks"]/$interval)-1;
			$totaladdr=ceil($data["addresses"]/$interval)-1;
			
			$buffer.= '<?xml version="1.0" encoding="ISO-8859-1"?>'."\n";
			$buffer.= '<sitemapindex xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'."\n";
			for($i=0;$i<=$totaltx;$i++)
			{
				$buffer.= "<sitemap>"."\n";
				$buffer.= "<loc>http://blockexplorer.com/sitemap-t-$i.xml</loc>"."\n";
				$buffer.= "</sitemap>"."\n";
			}
			for($i=0;$i<=$totaladdr;$i++)
			{
				$buffer.= "<sitemap>"."\n";
				$buffer.= "<loc>http://blockexplorer.com/sitemap-a-$i.xml</loc>"."\n";
				$buffer.= "</sitemap>"."\n";
			}
			for($i=0;$i<=$totalblk;$i++)
			{
				$buffer.= "<sitemap>"."\n";
				$buffer.= "<loc>http://blockexplorer.com/sitemap-b-$i.xml</loc>"."\n";
				$buffer.= "</sitemap>"."\n";
			}
			$buffer.= '</sitemapindex>';
		}
		$bufferhash=hash("md4",$buffer);
		if($returnedresults==$interval)
		{
			$expires=604800;
		}
		else
		{
			$expires=600;
		}
		cache($bufferhash,$expires,true);
		header("Content-type: text/xml");
		echo $buffer;
		die();
	}
}

/*if($page=="robots.txt")
{
	cache();
	header("");
	header("Content-type: text/plain");
	echo "User-agent: *
Disallow: /t/
Disallow: /b/
Disallow: /a/
Allow: /";
	die();
}*/

//This must be set
if($error===false)
{
	$error=true;
}
//Haven't done anything? Page doesn't exist.
if($title=="Error"&&is_bool($error))
{
error("No such page",404);
}
headerend:
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
   "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<script type="text/javascript">

  var _gaq = _gaq || [];
  _gaq.push(['_setAccount', 'UA-38773634-1']);
  _gaq.push(['_trackPageview']);

  (function() {
    var ga = document.createElement('script'); ga.type = 'text/javascript'; ga.async = true;
    ga.src = ('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
    var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
  })();

</script>
<link rel="shortcut icon" href="/favicon.ico">
<meta http-equiv="Content-type" content="text/html;charset=ISO-8859-1">
<?php
if(isset($keywords))
{
	if($keywords!="")
	{
		$keywords=", ".$keywords;
	}
	echo "<meta name=\"keywords\" content=\"bitcoin, testnet, search, data$keywords\">\n";
}
if(isset($description))
{
	echo "<meta name=\"description\" content=\"$description\">\n";
}
?>
<title><?php echo "$title - TEST Bitcoin Block Explorer"; ?></title>
<style type="text/css">
.infoList{list-style-type:none;margin-left:0;padding-left:0}
table{border-collapse:collapse}
table,td,th{border:1px solid black;padding:4px}
div.hugeCell{width:300px;overflow:auto}
div.hugeData{width:700px;overflow:auto}
#footer{text-align:center;font-size:smaller;margin-top:2em}
div#shortlink{font-size:smaller;margin-top:-1.5em;margin-bottom:-1em;margin-left:0.5em}
.help{cursor:help}
</style>
<script type="text/javascript">
function highlightNamedAnchor()
{
	if(location.hash!="")
	{
		document.getElementsByName(location.hash.substr(1,location.hash.length))[0].parentNode.parentNode.style.backgroundColor="#FFFDD0";
	}
}
function informHelp()
{
	alert("These question mark links produce help text when you hover your mouse cursor over them.");
}
</script>
</head>
<body onLoad="highlightNamedAnchor()">
<?php
//error handling for header errors
if(!is_bool($error))
{
	echo $error;
	goto end;
}

if($page=="nojshelp")
{
	$param1=htmlspecialchars(urldecode($param1));
	if(strlen($param1)<4000)
	{
		echo "<p>These question mark links produce help text when you hover your mouse cursor over them. In case your client does not support displaying HTML title attributes, here is the text for the link you clicked:</p>\n";
		echo "<p><i>$param1</i></p>\n";
	}
}
///search
if($page=="search")
{
	$donesomething=false;
	if(!empty($transactions))
	{
		echo "<h3>Transactions</h3>\n";
		echo "<ul>\n";
		foreach ($transactions as $i)
		{
			echo "<li><a href=\"/testnet/tx/$i\">$i</a></li>\n";
			$donesomething=true;
		}
		echo "</ul>\n";
	}
	if(!empty($addresses))
	{
		echo "<h3>Addresses</h3>\n";
		echo "<ul>\n";
		foreach ($addresses as $i)
		{
			echo "<li><a href=\"/testnet/address/$i\">$i</a></li>\n";
			$donesomething=true;
		}
		echo "</ul>\n";
	}
	if(!empty($blocks))
	{
		echo "<h3>Blocks</h3>\n";
		echo "<ul>\n";
		foreach ($blocks as $i)
		{
			echo "<li><a href=\"/testnet/block/$i\">$i</a></li>\n";
			$donesomething=true;
		}
		echo "</ul>\n";
	}
	if($donesomething===false)
	{
		error("No results.");
		goto end;
	}
	else
	{
		echo "<p>Note: results may not be complete if the search would return more than 100 items.</p>\n";
	}
}

if($page=="home")
{
echo '
	<h1>Bitcoin Block Explorer</h1>
	<p>Bitcoin Block Explorer allows you to easily view information about the
	<a href="http://www.bitcoin.org/wiki/doku.php?id=block">blocks</a>, <a href="http://www.bitcoin.org/wiki/doku.php?id=address">addresses</a>, and <a href="http://www.bitcoin.org/wiki/doku.php?id=transactions">transactions</a> created by <a href="http://bitcoin.org">Bitcoin</a>.
	It uses the <a href="https://www.bitcoin.org/smf/index.php?topic=724.0">getblock</a> patch by jgarzik, but also does a
	ton of processing to make certain tasks, such as tracking transactions, easier. Help text is included in the tooltips produced by superscript question marks: '.help("Like this").'. All times are UTC. Tell me (theymos) if you find any bugs.';
	
	echo '<p><span style="color:red">This data is for the testnet!</span> Data for the main Bitcoin network is <a href="'.$scheme.'blockexplorer.com">here</a>. I will probably forget to move new features from the main BBE to here. I\'ll also likely create some accidental broken links. Tell me if you see any such bugs.</p>';
	$latestblock=pg_fetch_array(pg_query($db,"SELECT max(number) AS latest FROM blocks;"));
	$latestblock=$latestblock["latest"];
	if($latestblock<CHECKPOINT)
	{
		echo "<p><span style=\"color:red\">Notice:</span> The block database is currently being reloaded (probably to enable some cool new feature). You can refresh this page to see the progress.</p>";
	}
	
	echo "<h3>Search</h3>\n";
	echo '<p>You can enter all or part of a block number, address, block hash, transaction hash, hash160, or public key. Input must be at least 6 characters long. Hashes are expressed in hexadecimal.</p>'."\n";
	echo '<form action="/testnet/search" method="post" ><p><input type="text" name="q" size="50"> <input type="submit" value="Search"></p></form>'."\n";
	
	///latest blocks
	echo "<h3>Latest blocks".help("Up to two minutes delay.")."</h3>\n";
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Number".help("A count of the number of blocks up to this one, with the genesis block being 0.")."</th><th>Hash".help("Truncated hash of this block.")."</th><th>Time".help("UTC time included in this block. The network's time must not be relied upon for precision, but it is generally accurate.")."</th><th>Transactions".help("Number of transactions in this block. All blocks have at least one generation transaction.")."</th><th>Total BTC".help("Total BTC moved by the transactions in this block.")."</th><th>Size (kB)".help("The data size of this block. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).")."</th></tr>\n";
	while($oneblock)
	{
		$hash=$oneblock["hash"];
		$hashtrunc=removeLeadingZeroes($hash);
		$hashtrunc=substr($hashtrunc,0,10)."...";
		
		$number=$oneblock["number"];
		$time=$oneblock["time"];
		$total=$oneblock["count"];
		$size=round($oneblock["size"]/1000,3);
		$totalbtc=removeTrailingZeroes($oneblock["sum"]);
		echo "<tr>\n";
		echo "<td><a href=\"/testnet/block/$hash\">$number</a></td>\n";
		echo "<td><a href=\"/testnet/block/$hash\">$hashtrunc</a></td>\n";
		echo "<td>$time</td>\n";
		echo "<td>$total</td>\n";
		echo "<td>$totalbtc</td>\n";
		echo "<td>$size</td>\n";
		echo "</tr>\n";
		$oneblock=pg_fetch_assoc($result);
	}
	echo "</table>\n";
	
	///largest transactions last 300 blocks
	$result=pg_query($db,"SELECT encode(inputs.tx,'hex') AS hash,sum(inputs.value) AS totalvalue, encode(blocks.hash,'hex') AS blockhash, blocks.number AS blocknum,blocks.time AT TIME ZONE 'UTC' AS time FROM blocks JOIN inputs ON (inputs.block=blocks.hash) WHERE blocks.number>(SELECT max(blocks.number)-300 FROM blocks) GROUP BY blocks.time,inputs.tx,blocks.hash,blocks.number ORDER BY totalvalue DESC LIMIT 20;");
	echo "<h3>Largest transactions (last 300 blocks)".help("Sorted by BTC moved")."</h3>\n";
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Transaction".help("Truncated transaction hash.")."</th><th>Amount".help("BTC moved.")."</th><th>Block".help("The block this transaction appeared in.")."</th><th>Time".help("UTC network time of the block this appeared in (somewhat unreliable).")."</th></tr>\n";
	$onetx=pg_fetch_assoc($result);
	while($onetx)
	{
		$txhash=$onetx["hash"];
		$txhashtrunc="<a href=\"/testnet/tx/$txhash\">".substr($txhash,0,10)."...</a>";
		$amount=removeTrailingZeroes($onetx["totalvalue"]);
		$blockhash=$onetx["blockhash"];
		$blockview="<a href=\"/testnet/block/$blockhash\">".$onetx["blocknum"]."</a>";
		$time=$onetx["time"];
		echo "<tr><td>$txhashtrunc</td><td>$amount</td><td>$blockview</td><td>$time</td></tr>";
		$onetx=pg_fetch_assoc($result);
	}
	echo "</table>\n";
	
	///latest strange transactions
	echo "<h3>Latest strange transactions</h3>";
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Transaction".help("Truncated transaction hash.")."</th><th>Block".help("The block this transaction appeared in.")."</th><th>Time".help("UTC network time of the block this appeared in (somewhat unreliable).")."</th></tr>\n";
	$result=pg_query($db,"SELECT DISTINCT encode(outputs.tx,'hex') AS txhash,encode(outputs.block,'hex') AS blockhash,outputs.id,blocks.number AS blocknum,blocks.time AT TIME ZONE 'UTC' AS time FROM outputs JOIN blocks ON (blocks.hash=outputs.block) WHERE outputs.type='Strange' ORDER BY outputs.id DESC LIMIT 20;"); 
	$onetx=pg_fetch_assoc($result);
	while($onetx)
	{
		$txhash=$onetx["txhash"];
		$txhashtrunc="<a href=\"/testnet/tx/$txhash\">".substr($txhash,0,10)."...</a>";
		$blockhash=$onetx["blockhash"];
		$blockview="<a href=\"/testnet/block/$blockhash\">".$onetx["blocknum"]."</a>";
		$time=$onetx["time"];
		echo "<tr><td>$txhashtrunc</td><td>$blockview</td><td>$time</td></tr>";
		$onetx=pg_fetch_assoc($result);
	}
	echo "</table>";
}

if($page=="block")
{
	//process data
	$target=(string)decodeCompact($result["bits"]);
	$difficulty=thousands(removeTrailingZeroes(bcdiv("26959535291011309493156476344723991336010898738574164086137773096960",$target,6)));
	$prev=$result["prev"];
	$totalvalue=removeTrailingZeroes($result["totalvalue"]);
	$transactioncount=$result["count"];
	
	echo "<h1>Block {$result["number"]}".help("The number is a count of the number of blocks up to this one, with the genesis block being 0.")."</h1>\n";
	$shortlink="/testnet/b/{$result["number"]}";
	echo "<div id=\"shortlink\">Short link: <a href=\"$shortlink\">".$scheme."blockexplorer.com$shortlink</a></div>";
	echo "<ul class=\"infoList\">\n";
	echo "<li>Hash".help("Full hash of this block. Sometimes this is expressed without the leading zeroes.").": $block</li>\n";
	if($prev!="0000000000000000000000000000000000000000000000000000000000000000")
	{
		echo "<li>Previous block".help("Every block builds on another, forming a chain. This is the full hash of the previous block.").": <a href=\"/testnet/block/$prev\">$prev</a></li>\n";
	}
	if($next!=false&&!is_null($next))
	{
		echo "<li>Next block".help("The full hash of the block that will build onto this one. This field is not included in real blocks.").": <a href=\"/testnet/block/$next\">$next</a></li>\n";
	}
	echo "<li>Time".help("UTC time included in this block. The network's time must not be relied upon for precision, but it is generally accurate.").": {$result["time"]}</li>\n";
	echo "<li>Difficulty equivalent".help("The difficulty of producing blocks at the time this block was created. This is calculated in relation to the main network's minimum target, *not* the test network's.").": $difficulty (\"Bits\"".help("This is the compact form of the 256-bit target used when generating. This is included in actual blocks. The difficulty number is derived from this.").": ".strtolower(encodeHex($result["bits"])).")</li>\n";
	echo "<li>Transactions".help("Number of transactions in this block (listed below)").": $transactioncount</li>\n";
	echo "<li>Total BTC".help("Total BTC sent through this block, including fees").": $totalvalue</li>\n";
	$properbytes=$result["size"];
	if($properbytes<1000)
	{
		$properbytes="$properbytes bytes";
	}
	else
	{
		$properbytes=$properbytes/1000;
		$properbytes="$properbytes kilobytes";
	}
	echo "<li>Size".help("The data size of this block. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).").": $properbytes</li>\n";
	echo "<li>Merkle root".help("The root hash in a hash tree of all transactions.").": {$result["root"]}</li>\n";
	echo "<li>Nonce".help("When generating, Bitcoin starts this number at 1 and increments for each hash attempt.").": {$result["nonce"]}</li>\n";
	echo "<li><a href=\"/testnet/rawblock/$block\">Raw block</a>".help("Almost the same as getblock's output.")."</li>\n";
	echo "</ul>";
	
	echo "<h3>Transactions</h3>\n";
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Transaction".help("Truncated hash of this transaction")."</th><th>Fee".help("Fee given - the difference between total input value and total output value. This goes to the generator of the block.")."</th><th>Size (kB)".help("The data size of this transaction. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).")."</th><th>From (amount)".help("List of all addresses that appear in an input. Whoever sent this transaction owns all of these addresses.")."</th><th>To (amount)".help("A list of all addresses that have received bitcoins from this transaction")."</th></tr>\n";
	
	//prepare SQL
	pg_prepare($db,"transactions","SELECT encode(hash,'hex') AS hash,abs(fee) AS fee,size FROM transactions WHERE block=decode($1,'hex') ORDER BY id;") or die;
	pg_prepare($db,"outputs","SELECT outputs.value AS value,keys.address AS address FROM outputs LEFT JOIN keys ON keys.hash160=outputs.hash160 WHERE outputs.tx=decode($1,'hex') ORDER BY outputs.id;") or die;
	pg_prepare($db,"inputs","SELECT inputs.value AS value,keys.address AS address FROM inputs LEFT JOIN keys ON keys.hash160=inputs.hash160 WHERE inputs.tx=decode($1,'hex') ORDER BY inputs.id;") or die;
	
	$coinbase=true;
	
	//special transactions
	$result=pg_query_params($db,"SELECT encode(tx,'hex') AS hash FROM special WHERE block=decode($1,'hex')",array($block));
	$row=pg_fetch_assoc($result);
	while($row)
	{
		$hash=$row["hash"];
		$hashtrunc="<a href=\"/testnet/tx/$hash\">".substr($hash,0,10)."...</a>";
		echo "<tr><td colspan=\"5\">This transaction is an exact copy of $hashtrunc. This is usually caused by flawed custom miner code that rarely changes the keys used by generations, and is therefore likely to produce a generation transaction with the exact same data as a previous one by the same person. The network sees duplicate transactions as the same: only one can be redeemed.</td></tr>\n";
		$coinbase=false;
		$row=pg_fetch_assoc($result);
	}
	
	//get list of transactions
	$result=pg_execute($db,"transactions",array($block));
	$row=pg_fetch_assoc($result);
	
	//go through each transaction
	while($row)
	{
	echo "<tr>\n";
		$hash=$row["hash"];
		/////UPDATE
		$hashtrunc="<a href=\"/testnet/tx/$hash\">".substr($hash,0,10)."...</a>";
		$fee=removeTrailingZeroes($row["fee"]);
		if($coinbase==true)
		{
			$totalfee=$fee;
			$fee="0";
		}
		$size=round($row["size"]/1000,3);
		echo "<td>$hashtrunc</td><td>$fee</td><td>$size</td>\n";
		
		//collect inputs
		$inputs=pg_execute($db,"inputs",array($hash));
		$oneinput=pg_fetch_assoc($inputs);
		echo "<td><ul class=\"infoList\">\n";
		//go through inputs
		while($oneinput)
		{
			//parse and linkify address
			$address=$oneinput["address"];
			if(is_null($address))
			{
				$address="Unknown";
			}
			else
			{
				$address="<a href=\"/testnet/address/$address\">$address</a>";
			}
			
			//parse value
			$value=removeTrailingZeroes($oneinput["value"]);
			if(is_null($value))
			{
				error("No value found",500);
				goto end;
			}
			if($coinbase==true)
			{
				$address="Generation";
				$value="$value + $totalfee total fees";
			}
			
			echo "<li>$address: $value</li>\n";
			
			$oneinput=pg_fetch_assoc($inputs);
		}
		echo "</ul></td>\n";
		
		//collect outputs
		$outputs=pg_execute($db,"outputs",array($hash));
		$oneoutput=pg_fetch_assoc($outputs);
		echo "<td><ul class=\"infoList\">\n";
		//go through outputs
		while($oneoutput)
		{
			//parse and linkify addresses
			$address=$oneoutput["address"];
			if(is_null($address))
			{
				$address="Unknown";
			}
			else
			{
				$address="<a href=\"/testnet/address/$address\">$address</a>";
			}
			
			//parse value
			$value=removeTrailingZeroes($oneoutput["value"]);
			if(is_null($value))
			{
				error("No value found",500);
				goto end;
			}
			
			echo "<li>$address: $value</li>\n";
			
			$oneoutput=pg_fetch_assoc($outputs);
		}
		echo "</ul></td>\n";
		
		$coinbase=false;
		
		echo "</tr>\n";
		
		$row=pg_fetch_assoc($result);		
	}
	
	echo "</table>\n";
}

if($page=="tx")
{
	echo "<h1>Transaction</h1>\n";
	$shortlink=encodeBase58(substr(strtoupper($hash),0,14));
	$shortlink="/testnet/t/$shortlink";
	echo "<div id=\"shortlink\">Short link: <a href=\"$shortlink\">".$scheme."blockexplorer.com$shortlink</a></div>";
	echo "<ul class=\"infoList\">\n";
	
	echo "<li>Hash".help("Full hash of this transaction").": $hash</li>\n";
	echo "<li>Appeared in <a href=\"/testnet/block/{$tx["block"]}\">block {$tx["blocknumber"]}</a> ({$tx["time"]})</li>\n";
	echo "<li>Number of inputs".help("Total number of previous outputs this transaction redeems").": $numin (<a href=\"#inputs\">Jump to inputs</a>)</li>\n";
	echo "<li>Total BTC in".help("Total BTC redeemed from previous transactions").": $totalin</li>\n";
	echo "<li>Number of outputs: $numout (<a href=\"#outputs\">Jump to outputs</a>)</li>\n";
	echo "<li>Total BTC out".help("Total BTC sent with this transaction.").": $totalout</li>\n";
	$properbytes=$tx["size"];
	if($properbytes<1000)
	{
		$properbytes="$properbytes bytes";
	}
	else
	{
		$properbytes=$properbytes/1000;
		$properbytes="$properbytes kilobytes";
	}
	echo "<li>Size".help("The data size of this transaction. This is the number that Bitcoin uses for block size limits and fees -- it may not be the actual size on disk. 1 kilobyte = 1000 bytes (this is how Bitcoin does it).").": $properbytes</li>\n";
	echo "<li>Fee".help("The amount of BTC given to the person who generated the block this appeared in. It's the difference between total BTC in and total BTC out.").": $fee</li>\n";
	echo "<li><a href=\"/testnet/rawtx/$hash\">Raw transaction</a>".help("Almost the same as getblock.")."</li>\n";
	//duplicate transactions
	$duplicate="";
	$duplicates=pg_query_params($db,"SELECT encode(block,'hex') AS block FROM special WHERE tx=decode($1,'hex')",array($hash));
	$onedup=pg_fetch_assoc($duplicates);
	$firstdup=true;
	if($onedup)
	{
		$duplicate.="<li>Duplicates".help("An exact copy of this transaction appeared in these blocks. These copies are not spendable.").":";
		while($onedup)
		{
			$hashtrunc=substr($hashtrunc,0,10)."...";
			$blockhash=$onedup["block"];
			if(!$firstdup)
			{
				$duplicate.=" ,";
			}
			$duplicate.="<a href=\"/testnet/block/$blockhash\">".substr(removeLeadingZeroes($blockhash),0,10)."</a>";
			$onedup=pg_fetch_assoc($duplicates);
		}
		$duplicate.="</li>";
	}
	echo $duplicate;
	//end duplicates
	echo "</ul>\n";
	//inputs
	echo "<h3><a name=\"inputs\">Inputs</a>".help("Each input redeems a previous output with a signature.")."</h3>\n";
	
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Previous output (index)".help("The truncated hash of a previous transaction and the index of the output that this input is redeeming (after the colon). The first output in a transaction has an index of 0.")."</th><th>Amount".help("Amount of BTC gotten from this output")."</th><th>From address".help("The addresses of the referenced outputs. Whoever sent this transaction owns all of these addresses.")."</th><th>Type".help("The type of the referenced output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")."</th><th>ScriptSig".help("This script is matched with the referenced output's scriptPubKey. It usually contains a signature, and possibly a public key. ScriptSigs of generation inputs are sometimes called the 'coinbase' parameter, and they contain the current compact target and the extraNonce variable")."</th></tr>\n";
	//go through inputs
	while($oneinput)
	{
		$type=$oneinput["type"];
		$value=removeTrailingZeroes($oneinput["value"]);
		if($type=="Generation")
		{
			$value="$value + fees";
		}
		$myid=$oneinput["id"];
		echo "<tr>\n";
		$prev=$oneinput["prev"];
		$previndex=$oneinput["index"];
		$prevtrunc=substr($prev,0,12)."...:$previndex";
		if(!is_null($prev))
		{
		echo "<td><a name=\"i$myid\" href=\"/testnet/tx/$prev#o$previndex\">$prevtrunc</a></td>\n";
		}
		else
		{
			echo "<td><a name=\"i$myid\">N/A</a></td>\n";
		}
		echo "<td>$value</td>\n";
		$address=$oneinput["address"];
		if(is_null($address))
		{
			$address="Unknown";
			if($type=="Generation")
			{
				$address="N/A";
			}
		}
		else
		{
			$address="<a href=\"/testnet/address/$address\">$address</a>";
		}
		echo "<td>$address</td>\n";
		echo "<td>$type</td>\n";
		echo "<td><div class=\"hugeCell\">{$oneinput["scriptsig"]}</div></td>\n";
		echo "</tr>\n";
		$oneinput=pg_fetch_assoc($inputs);
	}
	
	echo "</table>\n";
	
	//outputs
	echo "<h3><a name=\"outputs\">Outputs</a>".help("Each output sends BTC to some address. In the official client, usually one output sends coins to the destination, and one output sends coins back to a new address owned by the sender.")."</h3>\n";
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Index".help("Starts at 0 and increments for each output.")."</th><th>Redeemed at input".help("If this output has ever been redeemed, the transaction that did it is listed here. (If you look at these links, you will see that I assign a number to each input. This is an internal ID unrelated to Bitcoin.)")."</th><th>Amount".help("BTC sent by this output")."</th><th>To address".help("Addresses this output was sent to")."</th><th>Type".help("The type of the output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")."</th><th>ScriptPubKey".help("This script specifies the conditions that must be met by someone attempting to redeem this output. Usually it contains a hash160 (Bitcoin address) or a public key.")."</th></tr>\n";
	//prepare query
	pg_prepare($db,"redeemed","SELECT id,encode(tx,'hex') AS tx FROM inputs WHERE prev=decode('$hash','hex') AND index=$1");
	//go through outputs
	while($oneoutput)
	{
		$index=$oneoutput["index"];
		echo "<tr>\n";
		$redeemed=pg_fetch_assoc(pg_execute($db,"redeemed",array($index)));
		if($redeemed!==false)
		{
			$rtx=$redeemed["tx"];
			$rid=$redeemed["id"];
			$rtxtrunc="<a name=\"o$index\" href=\"/testnet/tx/$rtx#i$rid\">".substr($rtx,0,12)."...</a>";
		}
		else
		{
			$rtxtrunc="<a name=\"o$index\">Not yet redeemed</a>";
		}
		echo "<td>$index</td>\n";
		echo "<td>$rtxtrunc</td>\n";
		$value=removeTrailingZeroes($oneoutput["value"]);
		echo "<td>$value</td>\n";
		$address=$oneoutput["address"];
		if(is_null($address))
		{
			$address="Unknown";
		}
		else
		{
			$address="<a href=\"/testnet/address/$address\">$address</a>";
		}
		echo "<td>$address</td>\n";
		echo "<td>{$oneoutput["type"]}</td>\n";
		echo "<td><div class=\"hugeCell\">{$oneoutput["scriptpubkey"]}</div></td>\n";
		$oneoutput=pg_fetch_assoc($outputs);
		echo "</tr>\n";
	}
	echo "</table>\n";
}

if($page=="address")
{
	$currentaddress=$address;
	echo "<h1>Address $address</h1>\n";
	if($knownaddress)
	{
		$shortlink=encodeBase58(substr(strtoupper($hash160),0,14));
		$shortlink="/testnet/a/$shortlink";
		echo "<div id=\"shortlink\">Short link: <a href=\"$shortlink\">".$scheme."blockexplorer.com$shortlink</a></div>";
	}
	echo "<ul class=\"infoList\">\n";
	echo "<li>First seen".help("The first block this address was used in.").": $blockstring</li>\n";
	
	//loop through our transactions. This is all echoed later.
	$balance="0";
	pg_prepare($db,"outputcounter","SELECT DISTINCT outputs.type AS type,outputs.value AS value,outputs.id,keys.address AS address FROM outputs LEFT JOIN keys ON outputs.hash160=keys.hash160 WHERE outputs.tx=decode($1,'hex') ORDER BY outputs.id") or die();
	pg_prepare($db,"inputcounter","SELECT DISTINCT inputs.value AS value,inputs.id,inputs.type AS type,keys.address AS address FROM inputs LEFT JOIN keys ON inputs.hash160=keys.hash160 WHERE inputs.tx=decode($1,'hex') ORDER BY inputs.id;") or die();
	$echothis="";
	$totalcredits=0;
	$totalcredit="0";
	$totaldebits=0;
	$totaldebit="0";
	while($txcounter<=$txlimit)
	{
		$onetx=pg_fetch_assoc($mytxs,$txcounter);
		$txcounter++;
		$echothis.= "<tr>\n";
		
		//get value, type, and whether Credit OR Debit (echoed a little later)
		$value=removeTrailingZeroes($onetx["value"]);
		$type=$onetx["txtype"];
		if($onetx["type"]=="credit")
		{
			$cord="Received";
			$totalcredits++;
			$totalcredit=bcadd($totalcredit,$value,8);
		}
		else
		{
			$cord="Sent";
			$totaldebits++;
			$totaldebit=bcadd($totaldebit,$value,8);
		}
		
		//get tx info
		$tx=$onetx["tx"];
		$index=$onetx["id"];
		if($cord=="Received")
		{
			$txtrunc="<a href=\"/testnet/tx/$tx#o$index\">".substr($tx,0,10)."...</a>";
		}
		else
		{
			$txtrunc="<a href=\"/testnet/tx/$tx#i$index\">".substr($tx,0,10)."...</a>";
		}
		$echothis.= "<td>$txtrunc</td>\n";
		
		//get block info
		$block=$onetx["block"];
		$txtime=$onetx["time"];
		$blocknum=$onetx["blocknum"];
		$blockstring="<a href=\"/testnet/block/$block\">Block $blocknum</a> ($txtime)";
		$echothis.= "<td>$blockstring</td>\n";
		
		$echothis.= "<td>$value</td>\n";
		
		/////////transaction loop. Echoed just a bit later
		//loop through counter ins if out.
		$superechothis="";
		$superechothis.= "<td><ul class=\"infoList\">\n";
		if($cord=="Received")
		{
			$counter=pg_execute($db,"inputcounter",array($tx));
			$thisio=pg_fetch_assoc($counter);
			while($thisio)
			{
				$address=$thisio["address"];
				$addressstring="<a href=\"/testnet/address/$address\">$address</a>";
				if(is_null($address)||$address===false||$address=="Unknown")
				{
					$addressstring="Unknown";
					if($thisio["type"]=="Generation")
					{
						$addressstring="Generation";
					}
				}
				if($address==$currentaddress)
				{
				$addressstring="$address";
				}
				$superechothis.= "<li>$addressstring</li>\n";
				$thisio=pg_fetch_assoc($counter);
			}
		}
		else //loop through counter outs if in
		{
			$type="";
			$counter=pg_execute($db,"outputcounter",array($tx));
			$thisio=pg_fetch_assoc($counter);
			while($thisio)
			{
				//type determination
				$statedtype=$thisio["type"];
				if($type=="")
				{
					$type=$statedtype;
				}
				else
				{
					if($type!=$statedtype)
					{
						$type="Strange";
					}
				}
				$address=$thisio["address"];
				$addressstring="<a href=\"/testnet/address/$address\">$address</a>";
				if(is_null($address)||$address===false||$address=="Unknown")
				{
					$addressstring="Unknown";
				}
				if($address==$currentaddress)
				{
				$addressstring="$address";
				}
				$superechothis.= "<li>$addressstring</li>\n";
				$thisio=pg_fetch_assoc($counter);
			}
		}
		$superechothis.= "</ul></td>\n";
		/////////end tx loop
		
		//resume addr info
		$echothis.= "<td>$cord: $type</td>\n";
		//reintegrate ledger
		$echothis.=$superechothis;
		
		//calculate running balance
		if($cord=="Sent")
		{
			$balance=removeTrailingZeroes(bcsub($balance,$value,8));
		}
		else
		{
			$balance=removeTrailingZeroes(bcadd($balance,$value,8));
		}
		$echothis.= "<td>$balance</td>\n";
		$echothis.= "</tr>\n";
	}
	
	//addr info resumes
	$totalcredit=thousands(removeTrailingZeroes($totalcredit));
	$totaldebit=thousands(removeTrailingZeroes($totaldebit));
	echo "<li>Received transactions: $totalcredits</li>";
	echo "<li>Received BTC: $totalcredit</li>";
	echo "<li>Sent transactions: $totaldebits</li>";
	echo "<li>Sent BTC: $totaldebit</li>";
	echo "<li>Hash160".help("The hash160 is a hash of the public key. Bitcoin uses these hashes internally - transactions don't contain Bitcoin addresses directly. Bitcoin addresses contain a base58-encoded hash160, along with a version and a check code.").": $hash160</li>\n";
	echo "<li>Public key".help("It's impossible to determine the public key from a Bitcoin address, but if the public key was ever used on the network, it is listed here.").": <div class=\"hugeData\">$pubkey</div></li>\n";
	echo "</ul>\n";
	if($currentaddress=="1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28")
	{
		echo "<p><i>Thank you!</i></p>";
	}
	
	//echo ledger
	echo "<h3>Ledger".help("A list of all transactions involving this address, with the oldest listed first.")."</h3>\n";
	echo '<p>Note: While the last "balance" is the accurate number of bitcoins available to this address, it is likely not the balance available to this person. Every time a transaction is sent, some bitcoins are usually sent back to yourself <i>at a new address</i> (not included in the Bitcoin UI), which makes the balance of a single address misleading. See <a href="http://www.bitcoin.org/wiki/doku.php?id=transactions">the wiki</a> for more info on transactions.</p>'."\n";
	echo "<table class=\"txtable\">\n";
	echo "<tr><th>Transaction".help("Truncated transaction hash")."</th><th>Block".help("Block this transaction appeared in")."</th><th>Amount".help("Number of BTC sent or received")."</th><th>Type".help("The type of the output. Bitcoin only sends a few different types of transactions. 'Address' sends to an Bitcoin address. 'Pubkey' sends directly to a public key, and is used for IP transactions and generations. 'Strange' is an unusual transaction not created by the official Bitcoin client.")."</th><th>From/To".help("The addresses this was received from or sent to. When sending, Bitcoin usually sends some bitcoins back to a brand new address that you own.")."</th><th>Balance".help("Balance as of this transaction. The last balance is the current balance of this address.")."</th></tr>\n";
	echo $echothis;
	echo "</table>\n";
}

end:
?>
<div id="footer"><hr><a href="/">Bitcoin Block Explorer</a> (<span style="color:red">TESTnet</span>) - Donate: <a href="/address/1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28">1Cvvr8AsCfbbVQ2xoWiFD1Gb2VRbGsEf28</a></div>
</body>
</html>
