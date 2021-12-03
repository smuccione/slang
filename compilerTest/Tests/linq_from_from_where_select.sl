local numbers = { 1, 2, 3, 4 }
local letters = { "a", "b", "c" }
	
local y = from number in numbers
	let n2 = number + 1
	from letter in letters
	where number >= 2
	select { number : number , letter : letter, n2 : n2 };

foreach ( o in y )
{
	println ( o.number, " : ", o.letter, " : ", o.n2 )
}

