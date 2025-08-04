/*

	Symbol space

*/

#pragma once

#include <vector>
#include <string>

#include "Utility/sourceFile.h"

/*

Symbol space

*/

#pragma once

#include <vector>
#include <string>
#include <cassert>
#include <unordered_set>
#include "compilerAST/astNode.h"
#include "symbolSpace.h"
#include "Utility/sourceFile.h"

class symbolParamDef {
	public:
	cacheString							 name;
	cacheString							 displayName;
	stringi								 documentation;
	srcLocation							 location;
	class astNode						*initializer = 0;
	symbolTypeClass						 type = symUnknownType;
	symbolTypeClass						 localType = symUnknownType;
	bool								 accessed = false;
	bool								 assigned = false;			// note: we can't use type change to denote assignment... we may never know type and have to default to variant so it may alwasy be unknown
	bool								 closedOver = false;

	std::mutex							 accCtrl;

	public:
	symbolParamDef ( cacheString const &name, symbolTypeClass const &type, srcLocation const &location, class astNode *init, stringi const &documentation ) : name ( name ), documentation ( documentation ), location ( location ), initializer ( init ), type ( type ), localType ( type )
	{
	}

	symbolParamDef ( const symbolParamDef &old )
	{
		if ( old.initializer )
		{
			initializer = new astNode ( *old.initializer );
		}
		name = old.name;
		displayName = old.displayName;
		documentation = old.documentation;
		location = old.location;
		accessed = old.accessed;
		assigned = old.assigned;
		closedOver = old.closedOver;
		type = old.type;
		localType = old.localType;
	}
	symbolParamDef ( symbolParamDef &&old ) noexcept
	{
		*this = std::move ( old );
	}
	symbolParamDef &operator = ( const symbolParamDef &old )
	{
		if ( old.initializer )
		{
			initializer = new astNode ( *old.initializer );
		}
		name = old.name;
		displayName = old.displayName;
		documentation = old.documentation;
		accessed = old.accessed;
		assigned = old.assigned;
		closedOver = old.closedOver;
		location = old.location;
		type = old.type;
		localType = old.localType;
		return *this;
	}
	symbolParamDef &operator = ( symbolParamDef &&old ) noexcept
	{
		std::swap ( initializer, old.initializer );
		std::swap ( accessed, accessed );
		std::swap ( assigned, old.assigned );
		std::swap ( closedOver, old.closedOver );
		std::swap ( name, old.name );
		std::swap ( displayName, old.displayName );
		std::swap ( location, old.location );
		std::swap ( documentation, old.documentation );
		std::swap ( type, old.type );
		std::swap ( localType, old.localType );
		return *this;
	}

	symbolParamDef ( opFile *file, class sourceFile *srcFile, char const **expr );

	void serialize ( BUFFER *buff )
	{
		name.serialize ( buff );
		displayName.serialize ( buff );
		documentation.serialize ( buff );
		location.serialize ( buff );
		type.serialize ( buff );
		localType.serialize ( buff );
		initializer->serialize ( buff );
	}

	public:
	~symbolParamDef ( )
	{
		if ( initializer ) delete initializer;
	}
	cacheString getName ( ) const
	{
		return name;
	}
	symbolTypeClass const &getType ( ) const
	{
		return localType;
	}
	stringi const &getDocumentation () const
	{
		return documentation;
	}
	srcLocation const &getLocation () const
	{
		return location;
	}

	// this is called from WITHIN the function body... it changes the LOCAL value of the type wich MAY be different than the parameter types.
	bool setType ( symbolTypeClass const &newType, accessorType const &acc, unique_queue<accessorType> *scanQueue )
	{
		std::lock_guard g1 ( accCtrl );
		assigned = true;
		if ( localType == symbolType::symUnknown )
		{
			return type = newType;
		}
		if ( localType &= newType )
		{
			if ( scanQueue ) scanQueue->push ( acc );
			return true;
		}
		return false;
	}

