local hi
local lo

hi = 4
lo = 2

println ( $"hi: { hi } low: { lo }" )

{ hi, lo  } = {lo, hi}

println ( $"hi: { hi } low: { lo}")