/*

		Symbol space

*/

#pragma once

#include "Utility/settings.h"
#include "Utility/stringCache.h"

enum class symbolType {
	symUnknown	= 1,
	symAtom,
	symInt,
	symArray,
	symVArray,
	symDouble,
	symString,
	symVariant,
	symCodeblock,
	symObject,
	symFunc,
	symBool,				// used for casting only... 
	symWeakAtom,
	symWeakInt,
	symWeakBool,
	symWeakArray,
	symWeakVArray,
	symWeakDouble,
	symWeakString,
	symWeakObject,
	symWeakVariant,
	symWeakCodeblock,
	symWeakFunc,
};

class symbolTypeClass;

extern symbolTypeClass const symUnknownType;
extern symbolTypeClass const symAtomType;
extern symbolTypeClass const symIntType;
extern symbolTypeClass const symArrayType;
extern symbolTypeClass const symVArrayType;
extern symbolTypeClass const symDoubleType;
extern symbolTypeClass const symStringType;
extern symbolTypeClass const symVariantType;
extern symbolTypeClass const symCodeblockType;
extern symbolTypeClass const symObjectType;
extern symbolTypeClass const symFuncType;
extern symbolTypeClass const symBoolType;
extern symbolTypeClass const symWeakAtomType;
extern symbolTypeClass const symWeakIntType;
extern symbolTypeClass const symWeakBoolType;
extern symbolTypeClass const symWeakArrayType;
extern symbolTypeClass const symWeakVArrayType;
extern symbolTypeClass const symWeakDoubleType;
extern symbolTypeClass const symWeakStringType;
extern symbolTypeClass const symWeakObjectType;
extern symbolTypeClass const symWeakVariantType;
extern symbolTypeClass const symWeakCodeblockType;
extern symbolTypeClass const symWeakFuncType;

class symbolTypeClass {
	symbolType			 type;
	cacheString			 className;
	class astNode		*valueNode = nullptr;
	bool				 variantClass = false;
	bool				 potentiallyVariant = false;

public:
	~symbolTypeClass() noexcept
	{};
	symbolTypeClass	( class astNode *node );

	symbolTypeClass () noexcept
	{
		type		= symbolType::symUnknown;
		potentiallyVariant = true;
	}
	symbolTypeClass ( symbolType type ) noexcept
	{
//		assert ( type != symObject );
		if ( type == symbolType::symUnknown ) potentiallyVariant = true;
		this->type = type;
	}
	symbolTypeClass ( cacheString const &className  ) noexcept
	{
		this->type		= symbolType::symObject;
		this->className = className;
	}
	symbolTypeClass ( symbolType type, cacheString const &className  ) noexcept
	{
		assert ( className );
		this->type		= type;
		this->className	= className;
	}
	symbolTypeClass ( const symbolTypeClass &old ) noexcept
	{
		type		= old.type;
		className	= old.className;
		valueNode	= old.valueNode;
		variantClass = old.variantClass;
		potentiallyVariant = old.potentiallyVariant;
	}
	symbolTypeClass ( symbolTypeClass &&old ) noexcept
	{
		*this = std::move ( old );
	}
	symbolTypeClass &operator = ( const symbolTypeClass &old ) noexcept
	{
		type		= old.type;
		className	= old.className;
		valueNode	= old.valueNode;
		variantClass = old.variantClass;
		potentiallyVariant = old.potentiallyVariant;
		return *this;
	}
	symbolTypeClass &operator = ( symbolTypeClass &&old ) noexcept
	{
		std::swap ( type, old.type );
		std::swap ( className, old.className );
		std::swap ( valueNode, old.valueNode );
		std::swap ( variantClass, old.variantClass );
		std::swap ( potentiallyVariant, old.potentiallyVariant );
		return *this;
	}

	symbolTypeClass ( class opFile *file, char const **expr ) noexcept;
	void serialize ( BUFFER *buff );