	// setting the parameter type must change both the parameter type and the local type from which derives from it
	void setInitType ( symbolTypeClass const &newType, accessorType const &acc, unique_queue<accessorType> *scanQueue )
	{
		std::lock_guard g1 ( accCtrl );
		type &= newType;
		localType &= newType;
	}

	// setting the parameter type must change both the parameter type and the local type from which derives from it
	void setInitTypeNoThrow ( symbolTypeClass const &newType, accessorType const &acc, unique_queue<accessorType> *scanQueue )
	{
		std::lock_guard g1 ( accCtrl );

		type ^= newType;
		localType ^= newType;
	}
	void setAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc )
	{
		accessed = true;
	}
	bool isAccessed () const
	{
		return accessed;
	}
	bool isAssigned() const
	{
		return assigned;
	}
	bool isClosedOver () const
	{
		return closedOver;
	}
	friend class symbolSpaceParams;
};

class symbolSpaceParams : public symbol
{
public:
	std::vector<symbolParamDef>	 symbols;
public:
	symbolSpaceParams ( bool reverseIndex = true ) : symbol ( symbolSpaceType::symTypeParam )
	{
	}
	~symbolSpaceParams ( )
	{
	}
	symbolSpaceParams ( class opFile *file, class sourceFile *srcFile, char const ** expr ) : symbol ( symbolSpaceType::symTypeParam )
	{
		addStream ( file, srcFile, expr );
	}
	symbolSpaceParams ( const symbolSpaceParams &old ) : symbol ( symbolSpaceType::symTypeParam )
	{
		for ( auto &it : old.symbols )
		{
			symbols.push_back ( symbolParamDef ( it ) );
		}
	}
	symbolSpaceParams ( symbolSpaceParams &&old ) noexcept : symbol ( symbolSpaceType::symTypeParam )
	{
		std::swap ( symbols, old.symbols );
	}
	symbolSpaceParams & operator = ( const symbolSpaceParams &old )
	{
		for ( auto& it : old.symbols )
		{
			symbols.push_back ( symbolParamDef ( it ) );
		}
		return *this;
	}
	symbolSpaceParams & operator = ( symbolSpaceParams &&old ) noexcept
	{
		std::swap ( symbols, old.symbols );
		return *this;
	}

	void clear ( )
	{
		symbols.clear ( );
	}

	void addStream ( opFile *file, class sourceFile *srcFile, char const ** expr )
	{
		uint32_t	cnt;

		cnt = *((uint32_t *) *expr);
		(*expr) += sizeof ( uint32_t );
		for ( ; cnt; cnt-- )
		{
			symbols.push_back ( symbolParamDef ( file, srcFile, expr ) );
		}
	}

	bool setType ( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		if ( (*this)[name] )
		{
			return (*this)[name]->setType ( type, acc, scanQueue );
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}

	void setInitType ( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		if ( (*this)[name] )
		{
			return (*this)[name]->setInitType ( type, acc, scanQueue );
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}

	void setType ( cacheString const &name, bool isAccess, cacheString const &className ) override
	{
		if ( (*this)[name] )
		{
			(*this)[name]->type.setClass ( className );
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}

	void add ( cacheString const &name, symbolTypeClass const &type, srcLocation const &location, astNode *init, bool front, stringi const &documentation )
	{
		assert ( init );
		if ( (*this)[name] ) throw errorNum::scDUPLICATE_SYMBOL;

		if ( front )
		{
			symbols.insert ( symbols.begin ( ), symbolParamDef ( name, type, location, init, documentation ) );
		} else
		{
			symbols.push_back (  symbolParamDef ( name, type, location, init, documentation ) );
		}
	}
	void add ( astNode *node, symbolTypeClass const &type, stringi const &documentation, bool front = false )
	{
		if ( !node ) throw errorNum::scBAD_VARIABLE_DEF;
		switch ( node->getOp() )
		{
			case astOp::symbolValue:
				add ( node->symbolValue(), type, node->location, node, front, documentation );
				break;
			case astOp::assign:
				if ( node->left->getOp() != astOp::symbolValue )
				{
					throw errorNum::scBAD_VARIABLE_DEF;
				}
				add ( node->left->symbolValue(), type, node->left->location, node, front, documentation );
				break;
			default:
				throw errorNum::scBAD_VARIABLE_DEF;
		}
	}

	// symbol query and setting
	symbolParamDef *operator [] ( cacheString const & name )
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return &it;
			}
		}
		return 0;
	}

