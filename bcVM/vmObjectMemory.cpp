/*

	object memory header

*/

#include "bcVM.h"

GRIP::GRIP ( vmInstance *instance ) : om ( instance->om )
{
	om->collectionDisable ();
}

GRIP::GRIP ( objectMemory *om ) : om ( om )
{
	om->collectionDisable ();
}
GRIP::GRIP ( GRIP &&old ) noexcept
{
	std::swap ( om, old.om );
}
GRIP::~GRIP ()
{
	om->collectionEnable ();
}
