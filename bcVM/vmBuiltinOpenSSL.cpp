/*
		IO Support functions

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmAllocator.h"
#include "Target/vmTask.h"
#include "Utility/encodingTables.h"

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "Rijndael.h"

static VAR_STR stringToHex ( vmInstance *instance, VAR_STR *var )
{
	BUFFER buff;

	buff.makeRoom ( var->dat.str.len * 2 );
	for ( size_t i = 0; i< var->dat.str.len; i++ )
	{
		sprintf_s ( bufferBuff ( &buff ) + buff.size ( ), bufferFree ( &buff ), "%02x", var->dat.str.c[i] );
		buff.assume ( 2 );
	}
	return VAR_STR ( instance, bufferBuff ( &buff ), bufferSize ( &buff ) );
}

static VAR_STR pt ( vmInstance *instance, uint8_t *data, size_t len)
{
	BUFFER buff;

	buff.makeRoom ( len * 2 );
	for ( size_t i = 0; i< len; i++ )
	{
		sprintf_s ( bufferBuff ( &buff ) + buff.size(), bufferFree ( &buff ), "%02x", data[i] );
		buff.assume ( 2 );
	}
	return VAR_STR ( instance, bufferBuff ( &buff ), bufferSize ( &buff ) );
}

static VAR_STR hashSHA1 ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[SHA_DIGEST_LENGTH];

	SHA1 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return pt ( instance, md, sizeof ( md ) );
}

static VAR_STR hashSHA256 ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[SHA256_DIGEST_LENGTH];

	SHA1 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return pt ( instance, md, sizeof ( md ) );
}

static VAR_STR hashMD4 ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[MD4_DIGEST_LENGTH];

	MD4 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return pt ( instance, md, sizeof ( md ) );
}

static VAR_STR hashMD5 ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[MD5_DIGEST_LENGTH];

	MD5 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return pt ( instance, md, sizeof ( md ) );
}

static VAR_STR hashMD5Raw ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[MD5_DIGEST_LENGTH];

	MD5 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return VAR_STR ( instance, (char *) md, sizeof ( md ) );
}

static VAR_STR hashMD4Raw ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[MD4_DIGEST_LENGTH];

	MD4 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return VAR_STR ( instance, (char *) md, sizeof ( md ) );
}

static VAR_STR hashSHA1Raw ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[SHA_DIGEST_LENGTH];

	SHA1 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return VAR_STR ( instance, (char *) md, sizeof ( md ) );
}

static VAR_STR hashSHA256Raw ( vmInstance *instance, VAR *var )
{
	unsigned char	 md[SHA256_DIGEST_LENGTH];

	SHA1 ( (unsigned char *) var->dat.str.c, var->dat.str.len, md );

	return VAR_STR ( instance, (char *) md, sizeof ( md ) );
}

static VAR_STR HMAC_SHA1 ( vmInstance *instance, VAR *secret, VAR *data )
{
	unsigned char	 md[EVP_MAX_MD_SIZE];
	unsigned int     mdLen = sizeof ( md );

	HMAC ( EVP_sha1 ( ),
		   secret->dat.str.c,
		   (int)secret->dat.str.len,
		   (unsigned char *)data->dat.str.c,
		   data->dat.str.len,
		   md,
		   &mdLen
	);

	return VAR_STR ( instance, (char *) md, mdLen );
}

static VAR_STR base64Encode ( vmInstance *instance, unsigned char *in, size_t len )
{
	size_t		 loop;
	size_t		 outLen;
	uint8_t		*out;

	outLen = ((len - 1) / 3 + 1) * 4; /* round len to greatest 3rd and multiply by 4 */

	out = (uint8_t *)instance->om->alloc ( sizeof ( uint8_t ) * (outLen + 3 + 1) );

	VAR_STR res;
	res.type = slangType::eSTRING;
	res.dat.str.c = (char *) out;
	res.dat.str.len = outLen;

	for ( loop = 0; loop < len; loop += 3 )
	{
		unsigned char in2[3];
		in2[0] = in[0];
		in2[1] = loop + 1 < len ? in[1] : 0;
		in2[2] = loop + 2 < len ? in[2] : 0;

		out[0] = webBase64EncodingTable[(in2[0] >> 2)];
		out[1] = webBase64EncodingTable[((in2[0] & 0x03) << 4) | (in2[1] >> 4)];
		out[2] = webBase64EncodingTable[((in2[1] & 0x0F) << 2) | (in2[2] >> 6)];
		out[3] = webBase64EncodingTable[((in2[2] & 0x3F))];
		/* increment our pointers appropriately */
		out += 4;
		in += 3;
	}

	out[0] = 0;

	/* fill in "null" character... all 0's in important bits */
	switch ( len % 3 )
	{
		case 1:
			out[-2] = '=';
			// fall through 
		case 2:
			out[-1] = '=';
			break;
		case 0:
		default:
			break;
	}

	return res;
}

