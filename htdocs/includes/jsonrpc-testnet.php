<?php
function rpcQuery ($method,$params=array())
{
	//returns an associative array
	//$reult["r"] contains the result in *decoded* JSON
	//$result["e"] contains the error, or NULL if there is no error. This could be Bitcoin errors or rpcQuery errors.

	//I don't expect all possible errors to be caught. After running this, you should check that it's
	//returning reasonable data.
	
	$user="bitcoin";
	$password="7AvathEBracheCra";
	$id=6297; //pick any random number
	$target="127.0.0.1";
	$port=8331;

	//construct query
	$query=(object)array("method"=>$method,"params"=>$params,"id"=>$id);
	$query=json_encode($query);
	$auth=base64_encode($user.":".$password);
	$query=$query."\r\n";
	$length=strlen($query);
	$socket=socket_create(AF_INET,SOCK_STREAM,SOL_TCP);
	//socket_set_option($socket,SOL_SOCKET, SO_RCVTIMEO, array("sec"=>1,"usec"=>500));
	//socket_set_option($socket,SOL_SOCKET, SO_SNDTIMEO, array("sec"=>1,"usec"=>500));
	if(socket_connect($socket,$target,$port)===false)
	{
		$errorcode = socket_last_error();
		error_log("JSON: Socket error $errorcode: ".__LINE__);
		$error = socket_strerror($errorcode);
		goto returnResult;
	}
	$in="POST / HTTP/1.1\r\n";
	$in.="Connection: close\r\n";
	$in.="Content-Length: $length\r\n";
	$in.="Host: \r\n";
	$in.="Content-type: text/plain\r\n";
	$in.="Authorization: Basic $auth\r\n";
	$in.="\r\n";
	$in.=$query;
	$offset = 0;
	$len=strlen($in);
	//write loop for unreliable network
	while ($offset < $len)
	{
		$sent = socket_write($socket, substr($in, $offset), $len-$offset);
		if ($sent === false) {
			break;
		}
		$offset += $sent;
	}
	//did all of our data get out?
	if ($offset < $len) 
	{
		$errorcode = socket_last_error();
		error_log("JSON: Socket error $errorcode: ".__LINE__);
		$error = socket_strerror($errorcode);
		goto returnResult;
	}
	//read loop for unreliable network
	//Not totally sure this is always safe (I suppose socket_read might return an empty string if not
	//at the end), though I've run it hundreds of thousands of times without error. returnResult
	//will catch it if it ever fails, and the client should retry at least once.
	$reply = "";
	do
	{
		$recv = "";
		$recv = socket_read($socket, '1400');
		if($recv===false)
		{
			$errorcode = socket_last_error();
			$error = socket_strerror($errorcode);
			goto returnResult;
		}
		if($recv != "")
		{
			$reply .= $recv;
		}
	}
	while($recv != "");
	
	$result=strpos($reply,"\r\n\r\n");
	if($result===false)
	{
		$error="Could not parse result.";
	}
	$result=trim(substr($reply,$result+4));
	
	//construct final array
	returnResult:
	{
		$return=array("r"=>NULL,"e"=>NULL);
		if(isset($error))
		{
			$return["e"]=$error;
			return $return;
		}
		$result=json_decode($result,false,512);
		if($result==NULL||!is_object($result))
		{
			$return["e"]="Decode failed.";
			return $return;
		}
		if($result->id!=$id)
		{
			$return["e"]="Wrong ID.";
			return $return;
		}
		$return["r"]=$result->result;
		$return["e"]=$result->error;
		return $return;
	}
}

function indent($json) {

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
            for ($j=0; $j<$pos; $j++) {
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
}

?>