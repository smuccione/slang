function itemCheck ( arr )
{
	if ( arr[1] )
		return itemCheck ( arr[1] ) + arr[3] - itemCheck ( arr[2] )
	else
		return arr[3]
}

function bottomUpTree ( item, depth )
{
	return depth > 0 ? { bottomUpTree ( 2 * item - 1, depth - 1), bottomUpTree ( 2 * item, depth - 1), item } :  { 0, 0, item }
}

function main()
{
	local maxDepth
	local stretchDepth
	local check

	maxDepth = 3
	stretchDepth = maxDepth + 1

	check = itemCheck ( bottomUpTree ( 0, stretchDepth ) )

	printf ( "stretch tree of depth %i  check:  %i\r\n", stretchDepth, check );
}