static VAR_STR rsaSign ( vmInstance *instance, char const *rsa_pkey_file, char const *src )
{
	EVP_PKEY *priv_key = NULL;
	FILE *fd;

	if ( (fd = fopen ( rsa_pkey_file, "r" )) == NULL )
	{
		throw GetLastError ( );
	}

	if ( !PEM_read_PrivateKey ( fd, &priv_key, NULL, NULL ) )
	{
		throw GetLastError ( );
	}

	EVP_MD_CTX *ctx = EVP_MD_CTX_create ( );
	EVP_PKEY_CTX* pctx = 0;
	size_t slen = 0;

	EVP_DigestSignInit ( ctx, &pctx, EVP_sha1 ( ), NULL, priv_key );
	EVP_DigestSignUpdate ( ctx, src, strlen ( src ) );
	EVP_DigestSignFinal ( ctx, NULL, &slen );

	/* Allocate memory for the signature based on size in slen */
	unsigned char *sig = (unsigned char *) malloc ( slen );
	EVP_DigestSignFinal ( ctx, sig, &slen );

	// this will return it as a VAR in result (which is what we want anyway)
	VAR_STR rsp = base64Encode ( instance, sig, slen );

	free ( sig );
	EVP_PKEY_free ( priv_key );
	EVP_MD_CTX_destroy ( ctx );

	return rsp;
}

static VAR_STR vmEncryptAES ( vmInstance *instance, VAR_STR *key, VAR_STR *str )

{
	CRijndael	 oRijndael;
	char		*outData;
	char		*inData;
	size_t		 inLen;

	if ( key->dat.str.len != 16 ) throw errorNum::scINVALID_PARAMETER;

	inLen = (str->dat.str.len + sizeof ( unsigned long ) + 15) & ~15;

	if ( !inLen )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	outData = (char *)instance->om->alloc ( inLen + 1 );

	inData = (char *)instance->malloc ( inLen );
	*((unsigned long*) inData) = (unsigned long)str->dat.str.len;
	memcpy ( inData + sizeof ( unsigned long ), str->dat.str.c, str->dat.str.len );

	try
	{
		oRijndael.MakeKey ( key->dat.str.c, CRijndael::sm_chain0, 16, 16 );
		oRijndael.Encrypt ( inData, outData, inLen );
	} catch ( ... )
	{
		instance->free ( inData );
		throw errorNum::scINVALID_PARAMETER;
	}

	instance->free ( inData );
	return VAR_STR ( instance, outData, inLen );
}

static VAR_STR vmDecryptAES ( vmInstance *instance, VAR_STR *key, VAR_STR *str )
{
	CRijndael		 oRijndael;
	char			*outData;

	if ( key->dat.str.len != 16 ) throw errorNum::scINVALID_PARAMETER;

	auto inLen = (str->dat.str.len + 15) & ~15;

	outData = (char *) instance->om->alloc ( sizeof ( char ) * inLen + 1 );

	try
	{
		oRijndael.MakeKey ( key->dat.str.c, CRijndael::sm_chain0, 16, 16 );
		oRijndael.Decrypt ( str->dat.str.c, outData, inLen );
	} catch ( ... )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	if ( *(unsigned long *)outData > inLen )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	return VAR_STR ( instance, outData + sizeof ( unsigned long ), *(unsigned long *) outData );
}

void builtinOpenSSLInit( class vmInstance *instance, opFile *file )
{
	//--------------------------------------------------------------------------
	//                     General System Support Functions
	//--------------------------------------------------------------------------
	REGISTER( instance, file );
		FUNC ( "stringToHex", stringToHex) CONST PURE;
		FUNC ( "hashSha1", hashSHA1) CONST PURE;
		FUNC ( "hashSha256", hashSHA256) CONST PURE;
		FUNC ( "hashMd4", hashMD4) CONST PURE;
		FUNC ( "hashMd5", hashMD5) CONST PURE;
		FUNC ( "hashSha1Raw", hashSHA1Raw ) CONST PURE;
		FUNC ( "hashSha256Raw", hashSHA256Raw ) CONST PURE;
		FUNC ( "hashMd4Raw", hashMD4Raw ) CONST PURE;
		FUNC ( "hashMdRaw5", hashMD5Raw ) CONST PURE;
		FUNC ( "HMAC_SHA1", HMAC_SHA1 ) CONST PURE;
		FUNC ( "rsaSign", rsaSign) CONST PURE;

		FUNC ( "encryptAes", vmEncryptAES ) CONST PURE;
		FUNC ( "decryptAes", vmDecryptAES ) CONST PURE;
	END;
}
