<?php

function cacheput($key, $data, $secs)
{
    $shm = "/dev/shm/bbe/";
    if(!file_exists($shm)) {
        if(!mkdir($shm))
            return false;
        chmod($shm, 0775);
    }
    
    $length = strlen($data);
    $chk = crc32($data);
    $expires = time()+$secs;
    $header = "$expires;$length;$chk";
    $data = $header."\n\n".$data;
    
    $file = fopen($shm.$key, "c");
    if($file === false)
        return false;
    if(!flock($file, LOCK_EX|LOCK_NB))
        return false;
    ftruncate($file, 0);
    fwrite($file, $data);
    flock($file, LOCK_UN);
    fclose($file);
}

function cacheget($key)
{
    $shm = "/dev/shm/bbe/";
    if(!file_exists($shm)) {
        if(!mkdir($shm))
            return false;
        chmod($shm, 0775);
    }
    
    if(!file_exists($shm.$key))
        return false;
    
    $file = @fopen($shm.$key, "r");
    if($file === false)
        return false;

    if(!flock($file, LOCK_SH))
        return false;

    $header = explode(";", fgets($file));
    $time = $header[0];
    $length = $header[1];
    $chk = $header[2];
    if(empty($time) || empty($length) || empty($chk))
        return false;
    
    if($time<time()) {
        flock($file, LOCK_UN);
        fclose($file);
        return false; // we'll put new stuff in it later
    }
    
    fgets($file); //advance pointer
    $data = fread($file, $length);
    if(!empty($data) && strlen($data) == $length && crc32($data) == $chk) {
        return $data;
    } else {
        error_log("Bad cache file: $key, length $length");
        flock($file, LOCK_UN);
        fclose($file);
        return false;
    }
}

?>
