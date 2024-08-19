/*

	block processor


*/
#include "compilerBCGenerator.h"

void compExecutable::makeDot ( opFunction *func, char const *fPattern )
{
	symbolStack	sym ( file );

	if ( func->codeBlock )
	{
		BUFFER buff ( 1024ull * 1024 * 16 );
		bufferPrintf ( &buff, "digraph finite_state_machine {\r\n" );
		bufferPrintf ( &buff, "rankdir=LR;\r\n" );
//		bufferPrintf ( &buff, "size=\"50,50\"\r\n" );
		func->codeBlock->makeDotNodeList( &sym, &buff );
		func->codeBlock->makeDotGraph( &sym, &buff );
		bufferPrintf ( &buff, "}\r\n" );

		char fName[256];
		sprintf_s ( fName, sizeof ( fName ), fPattern, func->name.c_str() );
		for ( auto ptr = fName; *ptr; ptr++ )
		{
			switch ( ptr[0] )
			{
				case ':':
				case '\\':
					ptr[0] = '_';
					break;
			}
		}
		FILE *file;

		fopen_s ( &file, fName, "w+" );
		if ( file )
		{
			fwrite ( bufferBuff ( &buff ), 1, bufferSize ( &buff ),  file );
			fclose ( file );
		} else
		{
			printf ( "error: unable to open \"%s\" for writing\r\n", fName );
		}
	}
}

void compExecutable::makeDot ( char const *fPattern )
{
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		makeDot ( (*it).second, fPattern );
	}
}
