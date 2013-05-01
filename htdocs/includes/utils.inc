<?php

function get($arr, $key, $default = null) {
    return isset($arr[$key]) ? $arr[$key] : $default;
}
