
#include "webServer.h"
#include "webProtoH1.h"
#include "webProtoSSL.h"
#include "webServerIOCP.h"
#include "webServerTaskControl.h"
#include "webServerGlobalTables.h"

#include "Target/vmTask.h"

#include <openssl/rsa.h>       /* SSLeay stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static CRITICAL_SECTION *lock_cs = 0;

static std::unordered_map<statString, SSL_CTX *> ctxMap;

int sslUserIndex;

// class to free CTX structures upon application termination (makes tracing unused blocks much easier as we do a complete cleanup of ALL allocations)
static class ctxDestructor {
	std::vector<SSL_CTX *>	ctx;

public:
	~ctxDestructor()
	{
		for ( auto &it : ctx )
		{
			SSL_CTX_free ( it );
		}
	}

	void add ( SSL_CTX *newCtx )
	{
		ctx.push_back ( newCtx );
	}
} destroy_ctx;

int webServerSSLPasswordCB (char *buf, int size, int rwflag, void *userdata)
{
	webVirtualServer *server = (webVirtualServer *)userdata;

	strncpy ( buf, server->sslPassword.c_str(), size );
	return static_cast<int>(server->sslPassword.length());
}

static int webServerCTX_servername_callback (SSL *ssl, int *ad, void *arg) 
{
    if (ssl == NULL) return SSL_TLSEXT_ERR_NOACK;

    const char* serverName = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if ( !serverName ) return SSL_TLSEXT_ERR_NOACK;
	
	auto it = ctxMap.find ( statString ( (char *)serverName ) );
	if ( it != ctxMap.end() )
	{
		return SSL_TLSEXT_ERR_OK;
	}
	return SSL_TLSEXT_ERR_NOACK;
}

#define HTTP2_PROTO_VERSION_ID ("h2")
#define HTTP2_PROTO_VERSION_ID_LEN (2)

#define HTTP2_PROTO_ALPN "\x2h2"
#define HTTP2_PROTO_ALPN_LEN (sizeof(HTTP2_PROTO_ALPN) - 1)
#define HTTP2_HTTP_1_1_ALPN "\x8http/1.1"
#define HTTP2_HTTP_1_1_ALPN_LEN (sizeof(HTTP2_HTTP_1_1_ALPN) - 1)

static const unsigned char next_proto_list[] = {
	 HTTP2_PROTO_ALPN
	 HTTP2_HTTP_1_1_ALPN
};
static size_t next_proto_list_len = sizeof( next_proto_list ) - 1 ;

static int next_proto_cb( SSL *s, const unsigned char **data, unsigned int *len, void *arg )
{
	*data = next_proto_list;
	*len = (unsigned int) next_proto_list_len;
	return SSL_TLSEXT_ERR_OK;
}

static int select_next_protocol( unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, const char *key, unsigned int keylen )
{
	unsigned int i;
	for ( i = 0; i + keylen <= inlen; i += (unsigned int)(in[i] + 1) )
	{
		if ( memcmp( &in[i], key, keylen ) == 0 )
		{
			*out = (unsigned char *)&in[i + 1];
			*outlen = in[i];
			return 0;
		}
	}
	return -1;
}

serverProtocolTypes http2_select_next_protocol( unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen )
{
#if 0
	if ( select_next_protocol( out, outlen, in, inlen, HTTP2_PROTO_ALPN, HTTP2_PROTO_ALPN_LEN ) == 0 )
	{
		return serverProtocolTypes::serverProtoH2;
	}
#endif
	if ( select_next_protocol( out, outlen, in, inlen, HTTP2_HTTP_1_1_ALPN, HTTP2_HTTP_1_1_ALPN_LEN ) == 0 )
	{
		return serverProtocolTypes::serverProtoH1;
	}
	return  serverProtocolTypes::serverProtoH1;
}

