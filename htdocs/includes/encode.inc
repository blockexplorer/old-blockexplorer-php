<?php

class Address {

    static public $version = "00";

    static public function hash160ToAddress($hash160, $version = NULL)
    {
        if($version === NULL)
            $version = self::$version;

        $hash160 = $version . $hash160;
        $check = pack("H*" , $hash160);
        $check = hash("sha256", hash("sha256", $check, true));
        $check = substr($check, 0, 8);
        $hash160 = strtoupper($hash160.$check);
        return encodeBase58($hash160);
    }

    static public function check($addr)
    {
        if(!preg_match("/^[1-9A-HJ-NP-Za-km-z]{25,36}$/", $addr))
            return false;

        $addr = decodeBase58($addr);
        if(strlen($addr) != 50)
            return false;

        $version = substr($addr, 0, 2);
        if(hexdec($version)>hexdec(self::$version))
            return false;

        $check = substr($addr, 0, strlen($addr)-8);
        $check = pack("H*" , $check);
        $check = strtoupper(hash("sha256", hash("sha256", $check, true)));
        $check = substr($check, 0, 8);
        return $check == substr($addr, strlen($addr)-8);
    }

    static public function toHash160($addr)
    {
        $addr = decodeBase58($addr);
        $addr = substr($addr, 2, strlen($addr)-10);
        return $addr;
    }
}

function decodeHex($hex)
{
	$hex = strtoupper($hex);
	$chars = "0123456789ABCDEF";
	$return = "0";
	for($i = 0;$i<strlen($hex);$i++)
	{
		$current = (string)strpos($chars, $hex[$i]);
		$return = (string)bcmul($return, "16", 0);
		$return = (string)bcadd($return, $current, 0);
	}
	return $return;
}

function encodeHex($dec)
{
	$chars = "0123456789ABCDEF";
	$return = "";
	while (bccomp($dec, 0) == 1)
	{
		$dv = (string)bcdiv($dec, "16", 0);
		$rem = (integer)bcmod($dec, "16");
		$dec = $dv;
		$return = $return.$chars[$rem];
	}
	return strrev($return);
}

function decodeBase58($base58)
{
	$origbase58 = $base58;
	
	$chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
	$return = "0";
	for($i = 0;$i<strlen($base58);$i++)
	{
		$current = (string)strpos($chars, $base58[$i]);
		$return = (string)bcmul($return, "58", 0);
		$return = (string)bcadd($return, $current, 0);
	}
	
	$return = encodeHex($return);
	
	// leading zeros
	for($i = 0;$i<strlen($origbase58) && $origbase58[$i] == "1";$i++)
	{
		$return = "00".$return;
	}
	
	if(strlen($return)%2 != 0)
	{
		$return = "0".$return;
	}
	
	return $return;
}

function encodeBase58($hex)
{
	if(strlen($hex)%2 != 0)
        $hex = "0".$hex;

	$orighex = $hex;
	
	$chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
	$hex = decodeHex($hex);
	$return = "";
	while (bccomp($hex, 0) == 1)
	{
		$dv = (string)bcdiv($hex, "58", 0);
		$rem = (integer)bcmod($hex, "58");
		$hex = $dv;
		$return = $return.$chars[$rem];
	}
	$return = strrev($return);
	
	// leading zeros
	for($i = 0;$i<strlen($orighex) && substr($orighex, $i, 2) == "00";$i += 2)
	{
		$return = "1".$return;
	}
	
	return $return;
}

function hash160($data)
{
	$data = pack("H*" , $data);
	return strtoupper(hash("ripemd160", hash("sha256", $data, true)));
}

function remove0x($string)
{
	if(substr($string, 0, 2) == "0x" || substr($string, 0, 2) == "0X")
		$string = substr($string, 2);

	return $string;
}

function decodeCompact($c)
{
	$nbytes = ($c >> 24) & 0xFF;
	return bcmul($c & 0xFFFFFF, bcpow(2, 8 * ($nbytes - 3)));
}

function encodeCompact($in)
{
    $script = <<<"EOD"
python << EOF

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

print GetCompact($in); 
EOF

EOD;
    return exec($script);
}

