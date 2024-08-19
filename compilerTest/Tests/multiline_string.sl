local x = R"test(this is 
a
multiline
{string}
test )test"

println ( x )

local x2 = "this is a test"

println ( x2 )

local first = "steve"
local last = "muccione"

println ( $"{first} {last}" )

println ( $R"({first}
{last} )" )
