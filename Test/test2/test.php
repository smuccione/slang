<?php
	$y = '          ';
	for ( $loop = 0; $loop < 100000; $loop++ )
	{
		for ( $loop2 = 0; $loop2 < 100; $loop2++ )
		{
			$y .= '.';
		}
		$y .= "<br>";
	}
	echo $y;
?>