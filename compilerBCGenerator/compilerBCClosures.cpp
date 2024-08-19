#include "compilerBCGenerator.h"

void compExecutable::makeParentClosure( int64_t sourceIndex )
{
	bool	changed;

	// need to loop to handle nested closures
	do 
	{
		changed = false;
		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( !sourceIndex || (it->second->location.sourceIndex == sourceIndex) )
			{
				errorLocality e ( &file->errHandler, (*it).second->location );
				if ( (*it).second->parentClosure.size () )
				{
					opFunction *parent;

					parent = file->findFunc ( (*it).second->parentClosure );

					if ( parent->classDef && !(*it).second->classDef )
					{
						(*it).second->classDef = parent->classDef;
						changed = true;
					}
				}
			}
		}
	} while ( changed );
}
