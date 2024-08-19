class TreeNode
{
	local	left
	local	right
	local	item

	method itemCheck()
	{
		if ( left ) 
		{
			return left.itemCheck() + item - right.itemCheck()
		} else
		{
			return item;
		}
	}

	method new ( item, depth ) : item ( item )
	{
		if ( depth )
		{
			left = new treeNode ( item * 2 - 1, --depth );
			right = new treeNode ( item * 2, depth );
		}
	}
}

function main()
{
	local minDepth = 4
	local maxDepth = 16
	local stretchDepth = maxDepth + 1

	printf ( $"stretch tree of depth depth {stretchDepth} check: {new treeNode ( 0, stretchDepth ).itemCheck()}\r\n" );

	local longLivedTree = new treeNode ( 0, maxDepth );
	for ( local depth = minDepth; depth <= maxDepth; depth += 2 )
	{
		local iterations = 1 << (maxDepth - depth + minDepth)
		local check = 0;
		for ( local i=1; i <= iterations; i++ )
		{
			check += new treeNode (  i, depth ).itemCheck() 
			check += new treeNode ( -i, depth ).itemCheck()
		}
		printf ( $"{iterations * 2}\t trees of depth {depth}\t check: {check}\r\n" );
	}
	printf ( $"long lived tree of depth {maxDepth} check: {longLivedTree.itemCheck()}\r\n" );
}
