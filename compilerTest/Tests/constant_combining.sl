CLASS objUser
{
////////////////////////////////////////////////
  PUBLIC:
	const STATE_ACTIVE 			= "A"

	const STATUS_EMPLOYEE		= objUser::STYPE_EMPLOYEE + objUser::STATE_ACTIVE

	const STYPE_EMPLOYEE		= "E"
	const STYPE_CONTRACTOR		= "C"
	const STYPE_CONSULTANT		= "N"
	const STYPE_GUEST 			= "G"
	const STYPE_VOLUNTEER		= "V"
}

local x
local y

x = 5

y = 1 + x + 5 + 6
println ( y )

y = 1 + x + 5 - 6
println ( y )

y = 1 + 5 - 6 + x
println ( y )

y = 1 * x + 5 * 6
println ( y )

y = 1 + 0 * x + 5 - 6
println ( y )


println ( objUser::STATUS_EMPLOYEE );


