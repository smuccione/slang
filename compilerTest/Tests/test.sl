	foreach (loop in 1..10 )
		switch (loop % 2)
		{
			case 1:
				printf ( "%i  odd\r\n", loop );
				break;
			case 0:
				printf ( "%i  even\r\n", loop )
				break;
		}