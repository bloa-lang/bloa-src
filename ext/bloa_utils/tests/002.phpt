--TEST--
test1() Basic test
--EXTENSIONS--
bloa_utils
--FILE--
<?php
$ret = test1();

var_dump($ret);
?>
--EXPECT--
The extension bloa_utils is loaded and working!
NULL