	bool operator == ( symbolType const &type ) const
	{
		return symbolTypeClass (type).compType() == compType();
	}
	bool operator != ( symbolType const &type ) const
	{
		return type != compType();
	}
	bool operator == ( symbolTypeClass const &other ) const noexcept
	{
		if ( compType() == other.compType() )
		{
			if ( compType() == symbolType::symObject )
			{
				if ( className && other.className )
				{
					if ( !strccmp ( className, other.className ) )
					{
						return true;
					}
					return false;
				}
				return true;
			}
			return true;
		}
		return false;
	}
	bool operator != ( symbolTypeClass const &other ) const
	{
		return !(*this == other);
	}

	symbolType operator = ( symbolType type )
	{
		if ( type != symbolType::symUnknown ) potentiallyVariant = false;
		this->type = type;
		return type;
	}

	symbolType operator = ( cacheString const &className );

	bool isPotentiallyVariant ()
	{
		return potentiallyVariant;
	}
	bool makeKnown ( )
	{
		if ( type == symbolType::symUnknown )
		{
			type = symbolType::symWeakVariant;
			potentiallyVariant = false;
			return true;
		}
		return false;
	}

	symbolTypeClass weakify ( )
	{
		switch ( type )
		{
			case symbolType::symAtom:
				return symWeakAtomType;
			case symbolType::symInt:
				return symWeakIntType;
			case symbolType::symArray:
				return symWeakArrayType;
			case symbolType::symVArray:
				return symWeakVArrayType;
			case symbolType::symDouble:
				return symWeakDoubleType;
			case symbolType::symString:
				return symWeakStringType;
			case symbolType::symObject:
				return symbolTypeClass ( symbolType::symWeakObject, className );
			case symbolType::symVariant:
				return symWeakVariantType;
			case symbolType::symCodeblock:
				return symWeakCodeblockType;
			case symbolType::symFunc:
				return symWeakFuncType;
			default:
				return *this;
		}
	}

	void resetType ( )
	{
		switch ( type )
		{
			case symbolType::symWeakAtom:
			case symbolType::symWeakInt:
			case symbolType::symWeakArray:
			case symbolType::symWeakVArray:
			case symbolType::symWeakDouble:
			case symbolType::symWeakString:
			case symbolType::symWeakObject:
			case symbolType::symWeakVariant:
			case symbolType::symWeakCodeblock:
			case symbolType::symWeakFunc:
				type = symbolType::symUnknown;
				potentiallyVariant = false;
				variantClass = true;
				break;
			default:
				break;
		}
	}

	void forceVariant ()
	{
		switch ( type )
		{
			case symbolType::symWeakAtom:
			case symbolType::symWeakInt:
			case symbolType::symWeakArray:
			case symbolType::symWeakVArray:
			case symbolType::symWeakDouble:
			case symbolType::symWeakString:
			case symbolType::symWeakObject:
			case symbolType::symWeakVariant:
			case symbolType::symWeakCodeblock:
			case symbolType::symWeakFunc:
			case symbolType::symUnknown:
				type = symbolType::symWeakVariant;
				potentiallyVariant = false;
				variantClass = true;
				break;
			default:
				break;
		}
	}

	void forceStrongVariant ()
	{
		switch ( type )
		{
			case symbolType::symWeakAtom:
			case symbolType::symWeakInt:
			case symbolType::symWeakArray:
			case symbolType::symWeakVArray:
			case symbolType::symWeakDouble:
			case symbolType::symWeakString:
			case symbolType::symWeakObject:
			case symbolType::symWeakVariant:
			case symbolType::symWeakCodeblock:
			case symbolType::symWeakFunc:
			case symbolType::symUnknown:
				type = symbolType::symVariant;
				potentiallyVariant = false;
				variantClass = true;
				break;
			default:
				break;
		}
	}
	bool isStronglyTyped () const noexcept
	{
		switch ( type )
		{
			case symbolType::symAtom:
			case symbolType::symInt:
			case symbolType::symArray:
			case symbolType::symVArray:
			case symbolType::symDouble:
			case symbolType::symString:
			case symbolType::symObject:
			case symbolType::symVariant:
			case symbolType::symCodeblock:
			case symbolType::symFunc:
			case symbolType::symBool:
				return true;
			default:
				return false;
		}
	}

