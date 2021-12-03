#include "odbf.h"

bool _tbEof( TABLE * dbf, TABLE_CONNECTION *tblConn )
{
	return tblConn->eof ? true : false;
}
