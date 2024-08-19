/*
	symbol space implementation

*/

#include "fileParser.h"

void opSymbol::serialize ( BUFFER *buff )
{
	buff->write ( &symClass, sizeof ( opSymbol::symbolClass ) );
	location.serialize ( buff );
	type.serialize ( buff );
	name.serialize ( buff );
	enclosingName.serialize ( buff );
	documentation.serialize ( buff );
	initializer->serialize ( buff );
	buff->write ( &loadTimeInitializable, sizeof ( loadTimeInitializable ) );
	buff->write ( &isExportable, sizeof ( isExportable ) );
}

opSymbol::opSymbol ( class opFile *file, class sourceFile *src, char const **expr, bool isInterface ) : isInterface ( isInterface )
{
	assigned = false;

	symClass = *(opSymbol::symbolClass *) (*expr);
	(*expr) += sizeof ( opSymbol::symbolClass );

	location = srcLocation ( src, expr );

	type = symbolTypeClass ( file, expr );

	name = file->sCache.get ( expr );
	enclosingName = file->sCache.get ( expr );
	documentation = stringi ( expr );

	assert ( name.size ( ) );

	initializer = OP_NODE_DESERIALIZE ( file, src, expr );

	loadTimeInitializable = *(bool *) (*expr);
	(*expr) += sizeof ( bool );

	isExportable = *(bool *) (*expr);
	(*expr) += sizeof ( bool );

	globalIndex = 0;
}
