local	z
local   x
local	y

x = 0
y = 1

z = x || (y++)
println ( y )
println ( z )


x = 1
z = x || (y++)

println ( y )
println ( z )
