#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <memory>
#include <vector>
#include <string>

#include <winsock2.h>
#include <windows.h>
#include <Mswsock.h>
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <rpc.h>
#include <ntdsapi.h>
#include <tchar.h>
#include <process.h>

#include "webSocket.h"
#include "webProtoH1.h"

#include "Target/vmTask.h"

#define H2_FRAME_TYPE_LIST	\
	FRAME_TYPE ( h2DATA )	\
	FRAME_TYPE ( h2HEADERS )	\
	FRAME_TYPE ( h2PRIORITY )	\
	FRAME_TYPE ( h2RST_STREAM )	\
	FRAME_TYPE ( h2SETTINGS )	\
	FRAME_TYPE ( h2PUSH_PROMISE )	\
	FRAME_TYPE ( h2PING )	\
	FRAME_TYPE ( h2GOAWAY )	\
	FRAME_TYPE ( h2WINDOW_UPDATE )	\
	FRAME_TYPE ( h2CONTINUATION )	\

#undef FRAME_TYPE
#define FRAME_TYPE(x) x,
enum H2_FRAME_TYPE
{
	H2_FRAME_TYPE_LIST
};

enum H2_ERROR
{
	h2NO_ERROR					= 0x00,
	h2PROTOCOL_ERROR			= 0x01,
	h2INTERNAL_ERROR			= 0x02,
	h2FLOW_CONTROL_ERROR		= 0x03,
	h2SETTINGS_TIMEOUT_ERROR	= 0x04,
	h2STREAM_CLOSED_ERROR		= 0x05,
	h2FRAME_SIZE_ERROR			= 0x06,
	h2REFUSED_STREAM_ERROR		= 0x07,
	h2CANCEL_ERROR				= 0x08,
	h2COMPRESSION_ERROR			= 0x09,
	h2CONNECT_ERROR				= 0x0A,
	h2ENHANCE_YOUR_CALM_ERROR	= 0x0B,
	h2INADEQUATE_SECURITY_ERROR	= 0x0C,
	h2HTTP_1_1_REQUIRED_ERROR	= 0x0D,
};


#pragma pack ( push, 1 )

struct H2IntegerValue
{
	size_t	value;
	size_t		length;
	H2IntegerValue( uint8_t *ptr, size_t bits )
	{
	//	if (  )
	}
	size_t getValue( void )
	{
		return value;
	}
	size_t getLength( void )
	{
		return length;
	}
};


struct H2Data
{
	private:
	unsigned char		pad;

	public:
	uint32_t getPad( void )
	{
		return (uint32_t)pad;
	}
	uint8_t *getData( void )
	{
		return (uint8_t *)(this + 1) + pad;
	}
};

struct H2Headers
{
	private:
	unsigned char		pad;
	unsigned char		streamDependency[4];

	public:

	uint32_t getPad( void )
	{
		return (uint32_t)pad;
	}
	uint32_t getWeight( void )
	{
		return (uint32_t)pad;
	}
	uint32_t getDependenStreamId( void )
	{
		return (uint32_t)((streamDependency[0] << 24) + (streamDependency[1] << 16) + (streamDependency[2] << 8) + streamDependency[3]) & 0x7fffffff;
	}
	uint8_t *getHeaders( uint32_t flags )
	{
		if ( !(flags & 0x08) ) pad = 0;
		return (uint8_t *)(this + 1) + pad;
	}
};

struct H2Priority
{
	private:
	unsigned char		streamId[4];
	unsigned char		weight;

	public:

	uint8_t getWeight( void )
	{
		return weight;
	}


	uint32_t getDependentStreamId( void )
	{
		return (streamId[0] << 24) + (streamId[1] << 16) + (streamId[2] << 8) + streamId[3];
	}
};

struct H2RstStream
{
	private:
	unsigned char		errorCode[4];

	public:
	uint32_t getErrorCode( void )
	{
		return (errorCode[0] << 24) + (errorCode[1] << 16) + (errorCode[2] << 8) + errorCode[3];
	}
};

enum H2_Settings_Type
{
	h2SETTINGS_HEADER_TABLE_SIZE		= 0x01,
	h2SETTINGS_ENABLE_PUSH				= 0x02,
	h2SETTINGS_MAX_CONCURRENT_STREAMS	= 0x03,
	h2SETTINGS_INITIAL_WINDOW_SIZE		= 0x04,
	h2SETTINGS_MAX_FRAME_SIZE			= 0x05,
	h2SETTINGS_MAX_HEADER_LIST_SIZE		= 0x06,
};

struct H2Settings
{
	private:
	unsigned char		identifier[2];
	unsigned char		value[4];

