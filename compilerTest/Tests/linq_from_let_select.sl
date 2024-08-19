local numbers = { 1, 2, 3, 4 }
	
local y = from number in numbers
	let n2 = number + 10
	select { number : number , n2 : n2 };

foreach ( o in y )
{
	println ( o.number, " : ",  o.n2 )
}

