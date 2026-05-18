--TEST--
parallel_map helper uses worker threads
--FILE--
<?php
echo implode(',', parallel_map(fn($x) => $x * 2, [1,2,3,4], 2));
?>
--EXPECTF--
2,4,6,8
