local numbers = { 1, 2, 3 }
local letters = { "a", "b", "c" }
	
local y =	from number in numbers
			from letter in letters
			select { number : number , letter : letter };

foreach ( o in y )
{
	println ( o.number, " : ", o.letter )
}

