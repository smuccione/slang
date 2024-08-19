function xml2obj ( xml )
{
	if ( xml.nodeType == 1 )
	{
		if ( xml.firstChild )
		{
			local textChild = 0, cDataChild = 0, hasElementChild = false;

			for ( local n = xml.firstChild; n; n = n.nextSibling )
			{
				switch ( n.nodeType )
				{
					case 1:
						hasElementChild = true;
						break;

					case 3:
						textChild++;
						break;
					case 4:
						cDataChild++
						break;
				}
			}
			if ( hasElementChild )
			{
				if ( textChild < 2 || cDataChild < 2 )
				{
					local o = new aArray
					if ( xml.attributes.length )
					{
						foreach ( it in xml.attributes.array )
						{
							o["@"+ it[1]] = it[2];
						}		
					}
					for ( local n = xml.firstChild; n; n = n.nextSibling )	
					{
						if (n.nodeType == 3)  // text node
						{
							o["#text"] = n.value;
						} else if (n.nodeType == 4)  // cdata node
						{
							o["#cdata"] = n.value;
						} else if (o.has ( n.nodeName ) ) 
						{  
							// multiple occurence of element ..
							if ( type ( o[n.nodeName] ) == "A" )
							{
								o[n.nodeName][o[n.nodeName].len()+1] = xml2obj ( n );
							} else
							{
								o[n.nodeName] =  [o[n.nodeName], xml2obj(n)];
							}
						} else
						{
							// first occurence of element..
							o[n.nodeName] = xml2obj(n);
						}
					}
					return o;
				} else
				{
					if (!xml.attributes.length)
					{
						return xml.value;

					} else
					{
						local o = new aArray
						if ( xml.attributes.length )
						{
							foreach ( it in xml.attributes.array )
							{
								o["@"+ it[1]] = it[2];
							}		
						}
						o["#text"] = xml.value;
						return o;
					}
				}
			} else if (textChild) 
			{ 
				// pure text
				if (!xml.attributes.length)
				{
					return xml.value;
				} else
				{
					local o = new aArray
					if ( xml.attributes.length )
					{
						foreach ( it in xml.attributes.array )
						{
							o["@"+ it[1]] = it[2];
						}		
					}
					o["#text"] = xml.value;
					return o;
				}
			} else if (cdataChild) 
			{
#if 0
	// cdata
	if (cdataChild > 1)
	{
		//                    o = xml.value;
	} else
	{
		for (var n = xml.firstChild; n; n = n.nextSibling)
		{
			//                       o["#cdata"] = n.value;~
		}
	}
#endif            
			}
		}
	}
	return null;
}


function tojson ( dom)
{
	local x =  xml2Obj ( dom.firstChild )
	local t =  objToJson ( x );
	return t;
}

local xml = "<e><a>a text</a><a>a text 2</a></e>"
local o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e><a>a text</a><b>b text</b></e>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e> <a>a text</a> <b>b text</b> </e>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e><a>a text</a><a>a text 2</a></e>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e> text <a>text</a> </e>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e/>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e>text</e>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e name=\"value\" />"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

xml = "<e name=\"value\">text</e>"
o = new xmlDocument ( xml );
println ( toJson ( o ) )

