
#include "webServer.h"
#include "webSocket.h"
#include "webProtoH2.h"

#include "Target/vmTask.h"

#undef FRAME_TYPE
#define FRAME_TYPE(x) #x,
char const *H2_FRAME_TYPE_NAMES[]
{
	H2_FRAME_TYPE_LIST
};

void webProtocolH2::setOverlapped( OVERLAPPED *ov )
{
}

webBuffs *webProtocolH2::getBuffs()
{
	buffs.reset();
	switch ( connState )
	{
		case prefaceClient:
			buffs.addBuff( rxBuff.getBuffer(), rxBuff.getFree() );
			break;
		case prefaceServer:
			buffs.addBuff( server->h2Settings, server->h2SettingsLen );
			break;
		case parseFrame:
			buffs.addBuff( rxBuff.getBuffer(), rxBuff.getFree() );
			break;
		default:
			printf ( "ERROR: invalid state in webProtoH2::getBuffs - %i", state );
			throw 500;
	}
	return &buffs;
}

void webProtocolH2::accept( void )
{
	connState = prefaceClient;
}

void webProtocolH2::reset( void )
{
	infState = h2HeadStateStart;
	connState = prefaceClient;
	hPackContext.reset();
}

serverIOType webProtocolH2::getIoOp( void )
{
	switch ( connState )
	{
		case prefaceClient:
			return needRecv;
		case prefaceServer:
			return needSend;
		case parseFrame:
			return needRecv;
		case closeConnection:
			return needClose;
		default:
			printf ( "ERROR: invalid state in webProtoH2::connState - %i", state );
			throw 500;
	}
	return needNone;
}
#if 1