static int alpn_select_proto_cb( SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg )
{
	webProtocolSSL *sock = (webProtocolSSL *)SSL_get_ex_data( ssl, sslUserIndex );

	if ( sock->selectedProtocol != serverProtocolTypes::serverProtoNone ) return SSL_TLSEXT_ERR_OK;

	sock->selectedProtocol = http2_select_next_protocol ( (unsigned char **) out, outlen, in, inlen );

	switch ( sock->selectedProtocol )
	{
		case serverProtocolTypes::serverProtoH1:
		case serverProtocolTypes::serverProtoH2:
			return SSL_TLSEXT_ERR_OK;
		default:
			sock->selectedProtocol = serverProtocolTypes::serverProtoH1;
			return SSL_TLSEXT_ERR_NOACK;
	}
}

static const char *const DEFAULT_13_CIPHER_SUITE = TLS_DEFAULT_CIPHERSUITES;

static const char *const DEFAULT_CIPHER_LIST =
"ECDHE-ECDSA-AES128-GCM-SHA256:"
"ECDHE-RSA-AES128-GCM-SHA256:"
"ECDHE-ECDSA-AES256-GCM-SHA384:"
"ECDHE-RSA-AES256-GCM-SHA384:"
"ECDHE-ECDSA-CHACHA20-POLY1305:"
"ECDHE-RSA-CHACHA20-POLY1305:"
"DHE-RSA-AES128-GCM-SHA256:"
"DHE-RSA-AES256-GCM-SHA384:"
"ECDHE-ECDSA-AES128-SHA256:"
"ECDHE-RSA-AES128-SHA256:"
"ECDHE-ECDSA-AES128-SHA:"
"ECDHE-RSA-AES256-SHA384:"
"ECDHE-RSA-AES128-SHA:"
"ECDHE-ECDSA-AES256-SHA384:"
"ECDHE-ECDSA-AES256-SHA:"
"ECDHE-RSA-AES256-SHA:"
"DHE-RSA-AES128-SHA256:"
"DHE-RSA-AES256-SHA256:"
"DHE-RSA-AES256-SHA:"
"DHE-RSA-AES128-SHA:"
"ECDHE-ECDSA-DES-CBC3-SHA:"
"ECDHE-RSA-DES-CBC3-SHA:"
"EDH-RSA-DES-CBC3-SHA:"
"AES128-GCM-SHA256:"
"AES256-GCM-SHA384:"
"AES128-SHA256:"
"AES256-SHA256:"
"AES128-SHA:"
"AES256-SHA:"
"DES-CBC3-SHA:!DSS";

