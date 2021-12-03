local loop
local x = 1

loop = 0

while ( 1 )
{
	if ( loop > 10000000 )
	{
		break				// peephole optimize this into a single jmpcpop <x> rather than a jmpncpop jmp <x> pair
	}
	loop = loop + 1
	x = x + 1
}

println ( loop )