	public:
	H2Settings()
	{}

	H2Settings( H2_Settings_Type type, uint32_t value )
	{
		setType( type );
		setValue( value );
	}

	H2_Settings_Type	getType( void )
	{
		return (H2_Settings_Type)((identifier[0] << 8) + identifier[1]);
	}
	void setType( H2_Settings_Type type )
	{
		uint32_t val = (uint32_t)type;
		identifier[0] = (uint8_t)(val & 0xFF);
		identifier[1] = (uint8_t)((val >> 8) & 0xFF);
	}
	uint32_t getValue( void )
	{
		return (value[0] << 24) + (value[1] << 16) + (value[2] << 8) + value[3];
	}
	void setValue( uint32_t val)
	{
		value[0] = (uint8_t)(val & 0xFF);
		value[1] = (uint8_t)((val >> 8) & 0xFF);
		value[2] = (uint8_t)((val >> 16) & 0xFF);
		value[3] = (uint8_t)((val >> 24));
	}
};

struct H2PushPromise
{
	private:
	unsigned char		pad;
	unsigned char		promiseStreamId[4];

	public:
	uint32_t getPadding( void )
	{
		return (uint32_t)pad;
	}
	uint32_t getpromiseStreamId( void )
	{
		return (promiseStreamId[0] << 24) + (promiseStreamId[1] << 16) + (promiseStreamId[2] << 8) + promiseStreamId[3];
	}

};

struct H2Ping
{
	private:
	unsigned char		data[64];

	public:
	uint8_t *getData( void )
	{
		return (uint8_t *)data;
	}

};

struct H2GoAway
{
	private:
	unsigned char		lastStreamId[4];
	unsigned char		errorCode[4];

	public:
	uint32_t getLastStreamId( void )
	{
		return ((lastStreamId[0] << 24) + (lastStreamId[1] << 16) + (lastStreamId[2] << 8) + lastStreamId[3]) & 0x7FFFFFFF;
	}
	uint32_t getErrorCode( void )
	{
		return (errorCode[0] << 24) + (errorCode[1] << 16) + (errorCode[2] << 8) + errorCode[3];
	}
	uint8_t *getData( void )
	{
		return (uint8_t *)(this + 1);
	}
};

struct H2WindowUpdate
{
	private:
	unsigned char		sizeIncrement[4];

	public:
	uint32_t getWindowSizeIncrement( void )
	{
		return ((sizeIncrement[0] << 24) + (sizeIncrement[1] << 16) + (sizeIncrement[2] << 8) + sizeIncrement[3]) & 0x7FFFFFFF;
	}

};

struct H2Continuation
{
	private:

	public:
	uint8_t *getHeaders( void )
	{
		return (uint8_t *)(this + 1);
	}

};

struct H2Frame
{
	private:
	unsigned char		length[3];
	unsigned char		type;
	unsigned char		flags;
	unsigned char		streamId[4];

	public:
	uint32_t getLength( void )
	{
		return	(length[0] << 16) + (length[1] << 8) + length[2];
	}

	H2_FRAME_TYPE getType( void )
	{
		return (H2_FRAME_TYPE)type;
	}

	uint8_t getFlags( void )
	{
		return flags;
	}

	uint32_t getStreamId( void )
	{
		return (streamId[0] << 24) + (streamId[1] << 16) + (streamId[2] << 8) + streamId[3];
	}

	operator H2Data * (void)
	{
		assert( type == h2DATA );
		return (H2Data *)(this + 1);
	}

	operator H2Headers * (void)
	{
		assert( type == h2HEADERS );
		return (H2Headers *)(this + 1);
	}

	operator H2Priority * (void)
	{
		assert( type == h2PRIORITY);
		return (H2Priority *)(this + 1);
	}
	operator H2RstStream * (void)
	{
		assert( type == h2RST_STREAM );
		return (H2RstStream *)(this + 1);
	}
	operator H2Settings * (void)
	{
		assert( type == h2SETTINGS );
		return (H2Settings *)(this + 1);
	}
	operator H2PushPromise * (void)
	{
		assert( type == h2PUSH_PROMISE );
		return (H2PushPromise *)(this + 1);
	}
	operator H2Ping * (void)
	{
		assert( type == h2PING );
		return (H2Ping *)(this + 1);
	}
	operator H2GoAway * (void)
	{
		assert( type == h2GOAWAY );
		return (H2GoAway *)(this + 1);
	}
	operator H2WindowUpdate * (void)
	{
		assert( type == h2WINDOW_UPDATE );
		return (H2WindowUpdate *)(this + 1);
	}
	operator H2Continuation * (void)
	{
		assert( type == h2CONTINUATION );
		return (H2Continuation *)(this + 1);
	}

};

