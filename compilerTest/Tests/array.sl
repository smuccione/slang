local a
local x2 = 1
local b = { "am", "working" }

//println ( b[1], ' ', b[2], " - should be am working" )
//println ( b[1], ' ', b[2], " - should be am working" )

b[1] = "hello"
b[2] = "there"

println ( b[1], ' ', b[2] )

b = { {"am"}, "working" }

b[1][1] = "hello"
b[2] = "there"

println ( b[1][1], ' ', b[2] )


a = {}
a[1][1] = "hello"
a[2] = "there"
println ( a[1][1], ' ', a[2] )
println ( a[1,1] )


a=NULL
a[1][1] = 1
a[1][1]++
++a[1][1]
local x = a[1][1]++
x = ++a[1][1]
println ( a[1][1], " ", x )

a = {}

a[1] = "hello"
a[2] = "there"

println ( a[1], ' ', a[2] )