	symbolParamDef *operator [] ( size_t index )
	{
		return &symbols[index];
	}

	bool getFgxSymbol ( visibleSymbols &fgxSym, size_t index ) const override
	{
		fgxSym.isStackBased = true;
		fgxSym.noRef = false;
		fgxSym.name = symbols[index].name;

		return true;
	}

	bool isDefined ( cacheString const &name, bool isAccess ) const override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return true;
			}
		}
		return false;
	}
	bool isDefinedNS ( cacheString const &name, bool isAccess ) const override
	{
		return isDefined ( name, isAccess );
	}
	void setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return it.setAccessed ( acc, scanQueue, loc );
			}
		}
		assert ( 0 );
	}
	void setAllLocalAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override
	{
		for ( auto &it : symbols )
		{
			it.setAccessed ( acc, scanQueue, loc );
		}
		return;
	}
	void setClosedOver ( cacheString const &name ) override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				it.closedOver = true;
				return;
			}
		}
	}
	bool isAccessed ( cacheString const &name, bool isAccess ) const override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return it.accessed;
			}
		}
		assert ( 0 );
		return false;
	}
	bool					 isClosedOver ( cacheString const &name ) const override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return it.closedOver;
			}
		}
		assert ( 0 );
		return false;
	}
	bool					 isAssigned ( cacheString const &name, bool isAccess ) const override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return it.assigned;
			}
		}
		assert ( 0 );
		return false;
	}
	// TODO: make getType a reference for speed.... silly to copy
	symbolTypeClass	const	 getType ( cacheString const &name, bool isAccess )  const override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return it.getType();
			}
		}
		assert ( 0 );
		return symUnknownType;
	}
	int32_t					 getRollup ( void ) const  override { return (int32_t) symbols.size ( ); };
	bool					 isStackBased ( void ) const  override { return true; };

	cacheString const getSymbolName ( symbolSpaceScope scope, uint32_t index ) const noexcept override
	{
		switch ( scope )
		{
			case symbolSpaceScope::symLocal:
			case symbolSpaceScope::symLocalParam:
				break;
			default:
				return cacheString();
		}
		if ( index >= symbols.size ( ) ) return cacheString();
		return symbols[symbols.size ( ) - index - 1].name;
	}

	symbolSpaceScope		 getScope ( cacheString const &name, bool isAccess ) const  override { return symbolSpaceScope::symLocalParam; };
	int32_t				 getIndex ( cacheString const &name, bool isAccess ) const override
	{
		int32_t index = 0;

		for ( auto &it : symbols )
		{
			if ( it.name == name )
			{
				return (int32_t) symbols.size ( ) - index - 1;
			}
			index++;
		}
		throw errorNum::scINTERNAL;
	}

	// function query


	void rename ( cacheString const &oldName, cacheString const &newName ) override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == oldName )
			{
				it.name = newName;
				it.initializer->renameLocal ( oldName, newName );
				return;
			}
		}
		throw errorNum::scINTERNAL;
	}

	void serialize ( BUFFER *buff ) override
	{
		symbol::serialize ( buff );
		bufferPut32 ( buff, (uint32_t) symbols.size ( ) );
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			it->serialize ( buff );
		}
	}

	void forceVariant ( uint32_t start = 0 )
	{
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			std::lock_guard g1 ( it->accCtrl );
			it->type.forceVariant ();
			it->localType.forceVariant ();
		}
	}

	void forceStrongVariant ( uint32_t start = 0 )
	{
		for ( auto it = symbols.begin (); it != symbols.end (); it++ )
		{
			std::lock_guard g1 ( it->accCtrl );
			it->type.forceStrongVariant ();
			it->localType.forceStrongVariant ();
		}
	}

	bool makeKnown ( )
	{
		bool ret = false;
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			std::lock_guard g1 ( it->accCtrl );
			if ( it->type == symbolType::symUnknown )
			{
				ret = true;
				it->type.makeKnown ();
				it->localType &= it->type;
			}
			if ( it->localType == symbolType::symUnknown )
			{
				ret = true;
				it->localType.makeKnown ();
			}
		}
		return ret;
	}

	size_t size ( void ) const override
	{
		return 	(uint32_t) symbols.size ( );
	}

	void invert ( void )
	{
		std::reverse ( symbols.begin ( ), symbols.end ( ) );
	}
	void pruneRefs ( void )
	{
		for ( size_t loop = 0; loop < symbols.size ( ); loop++ )
		{
			if ( symbols[loop].initializer ) symbols[loop].initializer->pruneRefs ( true );
		}
	}
	void pruneUnused ( class symbolStack* sym )
	{
		for (auto it = symbols.begin(); it != symbols.end(); )
		{
			if ( !it->accessed && !it->assigned)
			{
				if (it->initializer->hasNoSideEffects(sym, false))
				{
					it = symbols.erase(it);
				} else
				{
					it++;
				}
			} else
			{
				it++;
			}
		}
	}
	bool hasCodeblock ( void )
	{
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			if ( it->initializer && it->initializer->hasCodeblock ( ) ) return true;
		}
		return false;
	}
	bool hasFunctionReference ( class symbolStack *sym )
	{
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			if ( it->initializer && it->initializer->hasFunctionReference ( sym ) ) return true;
		}
		return false;
	}
	void resetType ( class opFile *file ) override
	{
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			switch ( it->type )
			{
				case symbolType::symWeakAtom:
				case symbolType::symWeakInt:
				case symbolType::symWeakArray:
				case symbolType::symWeakDouble:
				case symbolType::symWeakString:
				case symbolType::symWeakObject:
				case symbolType::symWeakVariant:
				case symbolType::symWeakCodeblock:
				case symbolType::symWeakFunc:
					it->type = symUnknownType;
					break;
				default:
					break;
			}
			if ( it->initializer )
			{
				it->initializer->resetType ( file );
			}
		}
	}
	bool typeInfer ( class compExecutable *compDef, opFunction *func, errorHandler *err, symbolStack *sym, bool interProcedural, bool markUsed, unique_queue<accessorType> *scanQueue, bool isLS )
	{
		for ( auto it = symbols.begin ( ); it != symbols.end ( ); it++ )
		{
			if ( it->initializer )
			{
				if (it->initializer->getOp() == astOp::assign)
				{
					it->initializer->right->typeInfer ( compDef, func, err, sym, nullptr, interProcedural, false, markUsed, scanQueue, isLS );
				} else
				{
					it->initializer->typeInfer ( compDef, func, err, sym, nullptr, interProcedural, false, markUsed, scanQueue, isLS );
				}
			}
		}
		return false;
	}
	bool markInUse ( class compExecutable *compDef, opFunction *func, errorHandler *err, symbolStack *sym, bool interProcedural, unique_queue<accessorType> *scanQueue, bool isLS )
	{
		for ( auto it = symbols.begin (); it != symbols.end (); it++ )
		{
			if ( it->initializer )
			{
				if ( it->initializer->getOp () == astOp::assign )
				{
					it->initializer->right->markInUse ( compDef, func, err, sym, interProcedural, scanQueue, isLS );
				} else
				{
					it->initializer->markInUse ( compDef, func, err, sym, interProcedural, scanQueue, isLS );
				}
			}
		}
		return false;
	}

	symbolSource getSymbolSource ( cacheString const &className ) override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == className )
			{
				return &it;
			}
		}
		return std::monostate ();
	}
	srcLocation const &getDefinitionLocation ( cacheString const &className, bool isAccess ) const override
	{
		for ( auto &it : symbols )
		{
			if ( it.name == className )
			{
				return it.location;
			}
		}
		throw errorNum::scINTERNAL;
	}

};
