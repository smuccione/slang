class _getCount
{
	local	start
	local	stop
	local	state
	local	_current
	local	count_1

	method new ()
	{
		state = 0;
	}

	method moveNext()
	{
		switch ( state )
		{
			case 0:
				goto label_0;
			case 1:
				goto label_1;
			default:
				return false
		}

		label label_0;

		count_1 = start
		for ( count_1 = start; count_1 < stop; count_1++ )
		{
			state = 1;
			_current = count_1
			return true
			label label_1;
		}
		state = -1;
		return false;
	}

	method getEnumerator()
	{
		return self
	}

	access current()
	{
		return _current
	}
	
}

function getCount ( start, stop )
{
	local o = new _getCount();
	o.start = start;
	o.stop = stop;
	return o;
}

foreach ( x in getCount ( 10, 20 ) )
{
	println ( x )
}
