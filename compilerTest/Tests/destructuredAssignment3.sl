local hi
local lo
local med

hi = 4
med = 3
lo = 2

printf ( "hi: %d   low: %d\r\n", hi, lo );

{hi, med, lo} = {lo, hi};

printf ( "hi: %d   med: %d  low: %d\r\n", hi, med, lo );
