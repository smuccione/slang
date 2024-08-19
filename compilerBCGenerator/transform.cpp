/*

	Peeophole optimizer
*/

#include "compilerBCGenerator.h"
#include "transform.h"

struct {
	char const	*match;
	char const *res;
} pats[] = {
	{ "0 - exp",		"-exp"	},
	{ "990 + i1",		"i1+990" },				// ensure that all the constant operands are on the left branch to make handling 1 + x + 2 conversion into 3 + x easier
	{ "990 * i1",		"i1*990" },				// same reason as above case
	{ "i1 = i1",		"i1"	},
	{ "0 + i1",			"i1"	},				// here we can use exp... 0 isn't an object
	{ "i1 - 0",			"i1"	},
	{ "i1 + 0",			"i1"	},				// can't use exp here... may be an overloaded object
	{ "i1 * 0",			"0"		},
	{ "0 * exp",		"0"		},
	{ "i1 / 1",			"i1"	},
	{ "i1 * 1",			"i1"	},
	{ "i1 = i1 + 1",	"++i1"	},
	{ "i1 = 1 + i1",	"++i1"	},
	{ "i1 = i1 - 1",	"--i1"	},
	{ "i1 = 1 - i1",	"--i1"	},
	{ "i1 += 0",		"i1"	},
	{ "i1 -= 0",		"i1"	},
	{ "i1 += 1",		"++i1"	},
	{ "i1 -= 1",		"--i1"	},
	{ "i1 += exp",		"i1 = i1 + exp"	},				//  !!!! why???  because if not this will generate a VARIANT storeLocalAdd call...  we don't have integer specific versions
	{ "i1 -= exp",		"i1 = i1 - exp"	},
	{ "i1 *= exp",		"i1 = i1 * exp"	},
	{ "i1 /= exp",		"i1 = i1 / exp"	},
	{ "i1 *= 1",		"i1"	},
	{ "i1 /= 1",		"i1"	},
	{ "i1 &= 0",		"0"		},
	{ "i1 |= 0",		"i1"	},
	{ "i1 <<= 0",		"i1"	},
	{ "i1 >>= 0",		"i1"	},
	{ "990 + d1",		"d1+990" },				// ensure that all the constant operands are on the left branch to make handling 1 + x + 2 conversion into 3 + x easier
	{ "990 * d1",		"d1*990" },				// same reason as above case
	{ "d1 = d1",		"d1" },
	{ "0 + d1",			"d1" },				// here we can use exp... 0 isn't an object
	{ "d1 - 0",			"d1" },
	{ "d1 + 0",			"d1" },				// can't use exp here... may be an overloaded object
	{ "d1 * 0",			"0" },
	{ "d1 / 1",			"d1" },
	{ "d1 * 1",			"d1" },
	{ "d1 = d1 + 1",	"++d1" },
	{ "d1 = 1 + d1",	"++d1" },
	{ "d1 = d1 - 1",	"--d1" },
	{ "d1 = 1 - d1",	"--d1" },
	{ "d1 += 0",		"d1" },
	{ "d1 -= 0",		"d1" },
	{ "d1 += 1",		"++d1" },
	{ "d1 -= 1",		"--d1" },
	{ "d1 += exp",		"d1 = d1 + exp" },				//  !!!! why???
	{ "d1 -= exp",		"d1 = d1 - exp" },
	{ "d1 *= exp",		"d1 = d1 * exp" },
	{ "d1 /= exp",		"d1 = d1 / exp" },
	{ "d1 *= 1",		"d1" },
	{ "d1 /= 1",		"d1" },
	{ "'' + ''",		"''"	},
	{ "'' + s1",		"s1"	},
	{ "s1 + ''",		"s1"	},
	{ "0 && exp",		"0"		},
	{ "1 || exp",		"1"		},
	{ "i1 == 0",		"!i1"		},
	{ "iexp == 0",		"!exp"		},
	{ "0 == exp",		"!exp"		},
	{ 0, 0 }
};

std::vector<phPattern *>	patterns;

phPattern::phPattern ( char const * match, char const *tran )
{
	opFile			file;
	source src ( &file.srcFiles, file.sCache, "(INTERNAL)", match );
	source src2 ( &file.srcFiles, file.sCache, "(INTERNAL)", tran );

	if ( tran )
	{
		pat = file.parseExpr ( src, false, true, nullptr, false, false );
		res = file.parseExpr ( src2, false, true, nullptr, false, false );
	} else
	{
		res = 0;
	}
}

phPattern::~phPattern()
{
	if ( res )
	{
		delete res;
	}
	delete pat;
}

