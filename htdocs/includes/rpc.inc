<?php

class RPCError extends Exception {
}

class RPC {
    static private $conf;

    static function init($conf) 
    {
        self::$conf = $conf;
    }

    static function query($method, $params = array(), $timeout = array("send" => 0, "receive" => 10))
    {
        /* returns an associative array

           $reult["r"] contains the result in *decoded* JSON
           $result["e"] contains the error, or NULL if there is no error. This could be Bitcoin errors or rpcQuery errors.

           I don't expect all possible errors to be caught. After running this, you should check that it's
           returning reasonable data.
           $time_start = microtime(true); */

        if(!self::$conf)
            throw new RPCError("rpc not initialized");

        $conf = self::$conf;
        
        $id = 8284; // pick any random number

        // construct query
        $query = (object)array("method" => $method, "params" => $params, "id" => $id);
        $query = json_encode($query);
        $auth = base64_encode($conf['user'].":".$conf['password']);
        $query = $query."\r\n";

        $length = strlen($query);

        $in = "POST / HTTP/1.1\r\n";
        $in .= "Connection: close\r\n";
        $in .= "Content-Length: $length\r\n";
        $in .= "Host: \r\n";
        $in .= "Content-type: text/plain\r\n";
        $in .= "Authorization: Basic $auth\r\n";
        $in .= "\r\n";
        $in .= $query;
        $offset = 0;
        $len = strlen($in);
        
        // create connection
        $socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
        
        // timeouts
        if($timeout["send"] > 0)
            socket_set_option($socket, SOL_SOCKET, SO_SNDTIMEO, 
                              array("sec" => $timeout["send"], "usec" => 0));

        if($timeout["receive"] > 0)
            socket_set_option($socket, SOL_SOCKET, SO_RCVTIMEO, 
                              array("sec" => $timeout["send"], "usec" => 0));
        

        if(socket_connect($socket, $conf['target'], $conf['port']) === false) {
            $errorcode = socket_last_error();
            error_log("JSON: Socket error $errorcode: ".__LINE__);
            $error = socket_strerror($errorcode);

            throw new RPCError($error);
        }
        
        // write loop for unreliable network
        while ($offset < $len)
        {
            $sent = socket_write($socket, substr($in, $offset), $len-$offset);
            if ($sent === false) {
                break;
            }
            $offset += $sent;
        }

        // did all of our data get out?
        if ($offset < $len) 
        {
            $errorcode = socket_last_error();
            error_log("JSON: Socket error $errorcode: ".__LINE__);

            throw new RPCError($errorcode == 11 ? "Socket write timed out" : 
                                                   socket_strerror($errorcode));
        }

        // read loop for unreliable network
        // Not totally sure this is always safe (I suppose socket_read might return an empty string if not
        // at the end), though I've run it hundreds of thousands of times without error. returnResult
        // will catch it if it ever fails, and the client should retry at least once.
        $reply = "";
        do {
            $recv = "";
            $recv = socket_read($socket, '1400');

            if($recv === false) {

                $errorcode = socket_last_error();
                error_log("JSON: Socket error $errorcode: ".__LINE__);
                
                throw new RPCError($errorcode == 11 ? "Socket read timed out" : 
                                                       socket_strerror($errorcode));
            }
            if($recv != "")
                $reply .= $recv;

        }
        while($recv != "");
        
        // socket no longer needed -- close
        socket_shutdown($socket);
        socket_close($socket);
        
        $result = strpos($reply, "\r\n\r\n");
        if($result === false)
        {
            $error = "Could not parse result.";
        }
        $result = json_decode(trim(substr($reply, $result+4)), false, 512);

        if(!$result || !is_object($result))
            throw new RPCError("Decode failed");

        if($result->id != $id)
            throw new RPCError("Wrong ID.");

        if($result->error) 
            throw new RPCError($result->error);

        return $result->result;

    }
}
