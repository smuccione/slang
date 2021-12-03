/*
	document object model

	html parser


*/

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmNativeInterface.h"
#include "builtinDom.h"

struct xmlnSpace {
	stringi prefix;
	stringi uri;
};

static void xmlClassAssignString ( class vmInstance *instance, VAR_OBJ *obj, char const *member, char const *string, size_t len )
{
	*((*obj)[member]) = VAR_STR ( instance, string, len );
}

static void xmlClassAssignStringStatic ( class vmInstance *instance, VAR_OBJ *obj, char const *member, char const *string, size_t len )
{
	*((*obj)[member]) = VAR_STR ( string, len );
}

static void xmlClassAssignLong ( class vmInstance *instance, VAR_OBJ *obj, char const *member, int64_t num )
{
	*((*obj)[member]) = VAR ( num );
}

static void xmlClassAssignObject ( class vmInstance *instance, VAR_OBJ *obj, char const *member, VAR *obj2 )
{
	*((*obj)[member]) = *obj2;
}

static void xmlClassAssignReference ( class vmInstance *instance, VAR_OBJ *obj, char const *member, char const *member2 )
{
	*((*obj)[member]) = VAR_REF ( obj, (*obj)[member2] );
}

static void xmlClassAddStringToNodeMap ( class vmInstance *instance, VAR_OBJ *nodeMap, char const *name, size_t nameLen, char const *value, size_t valueLen )
{
	int64_t	 indicie[2]{};

	auto length = ((*nodeMap)["length"]);

	if ( TYPE ( length ) != slangType::eLONG )
	{
		length->type  = slangType::eLONG;
		length->dat.l = 0;
	}

	indicie[0] = length->dat.l;

	length->dat.l++;

	auto array = ((*nodeMap)["array"]);

	indicie[1] = 1;

	*_arrayGet ( instance, array, 2, indicie ) = VAR_STR ( instance, name, nameLen );
	indicie[1] = 2;
	*_arrayGet ( instance, array, 2, indicie ) = VAR_STR ( instance, value, valueLen );
}

static void xmlClassAddToNodeList ( class vmInstance *instance, VAR_OBJ *nodeList, VAR *obj )
{
	int64_t	 indicie[1]{};

	auto length = ((*nodeList)["length"]);

	if ( TYPE ( length ) != slangType::eLONG )
	{
		length->type  = slangType::eLONG;
		length->dat.l = 0;
	}

	indicie[0] = length->dat.l++;

	length->dat.l++;

	*_arrayGet ( instance, classIVarAccess ( nodeList, "ARRAY" ), 1, indicie ) = *obj;
}

static void xmlClassAddToParentsChildList ( class vmInstance *instance, VAR_OBJ *parent, VAR *obj )
{
	xmlClassAddToNodeList ( instance, static_cast<VAR_OBJ *>((*parent)["CHILDNODES"]), obj );
}


int xmlParseTag ( class vmInstance *instance, char *str, char *tagName, int lTagName, char *prefix, int lPrefix )
{
	int loop;
	int loop2;

	loop = 0;
	while ( str[loop] )
	{
		if ( str[loop] == ':' )
		{
			/* previous was a name space... we're now parsing the name proper */

			if ( loop + 1 > lPrefix)
			{
				throw errorNum::scINVALID_XML_DOCUMENT;
			}

			memcpy ( prefix, str, loop );
			prefix[loop] = 0;

			/* now parse off the tag name */

			loop++;	// skip over the ":"

			loop2 = loop;

			while ( str[loop2] )
			{
				if ( _isspacex ( &str[loop2] ) || (str[loop2] == '>') || (str[loop2] == '/') )
				{
					/* found the tag name */
					if ( loop2 + 1 - loop > lTagName)
					{
						throw errorNum::scINVALID_XML_DOCUMENT;
					}

					memcpy ( tagName, str + loop, loop2 - loop );
					tagName[loop2 - loop] = 0;

					return ( loop2 );
				}
				loop2++;
			}
			throw errorNum::scINVALID_XML_DOCUMENT;
		} else if ( _isspacex ( &str[loop] ) || (str[loop] == '>') || (str[loop] == '/') )
		{
			/* we just parsed the name... no name space */
			*prefix = 0;

			if ( loop + 1 > lTagName )
			{
				throw errorNum::scINVALID_XML_DOCUMENT;
			}

			memcpy ( tagName, str, loop );
			tagName[loop] = 0;

			return ( loop );
		}
		loop++;			
	}
	throw errorNum::scINVALID_XML_DOCUMENT;
}