phPatterns::phPatterns()
{
	int loop;

	if ( patterns.size() ) return;
	for ( loop = 0; pats[loop].match; loop++ )
	{
		patterns.push_back ( new phPattern ( pats[loop].match, pats[loop].res ) );
	}
}

phPatterns::~phPatterns ()
{
	std::vector<phPattern *>::size_type	loop;

	for ( loop = 0; loop < patterns.size(); loop++ )
	{
		delete patterns[loop];
	}
}

void opTransform::deleteNode ( astNode *node )
{
	if ( expUsed && (node == exp) )
	{
		return;
	} 

	if ( node->left )	deleteNode ( node->left );
	if ( node->right )	deleteNode ( node->right );
	node->left	= 0;
	node->right	= 0;
	delete node;
}

bool opTransform::phCompare ( astNode *pat, astNode *match, symbolStack *sym )
{
	if ( pat->getOp() == match->getOp() )
	{
		switch ( pat->getOp() )
		{
			case astOp::symbolValue:
				if (  pat->symbolValue() ==  i1Value  )
				{
					switch ( sym->getType ( match->symbolValue(), true ) )
					{
						case symbolType::symWeakInt:
						case symbolType::symInt:
							if ( i1 != match->symbolValue() )
							{
								return false;
							}
							i1 = match->symbolValue();
							break;
						default:
							return false;
					}
				} 
				if (  pat->symbolValue() ==  d1Value  )
				{
					switch ( sym->getType ( match->symbolValue(), true ) )
					{
						case symbolType::symWeakDouble:
						case symbolType::symDouble:
							if ( d1 != match->symbolValue() )
							{
								return false;
							}
							d1 = match->symbolValue();
							break;
						default:
							return false;
					}
				}
				if (  pat->symbolValue() ==  s1Value  )
				{
					switch ( sym->getType ( match->symbolValue(), true ) )
					{
						case symbolType::symWeakString:
						case symbolType::symString:
							if ( s1 != match->symbolValue() )
							{
								return false;
							}
							s1 = match->symbolValue();
							break;
						default:
							return false;
					}
				}
				if ( pat->symbolValue() == iExpValue  )
				{
					switch ( match->getType ( sym ) )
					{
						case symbolType::symWeakInt:
						case symbolType::symInt:
							if ( expValid )
							{
								return false;
							}
							expValid = true;
							exp = match;
							return true;
						default:
							break;
					}
				}
				if (  pat->symbolValue() == expValue )
				{
					if ( expValid )
					{
						return false;
					}
					expValid = true;
					exp = match;
					return true;
				}
				break;
			case astOp::stringValue:
				if (  pat->stringValue() ==  match->stringValue()  )
				{
					return true;
				}
				return false;
			case astOp::intValue:
				switch ( pat->intValue() )
				{
					case 990:
						if ( iValue1Valid && ( iValue1 != match->intValue() ) )
						{
							return false;
						}
						iValue1 = match->intValue();
						iValue1Valid = true;
						break;
					default:
						if ( pat->intValue() != match->intValue() )
						{
							return false;
						}
						break;
				}
				break;
			case astOp::doubleValue:
				if ( pat->doubleValue() == 990 )
				{
					if ( dValue1Valid && (dValue1 != match->doubleValue()) )
					{
						return false;
					}
					dValue1 = match->doubleValue();
					dValue1Valid = true;
				} else
				{
					if ( pat->doubleValue() != match->doubleValue() )
					{
						return false;
					}
				}
				break;
			default:
				break;
		}
	} else
	{
		switch ( pat->getOp() )
		{
			case astOp::symbolValue:
				if (  !expValid && pat->symbolValue() ==  expValue  )
				{
					exp = match;
					expValid = true;
					return true;
				}
				if ( !expValid && pat->symbolValue() == iExpValue )
				{
					switch ( match->getType ( sym ) )
					{
						case symbolType::symWeakInt:
						case symbolType::symInt:
							expValid = true;
							exp = match;
							return true;
						default:
							break;
					}
				}
				break;
			default:
				return false;
		}
	}
	if ( pat->left )
	{
		if ( match->left )
		{
			if ( !phCompare ( pat->left, match->left, sym ) )
			{
				return false;
			}
		} else
		{
			return false;
		}
	} else if ( match->left )
	{
		return false;
	}
		
	if ( pat->right )
	{
		if ( match->right )
		{
			if ( !phCompare ( pat->right, match->right, sym ) )
			{
				return false;
			}
		} else
		{
			return false;
		}
	} else if ( match->right )
	{
		return false;
	}
	return true;
}

