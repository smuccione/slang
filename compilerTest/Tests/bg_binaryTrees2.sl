function itemCheck ( arr )
{
	if ( arr[1] )
	{
		return itemCheck ( arr[1] ) + arr[3] - itemCheck ( arr[2] )
	} else
	{
		return arr[3]
	}
}

function bottomUpTree ( item, depth )
{
	return depth > 0 ? { bottomUpTree ( 2 * item - 1, depth - 1), bottomUpTree ( 2 * item, depth - 1), item } :  { 0, 0, item }
}

function main()
{
	local minDepth = 4
	local maxDepth = 16
	local stretchDepth = maxDepth + 1

	printf ( "stretch tree of depth %i  check:  %i\r\n", stretchDepth, bottomUpTree ( 0, stretchDepth ).itemCheck() );

	local longLivedTree = bottomUpTree ( 0, maxDepth );
	for ( local depth = minDepth; depth <= maxDepth; depth += 2 )
	{
		local iterations = 1 << (maxDepth - depth + minDepth)
		local check = 0;
		for ( local i=1; i <= iterations; i++ )
		{
			check += bottomUpTree (  i, depth ).itemCheck() 
			check += bottomUpTree ( -i, depth ).itemCheck()
		}
		printf ( "%i\t trees of depth %i\t check: %i\r\n", iterations * 2, depth, check );
	}
	local check = longLivedTree.itemCheck();
	printf ( "long lived tree of depth %i\t check: %i\r\n", maxDepth, check );
}