static uint32_t parseInteger ( uint32_t *res, size_t *shift_ptr, int *final, uint32_t initial, size_t shift, const uint8_t *in, const uint8_t *last, size_t prefix )
{
	uint32_t k = (uint8_t)((1 << prefix) - 1);
	uint32_t n = initial;
	const uint8_t *start = in;

	*shift_ptr = 0;
	*final = 0;

	if ( n == 0 )
	{
		if ( (*in & k) != k )
		{
			*final = 1;
			*res = (*in) & k;
			return 1;
		}

		n = k;

		if ( ++in == last )
		{
			*res = n;
			return (uint32_t)(in - start);
		}
	}

	for ( ; in != last; ++in, shift += 7 )
	{
		uint32_t add = *in & 0x7f;

		if ( (UINT32_MAX >> shift) < add )
		{
			throw 400;
		}

		add <<= shift;

		if ( UINT32_MAX - add < n )
		{
			throw 400;
		}

		n += add;

		if ( (*in & (1 << 7)) == 0 )
		{
			break;
		}
	}

	*shift_ptr = shift;

	if ( in == last )
	{
		*res = n;
		return (uint32_t)(in - start);
	}

	*res = n;
	*final = 1;
	return (uint32_t)(in + 1 - start);
}
#endif
static int_fast32_t decode_int(const uint8_t **src, const uint8_t *src_end, size_t prefix_bits)
{
	int32_t value, mult;
	uint8_t prefix_max = (1 << prefix_bits) - 1;

	if (*src >= src_end)
		return -1;

	value = (uint8_t) * (*src)++ & prefix_max;
	if (value != prefix_max) {
		return value;
	}

	/* we only allow at most 4 octets (excluding prefix) to be used as int (== 2**(4*7) == 2**28) */
	if (src_end - *src > 4)
		src_end = *src + 4;

	value = prefix_max;
	for (mult = 1;; mult *= 128) {
		if (*src >= src_end)
			return -1;
		value += (**src & 127) * mult;
		if ((*(*src)++ & 128) == 0)
			return value;
	}
}
#if 0
static int decode_header(h2o_mem_pool_t *pool, struct st_h2o_decode_header_result_t *result,
	h2o_hpack_header_table_t *hpack_header_table, const uint8_t **const src, const uint8_t *src_end,
	const char **err_desc)
{
	int32_t index = 0;
	int value_is_indexed = 0, do_index = 0;

Redo:
	if (*src >= src_end)
		return H2O_HTTP2_ERROR_COMPRESSION;

	/* determine the mode and handle accordingly */
	if (**src >= 128) {
		/* indexed header field representation */
		if ((index = decode_int(src, src_end, 7)) <= 0)
			return H2O_HTTP2_ERROR_COMPRESSION;
		value_is_indexed = 1;
	}
	else if (**src >= 64) {
		/* literal header field with incremental handling */
		if (**src == 64) {
			++*src;
		}
		else if ((index = decode_int(src, src_end, 6)) <= 0) {
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
		do_index = 1;
	}
	else if (**src < 32) {
		/* literal header field without indexing / never indexed */
		if ((**src & 0xf) == 0) {
			++*src;
		}
		else if ((index = decode_int(src, src_end, 4)) <= 0) {
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
	}
	else {
		/* size update */
		int new_apacity;
		if ((new_apacity = decode_int(src, src_end, 5)) < 0) {
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
		if (new_apacity > hpack_header_table->hpack_max_capacity) {
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
		hpack_header_table->hpack_capacity = new_apacity;
		while (hpack_header_table->num_entries != 0 && hpack_header_table->hpack_size > hpack_header_table->hpack_capacity) {
			header_table_evict_one(hpack_header_table);
		}
		goto Redo;
	}

	/* determine the header */
	if (index > 0) {
		/* existing name (and value?) */
		if (index < HEADER_TABLE_OFFSET) {
			result->name = (h2o_iovec_t *)h2o_hpack_static_table[index - 1].name;
			if (value_is_indexed) {
				result->value = (h2o_iovec_t *)&h2o_hpack_static_table[index - 1].value;
			}
		}
		else if (index - HEADER_TABLE_OFFSET < hpack_header_table->num_entries) {
			struct st_h2o_hpack_header_table_entry_t *entry =
				h2o_hpack_header_table_get(hpack_header_table, index - HEADER_TABLE_OFFSET);
			*err_desc = entry->err_desc;
			result->name = entry->name;
			if (!h2o_iovec_is_token(result->name))
				h2o_mem_link_shared(pool, result->name);
			if (value_is_indexed) {
				result->value = entry->value;
				h2o_mem_link_shared(pool, result->value);
			}
		}
		else {
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
	}
	else {
		/* non-existing name */
		const h2o_token_t *name_token;
		if ((result->name = decode_string(pool, src, src_end, 1, err_desc)) == NULL) {
			if (*err_desc == err_found_upper_case_in_header_name) {
				return H2O_HTTP2_ERROR_PROTOCOL;
			}
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
		if (!*err_desc) {
			/* predefined header names should be interned */
			if ((name_token = h2o_lookup_token(result->name->base, result->name->len)) != NULL) {
				result->name = (h2o_iovec_t *)&name_token->buf;
			}
		}
	}

	/* determine the value (if necessary) */
	if (!value_is_indexed) {
		if ((result->value = decode_string(pool, src, src_end, 0, err_desc)) == NULL) {
			return H2O_HTTP2_ERROR_COMPRESSION;
		}
	}

	/* add the decoded header to the header table if necessary */
	if (do_index) {
		struct st_h2o_hpack_header_table_entry_t *entry =
			header_table_add(hpack_header_table, result->name->len + result->value->len + HEADER_TABLE_ENTRY_SIZE_OFFSET, SIZE_MAX);
		if (entry != NULL) {
			entry->err_desc = *err_desc;
			entry->name = result->name;
			if (!h2o_iovec_is_token(entry->name))
				h2o_mem_addref_shared(entry->name);
			entry->value = result->value;
			if (!value_is_part_of_static_table(entry->value))
				h2o_mem_addref_shared(entry->value);
		}
	}

	return *err_desc ? H2O_HTTP2_ERROR_INVALID_HEADER_CHAR : 0;
}


#endif
void webProtocolH2::processHeaders( H2Frame *frame )
{
	H2Headers	*headers = *frame;

	uint8_t	tmp[4096];

	memset( tmp, 0, sizeof( tmp ) );


	uint8_t const *buff = headers->getHeaders( frame->getFlags() );
	auto len = frame->getLength() - sizeof( H2Headers );
	auto last = len + buff;



}

void webProtocolH2::processFrame( class vmInstance *instance )
{
	H2Frame *frame;
	size_t len;
	uint8_t *buff;

	buff = rxBuff.getBuffer();
	len = rxBuff.getSize();

	while ( 1 )
	{
		if ( len < sizeof( H2Frame ) )
		{
			rxBuff.remove( rxBuff.getSize() - len );
			return;
		}

		frame = (H2Frame *)buff;
		if ( frame->getLength() > len )
		{
			rxBuff.makeRoom( frame->getLength() - rxBuff.getSize() );
			return;
		}

		printf( "frameType: %20s  len=%d\r\n", H2_FRAME_TYPE_NAMES[frame->getType()], frame->getLength() );

		switch ( frame->getType() )
		{
			case h2DATA:
				break;
			case h2HEADERS:
				{
					H2Headers	*headers = *frame;
						
					uint8_t	tmp[4096];

					memset( tmp, 0, sizeof( tmp ) );

					auto flags = frame->getFlags();
					auto dep = frame->getStreamId();
					auto head = headers->getHeaders( frame->getFlags() );

					processHeaders( frame );

					webProtoH2HuffmanDecode( &hPackContext, headers->getHeaders( frame->getFlags() ), frame->getLength() - sizeof( H2Headers ), tmp, true );
				}
				break;
			case h2PRIORITY:
				break;
			case h2RST_STREAM:
				break;
			case h2SETTINGS:
				{
					H2Settings *settings = *frame;

					for ( auto fLen = frame->getLength(); fLen; fLen -= sizeof( H2Settings ) )
					{
						switch ( settings->getType() )
						{
							case h2SETTINGS_HEADER_TABLE_SIZE:
								headerTableSize = settings->getValue();
								printf( "tableSize: %d\r\n", settings->getValue() );
								break;
							case h2SETTINGS_ENABLE_PUSH:
								enablePush = settings->getValue() ? true : false;
								printf( "enable push: %d\r\n", settings->getValue() );
								break;
							case h2SETTINGS_MAX_CONCURRENT_STREAMS:
								maxConcurrentStreams = settings->getValue();
								printf( "max concurrent streams: %d\r\n", settings->getValue() );
								break;
							case h2SETTINGS_INITIAL_WINDOW_SIZE:
								initialWIndowSize = settings->getValue();
								printf( "initial window size: %d\r\n", settings->getValue() );
								break;
							case h2SETTINGS_MAX_FRAME_SIZE:
								maxFrameSize = settings->getValue();
								printf( "max frame size: %d\r\n", settings->getValue() );
								break;
							case h2SETTINGS_MAX_HEADER_LIST_SIZE:
								maxHeaderListSize = settings->getValue();
								break;
						}
						settings++;
					}
				}
				break;
			case h2PUSH_PROMISE:
				break;
			case h2PING:
				break;
			case h2GOAWAY:
				{
					H2GoAway *go = *frame;

					printf( "erroCode = %d\r\n", go->getErrorCode() );
				}
				break;
			case h2WINDOW_UPDATE:
				break;
			case h2CONTINUATION:
				break;
		}
		buff += frame->getLength() + sizeof ( H2Frame );
		len -= frame->getLength() + sizeof( H2Frame );
	}
}

void webProtocolH2::processBuffer( class vmInstance *instance, int32_t len )
{
	bool needMore = false;
	while ( !needMore )
	{
		switch ( connState )
		{
			case prefaceClient:
				rxBuff.assume( len );
				if ( rxBuff.getSize() < 24 ) return;
				if ( memcmp( rxBuff.getBuffer(), "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24 ) != 0 )
				{
					connState = closeConnection;
				} else
				{
					rxBuff.remove( 24 );
					len = 0;
					connState = prefaceServer;
				}
				needMore = true;
				break;
			case prefaceServer:
				len = 0;
				connState = parseFrame;
				break;
			case parseFrame:
				rxBuff.assume( len );
				processFrame( instance );
				needMore = true;
				break;
			default:
				throw 500;
		}
	}
}