#pragma pack ( pop )



enum webHuffDecodeFlags
{
	H2_HUFF_NONE = 0,
	/* FSA accepts this state as the end of huffman encoding sequence. */
	H2_HUFF_ACCEPTED = 1,
	/* This state emits symbol */
	H2_HUFF_SYM = 2,
	/* If state machine reaches this state, decoding fails. */
	H2_HUFF_FAIL = 4
};

struct H2_Huff_Decode
{
	/* huffman decoding state, which is actually the node ID of internal
	huffman tree.  We have 257 leaf nodes, but they are identical to
	root node other than emitting a symbol, so we have 256 internal
	nodes [1..255], inclusive. */
	uint8_t state;
	/* bitwise OR of zero or more of the nghttp2_huff_decode_flag */
	uint8_t flags;
	/* symbol if NGHTTP2_HUFF_SYM flag set */
	uint8_t sym;
};

typedef H2_Huff_Decode h2Huff_decode_table_type[16];

struct H2_Huff_ctx
{
	/* Current huffman decoding state. We stripped leaf nodes, so the value range is [0..255], inclusive. */
	uint8_t state;
	/* nonzero if we can say that the decoding process succeeds at this state */
	uint8_t accept;

	H2_Huff_ctx()
	{
		state = 0;
		accept = 1;
	}

	void reset( void )
	{
		state = 0;
		accept = 1;
	}
};

struct H2_Huffman_Sym
{
	/* The number of bits in this code */
	uint32_t nbits;
	/* Huffman code aligned to LSB */
	uint32_t code;
};

enum nghttp2_hd_opcode
{
	NGHTTP2_HD_OPCODE_NONE,
	NGHTTP2_HD_OPCODE_INDEXED,
	NGHTTP2_HD_OPCODE_NEWNAME,
	NGHTTP2_HD_OPCODE_INDNAME
};

enum nghttp2_hd_inflate_state
{
	h2HeadStateTableSize = 1,
	h2HeadStateStart,
	h2HeadStateOpcode,
	h2HeadStateReadIndex,
	h2HeadStateReadNameLen,
	h2HeadStateCheckNameLen,
	h2HeadStateReadNameHuff,
	h2HeadStateReadName,
	h2HeadStateReadValueLen,
	h2HeadStateCheckValueLen,
	h2HeadStateReadValueHuff,
	h2HeadStateReadValue
};

enum nghttp2_hd_indexing_mode
{
	NGHTTP2_HD_WITH_INDEXING,
	NGHTTP2_HD_WITHOUT_INDEXING,
	NGHTTP2_HD_NEVER_INDEXING
};

class webProtoH1Worker : public webProtocolH1
{
	webProtoH1Worker		*free;	// free list

	webProtocolH1			 H1;
	class webProtocolH2		*H2;
};

class webProtocolH2 : public serverProtocol
{
	public:

	webBuffs		buffs;

	serverBuffer	rxBuff;

	webServer		*server = nullptr;

	H2_Huff_ctx		 hPackContext;

	nghttp2_hd_inflate_state	infState;

	size_t			hdTableBuffSizeMax;
	size_t			headerTableSize;
	bool			enablePush;					// currently not used
	size_t			maxConcurrentStreams;
	size_t			initialWIndowSize;
	size_t			maxFrameSize;
	size_t			maxHeaderListSize;

	CRITICAL_SECTION									workerTx;
	CRITICAL_SECTION									workerAccess;
	std::unordered_map<size_t, webProtoH1Worker *>		workers;

	webProtocolH2 ( webServer *server ) : server ( server )
	{
	}

	~webProtocolH2 ( )
	{
	}

	enum stateType
	{
		prefaceClient = 0,
		prefaceServer,
		parseFrame,
		closeConnection
	}					connState;

	operator HANDLE () { return 0; }
	void setOverlapped( OVERLAPPED *ov );	
	webBuffs *getBuffs();
	void accept( void );
	void reset( void );
	serverIOType getIoOp( void );
	void processHeaders( H2Frame *frame );
	void processFrame( class vmInstance *instance );
	void processBuffer( class vmInstance *instance, int32_t len );
};


extern const H2_Huffman_Sym webProtoH2HuffmanSymTable[];
extern const H2_Huff_Decode webProtoH2HuffmanDecodeTable[][16];

extern	size_t	webProtoH2HuffmanDecode( H2_Huff_ctx *ctx, uint8_t *in, size_t len, uint8_t *out, bool last );
