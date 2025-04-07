/*
	document object model

	html parser


*/

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmNativeInterface.h"
#include "builtinDom.h"

struct HTML_ATTRIB {
	char const	*attribName;
	char const	*domName;
	bool		 isFlag;			/* no VALUE field */
	bool		 isNumeric;			/* must be set to 1 for isFlag */
};

enum class HTML_SPECIAL {
	HTML_S_NONE,
	HTML_S_LINKS,
	HTML_S_IMAGES,
	HTML_S_FORMS,
	HTML_S_APPLETS,
	HTML_S_ANCHORS,
	HTML_S_OPTION,
	HTML_S_TABLE,
	HTML_S_TBODY,
	HTML_S_TROW,
	HTML_S_TFOOT,
	HTML_S_THEAD,
	HTML_S_TCAPTION,
	HTML_S_CELL,
	HTML_S_SCRIPT,
	HTML_S_INPUT,
};

struct HTML_TAG {
	char const		 *tag;
	char const		 *domClassName;
	HTML_ATTRIB		 *attribs;
	char const		 *colId;
	int				  trimmable;
	int				  canHaveSiblings;
	HTML_SPECIAL	  special;
};

HTML_ATTRIB	NAME_ATTR[] =  {
								{ "name", "ID", 0, 0 },
								{ 0, 0, 0, 0 }
							};

HTML_ATTRIB	META_ATTR[] =  {
								{ "http-equiv", "HTTPEQUIV", 0, 0 },
								{ 0, 0, 0, 0 }
							};

HTML_ATTRIB	TD_ATTR[] =  {
							{ "NOWRAP", "NOWRAP", 1, 1 },
							{ 0, 0, 0, 0 }
						};

HTML_ATTRIB	x_ATTR[] =  {
							{ "", 0 },
							{ 0, 0, 0, 0 }
						};

// must be smallest common word LAST

