/*
	symbol space implementation

*/

#include "compilerParser/fileParser.h"
#include "astNode.h"
#include "symbolType.h"

symbolTypeClass const symUnknownType ( symbolType::symUnknown );
symbolTypeClass const symAtomType ( symbolType::symAtom );
symbolTypeClass const symIntType ( symbolType::symInt );
symbolTypeClass const symArrayType ( symbolType::symArray );
symbolTypeClass const symVArrayType ( symbolType::symVArray );
symbolTypeClass const symDoubleType ( symbolType::symDouble );
symbolTypeClass const symStringType ( symbolType::symString );
symbolTypeClass const symVariantType ( symbolType::symVariant );
symbolTypeClass const symCodeblockType ( symbolType::symCodeblock );
symbolTypeClass const symObjectType ( symbolType::symObject );
symbolTypeClass const symFuncType ( symbolType::symFunc );
symbolTypeClass const symBoolType ( symbolType::symBool );				// used for casting only... 
symbolTypeClass const symWeakAtomType ( symbolType::symWeakAtom );
symbolTypeClass const symWeakIntType ( symbolType::symWeakInt );
symbolTypeClass const symWeakBoolType ( symbolType::symWeakBool );
symbolTypeClass const symWeakArrayType ( symbolType::symWeakArray );
symbolTypeClass const symWeakVArrayType ( symbolType::symWeakVArray );
symbolTypeClass const symWeakDoubleType ( symbolType::symWeakDouble );
symbolTypeClass const symWeakStringType ( symbolType::symWeakString );
symbolTypeClass const symWeakObjectType ( symbolType::symWeakObject );
symbolTypeClass const symWeakVariantType ( symbolType::symWeakVariant );
symbolTypeClass const symWeakCodeblockType ( symbolType::symWeakCodeblock );
symbolTypeClass const symWeakFuncType ( symbolType::symWeakFunc );