astNode *opTransform::phGen ( astNode *pat )
{
	astNode	*newNode;

	switch ( pat->getOp() )
	{
		case astOp::symbolValue:
			if ( expValid &&  pat->symbolValue() ==  expValue  )
			{
				return ( exp );
			}
			newNode = new astNode ( *pat );
			if ( i1.size() &&  pat->symbolValue() ==  i1Value )
			{
				newNode->symbolValue() = i1;
			} else if ( d1.size ( ) &&  pat->symbolValue() == d1Value )
			{
				newNode->symbolValue() = d1;
			} else if ( s1.size() &&  pat->symbolValue() == s1Value )
			{
				newNode->symbolValue() = s1;
			}
			break;
		case astOp::intValue:
			newNode = new astNode ( *pat );
			if ( iValue1Valid  )
			{
				newNode->intValue() = iValue1;
			}
			break;
		case astOp::doubleValue:
			newNode = new astNode ( *pat );
			if ( dValue1Valid )
			{
				newNode->doubleValue() = dValue1;
			}
			break;
		case astOp::stringValue:
			newNode = new astNode ( *pat );
			if ( sValue1Valid )
			{
				newNode->stringValue() = sValue1;
			}
			break;
		default:
			newNode = new astNode ( *pat );
			break;
	}

	if ( pat->right )
	{
		newNode->right = phGen ( pat->right );
	}
	if ( pat->left )
	{
		newNode->left = phGen ( pat->left );
	}
	return newNode;
}

astNode	*opTransform::phScan ( astNode *node, symbolStack *sym )
{
	std::vector<phPattern *>::size_type	loop;

	for ( loop = 0; loop < patterns.size(); loop++ )
	{
		i1 = cacheString ();
		d1 = cacheString ();
		s1 = cacheString ();
		iValue1Valid = false;
		dValue1Valid = false;
		sValue1Valid = false;
		expValid = false;
		if ( phCompare ( patterns[loop]->pat, node, sym ) )
		{
			if ( patterns[loop]->res )
			{
				return phGen ( patterns[loop]->res );
			} else
			{
				return (new astNode ( file->sCache, astOp::dummy ))->setLocation ( node );
			}
		}
	}
	return (astNode *)nullptr;
}

astNode *opTransform::doTransform ( astNode *pat )
{
	if ( !pat )
	{
		return nullptr;
	}

	auto newNode = new astNode ( *pat );

	switch ( pat->getOp() )
	{
		case astOp::symbolValue:
			if ( i1 && pat->symbolValue() == i1Value )
			{
				newNode->symbolValue() = i1;
			} else if ( d1 && pat->symbolValue() == d1Value )
			{
				newNode->symbolValue() = d1;
			} else if ( s1 && pat->symbolValue() == s1Value )
			{
				newNode->symbolValue() = s1;
			} else if ( expValid && pat->symbolValue() == expValue )
			{
				// don't need newNode... returning the old expression instead
				delete newNode;
				expUsed = true;
				return exp;
			} else
			{
				newNode->symbolValue() = pat->symbolValue();
			}
			break;
		case astOp::stringValue:
			if ( sValue1Valid && !memcmp ( pat->stringValue().c_str(), "990", 4 ) )
			{
				pat->stringValue() = pat->stringValue();
			}
			break;
		case astOp::atomValue:
		case astOp::fieldValue:
			newNode->symbolValue() = pat->symbolValue();
			break;
		case astOp::intValue:
			if ( iValue1Valid && (pat->intValue() == 990 ) )
			{
				newNode->intValue() = iValue1;
			}
			break;
		default:
			break;
	}

	newNode->left	= doTransform ( pat->left );
	newNode->right	= doTransform ( pat->right );

	return newNode;
}

astNode	*opTransform::transform ( astNode *node, symbolStack *sym, bool *transformMade  )
{
	astNode	*newNode;
	size_t	loop;
	
	if (!node) return node;
	if ( node->left ) node->left = transform ( node->left, sym, transformMade );
	if ( node->right ) node->right = transform ( node->right, sym, transformMade );

	// now go through the patterns and see if we match any
	for ( loop = 0; loop < patterns.size(); loop++ )
	{
		// reset our save slots
		i1 = cacheString ();
		d1 = cacheString ();
		s1 = cacheString ();
		sValue1Valid = false;
		iValue1Valid = false;
		dValue1Valid = false;
		expValid	 = false;
		expUsed		 = false;

		// do the comparison
		if ( phCompare ( patterns[loop]->pat, node, sym ) )
		{
			newNode = doTransform ( patterns[loop]->res );
			deleteNode ( node );
			return newNode;
		}
	}
	return node;
}

opTransform::opTransform ( class opFile *file ) : file ( file )
{
	i1Value = file->sCache.get ( "i1" );
	d1Value = file->sCache.get ( "d1" );
	s1Value = file->sCache.get ( "s1" );
	expValue = file->sCache.get ( "exp" );
	iExpValue = file->sCache.get ( "iExp" );
}
