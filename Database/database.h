#pragma once

#include <stdint.h>
#include <Windows.h>

#pragma pack ( push, 1 )

#pragma pack ( pop )

class index
{
//	vmTaskInstance		*instance;
};

class database
{
	friend class databaseConnection;

	SRWLOCK	 headerLock;
	SRWLOCK	 lruLock;

};

class databaseConnection
{
	enum fieldTypes
	{
		fInteger,
		fDouble,
		fString,
		fBlob,
		fBool,
	};

	public:

	// record maniuplation
	void append( uint64_t numRecs );
	void remove (void);

	// field manipulation
	char const *getFieldString( uint32_t fieldNum );
	char const *getFieldInteger( uint32_t fieldNum );
	char const *getFieldDouble( uint32_t fieldNum );
	char const *getFieldBool( uint32_t fieldNum );
	void setField( uint32_t fieldNum, char const *data );
	void setField( uint32_t fieldNum, uint64_t data );
	void setField( uint32_t fieldNum, double data );
	void setField( uint32_t fieldNum, bool data );

	fieldTypes	getFieldType( uint32_t fieldNum );
	uint32_t	getFieldLen( uint32_t fieldNum );
	uint32_t	getFieldSize( uint32_t fieldNum );



	// record status
	bool isBOF( void );
	bool isEOF( void );

	// database status
	uint64_t	numRecs( void );

};
