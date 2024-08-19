local e = 0 ? "fail" : 
	0 ? "fail" :
	1 ? "correct" :
	0 ? "fail" : 
	"fail"
	
println ( e )

local arg2 = "0"
local arg = 'T'
local vehicle = ( ( arg == 'B' )	? 'wrong' :
            ( arg == 'A' )	? 'wrong' :
			arg2			? "correct " :
            ( arg == 'T' )	? 'wrong' :
            ( arg == 'C' )	? 'wrong' :
            ( arg == 'H' )	? 'wrong' :
            'feet' )
println (  vehicle )
