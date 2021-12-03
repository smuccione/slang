local x = R"test(this is 
a
multiline
{string}
test )test"

println ( x )

local x = "this is a test"

println ( x )

local first = "steve"
local last = "muccione"

println ( $"{first} {last}" )

println ( $R"({first}
{last} )" )

println ( "----------------------------------------------------------------" )
#define fName2(x) F##x
#define fName(x) fName2(x)
println ( fName ( __FILE__ ) )