local x = { 1, 1.0, "hello" }

println ( x[1], " ", x[2], " ", x[3] )

local p = pack ( x )
local u = unpack ( p )

println ( u[1], " ", u[2], " ", u[3] )
