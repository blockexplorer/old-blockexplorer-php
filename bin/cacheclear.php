#!/usr/bin/php

<?php
$shm="/dev/shm/bbe/";
$ls=scandir($shm);
foreach($ls as $file)
{
	$filename=$shm.$file;
	echo $filename . "\n";
	if(is_dir($filename))
		continue;
	
	//copied from cacheget
	$file=@fopen($filename,"r");
	if($file===false)
		continue;
	if(!flock($file,LOCK_SH))
		continue;
	$header=explode(";",fgets($file));
	$time=$header[0];
	$length=$header[1];
	$chk=$header[2];
	if(empty($time)||empty($length)||empty($chk))
		continue;
	
	if($time<time())
	{
		flock($file,LOCK_UN);
		fclose($file);
		$file=fopen($filename,"c");
		if($file===false)
			continue;
		flock($file,LOCK_EX);
		unlink($filename);
		flock($file,LOCK_UN);
		fclose($file);
		continue;
	}
	
	fgets($file); //advance pointer
	$data=fread($file,$length);
	if(!empty($data)&&strlen($data)==$length&&crc32($data)==$chk)
	{
		continue;
	}
	else
	{
		flock($file,LOCK_UN);
		fclose($file);
		$file=fopen($filename,"c");
		flock($file,LOCK_EX);
		unlink($filename);
		flock($file,LOCK_UN);
		fclose($file);
		continue;
	}
}

$shm="/tmp/bbe/";
$ls=scandir($shm);
foreach($ls as $file)
{
	$filename=$shm.$file;
	echo $filename . "\n";
	if(is_dir($filename))
		continue;
	
	//copied from cacheget
	$file=@fopen($filename,"r");
	if($file===false)
		continue;
	if(!flock($file,LOCK_SH))
		continue;
	$header=explode(";",fgets($file));
	$time=$header[0];
	$length=$header[1];
	$chk=$header[2];
	if(empty($time)||empty($length)||empty($chk))
		continue;
	
	if($time<time())
	{
		flock($file,LOCK_UN);
		fclose($file);
		$file=fopen($filename,"c");
		if($file===false)
			continue;
		flock($file,LOCK_EX);
		unlink($filename);
		flock($file,LOCK_UN);
		fclose($file);
		continue;
	}
	
	fgets($file); //advance pointer
	$data=fread($file,$length);
	if(!empty($data)&&strlen($data)==$length&&crc32($data)==$chk)
	{
		continue;
	}
	else
	{
		flock($file,LOCK_UN);
		fclose($file);
		$file=fopen($filename,"c");
		flock($file,LOCK_EX);
		unlink($filename);
		flock($file,LOCK_UN);
		fclose($file);
		continue;
	}
}

//exit(0);

