local hi
local lo

hi = 4
lo = 2

printf ( "hi: %d   low: %d\r\n", hi, lo );

{hi, lo} = {lo, hi};

printf ( "hi: %d   low: %d\r\n", hi, lo );