	bool operator &= ( symbolTypeClass const &newType );
	bool operator ^= ( symbolTypeClass const &newType );

	bool isAnObject ( void ) const noexcept
	{
		switch ( type )
		{
			// these are things that MAY be objects
			case symbolType::symObject:
			case symbolType::symWeakObject:
			case symbolType::symWeakVariant:
			case symbolType::symVariant:
				return true;
			default:
				return false;
		}
	}
	bool hasClass ( void ) const noexcept
	{
		switch ( type )
		{
			case symbolType::symWeakObject:
			case symbolType::symObject:
				if ( !className || variantClass )
				{
					return false;
				}
				if ( className ) return true;
				return false;
			default:
				return false;
		}
	}
	cacheString getClass ( void ) const
	{
		switch ( type )
		{
			case symbolType::symWeakObject:
			case symbolType::symObject:
				if ( variantClass )
				{
					throw errorNum::scINTERNAL;
				}
				if ( className ) return className;
				throw errorNum::scINTERNAL;
			default:
				throw errorNum::scINTERNAL;
		}
	}
	void setClass ( cacheString const &className )
	{
		if ( this->type != symbolType::symObject ) throw errorNum::scINTERNAL;
		if ( variantClass )
		{
			// multiple class types
			return;
		}
		this->className = className;
	}
	operator symbolType () const noexcept
	{
		return type;
	}
	operator char const *() const
	{
		switch ( type )
		{
			case symbolType::symUnknown:
				return "unknown";
			case symbolType::symInt:
				return "integer";
			case symbolType::symArray:
				return "array";
			case symbolType::symVArray:
				return "varray";
			case symbolType::symDouble:
				return "double";
			case symbolType::symString:
				return "string";
			case symbolType::symVariant:
				return "variant";
			case symbolType::symCodeblock:
				return "codeBlock";
			case symbolType::symObject:
				if ( className && !variantClass )
				{
					return className;
				}
				return "object";
			case symbolType::symFunc:
				return "func";
			case symbolType::symWeakInt:
				return "weakInteger";
			case symbolType::symWeakArray:
				return "weakArray";
			case symbolType::symWeakVArray:
				return "weakvArray";
			case symbolType::symWeakDouble:
				return "weakDouble";
			case symbolType::symWeakString:
				return "weakString";
			case symbolType::symWeakObject:
				if ( className && !variantClass )
				{
					return className;
				}
				return "weakObject";
			case symbolType::symWeakVariant:
				return "weakVariant";
			case symbolType::symWeakCodeblock:
				return "weakCodeBlock";
			case symbolType::symWeakFunc:
				return "weakFunc";
			case symbolType::symWeakAtom:
				return "weakAtom";
			case symbolType::symAtom:
				return "atom";
			default:
				throw errorNum::scINTERNAL;
		}
	}
	operator stringi() const
	{
		switch ( type )
		{

			case symbolType::symUnknown:
				return "unknown";
			case symbolType::symInt:
			case symbolType::symWeakInt:
				return "integer";
			case symbolType::symArray:
			case symbolType::symWeakArray:
				return "array";
			case symbolType::symVArray:
			case symbolType::symWeakVArray:
				return "varray";
			case symbolType::symDouble:
			case symbolType::symWeakDouble:
				return "double";
			case symbolType::symString:
			case symbolType::symWeakString:
				return "string";
			case symbolType::symVariant:
			case symbolType::symWeakVariant:
				return "variant";
			case symbolType::symCodeblock:
			case symbolType::symWeakCodeblock:
				return "codeBlock";
			case symbolType::symWeakObject:
			case symbolType::symObject:
				if ( className && !variantClass )
				{
					return className;
				}
				return "object";
			case symbolType::symFunc:
			case symbolType::symWeakFunc:
				return "func";
			case symbolType::symAtom:
			case symbolType::symWeakAtom:
				return "atom";
			default:
				throw errorNum::scINTERNAL;
		}
	}
	symbolType compType ( void ) const noexcept
	{
		switch ( type )
		{
			case symbolType::symUnknown:
				return symbolType::symUnknown;
			case symbolType::symAtom:
			case symbolType::symWeakAtom:
				return symbolType::symAtom;
			case symbolType::symInt:
			case symbolType::symWeakInt:
				return symbolType::symInt;
			case symbolType::symVArray:
			case symbolType::symWeakVArray:
				return symbolType::symVArray;
			case symbolType::symArray:
			case symbolType::symWeakArray:
				return symbolType::symArray;
			case symbolType::symDouble:
			case symbolType::symWeakDouble:
				return symbolType::symDouble;
			case symbolType::symString:
			case symbolType::symWeakString:
				return symbolType::symString;
			case symbolType::symVariant:
			case symbolType::symWeakVariant:
				return symbolType::symVariant;
			case symbolType::symCodeblock:
			case symbolType::symWeakCodeblock:
				return symbolType::symCodeblock;
			case symbolType::symObject:
			case symbolType::symWeakObject:
				return symbolType::symObject;
			case symbolType::symFunc:
			case symbolType::symWeakFunc:
				return symbolType::symFunc;
			case symbolType::symBool:
			case symbolType::symWeakBool:
				return symbolType::symBool;
			default:
				assert ( false );
				return symbolType::symUnknown;
		}
	}