static void webServerSelfCert( webVirtualServer *virtServer, SSL_CTX *ctx )
{
	auto pkey = EVP_PKEY_new();
	auto rsa = RSA_generate_key(
		2048,   /* number of bits for the key - 2048 is a sensible value */
		RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
		NULL,   /* callback - can be NULL if we aren't displaying progress */
		NULL    /* callback argument - not needed in this case */
	);

	EVP_PKEY_assign_RSA( pkey, rsa );

	auto x509 = X509_new();
	ASN1_INTEGER_set( X509_get_serialNumber( x509 ), 1 );

	X509_gmtime_adj( X509_get_notBefore( x509 ), 0 );
	X509_gmtime_adj( X509_get_notAfter( x509 ), 31536000L );

	X509_set_pubkey( x509, pkey );

	auto name = X509_get_subject_name( x509 );

	X509_NAME_add_entry_by_txt( name, "C", MBSTRING_ASC, (unsigned char *)"US", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( name, "O", MBSTRING_ASC, (unsigned char *)"Slang.", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( name, "CN", MBSTRING_ASC, (unsigned char *)virtServer->alias[0].c_str(), -1, -1, 0 );

	X509_set_issuer_name( x509, name );

	X509_sign( x509, pkey, EVP_sha1() );

	SSL_CTX_use_certificate( ctx, x509 );
	SSL_CTX_use_PrivateKey( ctx, pkey );
}

bool webServerSLLAddContext ( webVirtualServer *virtServer, char const *certFile, char const *keyFile, char const *chainFile, char const *cipherList, char const *cipherSuite, char const *password )
{
	SSL_CTX	*ctx;

	ctx = SSL_CTX_new(SSLv23_server_method());

	SSL_CTX_set_default_passwd_cb_userdata	( ctx, virtServer );
	SSL_CTX_set_default_passwd_cb			( ctx, webServerSSLPasswordCB );

	auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
			  		SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
					SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
					SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_TICKET |
					SSL_OP_CIPHER_SERVER_PREFERENCE;

	SSL_CTX_set_options( ctx, ssl_opts );
//	SSL_CTX_set_mode( ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER );
	SSL_CTX_set_mode( ctx, SSL_MODE_AUTO_RETRY );
	SSL_CTX_set_mode( ctx, SSL_MODE_RELEASE_BUFFERS );

	const unsigned char sid_ctx[] = "slangE";
	SSL_CTX_set_session_id_context( ctx, sid_ctx, sizeof( sid_ctx ) - 1 );

	SSL_CTX_set_tlsext_servername_callback	( ctx, webServerCTX_servername_callback );
	SSL_CTX_set_session_cache_mode			( ctx, SSL_SESS_CACHE_SERVER  );
	SSL_CTX_sess_set_cache_size( ctx, 1 );
//	SSL_CTX_set_min_proto_version			( ctx, TLS1_VERSION );

	auto ecdh = EC_KEY_new_by_curve_name( NID_X9_62_prime256v1 );
	SSL_CTX_set_tmp_ecdh( ctx, ecdh );
	EC_KEY_free( ecdh );

	if ( !certFile )
	{
		printf( "warning: missing ssl configuration for %s - using self signed certificate\r\n", virtServer->name.c_str() );
		webServerSelfCert( virtServer, ctx );
	} else
	{
		if ( SSL_CTX_use_certificate_file( ctx, certFile, SSL_FILETYPE_PEM ) <= 0 )
		{
			printf ( "Error: unable to open certificate file: %s\r\n", certFile );
			ERR_print_errors_fp(stderr);
			return false;
		}

		if ( SSL_CTX_use_PrivateKey_file( ctx, keyFile, SSL_FILETYPE_PEM ) <= 0 )
		{
			printf ( "Error: unable to open key file: %s\r\n", keyFile );
			ERR_print_errors_fp(stderr);
			return false;
		}

		if ( !SSL_CTX_check_private_key( ctx ) )
		{
			printf ( "Error: incorrect key\r\n" );
			ERR_print_errors_fp(stderr);
			return false;
		}

		if ( chainFile && !SSL_CTX_use_certificate_chain_file( ctx, chainFile ) )
		{
			printf ( "Error: unable to open chain file: %s\r\n", chainFile );
			ERR_print_errors_fp(stderr);
			return false;
		}
	}

	if ( cipherList )
	{
		if ( !SSL_CTX_set_cipher_list ( ctx, cipherList ) )
		{
			printf( "no ciphers set\r\n" );
			ERR_print_errors_fp(stderr);
			return false;
		}
	} else
	{
		if ( !SSL_CTX_set_cipher_list( ctx, DEFAULT_CIPHER_LIST ) )
		{
			printf( "no ciphers set\r\n" );
			ERR_print_errors_fp(stderr);
			return false;
		}
	}

	if ( cipherSuite )
	{
		if ( !SSL_CTX_set_ciphersuites ( ctx, cipherSuite ) )
		{
			printf ( "no ciphers set\r\n" );
			ERR_print_errors_fp(stderr);
			return false;
		}
	} else
	{
		if ( !SSL_CTX_set_ciphersuites ( ctx, DEFAULT_13_CIPHER_SUITE ) )
		{
			printf ( "no ciphers set\r\n" );
			ERR_print_errors_fp(stderr);
			return false;
		}
	}

	SSL_CTX_set1_groups_list ( ctx, "X25519:P-256" );

	SSL_CTX_set_min_proto_version ( ctx, TLS1_2_VERSION );
	SSL_CTX_set_max_proto_version ( ctx, TLS_MAX_VERSION );

	// add each instance of the context string to the map
	for ( auto &aliasIt : virtServer->alias )
	{
		ctxMap[statString ( (char *)aliasIt.c_str(), aliasIt.size() )] = ctx;
	}


	SSL_CTX_set_alpn_select_cb( ctx, alpn_select_proto_cb, NULL );
	SSL_CTX_set_next_protos_advertised_cb( ctx, next_proto_cb, NULL );

	// add it to our app cleanup;
	destroy_ctx.add( ctx );

	return true;
}

static void thread_setup(void)
{
	int i;
	
	lock_cs = (CRITICAL_SECTION *)malloc(CRYPTO_num_locks() * sizeof(CRITICAL_SECTION));
	for ( i = 0; i < CRYPTO_num_locks(); i++ )
	{
		InitializeCriticalSectionAndSpinCount ( &(lock_cs[i]), 4000 );
	}
}

void thread_cleanup(void)
{
	int i;
	
	CRYPTO_set_locking_callback(NULL);
	for ( i = 0; i < CRYPTO_num_locks(); i++)
	{
		DeleteCriticalSection ( &(lock_cs[i]) );
	}
	free(lock_cs);
}

void webProtocolSSL::accept( void )
{
	ssl = SSL_new( ctxMap.begin()->second );
	pBioRead = BIO_new( BIO_s_mem() );
	pBioWrite = BIO_new( BIO_s_mem() );

	SSL_set_bio( ssl, pBioRead, pBioWrite );
	SSL_set_accept_state( ssl );
	sslState = webProtocolSSL_Recv;

	SSL_set_ex_data( ssl, sslUserIndex, this );
}


webBuffs	*webProtocolSSL::getBuffs()
{
	switch ( sslState )
	{
		case webProtocolSSL_Accept:
			wsaBuff.reset();
			sslBuff.reset();
			state = ssAcceptSocket;
			wsaBuff.addBuff ( sslBuff.getFreeBuff(), sslBuff.getFree() );
			break;
		case webProtocolSSL_Recv:
		case webProtocolSSL_SendNeedRecv:
			wsaBuff.reset();
			sslBuff.reset();
			state = ssRecvSocket;
			wsaBuff.addBuff ( sslBuff.getFreeBuff(), sslBuff.getFree() );
			break;
		case webProtocolSSL_Send:
		case webProtocolSSL_RecvNeedSend:
			if ( wsaBuff.totalIo ) 
			{
				// incomplete transmission... more to send
				return &wsaBuff;
			}
			wsaBuff.reset();
			state = ssSendSocket;
			wsaBuff.addBuff ( sslBuff.getBuffer(), sslBuff.getSize() );
			break;
		case webProtocolSSL_Read:
		case webProtocolSSL_Write:
			return protoChild->getBuffs();
		case webProtocolSSL_Close:
		default:
			printf ( "ERROR: invalid state webProtocolSSL::getBuffs - %i\r\n", sslState );
			throw errorNum::scINTERNAL;
	}
	return activeBuffs = &wsaBuff;
}

serverIOType		 webProtocolSSL::getIoOp ( void )
{
	switch ( sslState )
	{
		case webProtocolSSL_Accept:
		case webProtocolSSL_Recv:
		case webProtocolSSL_SendNeedRecv:
			return needRecv;
		case webProtocolSSL_Send:
		case webProtocolSSL_RecvNeedSend:
			return needSend;
		case webProtocolSSL_Close:
			return needClose;
		case webProtocolSSL_Read:
			return needRead;
		case webProtocolSSL_Write:
			return needWrite;
		default:
			printf ( "ERROR: invalid state webProtocolSSL::getiop - %i\r\n", sslState );
			throw errorNum::scINTERNAL;
	}
}

void webProtocolSSL::reset	( void )
{
	sslState = webProtocolSSL_Accept;
	if ( protoChild )
	{
		protoChild->reset();
		server->freeProto ( protoChild );
		protoChild = 0;
	}
	this->selectedProtocol = serverProtocolTypes::serverProtoNone;
	sslBuff.reset ( );
	if ( ssl )
	{
		SSL_free ( ssl );
		ssl = 0;
	}
	pBioRead = 0;
	pBioWrite = 0;
}

void webProtocolSSL::processBuffer ( class vmInstance *instance, int32_t len )
{
	try
	{
		if ( len <= 0 )
		{
			if ( protoChild )
			{
				activeBuffs->reset ( );
				wsaBuff.reset ( );
				protoChild->processBuffer ( instance, -1 );
				if ( protoChild->getIoOp ( ) == needClose )
				{
					sslState = webProtocolSSL_Close;
				} else
				{
					sslState = webProtocolSSL_Write;
				}
				return;
			} else
			{
				activeBuffs->reset ( );
				wsaBuff.reset ( );
				sslState = webProtocolSSL_Close;
				return;
			}
		}
		switch ( sslState )
		{
			case webProtocolSSL_Recv:
				wsaBuff.reset ( );
				Recv ( instance, len );
				break;
			case webProtocolSSL_RecvNeedSend:
				if ( wsaBuff.consume ( len ) )
				{
					wsaBuff.reset ( );
					RecvNeedSend ( instance, len );
					break;
				}
				return;
			case webProtocolSSL_Send:
				if ( wsaBuff.consume ( len ) )
				{
					wsaBuff.reset ( );
					Send ( instance, len );
					break;
				}
				return;
			case webProtocolSSL_SendNeedRecv:
				wsaBuff.reset ( );
				SendNeedRecv ( instance, len );
				break;
			case webProtocolSSL_Read:
			case webProtocolSSL_Write:
				protoChild->processBuffer ( instance, len );
				break;
			default:
				throw 500;
		}
		if ( protoChild )
		{
			process ( instance );
		} else if ( !protoChild && len )
		{
			protoChild = server->allocProto( serverProtoH1, this );
			process( instance );
			switch ( selectedProtocol )
			{
				case serverProtocolTypes::serverProtoH1:
				case serverProtocolTypes::serverProtoNone:
					break;
				case serverProtocolTypes::serverProtoH2:
					{

						auto newChild = protoChild->protoChild;
						protoChild->protoChild = nullptr;
						server->freeProto( protoChild );
						protoChild = server->allocProto( serverProtoH2, this );
						protoChild->protoChild = newChild;
						process( instance );
					}
					break;
				case serverProtocolTypes::serverProtoError:
					resetSocket();
					return;
				default:
					throw 500;
			}
		}
	} catch ( ... )
	{
		sslState = webProtocolSSL_Close;
	}
}

void webProtocolSSL::process ( class vmInstance *instance )
{
	webBuffs	*inProcessBuffs;
	int32_t		 nWritten;
	int32_t		 len;

	for ( ;; )
	{
		switch ( protoChild->getIoOp() )
		{
			case needRead:
				sslState = webProtocolSSL_Read;
				return;
			case needWrite:
				sslState = webProtocolSSL_Write;
				return;
			case needRecv:
				// see if we can read from the decrypted ssl interface

				// get our io buffers to use from the socket
				inProcessBuffs = protoChild->getBuffs();

				// grow our temporary buffer to the size needed
				sslBuff.erase();
				sslBuff.makeRoom ( inProcessBuffs->totalIo );

				// see how much we can read
				len = SSL_read ( ssl, sslBuff.getFreeBuff(), static_cast<int>(inProcessBuffs->totalIo) );
			
				if ( len <= 0 )
				{
					switch ( SSL_get_error ( ssl, len ) )
					{
						case SSL_ERROR_WANT_READ:
						case SSL_ERROR_WANT_WRITE:
							if ( (len = BIO_pending(pBioWrite)) > 0 )
							{
								sslBuff.makeRoom ( len );
								BIO_read ( pBioWrite, sslBuff.getFreeBuff(), len );
								sslBuff.assume ( len );
								sslState = webProtocolSSL_RecvNeedSend;
							} else
							{
								sslState = webProtocolSSL_Recv;
							}
							break;
						default:
							resetSocket();
							break;
					}
					return;
				} else 
				{
					// receive the data into higher layers - at this point, sslBuff has decrypted data read from the SSL_read interface.
					sslBuff.assume( len );
					memset( sslBuff.getFreeBuff(), 0xFF, sslBuff.getFree() );
					inProcessBuffs->write ( sslBuff.getBuffer(), len );
					if ( protoChild ) protoChild->processBuffer ( instance, static_cast<int32_t>(len) );

					// don't return here... get the next ioState so we can see what we need to do 
				}			
				break;
			case needSend:
				inProcessBuffs = protoChild->getBuffs();

				len = static_cast<int32_t>(inProcessBuffs->nextLen());
				if ( len > 65536 ) len = 65536;

				if ( len )
				{
					nWritten = SSL_write ( ssl, inProcessBuffs->nextBuff ( ), len );

					if ( nWritten <= 0 )
					{
						switch ( SSL_get_error ( ssl, nWritten ) )
						{
							case SSL_ERROR_WANT_READ:
							case SSL_ERROR_WANT_WRITE:
								if ( (len = BIO_pending ( pBioWrite )) > 0 )
								{
									sslBuff.erase ( );
									sslBuff.makeRoom ( len );
									BIO_read ( pBioWrite, sslBuff.getFreeBuff ( ), len );
									sslBuff.assume ( len );
									sslState = webProtocolSSL_RecvNeedSend;
								} else
								{
									sslState = webProtocolSSL_Recv;
								}
								break;
							default:
								resetSocket ( );
								break;
						}
						return;
					} else
					{
						// we sucessfully wrote nWritten bytes into the ssl encryptor.   This is sufficient for us to tell the upper layers that we've processed them, even though they have not yet been sent.
						protoChild->processBuffer ( instance, nWritten );
						if ( (len = BIO_pending ( pBioWrite )) > 0 )
						{
							// we have stuff on the bio that can be written out... let's do so now.
							sslBuff.erase ( );
							sslBuff.makeRoom ( len );
							BIO_read ( pBioWrite, sslBuff.getFreeBuff ( ), len );
							sslBuff.assume ( len );
							sslState = webProtocolSSL_Send;
							return;
						}
					}
					// don't return, loop and get the next io state so we can act appropriately
				}
				break;
			case needClose:
				sslState = webProtocolSSL_Close;
				return;
			default:
				resetSocket();
				return;
		}
	}
}

void webProtocolSSL::Recv	( class vmInstance *instance, int32_t len )
{
	// write recevied data into bio
	BIO_write ( pBioRead, sslBuff.getBuffer (), static_cast<int>(len) );
}

void webProtocolSSL::SendNeedRecv	( class vmInstance *instance, int32_t len )
{
	// write recevied data into bio
	BIO_write ( pBioRead, sslBuff.getBuffer (), static_cast<int>(len) );
}

void webProtocolSSL::RecvNeedSend	( class vmInstance *instance, int32_t len )
{
}

void webProtocolSSL::Send	( class vmInstance *instance, int32_t len )
{
}

webProtocolSSL::webProtocolSSL()
{
}

void webProtocolSSL::resetSocket( void )
{
	reset ( );
}

static struct initializer
{
	initializer ( )
	{
		thread_setup ( );
	}
	~initializer ( )
	{
		thread_cleanup ( );
	}
} sslInitializer;

bool webServerSSLInit ( webServerTaskControl *ctrl, char const *configFile, vmInstance *instance )
{
	serverIOCP	*server;

	webServerGlobalTablesInit ();

	sslUserIndex = SSL_get_ex_new_index( 0, const_cast<char *>("engine index"), 0, 0, 0 );

	*ctrl += (server = new serverIOCP());
	server->init ( serverIocpHandle, configFile, instance );

//	_beginthread ( webServerCheckTimeout, 1024 * 1024, server );

	return true;
}
