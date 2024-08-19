local o = from new csvReader ( 'c:\agisent_data\dataConversion\il\woodford\yn.csv', true );

o.skip ( 50000 )

local o2 = from x in o where x.ynname == "CHRISTOPHER BEARD" select x;

foreach ( line in o2 )
{
	println ( "   ", line.ynkey, ":", line.ynname );
}