HTML_TAG htmlTags[] =	{
							{ "ABBR",			"HTMLABBRELEMENT",			0,		 0,				1, 1, HTML_SPECIAL::HTML_S_NONE			},
							{ "ACRONYM",		"HTMLACRONYMELEMENT",		0,		 0,				1, 1, HTML_SPECIAL::HTML_S_NONE			},
							{ "ADDRESS",		"HTMLADDRESSELEMENT",		0,		 0,				1, 1, HTML_SPECIAL::HTML_S_NONE			},
							{ "APPLET",			"HTMLAPPLETELEMENT",		0,		 "APPLETS",		1, 1, HTML_SPECIAL::HTML_S_APPLETS		},				// needs special
							{ "AREA",			"HTMLARRAYELEMENT",			0,		 0,				1, 0, HTML_SPECIAL::HTML_S_NONE			},
							{ "A",				"HTMLANCHORELEMENT",		NAME_ATTR, "ANCHORS",	0, 1, HTML_SPECIAL::HTML_S_ANCHORS		},
							{ "BASEFONT",		"HTMLBASEFONE",				0,		 0,				1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BASE",			"HTMLBASEELEMENT",			0,		 0,				1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BDO",			"HTMLBDODLEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BIG",			"HTMLBIGELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BLOCKQUOTE",		"HTMLQUOTEELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BODY",			"HTMLBODYELEMENT",			0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BR",				"HTMLBRELEMENT",			0,		 0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "BUTTON",			"HTMLBUTTONELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "B",				"HTMLBELEMENT",				0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "CAPTION",		"HTMLTABLECAPTIONELEMENT",	0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_TCAPTION		},
							{ "CENTER",			"HTMLCENTERELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "CITE",			"HTMLCITEELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "CODE",			"HTMLCODEELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "COL",			"HTMLTABLECOLELEMENT",		0,		 0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "COLGROUP",		"HTMLTABLECOLELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "DD",				"HTMLDDELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "DEL",			"HTMLDELELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "DFN",			"HTMLDFNELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "DIR",			"HTMLDIRELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "DIV",			"HTMLDIVELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "DL",				"HTMLDLISTELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE			},
							{ "DT",				"HTMLDTELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "EM",				"HTMLEMELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "FIELDSET",		"HTMLFIELDSETELEMENT",		0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "FONT",			"HTMLFONTELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "FORM",			"HTMLFORMELEMENT",			0,		 "FORMS",		0, 1, HTML_SPECIAL::HTML_S_FORMS,		},
							{ "FRAMESET",		"HTMLFRAMESETELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "FRAME",			"HTMLFRAMEELEMENT",			0,		 0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "H1",				"HTMLHEADINGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "H2",				"HTMLHEADINGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "H3",				"HTMLHEADINGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "H4",				"HTMLHEADINGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "H5",				"HTMLHEADINGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "H6",				"HTMLHEADINGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "HEAD",			"HTMLHEADELEMENT",			0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "HR",				"HTMLHRELEMENT",			0,		 0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "HTML",			"HTMLHTMLELEMENT",			0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "IFRAME",			"HTMLLIFRAMEELEMENT",		0,		 0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "IMG",			"HTMLIMAGEELEMENT",			0,		 "IMAGES",		0, 0, HTML_SPECIAL::HTML_S_IMAGES		},
							{ "INPUT",			"HTMLINPUTELEMENT",			0,		 0, 			0, 0, HTML_SPECIAL::HTML_S_INPUT		},
							{ "INS",			"HTMLINSELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "ISINDEX",		"HTMLISINDEXELEMENT",		0,		 0, 			0, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "I",				"HTMLIELEMENT",				0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "KBD",			"HTMLKBDELEMENT",			0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "LABEL",			"HTMLLABELELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "LEGEND",			"HTMLLEGENEELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "LINK",			"HTMLLINKELEMENT",			0,		 "LINKS",		0, 0, HTML_SPECIAL::HTML_S_LINKS		},
							{ "LI",				"HTMLLIELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "MAP",			"HTMLMAPELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "MENU",			"HTMLMENUELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "META",			"HTMLMETAELEMENT",			META_ATTR,0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "NOFRAMES",		"HTMLNOFRAMESELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "NOSCRIPT",		"HTMLNOSCRIPTELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "OBJECT",			"HTMLOBJECTELEMENT",		0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "OL",				"HTMLOLISTELEMENT",			0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "OPTGROUP",		"HTMLOPTGROUPELEMENT",		0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "OPTION",			"HTMLOPTIONELEMENT",		0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_OPTION		},
							{ "PARAM",			"HTMLPARAMELEMENT",			0,		 0, 			1, 0, HTML_SPECIAL::HTML_S_NONE 		},
							{ "PRE",			"HTMLPREELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "P",				"HTMLPARAGRAPHELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "Q",				"HTMLQELEMENT",				0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "SAMP",			"HTMLSAMPELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "SCRIPT",			"HTMLSCRIPTELEMENT",		0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_SCRIPT		},
							{ "SELECT",			"HTMLSELECTELEMENT",		0,		 0, 			0, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "SMALL",			"HTMLSMALLELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "SPAN",			"HTMLSPANELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "STRIKE",			"HTMLSTRIKEELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "STRONG",			"HTMLSTRONGELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "STYLE",			"HTMLSTYLEELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "SUB",			"HTMLSUBELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "SUP",			"HTMLSUPELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "S",				"HTMLSELEMENT",				0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "TABLE",			"HTMLTABLEELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_TABLE		},
							{ "TBODY",			"HTMLTABLESECTIONELEMENT",	0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_TBODY		},
							{ "TD",				"HTMLTABLECELLELEMENT",		TD_ATTR,	 0, 			1, 1, HTML_SPECIAL::HTML_S_CELL		},
							{ "TEXTAREA",		"HTMLTEXTARRAYELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "TFOOT",			"HTMLTABLESECTIONELEMENT",	0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_TFOOT		},
							{ "THEAD",			"HTMLTABLESECTIONELEMENT",	0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_THEAD		},
							{ "TH",				"HTMLTABLECELLELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_CELL		},
							{ "TITLE",			"HTMLTITLEELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "TR",				"HTMLTABLEROWELEMENT",		0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_TROW		},
							{ "TT",				"HTMLTTELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "UL",				"HTMLULISTELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "U",				"HTMLUELEMENT",				0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "VAR",			"HTMLVARELEMENT",			0,		 0, 			1, 1, HTML_SPECIAL::HTML_S_NONE 		},
							{ "!--",			0,							0,		 0, 			0, 0, HTML_SPECIAL::HTML_S_NONE 		},	// dummy... for tag match but handled seprately
							{ 0,				0,							0,		 0, 			0, 0, HTML_SPECIAL::HTML_S_NONE 		},	// last one
						};

static void domClassAssignString ( class vmInstance *instance, VAR_OBJ *obj, char const *member, char const *string, size_t len )
{
	if ( string )
	{
		*((*obj)[member]) = VAR_STR ( instance, string, len );
	} else
	{
		*((*obj)[member]) = VAR_STR ( "" );
	}
}

static void domClassAssignStringStatic ( class vmInstance *instance, VAR_OBJ *obj, char const *member, char const *string, size_t len )
{
	*((*obj)[member]) = VAR_STR ( instance, string, len );
}

static void domClassAssignLong ( class vmInstance *instance, VAR_OBJ *obj, char const *member, size_t num )
{
	*((*obj)[member]) = VAR ( num );
}

static void domClassAssignObject ( class vmInstance *instance, VAR_OBJ *obj, char const *member, VAR_OBJ *obj2 )
{
	*((*obj)[member]) = *obj2;
}

static void domClassAddStringToNodeMap ( class vmInstance *instance, VAR_OBJ *nodeMap, char const *name, size_t nameLen, char const *value, size_t valueLen )
{
	int64_t	 indicie[2]{};

	auto length = (*nodeMap)["length"];

	if ( TYPE ( length ) != slangType::eLONG )
	{
		length->type = slangType::eLONG;
		length->dat.l = 0;
	}

	indicie[0] = length->dat.l;

	length->dat.l++;

	auto array = (*nodeMap)["ARRAY"];

	// NOTE: can't use array[1] as this just access.. it does NOT create a new element but will return nullptr if the new element doesn't exists so we need to use _arrayGet
	indicie[1] = 1;
	*_arrayGet ( instance, array, 2, indicie ) = VAR_STR ( instance, name, nameLen );
	indicie[1] = 2;
	*_arrayGet ( instance, array, 2, indicie ) = VAR_STR ( instance, value, valueLen );
}

static void domClassAddLongToNodeMap ( class vmInstance *instance, VAR_OBJ *nodeMap, char const *name, size_t nameLen, int64_t numericValue )
{
	int64_t	 indicie[2]{};

	auto length = (*nodeMap)["length"];

	if ( TYPE ( length ) != slangType::eLONG )
	{
		length->type = slangType::eLONG;
		length->dat.l = 0;
	}

	indicie[0] = length->dat.l;

	length->dat.l++;

	auto array = (*nodeMap)["array"];

	indicie[1] = 1;
	*_arrayGet ( instance, array, 2, indicie ) = VAR_STR ( instance, name, nameLen );

	indicie[1] = 2;
	*_arrayGet ( instance, array, 2, indicie ) = VAR ( numericValue );
}

static void domClassAddToNodeList ( class vmInstance *instance, VAR_OBJ *nodeList, VAR_OBJ *obj )
{
	int64_t indicie[1]{};

	auto length = (*nodeList)["length"];

	if ( TYPE ( length ) != slangType::eLONG )
	{
		length->type  = slangType::eLONG;
		length->dat.l = 0;
	}

	indicie[0] = length->dat.l;

	length->dat.l++;

	auto array = (*nodeList)["array"];

	*_arrayGet ( instance, array, 1, indicie ) = *obj;
}

static void domClassAddToParentsChildList ( class vmInstance *instance, VAR_OBJ *parent, VAR_OBJ *obj )
{
	auto nodeListObj = (*parent)["childNodes"];
	VAR_OBJ obj2 ( nodeListObj->dat.ref.v );
	domClassAddToNodeList ( instance, &obj2, obj );
}


static int64_t domClassAddToCollection ( class vmInstance *instance, VAR_OBJ *owner, char const *colName, VAR_OBJ *obj )
{
	int64_t indicie[1]{};

	auto collection = static_cast<VAR_OBJ *>((*owner)[colName]);
	auto length = (*collection)["length"];
	if ( TYPE ( length ) != slangType::eLONG )
	{
		length->type  = slangType::eLONG;
		length->dat.l = 0;
	}

	indicie[0] = length->dat.l;

	length->dat.l++;

	auto array = classIVarAccess ( collection, "ARRAY" );
	*_arrayGet ( instance, array, 1, indicie ) = *obj;

	return indicie[0];
}

static int64_t domClassAddToCollectionOfType ( class vmInstance *instance, VAR_OBJ *start, char const *className, char const *colName, VAR_OBJ *obj )
{
	while ( start && TYPE ( start ) == slangType::eOBJECT_ROOT )
	{
		if ( !strccmp ( start->dat.ref.v->dat.obj.classDef->name, className ) )
		{
			return ( domClassAddToCollection ( instance, start, colName, obj ) );
		}

		start = static_cast<VAR_OBJ *>((*start)["PARENT"]);
		while ( TYPE ( start ) == slangType::eREFERENCE )
		{
            start = static_cast<VAR_OBJ *>(static_cast<VAR *>(start->dat.ref.v));
		}
	}
	return 0;
}

static void domClassSetMemberOfType ( class vmInstance *instance, VAR_OBJ *start, char const *className, char const *member, VAR_OBJ *obj )
{
	while ( start && TYPE ( start ) == slangType::eOBJECT_ROOT )
	{
		if ( !strccmp ( start->dat.ref.v->dat.obj.classDef->name, className ) )
		{
			*(*start)[member] = *obj;
			return;
		}

		start = static_cast<VAR_OBJ *>((*start)["PARENT"]);
		while ( TYPE ( start ) == slangType::eREFERENCE )
		{
			start = static_cast<VAR_OBJ *>(static_cast<VAR *>(start->dat.ref.v));
		}
	}
}


static void domParsePage ( class vmInstance *instance, VAR_OBJ *owner, VAR_OBJ *parent, char ** pageData, bool optionClose, int64_t tagsOnly )
{
	size_t		 loop;
	size_t		 loop2;
	size_t		 len;
	VAR			*tagId;
	char		*tagStart;
	char		 tmpC;
	char		*tmpCPtr;
	char		*tmpEnd;
	char		*tmpTag;
	char const	*name;
	int			 skip;
	int64_t		 index;
	size_t		 nameLen;
	bool		 isFlag;
	bool		 isNumeric;
	bool		 autoClose;
	char		*value;
	size_t		 valueLen;
	int64_t		 numericValue;

	bool firstChild = true;
	VAR  lastSibling{};

	while ( **pageData )
	{
		if ( **pageData == '<' )
		{
			tagStart = *pageData;

			(*pageData)++;
			while ( **pageData && (_isspace ( *pageData ) || _iseol ( *pageData )) )
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

				for ( loop = 0; htmlTags[loop].tag; loop++ )
				{
					if ( !memcmpi ( tmpTag, htmlTags[loop].tag, (len = strlen ( htmlTags[loop].tag) )) )
					{
						if ( _isspace ( tmpTag + len ) || _iseol ( tmpTag + len ) || (*(tmpTag + len) == '>') )
						{
							if ( (tagsOnly != 2) || !htmlTags[loop].trimmable )
							{
								/* found it - is it a valid end tag? */
								if ( htmlTags[loop].canHaveSiblings )
								{
									/* ok.. this here is absolutely BS... if we happen to receive an end tag that has
									   NO matching open tag what do we do... if we exit early then we won't parse the page
									   so... we have to check to see if it ever DOES exist in the parent chain... sigh...
									   if it doesn't then we have to skip it
									*/

									auto caller = parent;

									while ( caller && (TYPE (caller) == slangType::eOBJECT) )
									{
										tagId = classIVarAccess ( caller, "NODENAME" );

										while ( TYPE ( tagId ) == slangType::eREFERENCE )
										{
											tagId = tagId->dat.ref.v;
										}

										if ( !memcmpi ( tagId->dat.str.c, tmpTag, tagId->dat.str.len ) )
										{
											return;
										}

										caller = static_cast<VAR_OBJ *>((*caller)["PARENTNODE"]);

										if ( caller )
										{
											while ( TYPE (caller) == slangType::eREFERENCE )
											{
												caller = static_cast<VAR_OBJ *>(static_cast<VAR *>(caller->dat.ref.v));
											}
										}
									}

									if ( !caller || (TYPE ( caller ) == slangType::eNULL) )
									{
										/* bad damn unmatched end tag... just eat it up... yup!!! */
										while ( **pageData && (**pageData != '>') )
										{
											(*pageData)++;
										}
										if ( **pageData )
										{
											(*pageData)++;
										}
									}
								}
							} else
							{
								/* trimmed... eat it up all gone */
								while ( **pageData && (**pageData != '>') )
								{
									(*pageData)++;
								}
								if ( **pageData )
								{
									(*pageData)++;
								}
							}
						}
					}
				}

				if ( !htmlTags[loop].tag )
				{
					/* not a valid end tag */
					(*pageData)--;
					goto process_as_text;
				}
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

				if ( !tagsOnly )
				{
					/* just create a generic element for this tag... don't know how to deal with it */
					auto newObj = *classNew ( instance, "COMMENT", 0 );

					domClassAssignLong				( instance, &newObj, "NODETYPE", 8 );
					domClassAssignStringStatic		( instance, &newObj, "NODENAME", "#comment", 8 );
					domClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
					domClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
					domClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
					domClassAssignLong				( instance, &newObj, "LENGTH", tmpTag - *pageData );
					domClassAddToParentsChildList	( instance, parent, &newObj );

					if ( firstChild )
					{
						firstChild = false;

						domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
					} else
					{
						domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
						domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
					}
					lastSibling = *newObj;
				}

				*pageData = tmpTag + 3;
			} else if ( !memcmp ( *pageData, "?", 1 ) )
			{
				/* processing instruction tag */

				*pageData += 1;

				tmpTag = *pageData;

				/* skip over the first character... we DON'T want to test... this may be a bad tag so
				   we need to convert it to text and not try to reparse
				*/
				tmpTag++;
				tmpEnd = 0;
				while ( *tmpTag && (memcmp ( tmpTag, "?>", 2 ) != 0) )
				{
					if ( (*tmpTag == ' ') || (*tmpTag == '\r') || (*tmpTag == '\n') || (*tmpTag == '\t') )
					{
						tmpEnd = tmpTag;
					}
					tmpTag++;
				}

				if ( !tagsOnly )
				{
					/* just create a generic element for this tag... don't know how to deal with it */
					auto newObj = *classNew ( instance, "PROCESSINGINSTRUCTION", 0 );

					domClassAssignLong				( instance, &newObj, "NODETYPE", 7 );
					domClassAssignStringStatic		( instance, &newObj, "NODENAME", "#processing-instruction", 23 );
					domClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
					domClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
					domClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
					domClassAssignLong				( instance, &newObj, "LENGTH", tmpEnd - *pageData );
					domClassAddToParentsChildList	( instance, parent, &newObj );

					if ( firstChild )
					{
						firstChild = false;

						domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
					} else
					{
						domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
						domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
					}
					lastSibling = newObj;
				}

				*pageData = tmpTag + 3;
			} else if ( !memcmpi ( *pageData, "![CDATA[", 8 ) )
			{
				/* CDATA section */

				*pageData += 1;

				tmpTag = *pageData;

				/* skip over the first character... we DON'T want to test... this may be a bad tag so
				   we need to convert it to text and not try to reparse
				*/
				tmpTag++;
				while ( *tmpTag && (memcmp ( tmpTag, "]]>", 3 ) != 0) )
				{
					tmpTag++;
				}

				if ( !tagsOnly )
				{
					/* just create a generic element for this tag... don't know how to deal with it */
					auto newObj = *classNew ( instance, "CHARACTERDATA", 0 );

					domClassAssignLong				( instance, &newObj, "NODETYPE", 4 );
					domClassAssignStringStatic		( instance, &newObj, "NODENAME", "#cdata", 6 );
					domClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
					domClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
					domClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
					domClassAssignLong				( instance, &newObj, "LENGTH", tmpTag - *pageData );
					domClassAddToParentsChildList	( instance, parent, &newObj );

					if ( firstChild )
					{
						firstChild = false;

						domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
					} else
					{
						domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
						domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
					}
					lastSibling = newObj;
				}

				*pageData = tmpTag + 3;
			} else
			{
				/* got a tag, now lets find it */

				for ( loop = 0; htmlTags[loop].tag; loop++ )
				{
					if ( !memcmpi ( *pageData, htmlTags[loop].tag, (len = strlen ( htmlTags[loop].tag) )) )
					{
						break;
					}
				}

				if ( !htmlTags[loop].tag || ((tagsOnly == 2) && htmlTags[loop].trimmable) )
				{
					/* no such possible tag or it's being trimmed... send it as text */
					goto process_as_text;
				}

				if ( _isspace ( *pageData + len ) || _iseol ( *pageData + len ) || (*(*pageData + len) == '>') )
				{
					/* we found the tag */

					// we need to take care of any IMPLIED members here... damn...
					switch ( htmlTags[loop].special )
					{
						case HTML_SPECIAL::HTML_S_INPUT:
							loop++;
							loop--;
							break;
						case HTML_SPECIAL::HTML_S_TROW:
							if ( parent && TYPE ( parent ) == slangType::eOBJECT_ROOT )
							{
								if ( strccmp ( parent->dat.ref.v->dat.obj.classDef->name, "HTMLTABLESECTIONELEMENT" ) )
								{
									// ok... parent must be either a TBODY, TFOOT or THEAD...
									// damn... now we need to create one instead
									// basically mal-formed html, but we're still going to try to keep processing it

									auto newObj = *classNew ( instance, "HTMLTABLESECTIONELEMENT", 0 );

									domClassAssignLong				( instance, &newObj, "NODETYPE", 1 );
									domClassAssignStringStatic		( instance, &newObj, "NODENAME",	"TBODY", 5 );
									domClassAssignStringStatic		( instance, &newObj, "TAGNAME",	"TBODY", 5 );
									domClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
									domClassAssignObject			( instance, &newObj, "ATTRIBUTES", classNew ( instance, "NAMEDNODEMAP", 0 ) );
									domClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
									domClassAssignObject			( instance, parent, "LASTCHILD", &newObj );
									domClassAddToCollectionOfType	( instance, parent, "HTMLTABLEELEMENT", "BODIES", &newObj );

									domClassAddToParentsChildList ( instance, parent, &newObj );

									if ( firstChild )
									{
										firstChild = false;

										domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
									} else
									{
										domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
										domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
									}
									lastSibling = newObj;

									parent = static_cast<VAR_OBJ *>(instance->om->alloc ( 1 ));
									*parent = newObj;
								}
							}
							break;
						default:
							break;
					}

					*pageData += len;

					/* special processing for this tag */
					switch ( htmlTags[loop].special )
					{
						case HTML_SPECIAL::HTML_S_OPTION:
							if ( optionClose )
							{
								*pageData = tagStart;
								/* optional end tag for <option> at start of next <option> */
								return;
							}
							break;
						default:
							break;
					}

					autoClose = 0;

					{
						/* use generic parsing for this tag */
						VAR_OBJ *p;

						if ( htmlTags[loop].domClassName )
						{
							if ( !(p = classNew ( instance, htmlTags[loop].domClassName, 0 )) )
							{
								printf ( "ERROR: bad class name for %s\r\n", htmlTags[loop].tag );

								/* just create a generic html element for this tag... don't know how to deal with it */
								p = classNew ( instance, "HTMLELEMENT", 0 );
							}
						} else
						{
							/* just create a generic html element for this tag... don't know how to deal with it */
							p = classNew ( instance, "HTMLELEMENT", 0 );
						}

						VAR_OBJ newObj ( p );
						/* allocate a named node map for the attributes */
						auto attribs = *classNew ( instance, "NAMEDNODEMAP", 0 );

						domClassAssignLong			( instance, &newObj, "NODETYPE", 1 );
						domClassAssignStringStatic  ( instance, &newObj, "NODENAME", htmlTags[loop].tag, strlen ( htmlTags[loop].tag ) );
						domClassAssignStringStatic  ( instance, &newObj, "TAGNAME",  htmlTags[loop].tag, strlen ( htmlTags[loop].tag ) );
						if ( parent ) domClassAssignObject		( instance, &newObj, "PARENTNODE", parent );
						domClassAssignObject		( instance, &newObj, "ATTRIBUTES", &attribs );
						domClassAssignObject		( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
						if ( parent )	domClassAssignObject		( instance, parent, "LASTCHILD", &newObj );
						if ( parent )	domClassAddToParentsChildList ( instance, parent, &newObj );

						if ( firstChild )
						{
							firstChild = false;

							if ( parent ) domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
						} else
						{
							domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
							domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
						}
						lastSibling = newObj;

						/* special processing for this tag */
						switch ( htmlTags[loop].special )
						{
							case HTML_SPECIAL::HTML_S_LINKS:
								domClassAddToCollection ( instance, owner, "LINKS", &newObj );
								break;
							case HTML_SPECIAL::HTML_S_IMAGES:
								domClassAddToCollection ( instance, owner, "IMAGES", &newObj );
								break;
							case HTML_SPECIAL::HTML_S_FORMS:
								domClassAddToCollection ( instance, owner, "FORMS", &newObj );
								break;
							case HTML_SPECIAL::HTML_S_APPLETS:
								domClassAddToCollection ( instance, owner, "APPLETS", &newObj );
								break;
							case HTML_SPECIAL::HTML_S_ANCHORS:
								domClassAddToCollection ( instance, owner, "ANCHORS", &newObj );
								break;
							default:
								break;
						}

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
							return;
						}
						/* make it temporarily into a null for processing */
						*tmpEnd   = 0;

						/* now to parse the attributes out */

						*pageData = tmpEnd + 1;

						/* parse out the attributes */

						while ( *tmpTag )
						{
							/* skip white space */
							while ( *tmpTag && (_isspace ( tmpTag ) || _iseol ( tmpTag )) )
							{
								tmpTag++;
							}

							name = tmpTag;
							tmpCPtr = tmpTag;
							while ( *tmpTag && !(_isspace ( tmpTag ) || _iseol ( tmpTag )) && (*tmpTag != '=') )
							{
								tmpTag++;
								tmpCPtr++;
							}
							nameLen = tmpTag - name;

							/* does this tag have a modified DOM name? */

							// this will be 0 only if we have to do a forced termination
							tmpC = 0;

							isFlag = false;
							isNumeric = false;
							if ( htmlTags[loop].attribs )
							{
								for ( loop2 = 0; htmlTags[loop].attribs[loop2].attribName; loop2++ )
								{
									if ( !memcmpi ( htmlTags[loop].attribs[loop2].attribName, name, nameLen ) )
									{
										name = htmlTags[loop].attribs[loop2].domName;
										nameLen = strlen ( name );

										isFlag = htmlTags[loop].attribs[loop2].isFlag;
										isNumeric = htmlTags[loop].attribs[loop2].isNumeric;
										break;
									}
								}

								if ( !htmlTags[loop].attribs[loop2].attribName )
								{
									tmpC = name[nameLen];
								}
							} else
							{
								tmpC = name[nameLen];
							}

							if ( *tmpTag == '=' )
							{
								tmpTag++;
							} else
							{
								/* skip white space until the '=' sign */
								while ( *tmpTag && (_isspace ( tmpTag ) || _iseol ( tmpTag )) && (*tmpTag != '=') )
								{
									tmpTag++;
								}
								if ( *tmpTag == '=' )
								{
									tmpTag++;
								}
							}

							value = 0;
							valueLen = 0;
							if ( isFlag )
							{
								numericValue = 1;
							} else
							{
								/* skip white space */
								while ( *tmpTag && (_isspace ( tmpTag ) || _iseol ( tmpTag )) )
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
									while ( *tmpTag && !(_isspace ( tmpTag ) || _iseol ( tmpTag )) && (*tmpTag != '=') )
									{
										tmpTag++;
									}
									valueLen = tmpTag - value;
								}

								if ( isNumeric )
								{
									char tmpNum;

									tmpNum = value[valueLen];
									value[valueLen] = 0;
									numericValue = _atoi64 ( value );
									value[valueLen] = tmpNum;
								}
							}

							if ( (*name == '/') && (!*(name + 1)) )
							{
								autoClose = 1;
							} else
							{
								/* ok... we've a name/value pair... now to both try to set it directly
								   in our object and to add it to the node map */

								if ( isNumeric )
								{
									domClassAddLongToNodeMap ( instance, &attribs, name, nameLen, numericValue );
								} else
								{
									domClassAddStringToNodeMap ( instance, &attribs, name, nameLen, value, valueLen );
								}

								if ( tmpC )
								{
									*tmpCPtr = 0;
								}

								if ( isNumeric )
								{
									domClassAssignLong ( instance, &newObj, name, numericValue );
								} else
								{
									if (value)
									{
										domClassAssignString(instance, &newObj, name, value, valueLen);
									}
									else
									{
										domClassAssignString(instance, &newObj, name, "", 0);
									}
								}
							}

							if ( tmpC )
							{
								*tmpCPtr = tmpC;
							}

							/* skip white space */
							while ( *tmpTag && (_isspace ( tmpTag ) || _iseol ( tmpTag )) )
							{
								tmpTag++;
							}
						}

						/* put back the tag end identifier that we made into a null */
						*tmpEnd = '>';

						if ( htmlTags[loop].colId )
						{
							domClassAddToCollection ( instance, owner, htmlTags[loop].colId, &newObj );
						}

						/* can we have siblings? */
						if ( htmlTags[loop].canHaveSiblings && !autoClose )
						{
							switch ( htmlTags[loop].special )
							{
								case HTML_SPECIAL::HTML_S_TBODY:
									domClassAddToCollectionOfType ( instance, parent, "HTMLTABLEELEMENT", "TBODIES", &newObj );
									break;
								case HTML_SPECIAL::HTML_S_TROW:
									index = domClassAddToCollectionOfType ( instance, parent, "HTMLTABLEELEMENT", "ROWS", &newObj );
									domClassAssignLong ( instance, &newObj, "ROWINDEX", index );
									index = domClassAddToCollectionOfType ( instance, parent, "HTMLSECTIONELEMENT", "ROWS", &newObj );
									domClassAssignLong ( instance, &newObj, "sectionRowIndex", index );
									break;
								case HTML_SPECIAL::HTML_S_TFOOT:
									domClassSetMemberOfType		  ( instance, parent, "HTMLTABLEELEMENT", "TFOOT", &newObj );
									domClassAddToCollectionOfType ( instance, parent, "HTMLTABLEELEMENT", "TBODIES", &newObj );
									// set rowindex
									break;
								case HTML_SPECIAL::HTML_S_THEAD:
									domClassSetMemberOfType		  ( instance, parent, "HTMLTABLEELEMENT", "THEAD", &newObj );
									domClassAddToCollectionOfType ( instance, parent, "HTMLTABLEELEMENT", "TBODIES", &newObj );
									break;
								case HTML_SPECIAL::HTML_S_TCAPTION:
									domClassSetMemberOfType ( instance, parent, "HTMLTABLEELEMENT", "CAPTION", &newObj );
									break;
								case HTML_SPECIAL::HTML_S_CELL:
									domClassSetMemberOfType ( instance, parent, "HTMLTABLEROWELEMENT", "CELLS", &newObj );
									break;
								default:
									break;
							}


							switch ( htmlTags[loop].special )
							{
								case HTML_SPECIAL::HTML_S_OPTION:
									domParsePage ( instance, owner, &newObj, pageData, 1, tagsOnly );
									break;

								case HTML_SPECIAL::HTML_S_TABLE:
									{
										// for tables we need to create another set of collections
										domClassAssignObject ( instance, &newObj, "ROWS", classNew ( instance, "HTMLCOLLECTION", 0 ) );
										domClassAssignObject ( instance, &newObj, "TBODIES", classNew ( instance, "HTMLCOLLECTION", 0 ) );
										domParsePage ( instance, owner, &newObj, pageData, 1, tagsOnly );
									}
									break;

								case HTML_SPECIAL::HTML_S_SCRIPT:
									/* don't descend... just blow throught until we find a close script */

									tmpTag = *pageData;

									while ( *tmpTag )
									{
										if ( *tmpTag == '<' )
										{
											if ( !memcmpi ( tmpTag + 1, "/SCRIPT", 7 ) )
											{
												if ( !tagsOnly )
												{
													/* emith the script code as a #text element.. is this correct? */
													newObj = *classNew ( instance, "TEXT", 0 );

													domClassAssignLong					( instance, &newObj, "NODETYPE", 3 );
													domClassAssignStringStatic			( instance, &newObj, "NODENAME", "#text", 6 );
													if ( parent ) domClassAssignObject	( instance, &newObj, "PARENTNODE", parent );
													domClassAssignObject				( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
													if ( parent ) domClassAddToParentsChildList		( instance, parent, &newObj );

													if ( firstChild )
													{
														firstChild = false;

														if ( parent ) domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
													} else
													{
														domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
														domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
													}
													lastSibling = newObj;

													domClassAssignString ( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
													domClassAssignLong   ( instance, &newObj, "LENGTH", tmpTag - *pageData );
												}

												*pageData = tmpTag + 1;	// should be /SCRIPT not </SCRIPT
												break;
											}

										}
										tmpTag++;
									}
									break;

								default:
									domParsePage ( instance, owner, &newObj, pageData, optionClose, tagsOnly );
									break;
							}

							if ( !**pageData )
							{
								return;
							}

							(*pageData)++;

							if ( !memcmpi ( htmlTags[loop].tag, *pageData, strlen ( htmlTags[loop].tag )) )
							{
								/* ok... got our end tag */
								*pageData += strlen ( htmlTags[loop].tag );

								while ( **pageData && (**pageData != '>') )
								{
									(*pageData)++;
								}
								if ( **pageData )
								{
									(*pageData)++;
								}
							} else
							{
								/* ok.. this here is absolutely BS... if we happen to receive an end tag that has
								   NO matching open tag what do we do... if we exit early then we won't parse the page
								   so... we have to check to see if it ever DOES exist in the parent chain... sigh...
								   if it doesn't then we have to skip it
								*/

								VAR *caller = (VAR *)parent;

								while ( caller && (TYPE (caller) == slangType::eOBJECT) )
								{
									tagId = classIVarAccess ( caller, "NODENAME" );

									while ( TYPE ( tagId ) == slangType::eREFERENCE )
									{
										tagId = tagId->dat.ref.v;
									}

									if ( !memcmpi ( tagId->dat.str.c, *pageData, strlen ( htmlTags[loop].tag )) )
									{
										/* found a match... there was a preceeding optional end tag */
										(*pageData)--;

										return;
									}

									caller = classIVarAccess ( caller, "PARENTNODE" );

									if ( caller )
									{
										while ( TYPE (caller) == slangType::eREFERENCE )
										{
											caller = caller->dat.ref.v;
										}
									}
								}

								if ( !caller || (TYPE ( caller ) == slangType::eNULL) )
								{
									while ( **pageData && (**pageData != '>') )
									{
										(*pageData)++;
									}
									if ( **pageData )
									{
										(*pageData)++;
									}
								}
							}
						}
					}
				} else
				{
					(*pageData)--;
					goto process_as_text;
				}
			} // end tag processing */
		} else
		{
process_as_text:
			tmpTag = *pageData;

			/* skip over the first character... we DON'T want to test... this may be a bad tag so
			   we need to convert it to text and not try to reparse
			*/
			tmpTag++;

			while ( *tmpTag )
			{
				if ( *tmpTag == '<' )
				{
					/* see if it matches a tag */
					if( *(tmpTag + 1) == '/' )
					{
						skip = 2;
					} else
					{
						skip = 1;
					}

					for ( loop = 0; htmlTags[loop].tag; loop++ )
					{
						if ( !memcmpi ( tmpTag + skip, htmlTags[loop].tag, (len = strlen ( htmlTags[loop].tag) )) )
						{
							if ( _isspace ( tmpTag + skip + len ) || _iseol ( tmpTag + skip + len ) || (*(tmpTag + skip + len) == '>') )
							{
								break;
							}
						}
					}
					if ( htmlTags[loop].tag )
					{
						/* found a tag */
						break;
					}

				}
				tmpTag++;
			}

			if ( !tagsOnly )
			{
				/* just create a generic element for this tag... don't know how to deal with it */
				auto newObj = *classNew ( instance, "TEXT", 0 );

				domClassAssignLong				( instance, &newObj, "NODETYPE", 3 );
				domClassAssignStringStatic		( instance, &newObj, "NODENAME", "#text", 6 );
				domClassAssignObject			( instance, &newObj, "PARENTNODE", parent );
				domClassAssignObject			( instance, &newObj, "CHILDNODES", classNew ( instance, "NODELIST", 0 ) );
				domClassAssignString			( instance, &newObj, "DATA", *pageData, tmpTag - *pageData );
				domClassAssignLong				( instance, &newObj, "LENGTH", tmpTag - *pageData );
				if ( parent ) domClassAddToParentsChildList	( instance, parent, &newObj );

				if ( firstChild )
				{
					firstChild = false;

					domClassAssignObject ( instance, parent, "FIRSTCHILD", &newObj );
				} else
				{
					domClassAssignObject ( instance, static_cast<VAR_OBJ *>(&lastSibling), "NEXTSIBLING", &newObj );
					domClassAssignObject ( instance, &newObj, "PREVIOUSSIBLING", static_cast<VAR_OBJ *>(&lastSibling) );
				}
				lastSibling = newObj;
			}

			*pageData = tmpTag;
		}
	}
}

void domParse ( class vmInstance *instance, VAR_OBJ *docRef, char *pageData, int64_t tagsOnly )
{
	domClassAssignLong			( instance, docRef, "NODETYPE",		9 );
	domClassAssignStringStatic  ( instance, docRef, "NODENAME",		"#document", 9 );
	domClassAssignObject		( instance, docRef, "CHILDNODES",	classNew ( instance, "NODELIST", 0 ) );
	domClassAssignObject		( instance, docRef, "IMAGES",		classNew ( instance, "HTMLCOLLECTION", 0 ) );
	domClassAssignObject		( instance, docRef, "APPLETS",		classNew ( instance, "HTMLCOLLECTION", 0 ) );
	domClassAssignObject		( instance, docRef, "LINKS",		classNew ( instance, "HTMLCOLLECTION", 0 ) );
	domClassAssignObject		( instance, docRef, "FORMS",		classNew ( instance, "HTMLCOLLECTION", 0 ) );
	domClassAssignObject		( instance, docRef, "ANCHORS",		classNew ( instance, "HTMLCOLLECTION", 0 ) );

	domParsePage ( instance, docRef, docRef, &pageData, 0, (int64_t)tagsOnly );
}