	bool needConversion ( symbolTypeClass const &needType ) const noexcept
	{
		if ( needType.compType() == symbolType::symVariant ) return false;
		if ( needType == *this ) return false;		// same type, nothign to do
		if ( (needType.compType() == symbolType::symInt) && (compType() == symbolType::symBool) ) return false;
		if ( (needType.compType ( ) == symbolType::symBool) && (compType ( ) == symbolType::symInt) ) return false;
		return true;
	}
	bool convPossible ( symbolTypeClass const &needType ) const noexcept
	{
		if ( needType.compType() == symbolType::symVariant ) return true;		// nothing to do here as variant is all encompasing
		if ( compType() == symbolType::symVariant ) return true;
		if ( needType == *this ) return true;		// same type, nothign to do
		switch ( needType.compType() )
		{
			case symbolType::symObject:
				if ( needType.variantClass )
				{
					return true;
				}
				if ( !needType.className )
				{
					return true;
				}
				if ( needType.className && className )
				{
					return strccmp ( needType.className, className ) ? false : true;
				}
				return false;
				break;
			case symbolType::symAtom:
				switch ( compType ( ) )
				{
					case symbolType::symString:
					case symbolType::symAtom:
						return true;
					default:
						return false;
				}
				break;
			case symbolType::symBool:
				switch ( compType() )
				{
					case symbolType::symInt:
					case symbolType::symString:
					case symbolType::symDouble:
					case symbolType::symVariant:
					case symbolType::symArray:
					case symbolType::symVArray:
					case symbolType::symAtom:
					case symbolType::symCodeblock:
					case symbolType::symFunc:
					case symbolType::symObject:
						return true;
					default:
						break;
				}
				return false;
			case symbolType::symInt:
			case symbolType::symString:
			case symbolType::symDouble:
				switch ( compType() )
				{
					case symbolType::symInt:
					case symbolType::symString:
					case symbolType::symDouble:
					case symbolType::symVariant:
					case symbolType::symBool:			// internally these are int's
						// can always convert between long, int and double types
						return true;
					default:
						break;
				}
				return false;
			case symbolType::symVariant:
				return true;
			default:
				if ( compType ( ) == symbolType::symVariant ) return true;
				return false;
		}
	}
};

