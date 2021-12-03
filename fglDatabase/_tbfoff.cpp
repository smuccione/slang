#include "odbf.h"

size_t _tbFieldOff( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count )
{
   if( count > 0 && count <= dbf->fcount)
   {
	  return dbf->fields[count - 1].offset + sizeof ( DBF_REC_HEADER );
   }
   return 0;
}