symbolTypeClass::symbolTypeClass ( astNode *node )
{
	valueNode = node;
	className = cacheString ();

	switch ( node->getOp () )
	{
		case astOp::atomValue:
			type = symbolType::symAtom;
			break;
		case astOp::funcValue:
			type = symbolType::symFunc;
			break;
		case astOp::codeBlockExpression:
		case astOp::codeBlockValue:
			type = symbolType::symCodeblock;
			break;
		case astOp::stringValue:
			type = symbolType::symString;
			break;
		case astOp::intValue:
			type = symbolType::symInt;
			break;
		case astOp::doubleValue:
			type = symbolType::symDouble;
			break;
		case astOp::varrayValue:
			if ( node->arrayData().nodes.size () )
			{
				type = (symbolType::symVArray);
			} else
			{
				type = symbolType::symVariant;
			}
			break;
		case astOp::arrayValue:
			if ( node->arrayData().nodes.size () )
			{
				type = (symbolType::symArray);
			} else
			{
				type = symbolType::symVariant;
			}
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

symbolTypeClass::symbolTypeClass ( opFile *file, char const **expr ) noexcept
{
	// deserializer
	type = (symbolType) * *expr;
	(*expr)++;

	if ( type == symbolType::symObject )
	{
		className = file->sCache.get ( expr );
	}
}
void symbolTypeClass::serialize ( BUFFER *buff )
{
	bufferPut8 ( buff, (char) type );
	if ( type == symbolType::symObject )
	{
		className.serialize ( buff );
	}
}

symbolType symbolTypeClass::operator = ( cacheString const &className )
{
	if ( this->type == symbolType::symWeakObject )
	{
		if ( !variantClass )
		{
			if ( this->className )
			{
				if ( this->className == className )
				{
					this->className = cacheString ();			// variant object... no designated class
					this->variantClass = true;
				}
				return symbolType::symWeakObject;
			} else
			{
				this->className = className;
			}
		}
	} else
	{
		this->type = symbolType::symWeakObject;
		this->className = className;
	}
	return symbolType::symWeakObject;
}

bool symbolTypeClass::operator &= ( symbolTypeClass const &newType )
{
	if ( newType == symbolType::symUnknown && type != symbolType::symWeakVariant )
	{
		potentiallyVariant = true;
		return false;
	}

	auto self = this;
	switch ( type )
	{
		case symbolType::symWeakVariant:
		case symbolType::symVariant:
			// can never change... although weakVariant can be reset
			break;
		case symbolType::symAtom:
			if ( newType.type != symbolType::symWeakVariant && newType.type != symbolType::symVariant && newType.type != symbolType::symWeakAtom && newType.type != symbolType::symAtom )
			{
				throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case symbolType::symArray:
			if ( newType.type != symbolType::symWeakVariant && newType.type != symbolType::symVariant && newType.type != symbolType::symWeakArray && newType.type != symbolType::symArray )
			{
				throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case symbolType::symVArray:
			if ( newType.type != symbolType::symWeakVariant && newType.type != symbolType::symVariant && newType.type != symbolType::symWeakArray && newType.type != symbolType::symArray && newType.type != symbolType::symWeakVArray && newType.type != symbolType::symVArray )
			{
				throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case symbolType::symDouble:
		case symbolType::symString:
		case symbolType::symInt:
			switch ( newType )
			{
				case symbolType::symVariant:
				case symbolType::symWeakVariant:
				case symbolType::symDouble:
				case symbolType::symWeakDouble:
				case symbolType::symInt:
				case symbolType::symWeakInt:
				case symbolType::symString:
				case symbolType::symWeakString:
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case symbolType::symCodeblock:
			if ( newType.type != symbolType::symWeakVariant && newType.type != symbolType::symVariant && newType.type != symbolType::symWeakCodeblock && newType.type != symbolType::symCodeblock )
			{
				throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case symbolType::symObject:
			if ( newType == symbolType::symWeakVariant || newType == symbolType::symVariant )
			{
				break;
			}
			if ( newType.type != symbolType::symWeakObject && newType.type != symbolType::symObject )
			{
				throw errorNum::scTYPE_CONVERSION;
			}
			if ( !variantClass && className )
			{
				if ( (newType.className && className != newType.className) || newType.variantClass )
				{
					throw errorNum::scTYPE_CONVERSION;
				}
			}
			break;
		case symbolType::symFunc:
			if ( newType.type != symbolType::symWeakVariant && newType.type != symbolType::symVariant && newType.type != symbolType::symWeakFunc && newType.type != symbolType::symFunc)
			{
				throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case symbolType::symBool:
			break;
		case symbolType::symUnknown:
			*self = newType;
			switch ( type )
			{
				case symbolType::symAtom:
					type = symbolType::symWeakAtom;
					break;
				case symbolType::symInt:
					type = symbolType::symWeakInt;
					break;
				case symbolType::symArray:
					type = symbolType::symWeakArray;
					break;
				case symbolType::symVArray:
					type = symbolType::symWeakVArray;
					break;
				case symbolType::symDouble:
					type = symbolType::symWeakDouble;
					break;
				case symbolType::symString:
					type = symbolType::symWeakString;
					break;
				case symbolType::symVariant:
					type = symbolType::symWeakVariant;
					break;
				case symbolType::symCodeblock:
					type = symbolType::symWeakCodeblock;
					break;
				case symbolType::symObject:
					type = symbolType::symWeakObject;
					break;
				case symbolType::symFunc:
					type = symbolType::symWeakFunc;
					break;
				case symbolType::symBool:
					type = symbolType::symWeakBool;
					break;
				default:
					break;
			}
			return true;
		case symbolType::symWeakAtom:
			switch ( newType )
			{
				case symbolType::symWeakAtom:
				case symbolType::symAtom:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakBool:
			switch ( newType )
			{
				case symbolType::symWeakInt:
				case symbolType::symInt:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakInt:
			switch ( newType )
			{
				case symbolType::symWeakDouble:
				case symbolType::symDouble:
					// upgrade to type;
					type = symbolType::symWeakDouble;
					return true;
				case symbolType::symWeakInt:
				case symbolType::symInt:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakDouble:
			switch ( newType )
			{
				case symbolType::symWeakInt:
				case symbolType::symInt:
					//leave it as a double
				case symbolType::symDouble:
				case symbolType::symWeakDouble:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakString:
			switch ( newType )
			{
				case symbolType::symString:
				case symbolType::symWeakString:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakCodeblock:
			switch ( newType )
			{
				case symbolType::symCodeblock:
				case symbolType::symWeakCodeblock:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakFunc:
			switch ( newType )
			{
				case symbolType::symFunc:
				case symbolType::symWeakFunc:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakArray:
			switch ( newType )
			{
				case symbolType::symArray:
				case symbolType::symWeakArray:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakVArray:
			switch ( newType )
			{
				case symbolType::symVArray:
				case symbolType::symWeakVArray:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakObject:
			switch ( newType )
			{
				case symbolType::symObject:
					assert ( newType.className && !newType.variantClass );
					[[fallthrough]];
				case symbolType::symWeakObject:
					if ( variantClass )
					{
						// yes so can't become more generic, nothing changed
						return false;
					}

					if ( newType.variantClass )
					{
						// if we've gotten here then we're NOT a variant class already (or we would have returned) so we must become one
						className = cacheString ();
						variantClass = true;
						return true;
					}

					// do we not have a class name but the new one does?
					if ( !className && newType.className )
					{
						// yes, so set it and signal changed
						className = newType.className;
						return true;
					}

					// do we have a class name but the new one does not
					if ( className != newType.className )
					{
						// yes, so we become a variant class
						className = cacheString ();
						variantClass = true;
						return true;
					}
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
	}
	return false;
}

// this differs from &= by not throwing when a conversion can not be done... this is used when we don't know the specific function.  some parameters may be hard types that can't be changed
bool symbolTypeClass::operator ^= ( symbolTypeClass const &newType )
{
	if ( newType == symbolType::symUnknown )
	{
		potentiallyVariant = true;
		return false;
	}

	auto self = this;
	switch ( type )
	{
		case symbolType::symWeakVariant:
		case symbolType::symVariant:
			// can never change... although weakVariant can be reset
		case symbolType::symAtom:
		case symbolType::symInt:
		case symbolType::symArray:
		case symbolType::symVArray:
		case symbolType::symDouble:
		case symbolType::symString:
		case symbolType::symCodeblock:
		case symbolType::symObject:
		case symbolType::symFunc:
		case symbolType::symBool:
			break;
		case symbolType::symUnknown:
			*self = newType;
			switch ( type )
			{
				case symbolType::symAtom:
					type = symbolType::symWeakAtom;
					break;
				case symbolType::symInt:
					type = symbolType::symWeakInt;
					break;
				case symbolType::symArray:
					type = symbolType::symWeakArray;
					break;
				case symbolType::symVArray:
					type = symbolType::symWeakVArray;
					break;
				case symbolType::symDouble:
					type = symbolType::symWeakDouble;
					break;
				case symbolType::symString:
					type = symbolType::symWeakString;
					break;
				case symbolType::symVariant:
					type = symbolType::symWeakVariant;
					break;
				case symbolType::symCodeblock:
					type = symbolType::symWeakCodeblock;
					break;
				case symbolType::symObject:
					type = symbolType::symWeakObject;
					break;
				case symbolType::symFunc:
					type = symbolType::symWeakFunc;
					break;
				case symbolType::symBool:
					type = symbolType::symWeakBool;
					break;
				default:
					break;
			}
			return true;
		case symbolType::symWeakAtom:
			switch ( newType )
			{
				case symbolType::symWeakAtom:
				case symbolType::symAtom:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakBool:
			switch ( newType )
			{
				case symbolType::symWeakInt:
				case symbolType::symInt:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakInt:
			switch ( newType )
			{
				case symbolType::symWeakDouble:
				case symbolType::symDouble:
					// upgrade to type;
					type = symbolType::symWeakDouble;
					return true;
				case symbolType::symWeakInt:
				case symbolType::symInt:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakDouble:
			switch ( newType )
			{
				case symbolType::symWeakInt:
				case symbolType::symInt:
					//leave it as a double
				case symbolType::symDouble:
				case symbolType::symWeakDouble:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakString:
			switch ( newType )
			{
				case symbolType::symString:
				case symbolType::symWeakString:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakCodeblock:
			switch ( newType )
			{
				case symbolType::symCodeblock:
				case symbolType::symWeakCodeblock:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakFunc:
			switch ( newType )
			{
				case symbolType::symFunc:
				case symbolType::symWeakFunc:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakArray:
			switch ( newType )
			{
				case symbolType::symArray:
				case symbolType::symWeakArray:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakVArray:
			switch ( newType )
			{
				case symbolType::symVArray:
				case symbolType::symWeakVArray:
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
		case symbolType::symWeakObject:
			switch ( newType )
			{
				case symbolType::symObject:
					assert ( newType.className && !newType.variantClass );
					[[fallthrough]];
				case symbolType::symWeakObject:
					if ( variantClass )
					{
						// yes so can't become more generic, nothing changed
						return false;
					}

					if ( newType.variantClass )
					{
						// if we've gotten here then we're NOT a variant class already (or we would have returned) so we must become one
						className = cacheString ();
						variantClass = true;
						return true;
					}

					// do we not have a class name but the new one does?
					if ( !className && newType.className )
					{
						// yes, so set it and signal changed
						className = newType.className;
						return true;
					}

					// do we have a class name but the new one does not
					if ( className != newType.className )
					{
						// yes, so we become a variant class
						className = cacheString ();
						variantClass = true;
						return true;
					}
					return false;
				default:
					type = symbolType::symWeakVariant;
					return true;
			}
			break;
	}
	return false;
}
