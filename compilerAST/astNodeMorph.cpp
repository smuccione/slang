/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

void astNode::morphAssignment ( opFile *file )
{
	astNode	*snd = 0;

	try {
		snd = new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, file->selfValue ), left );
		left = 0;

		if ( snd->right->getOp() == astOp::symbolValue )
		{
			snd->right->forceOp ( astOp::nameValue );
		}
		
		auto newRight = pList().param[0];
		pList().param.clear ( );
		pList ().paramRegion.clear ();

		release ( );
		setOp ( file->sCache, astOp::assign );
		this->location = srcLocation ();
		right = newRight;
		left = snd;	
	} catch ( ... )
	{
		if ( snd ) delete snd;
		throw;
	}
}

void astNode::morphNew ( opFile *file )
{
	if ( op != astOp::funcCall )
	{
		throw errorNum::scINTERNAL;
	}

	if ( (left->getOp() != astOp::symbolValue) && (left->getOp() != astOp::atomValue) )
	{
		throw errorNum::scINTERNAL;
	}

	stringi	name ( left->symbolValue() );
	name = name + "::new";

	left->forceOp ( astOp::atomValue );
	left->symbolValue () = file->sCache.get ( name );
}

void astNode::morphRelease ( opFile *file )
{
	if ( op != astOp::funcCall )
	{
		throw errorNum::scINTERNAL;
	}

	if ( (left->getOp() != astOp::symbolValue) && (left->getOp() != astOp::atomValue) )
	{
		throw errorNum::scINTERNAL;
	}

	stringi	name ( left->symbolValue() );
	name = name + "::release";

	left->forceOp ( astOp::atomValue );
	left->symbolValue () = file->sCache.get ( name );
}