static int xmlParsePage ( class vmInstance *instance, VAR_OBJ *owner, VAR_OBJ *parent, char ** pageData, bool optionClose, bool noSimpleText, std::vector<xmlnSpace> &nSpace  )
{
	size_t		 len;
	char		 tmpC;
	char		*tmpEnd;
	char		*tmpTag;
	char		*name;
	ptrdiff_t	 nameLen;
	char		*prefixName;
	ptrdiff_t	 prefixNameLen;
	int			 autoClose;
	int			 allWhiteSpace;
	char const  *value;
	ptrdiff_t	 valueLen;
	char		 tagName[256];
	char		 prefix[256];
	char		 tagName2[256];
	char		 prefix2[256];
	int64_t		 nnSpaceSave;
	bool		 firstSibling = true;
	VAR			 lastSibling{};

	nnSpaceSave = (int64_t)nSpace.size ( );;

	while ( **pageData )
	{
		if ( **pageData == '<' )
		{
			(*pageData)++;
			while ( **pageData && _isspacex ( *pageData ) )
			{
				(*pageData)++;
			}

			if ( **pageData == '/' )
			{
				/* end tag... let the caller worry about it */

				/* check to see if this is a valid end tag for the freaking idiots who do </br>*/

				/* skip over the first character... we DON'T want to test... this may be a bad tag so
				   we need to conver it to text and not try to reparse
				*/
				tmpTag = *pageData;

				/* skip over the '/' character */
				tmpTag++;

				return ( 1 );
			} else if ( !memcmp ( *pageData, "!--", 3 ) )
			{
				/* comment tag */

				*pageData += 3;

				tmpTag = *pageData;

				/* skip over the first character... we DON'T want to test... this may be a bad tag so
				   we need to conver it to text and not try to reparse
				*/
				tmpTag++;

				while ( *tmpTag && (memcmp ( tmpTag, "-->", 3 ) != 0) )
				{
					tmpTag++;
				}

				/* just create a generic element for this tag... don't know how to deal with it */
				auto newObj = *classNew ( instance, "COMMENT", 0 );
				
				xmlClassAssignObject			( instance, &newObj, "ATTRIBUTES", classNew ( instance, "NAMEDNODEMAP", 0 ) );
				xmlClassAssignLong				( instance, &newObj, "NODETYPE", 8 );
				xmlClassAssignStringStatic		( instance, &newObj, "NODENAME", "#comment", 8 );
				xmlClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				xmlClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				xmlClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
				xmlClassAssignReference			( instance, &newObj, "NODEVALUE", "DATA" );
				xmlClassAddToParentsChildList	( instance, parent, &newObj );

				if ( !firstSibling )
				{
					firstSibling = true;
					xmlClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					xmlClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					xmlClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", &newObj );
				}
				lastSibling = newObj;

				*pageData = tmpTag + 3;
			} else if ( !memcmp ( *pageData, "?", 1 ) )
			{
				/* processing instruction tag */

				*pageData += 1;

				tmpTag = *pageData;

				tmpEnd = tmpTag;
				while ( *tmpEnd && (memcmp ( tmpEnd, "?>", 2 ) != 0) )
				{
					if ( *tmpEnd == '\"' )
					{
						tmpEnd++;
						while ( *tmpEnd && (*tmpEnd != '\"') )
						{
							tmpEnd++;
						}
						if ( *tmpEnd )
						{
							tmpEnd++;
						}
					} else
					{
						tmpEnd++;
					}
				}

				/* are we still ok? */
				if ( !*tmpEnd )
				{
					/* bad tag */
					return ( 0 );
				}
				/* make it temporarily into a null for processing */
				*tmpEnd   = 0;

				/* just create a generic element for this tag... don't know how to deal with it */
				auto newObj = *classNew ( instance, "PROCESSINGINSTRUCTION", 0 );

				/* allocate a named node map for the attributes */
				auto attribs = *classNew ( instance, "NAMEDNODEMAP", 0 );

				xmlClassAssignLong				( instance, &newObj, "NODETYPE", 7 );
				xmlClassAssignStringStatic		( instance, &newObj, "NODENAME", "#processing-instruction", 23 );
				xmlClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				xmlClassAssignObject			( instance, &newObj, "ATTRIBUTES", &attribs );
				xmlClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				xmlClassAssignString			( instance, &newObj, "DATA", tmpTag, tmpEnd - tmpTag );
				xmlClassAssignReference			( instance, &newObj, "NODEVALUE", "DATA" );
				xmlClassAddToParentsChildList	( instance, parent, &newObj );

				if ( firstSibling )
				{
					firstSibling = false;

					xmlClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					xmlClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					xmlClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", &lastSibling );
				}
				lastSibling = newObj;

				/* parse out the attributes */
				while ( *tmpTag )
				{
					/* skip white space */
					while ( *tmpTag && _isspacex ( tmpTag ) )
					{
						tmpTag++;
					}

					name = tmpTag;
					while ( *tmpTag && !_isspacex ( tmpTag ) && (*tmpTag != '=') )
					{
						tmpTag++;
					}
					nameLen = tmpTag - name;

					/* does this tag have a modified DOM name? */

					// this will be 0 only if we have to do a forced termination
					tmpC = name[nameLen];

					// skip whitespace
					while ( *tmpTag && _isspacex ( tmpTag ) )
					{
						tmpTag++;
					}

					// do we have an equal sign

					if ( *tmpTag == '=' )
					{
						tmpTag++;

						/* skip white space */
						while ( *tmpTag && _isspacex ( tmpTag ) )
						{
							tmpTag++;
						}

						if ( *tmpTag == '\"' )
						{
							tmpTag++;
							value = tmpTag;
							while ( *tmpTag && (*tmpTag != '\"') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
							tmpTag++;
						} else
						{
							value = tmpTag;
							while ( *tmpTag && !_isspacex ( tmpTag ) && (*tmpTag != '=') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
						}
					} else
					{
						value	 = "";
						valueLen = 0;
					}

					if ( !((*name == '/') && (!*(name + 1)) ) )
					{
						/* ok... we've a name/value pair... now to both try to set it directly
						   in our object and to add it to the node map */

						if ( name == *pageData )
						{
							// FGL extension... make pi localname == to the pi itself
							xmlClassAssignString	( instance, &newObj, "LOCALNAME", name, nameLen );
						}

						name[nameLen] = 0;
						xmlClassAddStringToNodeMap ( instance, &attribs, name, nameLen, value, valueLen );
						name[nameLen] = tmpC;
					}
				}

				/* put back the tag end identifier that we made into a null */
				*tmpEnd = '?';

				*pageData = tmpTag + 2;
			} else if ( !memcmpi ( *pageData, "![CDATA[", 8 ) )
			{
				/* CDATA section */

				*pageData += 8;

				tmpTag = *pageData;

				/* skip over the first character... we DON'T want to test... this may be a bad tag so
				   we need to convert it to text and not try to reparse
				*/
				tmpTag++;
				while ( *tmpTag && (memcmp ( tmpTag, "]]>", 3 ) != 0) )
				{
					tmpTag++;
				}

				/* just create a generic element for this tag... don't know how to deal with it */
				auto newObj = *classNew ( instance, "CHARACTERDATA", 0 );

				/* allocate a named node map for the attributes */
				auto attribs = *classNew ( instance, "NAMEDNODEMAP", 0 );
				xmlClassAssignObject			( instance, &newObj, "ATTRIBUTES", &attribs );
				xmlClassAssignLong				( instance, &newObj, "NODETYPE", 4 );
				xmlClassAssignStringStatic		( instance, &newObj, "NODENAME", "#cdata", 6 );
				xmlClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				xmlClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				xmlClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
				xmlClassAssignReference			( instance, &newObj, "NODEVALUE", "DATA" );
				xmlClassAddToParentsChildList	( instance, parent, &newObj );

				if ( firstSibling )
				{
					firstSibling = false;

					xmlClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					xmlClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					xmlClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", &lastSibling );
				}
				lastSibling = newObj;

				*pageData = tmpTag + 3;
			} else if ( !memcmpi ( *pageData, "!", 1 ) )
			{
				/* NOTATION section */

				*pageData += 1;

				tmpTag = *pageData;

				/* skip over the first character... we DON'T want to test... this may be a bad tag so
				   we need to convert it to text and not try to reparse
				*/
				tmpTag++;
				while ( *tmpTag && (memcmp ( tmpTag, ">", 1 ) != 0) )
				{
					tmpTag++;
				}

				/* just create a generic element for this tag... don't know how to deal with it */
				auto newObj = *classNew ( instance, "NOTATION", 0 );

				xmlClassAssignObject			( instance, &newObj, "ATTRIBUTES", classNew ( instance, "NAMEDNODEMAP", 0 ) );
				xmlClassAssignLong				( instance, &newObj, "NODETYPE", 4 );
				xmlClassAssignString			( instance, &newObj, "NODENAME", *pageData, tmpTag - *pageData );
				xmlClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				xmlClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				xmlClassAssignString			( instance, &newObj, "PUBLICID", *pageData, tmpTag - *pageData );
				xmlClassAddToParentsChildList	( instance, parent, &newObj );

				if ( firstSibling )
				{
					firstSibling = false;

					xmlClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					xmlClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					xmlClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", &lastSibling );
				}
				lastSibling = newObj;

				*pageData = tmpTag + 1;
			} else
			{
				if ( !(len = xmlParseTag ( instance, *pageData, tagName, sizeof ( tagName ), prefix, sizeof ( prefix ) )) )
				{
					return ( 0 );
				}
				*pageData += len;

				autoClose = 0;

				/* just create a generic element for this tag... */
				auto newObj = *classNew ( instance, "ELEMENT", 0 );

				/* allocate a named node map for the attributes */
				auto attribs = *classNew ( instance, "NAMEDNODEMAP", 0 );

				xmlClassAssignLong				( instance, &newObj, "NODETYPE", 1 );
				xmlClassAssignString			( instance, &newObj, "NODENAME", tagName, strlen ( tagName ) );
				xmlClassAssignString			( instance, &newObj, "TAGNAME",  tagName, strlen ( tagName ) );
				xmlClassAssignString			( instance, &newObj, "PREFIX", prefix, strlen ( prefix ) );
				xmlClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				xmlClassAssignObject			( instance, &newObj, "ATTRIBUTES", &attribs );
				xmlClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				xmlClassAssignObject			( instance, parent, "LASTCHILD", &newObj );
				xmlClassAddToParentsChildList	( instance, parent, &newObj );

				if ( firstSibling )
				{
					firstSibling = false;

					xmlClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					xmlClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					xmlClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", &lastSibling );
				}
				lastSibling = newObj;

				/* find the end of the tag */
				tmpTag = tmpEnd = *pageData;
				while ( *tmpEnd && (*tmpEnd != '>') )
				{
					if ( *tmpEnd == '\"' )
					{
						tmpEnd++;
						while ( *tmpEnd && (*tmpEnd != '\"') )
						{
							tmpEnd++;
						}
						if ( *tmpEnd )
						{
							tmpEnd++;
						}
					} else
					{
						tmpEnd++;
					}
				}

				/* are we still ok? */
				if ( !*tmpEnd )
				{
					/* bad tag */
					return ( 0 );
				}
				/* make it temporarily into a null for processing */
				*tmpEnd   = 0;

				/* now to parse the attributes out */
				*pageData = tmpEnd + 1;

				/* parse out the attributes */
				while ( *tmpTag )
				{
					/* skip white space */
					while ( *tmpTag && _isspacex ( tmpTag ) )
					{
						tmpTag++;
					}

					name = tmpTag;
					while ( *tmpTag && !_isspacex ( tmpTag ) && (*tmpTag != '=') && (*tmpTag != ':') )
					{
						tmpTag++;
					}
					nameLen = tmpTag - name;

					/* does this tag have a modified DOM name? */

					// this will be 0 only if we have to do a forced termination

					if ( *tmpTag == ':' )
					{
						prefixName = name;
						prefixNameLen = nameLen;

						// skip over :
						tmpTag++;

						// parse out the nSpace identifier
						name = tmpTag;
						while ( *tmpTag && !_isspacex ( tmpTag ) && (*tmpTag != '=') )
						{
							tmpTag++;
						}

						nameLen = tmpTag - name;

						/* skip white space until the '=' sign */
						while ( *tmpTag && _isspacex ( tmpTag ) && (*tmpTag != '=') )
						{
							tmpTag++;
						}
						if ( *tmpTag == '=' )
						{
							tmpTag++;
						}

						/* skip white space */
						while ( *tmpTag && _isspacex ( tmpTag ) )
						{
							tmpTag++;
						}

						if ( *tmpTag == '\"' )
						{
							tmpTag++;
							value = tmpTag;
							while ( *tmpTag && (*tmpTag != '\"') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
							tmpTag++;
						} else if ( *tmpTag == '\'' )
						{
							tmpTag++;
							value = tmpTag;
							while ( *tmpTag && (*tmpTag != '\'') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
							tmpTag++;
						} else
						{
							value = tmpTag;
							while ( *tmpTag && !_isspacex ( tmpTag ) && (*tmpTag != '=') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
						}
						if ( (nameLen == 5) && !memcmpi ( name, "xmlns", 5 ) )
						{
							// this is a nSpace decleration
							// now we've parse out name=prefix and value=uri	- add them to the nSpace list
							nSpace.push_back ( {} );
							nSpace.back ( ).prefix = stringi ( prefixName, prefixNameLen );
							nSpace.back ( ).uri = stringi ( value, valueLen );
						}
					} else 
					{
						if ( *tmpTag == '=' )
						{
							tmpTag++;
						} else 
						{
							/* skip white space until the '=' sign */
							while ( *tmpTag && _isspacex ( tmpTag ) && (*tmpTag != '=') )
							{
								tmpTag++;
							}
							if ( *tmpTag == '=' )
							{
								tmpTag++;
							}
						}

						/* skip white space */
						while ( *tmpTag && _isspacex ( tmpTag ) )
						{
							tmpTag++;
						}

						if ( *tmpTag == '\"' )
						{
							tmpTag++;
							value = tmpTag;
							while ( *tmpTag && (*tmpTag != '\"') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
							tmpTag++;
						} else if ( *tmpTag == '\'' )
						{
							tmpTag++;
							value = tmpTag;
							while ( *tmpTag && (*tmpTag != '\'') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
							tmpTag++;
						} else

						{
							value = tmpTag;
							while ( *tmpTag && !_isspacex ( tmpTag ) && (*tmpTag != '=') )
							{
								tmpTag++;
							}
							valueLen = tmpTag - value;
						}

						if ( (nameLen == 5) && !memcmpi ( name, "xmlns", 5 ) )
						{
							// now we've parse out name=prefix and value=uri	- add them to the nSpace list
							nSpace.push_back ( {} );
							nSpace.back ( ).prefix = "";
							nSpace.back ( ).uri = stringi ( value, valueLen );
							continue;
						}
					}

					if ( (*name == '/') && (!*(name + 1)) )
					{
						autoClose = 1;
					} else
					{
						/* ok... we've a name/value pair... now to both try to set it directly
						   in our object and to add it to the node map */
						tmpC = name[nameLen];
						name[nameLen] = 0;
						xmlClassAddStringToNodeMap ( instance, &attribs, name, nameLen, value, valueLen );
						name[nameLen] = tmpC;
					}
				}

				// add in our nSpace uri... should be setup by now
				for ( auto &it : nSpace )
				{
					if ( it.prefix == prefix )
					{
						xmlClassAssignString  ( instance, &newObj, "nSpaceURI", it.uri.c_str(), it.uri.size() );
						break;
					}
				}

				/* put back the tag end identifier that we made into a null */
				*tmpEnd = '>';

				if ( !autoClose )
				{
					if ( !xmlParsePage ( instance, owner, &newObj, pageData, optionClose, noSimpleText, nSpace ) )
					{
						return ( 0 );
					}

					if ( !**pageData )
					{
						return ( 1 );
					}

					(*pageData)++;

					if ( !(len = xmlParseTag ( instance, *pageData, tagName2, sizeof ( tagName2 ), prefix2, sizeof ( prefix2 ) )) )
					{
						return ( 0 );
					}

					if ( !strccmp ( tagName, tagName2 ) && !strccmp ( prefix, prefix2 ) )
					{
						/* ok... got our end tag */
						*pageData += len;

						while ( **pageData && (**pageData != '>') )
						{
							(*pageData)++;
						}
						if ( **pageData )
						{
							(*pageData)++;
						}

						nSpace.resize ( nnSpaceSave );
					} else
					{
						/* end does not match the open... badly formed XML document */
						throw errorNum::scINVALID_XML_DOCUMENT;
					}
				}
			} // end tag processing */
		} else
		{
			tmpTag = *pageData;

			// set our starting white space indicator properly
			if ( _isspacex ( tmpTag ) )
			{
				allWhiteSpace = 1;
			} else
			{
				allWhiteSpace = 0;
			}

			/* skip over the first character... we DON'T want to test... this may be a bad tag so
			   we need to convert it to text and not try to reparse
			*/

			tmpTag++;

			while ( *tmpTag )
			{
				if ( *tmpTag == '<' )
				{ 
					/* found a tag */
					break;
				}

				if ( !_isspacex ( tmpTag ) )
				{
					allWhiteSpace = 0;
				}
				tmpTag++;
			}

			if ( !noSimpleText || !allWhiteSpace )
			{
				/* just create a generic element for this tag... don't know how to deal with it */
				auto newObj = *classNew ( instance, "TEXT", 0 );

				/* allocate a named node map for the attributes */
				xmlClassAssignObject			( instance, &newObj, "ATTRIBUTES", classNew ( instance, "NAMEDNODEMAP", 0 ) );
				xmlClassAssignLong				( instance, &newObj, "NODETYPE", 3 );
				xmlClassAssignStringStatic		( instance, &newObj, "NODENAME", "#text", 6 );
				xmlClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				xmlClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				xmlClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
				xmlClassAssignReference			( instance, &newObj, "NODEVALUE", "DATA" );
				xmlClassAddToParentsChildList	( instance, parent, &newObj );

				if ( firstSibling  )
				{
					firstSibling = false;

					xmlClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					xmlClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					xmlClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", &lastSibling );
				}
				lastSibling = newObj;
			}
			*pageData = tmpTag;
		}
	}
	return ( 1 );
}

void xmlParse ( class vmInstance *instance, VAR_OBJ *docRef, char *pageData, bool noSimpleText )
{
	std::vector<xmlnSpace> nSpace;

	xmlClassAssignLong			( instance, docRef, "NODETYPE",		9 );
	xmlClassAssignStringStatic	( instance, docRef, "NODENAME",		"#document", 9 );
	xmlClassAssignObject		( instance, docRef, "CHILDNODES",	classNew ( instance, "NODELIST", 0 ) );

	xmlParsePage ( instance, docRef, docRef, &pageData, 0, noSimpleText, nSpace );
}
