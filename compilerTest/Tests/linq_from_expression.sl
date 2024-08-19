local a = { 1, 2, 5, 7, 8, 9, 23, 4, 6, 8, 2, 0 }
	
local y = (from x in a
			where x > 5 
			select x * 2).sum();

println ( "sum = ", y );



