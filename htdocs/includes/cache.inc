<?php

require_once('config.inc');

class CacheError extends Exception {
}

function _mkdir($path) {
	if(file_exists($path)) 
        return true;

    if(!mkdir($path))
        return false;

    chmod($path, 0775);

    return true;
}

if(!_mkdir(CACHE_TMPDIR))
    throw new CacheError("can't create CACHE_TMPDIR=" . CACHE_TMPDIR);

class Cache {

    static $tmpdir = CACHE_TMPDIR;
    static $enabled = CACHE_ENABLED;
    static $etag_magic = CACHE_ETAG_MAGIC;

    // set etag for this URI and expires headers. 
    // If browser has seen our etag, return a 304 Not modified

    static function handle_client_side_etag($etag = false, $ttl = false)
    {
        if(!self::$enabled || headers_sent()) 
            return true;

        $etag = sprintf('W/"%s"', $etag ? ("{$etag}-" . (string)self::$etag_magic) : (string)self::$etag_magic);
        header("ETag: $etag");

        if($ttl !== false)
            header('Expires: '.gmdate('D, d M Y H:i:s \G\M\T', time() + $ttl));
        else
            header('Expires: '.gmdate('D, d M Y H:i:s \G\M\T', time() -1500));
        
        $k = "HTTP_IF_NONE_MATCH";
        $v = isset($_SERVER[$k]) ? $_SERVER[$k] : null;
        if(!$v) 
            return false;

        $tags = preg_split("/, /", stripslashes($v));
        if(in_array($etag, $tags)) {
            header($_SERVER["SERVER_PROTOCOL"]." 304 Not Modified");
            die();
        }
    }

    static function put($key, $data, $ttl, $etag = null) {

        $header = implode(';', array(time() + $ttl,
                                     strlen($data),
                                     crc32($data),
                                     $etag));

        $fpath = self::$tmpdir . "/$key";
        if(!$fh = fopen($fpath, "c"))
            throw new CacheError("can't write to $fpath");

        if(!flock($fh, LOCK_EX))
            return;

        ftruncate($fh, 0);
        fwrite($fh, "$header\n\n$data");

        fclose($fh);
    }

    static function get($key) {

        if(!self::$enabled)
            return false;

        $fpath = self::$tmpdir . "/$key";
        if(!file_exists($fpath))
            return false;
        
        if(!$fh = fopen($fpath, "r"))
            return false;

        if(!flock($fh, LOCK_SH))
            return false;

        $header = explode(";", trim(fgets($fh)));

        $time = $header[0];
        $length = $header[1];
        $chk = $header[2];
        $etag = $header[3];
        
        if(empty($time) || empty($length) || empty($chk))
            return false;
        
        $locked_delete = function($fpath) {
            if(!$fh = fopen($fpath, "c"))
                return false;

            flock($fh, LOCK_EX);
            unlink($fpath);
            fclose($fh);

            return true;
        };

        // delete expired item in cache
        if($time < time()) {
            fclose($fh);
            $locked_delete($fpath);
            return false;
        }
        
        if($etag) {
            if($etag != "1") // (string)true == "1"
                self::handle_client_side_etag($etag);
            else
                self::handle_client_side_etag();
        }
        
        fgets($fh); // advance pointer
        $data = fread($fh, $length);
        fclose($fh);

        if(!empty($data) && strlen($data) == $length && crc32($data) == $chk)
            return $data;

        error_log("Bad cache file: $key, length $length");
        $locked_delete($fpath);
        return false;
    }
}

