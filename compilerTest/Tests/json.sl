local x = '[{"a":"here", "b":"there"},{"a":"here2", "b":"there2"}]'

local y = &x
println()
println()
println()
println()
println()
println ( y[1].a, " ", y[1].b );
println()
println()
println()
println()
local z =  json ( x )
println()
println()
println()
println()
println()
println ( z[1].a, " ", z[1].b );

