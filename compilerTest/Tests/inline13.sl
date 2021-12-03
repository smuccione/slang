class TreeNode
{
	local	left
	local	right
	local	item

	method itemCheck()
	{
		if ( left ) 
		{
			return left.itemCheck() + item + right.itemCheck()
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

	for ( local depth = minDepth; depth <= maxDepth; depth += 2 )
	{
		printf ( $"depth: {depth}   check: {new treeNode ( 1, depth ).itemCheck()}\r\n" )
	}
}
