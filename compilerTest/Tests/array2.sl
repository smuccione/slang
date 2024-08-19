function aIntersect ( a, b )
{
	local aIndex = 1
	local bIndex = 1
	local c = {}
	
	a = asort ( a )
	b = asort ( b )
	
	while ( (aIndex <= len ( a )) && (bIndex <= len ( b )) )
	{
		if ( a[aIndex] == b[bIndex] )
		{
			c[len(c) + 1] =	a[aIndex]
			aIndex++
			bIndex++
		} else if ( a[aIndex] < b[bIndex] )
		{
			aIndex++
		} else
		{
			bIndex++
		}
	}
	return c
}


function aUnion ( a, b )
{
	local aIndex = 1
	local bIndex = 1
	local c = {}
	
	a = asort ( a )
	b = asort ( b )
	
	while ( (aIndex <= len ( a )) && (bIndex <= len ( b )) )
	{
		if ( a[aIndex] == b[bIndex] )
		{
			c[len(c) + 1] =	a[aIndex]
			aIndex++
			bIndex++
		} else if ( a[aIndex] < b[bIndex] )
		{
			c[len(c) + 1] = a[aIndex]
			aIndex++
		} else
		{
			c[len(c) + 1] = b[bIndex]
			bIndex++
		}
	}
	
	// remainder of a
	while ( aIndex <= len ( a ) )
	{
		c[len(c) + 1] = a[aIndex]
		aIndex++
	}
	
	// remainder of b
	while ( bIndex <= len ( b ) )
	{
		c[len(c) + 1] = b[bIndex]
		bIndex++
	}
	return c
}

local a = { 1, 3, 5, 6, 7, 9, 11, 13, 15 }
local b = { 2, 4, 6, 7, 8, 10, 11 }

local c = aUnion ( a, b )
local d = aIntersect ( a, b )

print ( "a=" )
foreach ( x in a )
{
	print ( x, ' ' )
}
println();

print ( "b=" )
foreach ( x in b )
{
	print ( x, ' ' )
}
println()

print ( "union =" )
foreach ( x in c )
{
	print ( x, ' ' )
}
println()

print ( "intersection =" )
foreach ( x in d )
{
	print ( x, ' ' )
}
println()
