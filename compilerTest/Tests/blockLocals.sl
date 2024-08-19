function test()
{
	local arr = {}
	local loop
	for ( loop = 1; loop <= 10; loop++ )
	{
		local x = loop
		arr[loop] = function()
					{
						println ( "test: ", x )
					}
	}

	return arr
}

function test2()
{
	local arr = {}
	local loop
	local x

	for ( loop = 1; loop <= 10; loop++ )
	{
		x = loop
		arr[loop] = function()
					{
						println ( "test2: ", x )
					}
	}

	return arr
}

local a = test()
local loop

for ( loop = 1; loop <= len ( a ); loop++ )
{
	a[loop]()
}

println ( "------------------------------------" )

local b = test2()

for ( loop = 1; loop <= len ( b ); loop++ )
{
	b[loop]()
}
