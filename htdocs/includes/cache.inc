<?php

require_once('config.inc');

define('CACHE_ETAG_HASH', '__hash__');
define('CACHE_NOEXPIRE', -1);

class CacheError extends Exception {
}

function _mkdir($path) {
	if(file_exists($path)) 
        return true;

    if(!mkdir($path, 0775, true))
        return false;

    return true;
}

if(!_mkdir(CACHE_DIR))
    throw new CacheError("can't create CACHE_DIR=" . CACHE_DIR);

class Cache {

    static $dir = CACHE_DIR;
    static $dir_levels = CACHE_DIR_LEVELS;

    static $enabled = CACHE_ENABLED;
    static $app_magic = CACHE_APP_MAGIC;

    // set etag for this URI and expires headers. 
    // If browser has seen our etag, return a 304 Not modified
    // if no etag_token is provided, will default to app_magic

    static function handle_client_side_etag($etag_token = null, $ttl = null)
    {
        if(!self::$enabled || headers_sent()) 
            return true;

        // If etag_token is true, just use the default etag token
        if($etag_token === true)
            $etag_token = null;

        $etag = sprintf('W/"%s"', 
                        ($etag_token !== null) ? ("{$etag_token}-" . (string)self::$app_magic) :
                                                 (string)self::$app_magic);
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

        if($etag === true) 
            $etag = "true";

        $header = implode(';', array($ttl == CACHE_NOEXPIRE ? 0 : time() + $ttl,
                                     strlen($data),
                                     crc32($data),
                                     $etag));

        $fpath = self::_cached_path($key);
        _mkdir(dirname($fpath));

        if(!$fh = fopen($fpath, "c"))
            throw new CacheError("can't write to $fpath");

        if(!flock($fh, LOCK_EX))
            return;

        ftruncate($fh, 0);
        fwrite($fh, "$header\n\n$data");

        fclose($fh);
    }

    static function _cached_path($key) {
        $key = md5((string)self::$app_magic . $key);

        $path = "";
        for($i = 0; $i < self::$dir_levels; $i++)
            $path .= substr($key, $i*2, 2) . "/";

        $path .= substr($key, $i*2);

        return self::$dir . "/" . $path;
    }
    static function get($key) {

        if(!self::$enabled)
            return false;

        $fpath = self::_cached_path($key);
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
        $etag = ($header[3] === "true" ? true : 
                                         $header[3]);
        
        if(empty($length) || empty($chk))
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
        if($time && $time < time()) {
            fclose($fh);
            $locked_delete($fpath);
            return false;
        }
        
        if($etag)
            self::handle_client_side_etag($etag);
        
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

