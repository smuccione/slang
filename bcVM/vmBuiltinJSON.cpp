// fgl_json.cpp : Defines the initialization routines for the DLL.
//

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmAllocator.h"

#include <scoped_allocator>

/*
Copyright (c) 2005 JSON.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

The Software shall be used for Good, not Evil.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
Callbacks, comments, Unicode handling by Jean Gressmann (jean@0x42.de), 2007-2010.

Changelog:
2010-05-07
Added error handling for memory allocation failure (sgbeal@googlemail.com).
Added diagnosis errors for invalid JSON.

2010-03-25
Fixed buffer overrun in grow_parse_buffer & cleaned up code.

2009-10-19
Replaced long double in JSON_value_struct with double after reports
of strtold being broken on some platforms (charles@transmissionbt.com).

2009-05-17
Incorporated benrudiak@googlemail.com fix for UTF16 decoding.

2009-05-14
Fixed float parsing bug related to a locale being set that didn't
use '.' as decimal point character (charles@transmissionbt.com).

2008-10-14
Renamed states.IN to states.IT to avoid name clash which IN macro
defined in windef.h (alexey.pelykh@gmail.com)

2008-07-19
Removed some duplicate code & debugging variable (charles@transmissionbt.com)

2008-05-28
Made JSON_value structure ansi C compliant. This bug was report by
trisk@acm.jhu.edu

2008-05-20
Fixed bug reported by charles@transmissionbt.com where the switching
from static to dynamic parse buffer did not copy the static parse
buffer's content.
*/

/* Determine the integer type use to parse non-floating point numbers */
#if __STDC_VERSION__ >= 199901L || HAVE_LONG_LONG == 1
typedef long long JSON_int_t;
#define JSON_PARSER_INTEGER_SSCANF_TOKEN "%lld"
#define JSON_PARSER_INTEGER_SPRINTF_TOKEN "%lld"
#else 
typedef long JSON_int_t;
#define JSON_PARSER_INTEGER_SSCANF_TOKEN "%ld"
#define JSON_PARSER_INTEGER_SPRINTF_TOKEN "%ld"
#endif

#define JSON_INITIAL_VARS	16

typedef enum
{
	JSON_E_NONE = 0,
	JSON_E_INVALID_CHAR,
	JSON_E_INVALID_KEYWORD,
	JSON_E_INVALID_ESCAPE_SEQUENCE,
	JSON_E_INVALID_UNICODE_SEQUENCE,
	JSON_E_INVALID_NUMBER,
	JSON_E_NESTING_DEPTH_REACHED,
	JSON_E_UNBALANCED_COLLECTION,
	JSON_E_EXPECTED_KEY,
	JSON_E_EXPECTED_COLON,
	JSON_E_OUT_OF_MEMORY
} JSON_error;

typedef enum
{
	JSON_T_NONE = 0,
	JSON_T_ARRAY_BEGIN,
	JSON_T_ARRAY_END,
	JSON_T_OBJECT_BEGIN,
	JSON_T_OBJECT_END,
	JSON_T_INTEGER,
	JSON_T_FLOAT,
	JSON_T_NULL,
	JSON_T_TRUE,
	JSON_T_FALSE,
	JSON_T_STRING,
	JSON_T_KEY,
	JSON_T_MAX
} JSON_type;

typedef struct JSON_value_struct
{
	union
	{
		JSON_int_t integer_value;

		double float_value;

		struct
		{
			const char *value;
			size_t length;
		} str;
	} vu;
} JSON_value;

typedef struct JSON_parser_struct *JSON_parser;

/*! \brief JSON parser callback

\param ctx The pointer passed to new_JSON_parser.
\param type An element of JSON_type but not JSON_T_NONE.
\param value A representation of the parsed value. This parameter is NULL for
JSON_T_ARRAY_BEGIN, JSON_T_ARRAY_END, JSON_T_OBJECT_BEGIN, JSON_T_OBJECT_END,
JSON_T_NULL, JSON_T_TRUE, and SON_T_FALSE. String values are always returned
as zero-terminated C strings.

\return Non-zero if parsing should continue, else zero.
*/
typedef int (*JSON_parser_callback)(void *ctx, JSON_type type, const struct JSON_value_struct *value);


/*! \brief The structure used to configure a JSON parser object

\param depth If negative, the parser can parse arbitrary levels of JSON, otherwise
the depth is the limit
\param Pointer to a callback. This parameter may be NULL. In this case the input is merely checked for validity.
\param Callback context. This parameter may be NULL.
\param depth. Specifies the levels of nested JSON to allow. Negative numbers yield unlimited nesting.
\param allowComments. To allow C style comments in JSON, set to non-zero.
\param handleFloatsManually. To decode floating point numbers manually set this parameter to non-zero.

\return The parser object.
*/
typedef struct
{
	JSON_parser_callback     callback;
	void *callback_ctx;
	int                      depth;
	int                      allow_comments;
	int                      handle_floats_manually;
} JSON_config;


/*! \brief Initializes the JSON parser configuration structure to default values.

The default configuration is
- 127 levels of nested JSON (depends on JSON_PARSER_STACK_SIZE, see json_parser.c)
- no parsing, just checking for JSON syntax
- no comments

\param config. Used to configure the parser.
*/
void init_JSON_config ( JSON_config *config );

/*! \brief Create a JSON parser object

\param config. Used to configure the parser. Set to NULL to use the default configuration.
See init_JSON_config

\return The parser object.
*/
extern JSON_parser new_JSON_parser ( JSON_config *config );

/*! \brief Destroy a previously created JSON parser object. */
extern void delete_JSON_parser ( JSON_parser jc );

/*! \brief Parse a character.

\return Non-zero, if all characters passed to this function are part of are valid JSON.
*/
extern int JSON_parser_char ( JSON_parser jc, int next_char );

/*! \brief Finalize parsing.

Call this method once after all input characters have been consumed.

\return Non-zero, if all parsed characters are valid JSON, zero otherwise.
*/
extern int JSON_parser_done ( JSON_parser jc );

/*! \brief Determine if a given string is valid JSON white space

\return Non-zero if the string is valid, zero otherwise.
*/
extern int JSON_parser_is_legal_white_space_string ( const char *s );

/*! \brief Gets the last error that occurred during the use of JSON_parser.

\return A value from the JSON_error enum.
*/
extern int JSON_parser_get_last_error ( JSON_parser jc );


#ifdef _MSC_VER
#   if _MSC_VER >= 1400 /* Visual Studio 2005 and up */
#      pragma warning(disable:4996) // unsecure sscanf
#      pragma warning(disable:4127) // conditional expression is constant
#   endif
#endif


#define true  (1)
#define false (0)
#define ER   (-1)     /* the universal error code */

/* values chosen so that the object size is approx equal to one page (4K) */
#define JSON_PARSER_STACK_SIZE 128
#define JSON_PARSER_PARSE_BUFFER_SIZE 3500

#define JSON_parser_malloc(bytes, reason) malloc(bytes)
#define JSON_parser_free free

typedef unsigned short UTF16;

#define COUNTOF(x) (sizeof(x)/sizeof((x)[0])) 

/*
Characters are mapped into these character classes. This allows for
a significant reduction in the size of the state transition table.
*/


enum classes
{
	C_SPACE,  /* space */
	C_WHITE,  /* other whitespace */
	C_LCURB,  /* {  */
	C_RCURB,  /* } */
	C_LSQRB,  /* [ */
	C_RSQRB,  /* ] */
	C_COLON,  /* : */
	C_COMMA,  /* , */
	C_QUOTE,  /* " */
	C_SINGLE, /* ' - single quote */
	C_BACKS,  /* \ */
	C_SLASH,  /* / */
	C_PLUS,   /* + */
	C_MINUS,  /* - */
	C_POINT,  /* . */
	C_ZERO,  /* 0 */
	C_DIGIT,  /* 123456789 */
	C_LOW_A,  /* a */
	C_LOW_B,  /* b */
	C_LOW_C,  /* c */
	C_LOW_D,  /* d */
	C_LOW_E,  /* e */
	C_LOW_F,  /* f */
	C_LOW_L,  /* l */
	C_LOW_N,  /* n */
	C_LOW_R,  /* r */
	C_LOW_S,  /* s */
	C_LOW_T,  /* t */
	C_LOW_U,  /* u */
	C_ABCDF,  /* ABCDF */
	C_E,      /* E */
	C_ETC,    /* everything else */
	C_STAR,   /* * */
	NR_CLASSES
};

static signed char ascii_class[128] = {
	/*
	This array maps the 128 ASCII characters into character classes.
	The remaining Unicode characters should be mapped to C_ETC.
	Non-whitespace control characters are errors.
	*/
	ER,      ER,      ER,      ER,      ER,      ER,      ER,      ER,
	ER,      C_WHITE, C_WHITE, ER,      ER,      C_WHITE, ER,      ER,
	ER,      ER,      ER,      ER,      ER,      ER,      ER,      ER,
	ER,      ER,      ER,      ER,      ER,      ER,      ER,      ER,
	C_SPACE, C_ETC,   C_QUOTE, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_SINGLE,
	C_ETC,   C_ETC,   C_STAR,   C_PLUS,  C_COMMA, C_MINUS, C_POINT, C_SLASH,
	C_ZERO,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
	C_DIGIT, C_DIGIT, C_COLON, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,

	C_ETC,   C_ABCDF, C_ABCDF, C_ABCDF, C_ABCDF, C_E,     C_ABCDF, C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_LSQRB, C_BACKS, C_RSQRB, C_ETC,   C_ETC,

	C_ETC,   C_LOW_A, C_LOW_B, C_LOW_C, C_LOW_D, C_LOW_E, C_LOW_F, C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_LOW_L, C_ETC,   C_LOW_N, C_ETC,
	C_ETC,   C_ETC,   C_LOW_R, C_LOW_S, C_LOW_T, C_LOW_U, C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_LCURB, C_ETC,   C_RCURB, C_ETC,   C_ETC
};


/*
The state codes.
*/
enum states
{
	GO,  /* start    */
	OK,  /* ok       */
	OB,  /* object   */
	KE,  /* key      */
	CO,  /* colon    */
	VA,  /* value    */
	AR,  /* array    */
	ST,  /* string   */
	SQ,  /* single quote string */
	UK,	 /* unquoted key */
	ES,  /* escape   */
	U1,  /* u1       */
	U2,  /* u2       */
	U3,  /* u3       */
	U4,  /* u4       */
	MI,  /* minus    */
	ZE,  /* zero     */
	IT,  /* integer  */
	FR,  /* fraction */
	E1,  /* e        */
	E2,  /* ex       */
	E3,  /* exp      */
	T1,  /* tr       */
	T2,  /* tru      */
	T3,  /* true     */
	F1,  /* fa       */
	F2,  /* fal      */
	F3,  /* fals     */
	F4,  /* false    */
	N1,  /* nu       */
	N2,  /* nul      */
	N3,  /* null     */
	C1,  /* /        */
	C2,  /* / *     */
	C3,  /* *        */
	FX,  /* *.* *eE* */
	D1,  /* second UTF-16 character decoding started by \ */
	D2,  /* second UTF-16 character proceeded by u */
	NR_STATES
};

enum actions
{
	CB = -10, /* comment begin */
	CE = -11, /* comment end */
	FA = -12, /* false */
	TR = -13, /* false */
	NU = -14, /* null */
	DE = -15, /* double detected by exponent e E */
	DF = -16, /* double detected by fraction . */
	SB = -17, /* string begin */
	MX = -18, /* integer detected by minus */
	ZX = -19, /* integer detected by zero */
	IX = -20, /* integer detected by 1-9 */
	EX = -21, /* next char is escaped */
	UC = -22, /* Unicode character read */
	SK = -23, /* unquoted key */
	UE = -24, /* unquoted key end */
	CU = -25, /* unquoted key end on colon */
	SS = -26, /* single quote string begin */
};


static signed char state_transition_table[NR_STATES][NR_CLASSES] = {
	/*
	The state transition table takes the current state and the current symbol,
	and returns either a new state or an action. An action is represented as a
	negative number. A JSON text is accepted if at the end of the text the
	state is OK and if the mode is MODE_DONE.

	white                                      1-9                                   ABCDF  etc
	space |  {  }  [  ]  :  ,  "  '  \  /  +  -  .  0  |  a  b  c  d  e  f  l  n  r  s  t  u  |  E  |  *     */
	/*start  GO*/{GO,GO,-6,ER,-5,ER,ER,ER,ER,ER,ER,CB,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*ok     OK*/{OK,OK,ER,-8,ER,-7,ER,-3,ER,ER,ER,CB,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*object OB*/{OB,OB,ER,-9,ER,ER,ER,ER,SB,SS,ER,CB,ER,ER,ER,ER,ER,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,ER},
	/*key    KE*/{KE,KE,ER,ER,ER,ER,ER,ER,SB,SS,ER,CB,ER,ER,ER,ER,ER,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,SK,ER},
	/*colon  CO*/{CO,CO,ER,ER,ER,ER,-2,ER,ER,SS,ER,CB,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*value  VA*/{VA,VA,-6,ER,-5,ER,ER,ER,SB,SS,ER,CB,ER,MX,ER,ZX,IX,ER,ER,ER,ER,ER,FA,ER,NU,ER,ER,TR,ER,ER,ER,ER,ER},
	/*array  AR*/{AR,AR,-6,ER,-5,-7,ER,ER,SB,SS,ER,CB,ER,MX,ER,ZX,IX,ER,ER,ER,ER,ER,FA,ER,NU,ER,ER,TR,ER,ER,ER,ER,ER},
	/*string ST*/{ST,ER,ST,ST,ST,ST,ST,ST,-4,ST,EX,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST},
	/*string SQ*/{SQ,ER,SQ,SQ,SQ,SQ,SQ,SQ,SQ,-4,EX,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ,SQ},
	/*u key	 UK*/{UE,UE,ER,ER,ER,ER,CU,ER,ER,ER,ER,ER,ER,ER,ER,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK,UK},
	/*escape ES*/{ER,ER,ER,ER,ER,ER,ER,ER,ST,ER,ST,ST,ER,ER,ER,ER,ER,ER,ST,ER,ER,ER,ST,ER,ST,ST,ER,ST,U1,ER,ER,ER,ER},
	/*u1     U1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,U2,U2,U2,U2,U2,U2,U2,U2,ER,ER,ER,ER,ER,ER,U2,U2,ER,ER},
	/*u2     U2*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,U3,U3,U3,U3,U3,U3,U3,U3,ER,ER,ER,ER,ER,ER,U3,U3,ER,ER},
	/*u3     U3*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,U4,U4,U4,U4,U4,U4,U4,U4,ER,ER,ER,ER,ER,ER,U4,U4,ER,ER},
	/*u4     U4*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,UC,UC,UC,UC,UC,UC,UC,UC,ER,ER,ER,ER,ER,ER,UC,UC,ER,ER},
	/*minus  MI*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ZE,IT,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*zero   ZE*/{OK,OK,ER,-8,ER,-7,ER,-3,ER,ER,ER,CB,ER,ER,DF,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*int    IT*/{OK,OK,ER,-8,ER,-7,ER,-3,ER,ER,ER,CB,ER,ER,DF,IT,IT,ER,ER,ER,ER,DE,ER,ER,ER,ER,ER,ER,ER,ER,DE,ER,ER},
	/*frac   FR*/{OK,OK,ER,-8,ER,-7,ER,-3,ER,ER,ER,CB,ER,ER,ER,FR,FR,ER,ER,ER,ER,E1,ER,ER,ER,ER,ER,ER,ER,ER,E1,ER,ER},
	/*e      E1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,E2,E2,ER,E3,E3,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*ex     E2*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,E3,E3,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*exp    E3*/{OK,OK,ER,-8,ER,-7,ER,-3,ER,ER,ER,ER,ER,ER,ER,E3,E3,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*tr     T1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,T2,ER,ER,ER,ER,ER,ER,ER},
	/*tru    T2*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,T3,ER,ER,ER,ER},
	/*true   T3*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,CB,ER,ER,ER,ER,ER,ER,ER,ER,ER,OK,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*fa     F1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,F2,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*fal    F2*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,F3,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*fals   F3*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,F4,ER,ER,ER,ER,ER,ER},
	/*false  F4*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,CB,ER,ER,ER,ER,ER,ER,ER,ER,ER,OK,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*nu     N1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,N2,ER,ER,ER,ER},
	/*nul    N2*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,N3,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*null   N3*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,CB,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,OK,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*/      C1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,C2},
	/*/*     C2*/{C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C3},
	/**      C3*/{C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,CE,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C3},
	/*_.     FX*/{OK,OK,ER,-8,ER,-7,ER,-3,ER,ER,ER,ER,ER,ER,ER,FR,FR,ER,ER,ER,ER,E1,ER,ER,ER,ER,ER,ER,ER,ER,E1,ER,ER},
	/*\      D1*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,D2,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER},
	/*\      D2*/{ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,ER,U1,ER,ER,ER,ER},
};


/*
These modes can be pushed on the stack.
*/
enum modes
{
	MODE_ARRAY = 1,
	MODE_DONE = 2,
	MODE_KEY = 3,
	MODE_OBJECT = 4
};

struct JSON_parser_struct
{
	JSON_parser_callback callback;
	void *ctx;
	enum states state;
	JSON_type type;
	signed char  before_comment_state, escaped, comment, allow_comments, handle_floats_manually, error;
	char decimal_point;
	UTF16 utf16_high_surrogate;
	int current_char;
	int depth;
	int top;
	int stack_capacity;
	signed char *stack;
	char *parse_buffer;
	size_t parse_buffer_capacity;
	size_t parse_buffer_count;
	signed char static_stack[JSON_PARSER_STACK_SIZE];
	char static_parse_buffer[JSON_PARSER_PARSE_BUFFER_SIZE];
};


static void set_error ( JSON_parser jc )
{
	switch ( jc->state )
	{
		case GO:
			switch ( jc->current_char )
			{
				case '{': case '}': case '[': case ']':
					jc->error = JSON_E_UNBALANCED_COLLECTION;
					break;
				default:
					jc->error = JSON_E_INVALID_CHAR;
					break;
			}
			break;
		case OB:
			jc->error = JSON_E_EXPECTED_KEY;
			break;
		case AR:
			jc->error = JSON_E_UNBALANCED_COLLECTION;
			break;
		case CO:
			jc->error = JSON_E_EXPECTED_COLON;
			break;
		case KE:
			jc->error = JSON_E_EXPECTED_KEY;
			break;
			/* \uXXXX\uYYYY */
		case U1: case U2: case U3:case U4: case D1: case D2:
			jc->error = JSON_E_INVALID_UNICODE_SEQUENCE;
			break;
			/* true, false, null */
		case T1: case T2: case T3: case F1: case F2: case F3: case F4: case N1: case N2: case N3:
			jc->error = JSON_E_INVALID_KEYWORD;
			break;
			/* minus, integer, fraction, exponent */
		case MI: case ZE: case IT: case FR: case E1: case E2: case E3:
			jc->error = JSON_E_INVALID_NUMBER;
			break;
		default:
			jc->error = JSON_E_INVALID_CHAR;
			break;
	}
}

static int
push ( JSON_parser jc, int mode )
{
	/*
	Push a mode onto the stack. Return false if there is overflow.
	*/
	assert ( jc->top <= jc->stack_capacity );

	if ( jc->depth < 0 )
	{
		if ( jc->top == jc->stack_capacity )
		{
			const size_t bytes_to_copy = jc->stack_capacity * sizeof ( jc->stack[0] );
			const size_t new_capacity = jc->stack_capacity * 2;
			const size_t bytes_to_allocate = new_capacity * sizeof ( jc->stack[0] );
			void *mem = JSON_parser_malloc ( bytes_to_allocate, "stack" );
			if ( !mem )
			{
				jc->error = JSON_E_OUT_OF_MEMORY;
				return false;
			}
			jc->stack_capacity = (int) new_capacity;
			memcpy ( mem, jc->stack, bytes_to_copy );
			if ( jc->stack != &jc->static_stack[0] )
			{
				JSON_parser_free ( jc->stack );
			}
			jc->stack = (signed char *) mem;
		}
	} else
	{
		if ( jc->top == jc->depth )
		{
			jc->error = JSON_E_NESTING_DEPTH_REACHED;
			return false;
		}
	}
	jc->stack[++jc->top] = (signed char) mode;
	return true;
}


static int jpop ( JSON_parser jc, int mode )
{
	/*
	Pop the stack, assuring that the current mode matches the expectation.
	Return false if there is underflow or if the modes mismatch.
	*/
	if ( jc->top < 0 || jc->stack[jc->top] != mode )
	{
		return false;
	}
	jc->top -= 1;
	return true;
}


#define parse_buffer_clear(jc) \
    do {\
        (jc)->parse_buffer_count = 0;\
        (jc)->parse_buffer[0] = 0;\
    } while (0)

#define parse_buffer_jpop_back_char(jc)\
    do {\
        assert((jc)->parse_buffer_count >= 1);\
        --(jc)->parse_buffer_count;\
        (jc)->parse_buffer[(jc)->parse_buffer_count] = 0;\
    } while (0)    

void delete_JSON_parser ( JSON_parser jc )
{
	if ( jc )
	{
		if ( jc->stack != &jc->static_stack[0] )
		{
			JSON_parser_free ( (void *) jc->stack );
		}
		if ( jc->parse_buffer != &jc->static_parse_buffer[0] )
		{
			JSON_parser_free ( (void *) jc->parse_buffer );
		}
		JSON_parser_free ( (void *) jc );
	}
}


JSON_parser
new_JSON_parser ( JSON_config *config )
{
	/*
	new_JSON_parser starts the checking process by constructing a JSON_parser
	object. It takes a depth parameter that restricts the level of maximum
	nesting.

	To continue the process, call JSON_parser_char for each character in the
	JSON text, and then call JSON_parser_done to obtain the final result.
	These functions are fully reentrant.
	*/

	int depth = 0;
	JSON_config default_config;

	JSON_parser jc = (JSON_parser) JSON_parser_malloc ( sizeof ( struct JSON_parser_struct ), "parser" );

	if ( jc == NULL )
	{
		return NULL;
	}

	memset ( jc, 0, sizeof ( *jc ) );

	/* initialize configuration */
	init_JSON_config ( &default_config );

	/* set to default configuration if none was provided */
	if ( config == NULL )
	{
		config = &default_config;
	}

	depth = config->depth;

	/* We need to be able to push at least one object */
	if ( depth == 0 )
	{
		depth = 1;
	}

	jc->state = GO;
	jc->top = -1;

	/* Do we want non-bound stack? */
	if ( depth > 0 )
	{
		jc->stack_capacity = depth;
		jc->depth = depth;
		if ( depth <= (int) COUNTOF ( jc->static_stack ) )
		{
			jc->stack = &jc->static_stack[0];
		} else
		{
			jc->stack = (signed char *) JSON_parser_malloc ( jc->stack_capacity * sizeof ( jc->stack[0] ), "stack" );
			if ( jc->stack == NULL )
			{
				JSON_parser_free ( jc );
				return NULL;
			}
		}
	} else
	{
		jc->stack_capacity = (int) COUNTOF ( jc->static_stack );
		jc->depth = -1;
		jc->stack = &jc->static_stack[0];
	}

	/* set parser to start */
	push ( jc, MODE_DONE );

	/* set up the parse buffer */
	jc->parse_buffer = &jc->static_parse_buffer[0];
	jc->parse_buffer_capacity = COUNTOF ( jc->static_parse_buffer );
	parse_buffer_clear ( jc );

	/* set up callback, comment & float handling */
	jc->callback = config->callback;
	jc->ctx = config->callback_ctx;
	jc->allow_comments = config->allow_comments != 0 ? true : false;
	jc->handle_floats_manually = config->handle_floats_manually != 0 ? true : false;

	/* set up decimal point */
	jc->decimal_point = *localeconv ( )->decimal_point;

	return jc;
}

static int parse_buffer_grow ( JSON_parser jc )
{
	const size_t bytes_to_copy = jc->parse_buffer_count * sizeof ( jc->parse_buffer[0] );
	const size_t new_capacity = jc->parse_buffer_capacity * 2;
	const size_t bytes_to_allocate = new_capacity * sizeof ( jc->parse_buffer[0] );
	void *mem = JSON_parser_malloc ( bytes_to_allocate, "parse buffer" );

	if ( mem == NULL )
	{
		jc->error = JSON_E_OUT_OF_MEMORY;
		return false;
	}

	assert ( new_capacity > 0 );
	memcpy ( mem, jc->parse_buffer, bytes_to_copy );

	if ( jc->parse_buffer != &jc->static_parse_buffer[0] )
	{
		JSON_parser_free ( jc->parse_buffer );
	}

	jc->parse_buffer = (char *) mem;
	jc->parse_buffer_capacity = new_capacity;

	return true;
}

static int parse_buffer_reserve_for ( JSON_parser jc, unsigned chars )
{
	while ( jc->parse_buffer_count + chars + 1 > jc->parse_buffer_capacity )
	{
		if ( !parse_buffer_grow ( jc ) )
		{
			assert ( jc->error == JSON_E_OUT_OF_MEMORY );
			return false;
		}
	}

	return true;
}

#define parse_buffer_has_space_for(jc, count) \
    ((jc)->parse_buffer_count + (count) + 1 <= (jc)->parse_buffer_capacity)

#define parse_buffer_push_back_char(jc, c)\
    do {\
        assert(parse_buffer_has_space_for((jc), 1)); \
        (jc)->parse_buffer[(jc)->parse_buffer_count++] = (c);\
        (jc)->parse_buffer[(jc)->parse_buffer_count]   = 0;\
    } while (0)

#define assert_is_non_container_type(jc) \
    assert( \
        (jc)->type == JSON_T_NULL || \
        (jc)->type == JSON_T_FALSE || \
        (jc)->type == JSON_T_TRUE || \
        (jc)->type == JSON_T_FLOAT || \
        (jc)->type == JSON_T_INTEGER || \
        (jc)->type == JSON_T_STRING)


static int parse_parse_buffer ( JSON_parser jc )
{
	if ( jc->callback )
	{
		JSON_value value{}, *arg = NULL;

		if ( jc->type != JSON_T_NONE )
		{
			assert_is_non_container_type ( jc );

			switch ( jc->type )
			{
				case JSON_T_FLOAT:
					arg = &value;
					if ( jc->handle_floats_manually )
					{
						value.vu.str.value = jc->parse_buffer;
						value.vu.str.length = jc->parse_buffer_count;
					} else
					{
				/* not checking with end pointer b/c there may be trailing ws */
						value.vu.float_value = strtod ( jc->parse_buffer, NULL );
					}
					break;
				case JSON_T_INTEGER:
					arg = &value;
					sscanf ( jc->parse_buffer, JSON_PARSER_INTEGER_SSCANF_TOKEN, &value.vu.integer_value );
					break;
				case JSON_T_STRING:
					arg = &value;
					value.vu.str.value = jc->parse_buffer;
					value.vu.str.length = jc->parse_buffer_count;
					break;
				default:
					break;
			}

			if ( !(*jc->callback)(jc->ctx, jc->type, arg) )
			{
				return false;
			}
		}
	}

	parse_buffer_clear ( jc );

	return true;
}

#undef IS_HIGH_SURROGATE
#undef IS_LOW_SURROGATE

#define IS_HIGH_SURROGATE(uc) (((uc) & 0xFC00) == 0xD800)
#define IS_LOW_SURROGATE(uc)  (((uc) & 0xFC00) == 0xDC00)
#define DECODE_SURROGATE_PAIR(hi,lo) ((((hi) & 0x3FF) << 10) + ((lo) & 0x3FF) + 0x10000)
static unsigned char utf8_lead_bits[4] = { 0x00, 0xC0, 0xE0, 0xF0 };

static int decode_unicode_char ( JSON_parser jc )
{
	int i;
	unsigned uc = 0;
	char *p;
	int trail_bytes;

	assert ( jc->parse_buffer_count >= 6 );

	p = &jc->parse_buffer[jc->parse_buffer_count - 4];

	for ( i = 12; i >= 0; i -= 4, ++p )
	{
		unsigned int x = (unsigned int)(unsigned char)*p;

		if ( x >= 'a' )
		{
			x -= ('a' - 10);
		} else if ( x >= 'A' )
		{
			x -= ('A' - 10);
		} else
		{
			x &= ~0x30u;
		}

		assert ( x < 16 );

		uc |= x << i;
	}

	/* clear UTF-16 char from buffer */
	jc->parse_buffer_count -= 6;
	jc->parse_buffer[jc->parse_buffer_count] = 0;

	/* attempt decoding ... */
	if ( jc->utf16_high_surrogate )
	{
		if ( IS_LOW_SURROGATE ( uc ) )
		{
			uc = DECODE_SURROGATE_PAIR ( jc->utf16_high_surrogate, uc );
			trail_bytes = 3;
			jc->utf16_high_surrogate = 0;
		} else
		{
	/* high surrogate without a following low surrogate */
			return false;
		}
	} else
	{
		if ( uc < 0x80 )
		{
			trail_bytes = 0;
		} else if ( uc < 0x800 )
		{
			trail_bytes = 1;
		} else if ( IS_HIGH_SURROGATE ( uc ) )
		{
/* save the high surrogate and wait for the low surrogate */
			jc->utf16_high_surrogate = (UTF16) uc;
			return true;
		} else if ( IS_LOW_SURROGATE ( uc ) )
		{
/* low surrogate without a preceding high surrogate */
			return false;
		} else
		{
			trail_bytes = 2;
		}
	}

	jc->parse_buffer[jc->parse_buffer_count++] = (char) ((uc >> (trail_bytes * 6)) | utf8_lead_bits[trail_bytes]);

	for ( i = trail_bytes * 6 - 6; i >= 0; i -= 6 )
	{
		jc->parse_buffer[jc->parse_buffer_count++] = (char) (((uc >> i) & 0x3F) | 0x80);
	}

	jc->parse_buffer[jc->parse_buffer_count] = 0;

	return true;
}

static int add_escaped_char_to_parse_buffer ( JSON_parser jc, int next_char )
{
	assert ( parse_buffer_has_space_for ( jc, 1 ) );

	jc->escaped = 0;
	/* remove the backslash */
	parse_buffer_jpop_back_char ( jc );
	switch ( next_char )
	{
		case 'b':
			parse_buffer_push_back_char ( jc, '\b' );
			break;
		case 'f':
			parse_buffer_push_back_char ( jc, '\f' );
			break;
		case 'n':
			parse_buffer_push_back_char ( jc, '\n' );
			break;
		case 'r':
			parse_buffer_push_back_char ( jc, '\r' );
			break;
		case 't':
			parse_buffer_push_back_char ( jc, '\t' );
			break;
		case '"':
			parse_buffer_push_back_char ( jc, '"' );
			break;
		case '\\':
			parse_buffer_push_back_char ( jc, '\\' );
			break;
		case '/':
			parse_buffer_push_back_char ( jc, '/' );
			break;
		case 'u':
			parse_buffer_push_back_char ( jc, '\\' );
			parse_buffer_push_back_char ( jc, 'u' );
			break;
		default:
			return false;
	}

	return true;
}

static int add_char_to_parse_buffer ( JSON_parser jc, int next_char, int next_class )
{
	if ( !parse_buffer_reserve_for ( jc, 1 ) )
	{
		assert ( JSON_E_OUT_OF_MEMORY == jc->error );
		return false;
	}

	if ( jc->escaped )
	{
		if ( !add_escaped_char_to_parse_buffer ( jc, next_char ) )
		{
			jc->error = JSON_E_INVALID_ESCAPE_SEQUENCE;
			return false;
		}
	} else if ( !jc->comment )
	{
		if ( (jc->type != JSON_T_NONE) | !((next_class == C_SPACE) | (next_class == C_WHITE)) /* non-white-space */ )
		{
			parse_buffer_push_back_char ( jc, (char) next_char );
		}
	}

	return true;
}

#define assert_type_isnt_string_null_or_bool(jc) \
    assert((jc)->type != JSON_T_FALSE); \
    assert((jc)->type != JSON_T_TRUE); \
    assert((jc)->type != JSON_T_NULL); \
    assert((jc)->type != JSON_T_STRING)


int
JSON_parser_char ( JSON_parser jc, int next_char )
{
	/*
	After calling new_JSON_parser, call this function for each character (or
	partial character) in your JSON text. It can accept UTF-8, UTF-16, or
	UTF-32. It returns true if things are looking ok so far. If it rejects the
	text, it returns false.
	*/
	enum classes next_class;
	enum states next_state;

	/*
	Store the current char for error handling
	*/
	jc->current_char = next_char;

	/*
	Determine the character's class.
	*/
	if ( next_char < 0 )
	{
		jc->error = JSON_E_INVALID_CHAR;
		return false;
	}
	if ( next_char >= 128 )
	{
		next_class = C_ETC;
	} else
	{
		next_class = (classes) ascii_class[next_char];
		if ( (int) next_class <= (int) ER )
		{
			set_error ( jc );
			return false;
		}
	}

	if ( !add_char_to_parse_buffer ( jc, next_char, next_class ) )
	{
		return false;
	}

	/*
	Get the next state from the state transition table.
	*/
	next_state = (states) state_transition_table[jc->state][next_class];
	if ( next_state >= 0 )
	{
/*
Change the state.
*/
		jc->state = (states) next_state;
	} else
	{
/*
Or perform one of the actions.
*/
		switch ( (int) next_state )
		{
/* Unicode character */
			case UC:
				if ( !decode_unicode_char ( jc ) )
				{
					jc->error = JSON_E_INVALID_UNICODE_SEQUENCE;
					return false;
				}
				/* check if we need to read a second UTF-16 char */
				if ( jc->utf16_high_surrogate )
				{
					jc->state = D1;
				} else
				{
					jc->state = ST;
				}
				break;
				/* escaped char */
			case EX:
				jc->escaped = 1;
				jc->state = ES;
				break;
				/* integer detected by minus */
			case MX:
				jc->type = JSON_T_INTEGER;
				jc->state = MI;
				break;
				/* integer detected by zero */
			case ZX:
				jc->type = JSON_T_INTEGER;
				jc->state = ZE;
				break;
				/* integer detected by 1-9 */
			case IX:
				jc->type = JSON_T_INTEGER;
				jc->state = IT;
				break;

				/* floating point number detected by exponent*/
			case DE:
				assert_type_isnt_string_null_or_bool ( jc );
				jc->type = JSON_T_FLOAT;
				jc->state = E1;
				break;

				/* floating point number detected by fraction */
			case DF:
				assert_type_isnt_string_null_or_bool ( jc );
				if ( !jc->handle_floats_manually )
				{
/*
Some versions of strtod (which underlies sscanf) don't support converting
C-locale formated floating point values.
*/
					assert ( jc->parse_buffer[jc->parse_buffer_count - 1] == '.' );
					jc->parse_buffer[jc->parse_buffer_count - 1] = jc->decimal_point;
				}
				jc->type = JSON_T_FLOAT;
				jc->state = FX;
				break;
				/* string begin " */
			case SB:
				parse_buffer_clear ( jc );
				assert ( jc->type == JSON_T_NONE );
				jc->type = JSON_T_STRING;
				jc->state = ST;
				break;
				//* single quote begin ' */
			case SS:
				parse_buffer_clear ( jc );
				assert ( jc->type == JSON_T_NONE );
				jc->type = JSON_T_STRING;
				jc->state = SQ;
				break;
				/* unquoted key begin " */
			case SK:
				parse_buffer_clear ( jc );
				if ( !parse_buffer_reserve_for ( jc, 1 ) )
				{
					assert ( JSON_E_OUT_OF_MEMORY == jc->error );
					return false;
				}
				parse_buffer_push_back_char ( jc, (char) next_char );
				jc->type = JSON_T_STRING;
				jc->state = UK;
				break;
				/* unquoted key end on non-colon */
			case UE:
				parse_buffer_jpop_back_char ( jc );

				assert ( jc->type == JSON_T_STRING );
				jc->type = JSON_T_NONE;
				jc->state = CO;

				if ( jc->callback )
				{
					JSON_value value{};
					value.vu.str.value = jc->parse_buffer;
					value.vu.str.length = jc->parse_buffer_count;
					if ( !(*jc->callback)(jc->ctx, JSON_T_KEY, &value) )
					{
						return false;
					}
				}
				parse_buffer_clear ( jc );
				break;
				/*unquoted key end on colon */
			case CU:
				parse_buffer_jpop_back_char ( jc );

				assert ( jc->type == JSON_T_STRING );
				jc->type = JSON_T_NONE;
				jc->state = CO;

				if ( jc->callback )
				{
					JSON_value value{};
					value.vu.str.value = jc->parse_buffer;
					value.vu.str.length = jc->parse_buffer_count;
					if ( !(*jc->callback)(jc->ctx, JSON_T_KEY, &value) )
					{
						return false;
					}
				}
				parse_buffer_clear ( jc );

				// must also do colon processing here
				if ( !jpop ( jc, MODE_KEY ) || !push ( jc, MODE_OBJECT ) )
				{
					return false;
				}
				assert ( jc->type == JSON_T_NONE );
				jc->state = VA;
				break;

				/* n */
			case NU:
				assert ( jc->type == JSON_T_NONE );
				jc->type = JSON_T_NULL;
				jc->state = N1;
				break;
				/* f */
			case FA:
				assert ( jc->type == JSON_T_NONE );
				jc->type = JSON_T_FALSE;
				jc->state = F1;
				break;
				/* t */
			case TR:
				assert ( jc->type == JSON_T_NONE );
				jc->type = JSON_T_TRUE;
				jc->state = T1;
				break;

				/* closing comment */
			case CE:
				jc->comment = 0;
				assert ( jc->parse_buffer_count == 0 );
				assert ( jc->type == JSON_T_NONE );
				jc->state = (states) jc->before_comment_state;
				break;

				/* opening comment  */
			case CB:
				if ( !jc->allow_comments )
				{
					return false;
				}
				parse_buffer_jpop_back_char ( jc );
				if ( !parse_parse_buffer ( jc ) )
				{
					return false;
				}
				assert ( jc->parse_buffer_count == 0 );
				assert ( jc->type != JSON_T_STRING );
				switch ( jc->stack[jc->top] )
				{
					case MODE_ARRAY:
					case MODE_OBJECT:
						switch ( jc->state )
						{
							case VA:
							case AR:
								jc->before_comment_state = jc->state;
								break;
							default:
								jc->before_comment_state = OK;
								break;
						}
						break;
					default:
						jc->before_comment_state = jc->state;
						break;
				}
				jc->type = JSON_T_NONE;
				jc->state = C1;
				jc->comment = 1;
				break;
				/* empty } */
			case -9:
				parse_buffer_clear ( jc );
				if ( jc->callback && !(*jc->callback)(jc->ctx, JSON_T_OBJECT_END, NULL) )
				{
					return false;
				}
				if ( !jpop ( jc, MODE_KEY ) )
				{
					return false;
				}
				jc->state = OK;
				break;

			/* } */ case -8:
				parse_buffer_jpop_back_char ( jc );
				if ( !parse_parse_buffer ( jc ) )
				{
					return false;
				}
				if ( jc->callback && !(*jc->callback)(jc->ctx, JSON_T_OBJECT_END, NULL) )
				{
					return false;
				}
				if ( !jpop ( jc, MODE_OBJECT ) )
				{
					jc->error = JSON_E_UNBALANCED_COLLECTION;
					return false;
				}
				jc->type = JSON_T_NONE;
				jc->state = OK;
				break;

			/* ] */ case -7:
				parse_buffer_jpop_back_char ( jc );
				if ( !parse_parse_buffer ( jc ) )
				{
					return false;
				}
				if ( jc->callback && !(*jc->callback)(jc->ctx, JSON_T_ARRAY_END, NULL) )
				{
					return false;
				}
				if ( !jpop ( jc, MODE_ARRAY ) )
				{
					jc->error = JSON_E_UNBALANCED_COLLECTION;
					return false;
				}

				jc->type = JSON_T_NONE;
				jc->state = OK;
				break;

	/* { */ case -6:
				parse_buffer_jpop_back_char ( jc );
				if ( jc->callback && !(*jc->callback)(jc->ctx, JSON_T_OBJECT_BEGIN, NULL) )
				{
					return false;
				}
				if ( !push ( jc, MODE_KEY ) )
				{
					return false;
				}
				assert ( jc->type == JSON_T_NONE );
				jc->state = OB;
				break;

	/* [ */ case -5:
				parse_buffer_jpop_back_char ( jc );
				if ( jc->callback && !(*jc->callback)(jc->ctx, JSON_T_ARRAY_BEGIN, NULL) )
				{
					return false;
				}
				if ( !push ( jc, MODE_ARRAY ) )
				{
					return false;
				}
				assert ( jc->type == JSON_T_NONE );
				jc->state = AR;
				break;

	/* string end " */ case -4:
				parse_buffer_jpop_back_char ( jc );
				switch ( jc->stack[jc->top] )
				{
					case MODE_KEY:
						assert ( jc->type == JSON_T_STRING );
						jc->type = JSON_T_NONE;
						jc->state = CO;

						if ( jc->callback )
						{
							JSON_value value{};
							value.vu.str.value = jc->parse_buffer;
							value.vu.str.length = jc->parse_buffer_count;
							if ( !(*jc->callback)(jc->ctx, JSON_T_KEY, &value) )
							{
								return false;
							}
						}
						parse_buffer_clear ( jc );
						break;
					case MODE_ARRAY:
					case MODE_OBJECT:
						assert ( jc->type == JSON_T_STRING );
						if ( !parse_parse_buffer ( jc ) )
						{
							return false;
						}
						jc->type = JSON_T_NONE;
						jc->state = OK;
						break;
					default:
						return false;
				}
				break;

	/* , */ case -3:
				parse_buffer_jpop_back_char ( jc );
				if ( !parse_parse_buffer ( jc ) )
				{
					return false;
				}
				switch ( jc->stack[jc->top] )
				{
					case MODE_OBJECT:
						/*
						A comma causes a flip from object mode to key mode.
						*/
						if ( !jpop ( jc, MODE_OBJECT ) || !push ( jc, MODE_KEY ) )
						{
							return false;
						}
						assert ( jc->type != JSON_T_STRING );
						jc->type = JSON_T_NONE;
						jc->state = KE;
						break;
					case MODE_ARRAY:
						assert ( jc->type != JSON_T_STRING );
						jc->type = JSON_T_NONE;
						jc->state = VA;
						break;
					default:
						return false;
				}
				break;

	/* : */ case -2:
				/*
				A colon causes a flip from key mode to object mode.
				*/
				parse_buffer_jpop_back_char ( jc );
				if ( !jpop ( jc, MODE_KEY ) || !push ( jc, MODE_OBJECT ) )
				{
					return false;
				}
				assert ( jc->type == JSON_T_NONE );
				jc->state = VA;
				break;
				/*
				Bad action.
				*/
			default:
				set_error ( jc );
				return false;
		}
	}
	return true;
}

int
JSON_parser_done ( JSON_parser jc )
{
	if ( (jc->state == OK || jc->state == GO) && jpop ( jc, MODE_DONE ) )
	{
		return true;
	}

	jc->error = JSON_E_UNBALANCED_COLLECTION;
	return false;
}


int JSON_parser_is_legal_white_space_string ( const char *s )
{
	int c, char_class;

	if ( s == NULL )
	{
		return false;
	}

	for ( ; *s; ++s )
	{
		c = (int)(unsigned char)*s;

		if ( c < 0 || c >= 128 )
		{
			return false;
		}

		char_class = (int)(unsigned char)ascii_class[c];

		if ( char_class != C_SPACE && char_class != C_WHITE )
		{
			return false;
		}
	}

	return true;
}

int JSON_parser_get_last_error ( JSON_parser jc )
{
	return jc->error;
}


void init_JSON_config ( JSON_config *config )
{
	if ( config )
	{
		memset ( config, 0, sizeof ( *config ) );

		config->depth = JSON_PARSER_STACK_SIZE - 1;
	}
}

#define INDENT(str,amt) if ( !compact ) {for(size_t loop=0;loop<(amt);loop++) (str) << ' ';}

void toJson ( vmInstance *instance, VAR *val, std::ostringstream &str, std::vector<VAR *> &xref, uint64_t indent, bool emitNull, bool compact, bool quoteName )
{
	if ( val->packIndex < xref.size ( ) )
	{
		if ( xref[val->packIndex] == val )
		{
			throw errorNum::scCIRCULAR_REFERENCE;
		}
	}
	val->packIndex = (uint32_t) xref.size ( );
	xref.push_back ( val );

	while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
	switch ( TYPE ( val ) )
	{
		case slangType::eNULL:
			str << "null";
			break;
		case slangType::eLONG:
			str << val->dat.l;
			break;
		case slangType::eDOUBLE:
			str << val->dat.d;
			break;
		case slangType::eSTRING:
			str << "\"";
			str.write ( val->dat.str.c, val->dat.str.len );
			str << "\"";
			break;
		case slangType::eARRAY_ROOT:
			str << "[";
			if ( !compact ) str << std::endl;
			toJson ( instance, val->dat.ref.v, str, xref, indent + 3, emitNull, compact, quoteName );
			INDENT ( str, indent );
			str << "]";
			break;
		case slangType::eARRAY_FIXED:
			if ( val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex )
			{
				INDENT ( str, indent );
				toJson ( instance, &val[1], str, xref, indent, emitNull, compact, quoteName );
				for ( int64_t loop = 1; loop < val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1; loop++ )
				{
					str << ",";
					if ( !compact ) str << std::endl;
					INDENT ( str, indent );
					toJson ( instance, &val[loop + 1], str, xref, indent, emitNull, compact, quoteName );
				}
				if ( !compact ) str << std::endl;
			}
			break;
		case slangType::eARRAY_SPARSE:
			val = val->dat.aSparse.v;
			if ( val )
			{
				INDENT ( str, indent );
				toJson ( instance, val->dat.aElem.var, str, xref, indent, emitNull, compact, quoteName );
				val = val->dat.aElem.next;
				while ( val )
				{
					str << ",";
					if ( !compact ) str << std::endl;
					INDENT ( str, indent );
					toJson ( instance, val->dat.aElem.var, str, xref, indent, emitNull, compact, quoteName );
					val = val->dat.aElem.next;
				}
				if ( !compact ) str << std::endl;
			}
			break;
		case slangType::eOBJECT_ROOT:
			str << "{";
			if ( !compact ) str << std::endl;
			if ( val->dat.ref.v->dat.obj.classDef->cInspectorCB )
			{
				bool first = true;
				auto vals = val->dat.ref.v->dat.obj.classDef->cInspectorCB ( instance, 0, val, 0, 0 );
				for ( auto &it : vals->entry )
				{
					if ( first )
					{
						first = false;
					} else
					{
						str << ",";
						if ( !compact ) str << std::endl;
					}
					INDENT ( str, indent + 3 );
					if ( quoteName ) str << "\"";
					str << it->getName ( );
					if ( quoteName ) str << "\"";
					if ( !compact ) str << " ";
					str << ":";
					if ( !compact ) str << " ";
					toJson ( instance, it->getRawVar ( ), str, xref, indent + it->getNameLen ( ) + 6 + (quoteName?2:0), emitNull, compact, quoteName );
				}
				if ( !first )
				{
					if ( !compact ) str << std::endl;
				}
				delete vals;
			} else
			{
				toJson ( instance, val->dat.ref.v, str, xref, indent + 3, emitNull, compact, quoteName );
				if ( !compact ) str << std::endl;
			}
			INDENT ( str, indent );
			str << "}";
			break;
		case slangType::eOBJECT:
			{
				bool first = true;

				for ( size_t loop = 0; loop < val->dat.obj.classDef->nElements; loop++ )
				{
					if ( !val->dat.obj.classDef->elements[loop].methodAccess.objectStart )
					{
						switch ( val->dat.obj.classDef->elements[loop].type )
						{
							case fgxClassElementType::fgxClassType_iVar:
								// can never be virtual as they would not have a 0 objectStart
								// no reason to serialize NULL ivar's as they will default to NULL on unpack if not present
								if ( val[val->dat.obj.classDef->elements[loop].methodAccess.iVarIndex].type != slangType::eNULL )
								{
									if ( !first )
									{
										str << ",";
										if ( !compact ) str << std::endl;
									}
									first = false;
									INDENT ( str, indent );
									if ( quoteName ) str << "\"";
									str.write ( val->dat.obj.classDef->elements[loop].name, val->dat.obj.classDef->elements[loop].nameLen - 1 );
									if ( quoteName ) str << "\"";
									if ( !compact ) str << " ";
									str << ":";
									if ( !compact ) str << " ";
									toJson ( instance, &val[val->dat.obj.classDef->elements[loop].methodAccess.iVarIndex], str, xref, indent + val->dat.obj.classDef->elements[loop].nameLen - 1 + 3, emitNull, compact, quoteName );
								}
								break;
							case fgxClassElementType::fgxClassType_static:
								if ( val->dat.obj.classDef->loadImage->globals[val->dat.obj.classDef->elements[loop].methodAccess.globalEntry]->type != slangType::eNULL )
								{
									if ( !first )
									{
										str << ",";
										if ( !compact ) str << std::endl;
									}
									first = false;
									INDENT ( str, indent );
									if ( quoteName ) str << "\"";
									str.write ( val->dat.obj.classDef->elements[loop].name, val->dat.obj.classDef->elements[loop].nameLen - 1 );
									if ( quoteName ) str << "\"";
									if ( !compact ) str << " ";
									str << ":";
									if ( !compact ) str << " ";
									toJson ( instance, val->dat.obj.classDef->loadImage->globals[val->dat.obj.classDef->elements[loop].methodAccess.globalEntry], str, xref, indent + val->dat.obj.classDef->elements[loop].nameLen - 1 + 3, emitNull, compact, quoteName );
								}
							case fgxClassElementType::fgxClassType_inherit:	// NOLINT (bugprone-branch-clone)
//								toJson ( instance, &val[val->dat.obj.classDef->elements[loop].globalEntry], str, xref, indent, emitNull, compact, quoteName );
								break;
							default:
								break;
						}
					}
				}
			}
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

static VAR_STR vmToJson ( vmInstance *instance, VAR *val, bool emitNull, bool compact, bool quoteName )
{
	std::vector<VAR *>  xref;
	std::ostringstream	str;

	toJson ( instance, val, str, xref, 0, emitNull, compact, quoteName );

	return VAR_STR ( instance, str.str ( ) );
}

constexpr int NAME_LEN = 64;

struct JSON_OBJECT_NAME
{
	char name[NAME_LEN];
	bool caseInsensitive = false;

	JSON_OBJECT_NAME ( char const *n, bool caseInsensitive = false ) : caseInsensitive ( caseInsensitive )
	{
		strcpy_s ( name, sizeof ( name ), n );
	}
	char const *c_str () const  noexcept
	{
		return name;
	}
	inline size_t hash ( ) const noexcept
	{
		size_t h = 5381;
		auto p = name;

		// dan bernstein's djb2 hash algorithm
		while ( *p )
		{
			h = ((h << 5) + h) ^ caseInsenstiveTable[(uint8_t) *p];		// h(i) = h(i-1) * 33 ^ str[i]
			p++;
		}
		return h;
	}
};

struct JSON_OBJECT_NAME_CMP
{
	bool operator () ( JSON_OBJECT_NAME const &left, JSON_OBJECT_NAME const &right ) const noexcept
	{
		if ( left.caseInsensitive || right.caseInsensitive )
		{
			return !strccmp ( left.name, right.name );
		} else
		{
			return !strcmp ( left.name, right.name );
		}
	}
};

namespace std {
	template <>
	class hash<JSON_OBJECT_NAME>
	{
	public:
		size_t operator()( JSON_OBJECT_NAME const &obj ) const noexcept
		{
			return obj.hash ( );
		}
	};
}

struct JSON_OBJECT
{
	using vecType = std::vector<VAR *, vmAllocator<VAR *>>;
	using objTypeAllocator = std::scoped_allocator_adaptor<vmAllocator<std::pair<JSON_OBJECT_NAME const, VAR *>>>;
	using objType = std::unordered_map<JSON_OBJECT_NAME, VAR *, std::hash<JSON_OBJECT_NAME>, JSON_OBJECT_NAME_CMP, objTypeAllocator >;

	std::variant <std::monostate, objType, vecType> data;
};

//******************  garbage collection routines

static void cJsonGcCb ( class vmInstance *instance, VAR_OBJ *val, objectMemory::collectionVariables *col )
{
	auto obj = val->getObj<JSON_OBJECT> ();
	if ( col->doCopy ( obj ) )
	{
		// allocating a new object does not invalidate old one until garbage collection is complete
		auto newJobj = val->makeObj<JSON_OBJECT> ( col->getAge (), instance );

		if ( std::holds_alternative<JSON_OBJECT::vecType> ( obj->data ) )
		{
			auto &oldVec = std::get<JSON_OBJECT::vecType> ( obj->data );
			auto newVec = JSON_OBJECT::vecType ( vmAllocator<VAR *> ( instance, col->getAge () ) );

			newVec.reserve ( oldVec.size () );
			for ( auto &it : oldVec )
			{
				newVec.push_back ( nullptr );
				auto varPtr = &newVec.back ();
				*varPtr = instance->om->move ( instance, varPtr, it, false, col );
			}
			newJobj->data = newVec;
		} else if ( std::holds_alternative<JSON_OBJECT::objType> ( obj->data ) )
		{
			auto &oldObj = std::get<JSON_OBJECT::objType> ( obj->data );
			auto newObj = JSON_OBJECT::objType ( vmAllocator<std::pair<JSON_OBJECT_NAME, VAR *>> ( instance, col->getAge () ) );

			for ( auto &it : oldObj )
			{
				auto varPtr = &newObj[it.first];
				*varPtr = instance->om->move ( instance, varPtr, it.second, false, col );
			}
			newJobj->data = newObj;
		} else
		{
			throw errorNum::scINTERNAL;
		}
	} else
	{
		if ( std::holds_alternative<JSON_OBJECT::vecType> ( obj->data ) )
		{
			auto &oldVec = std::get<JSON_OBJECT::vecType> ( obj->data );

			for ( auto &it : oldVec )
			{
				it = instance->om->move ( instance, &it, it, false, col );
			}
		} else if ( std::holds_alternative<JSON_OBJECT::objType> ( obj->data ) )
		{
			auto &oldObj = std::get<JSON_OBJECT::objType> ( obj->data );
			for ( auto &it : oldObj )
			{
				it.second = instance->om->move ( instance, &it.second, it.second, false, col );
			}
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}
	return;
}

static void cJsonCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto obj = reinterpret_cast<VAR_OBJ *>(val)->getObj<JSON_OBJECT>( );
	auto newJobj = reinterpret_cast<VAR_OBJ *>(val)->makeObj<JSON_OBJECT>( instance );

	if ( std::holds_alternative<JSON_OBJECT::vecType> ( obj->data ) )
	{
		auto &oldVec = std::get<JSON_OBJECT::vecType> ( obj->data );
		auto newVec = JSON_OBJECT::vecType ( vmAllocator<VAR *> ( instance, 0 ) );

		newVec.reserve ( oldVec.size () );
		for ( auto &it : oldVec )
		{
			newVec.push_back ( copy ( instance, it, true, copyVar ) );
		}
		newJobj->data = newVec;
	} else if ( std::holds_alternative<JSON_OBJECT::objType> ( obj->data ) )
	{
		auto &oldObj = std::get<JSON_OBJECT::objType> ( obj->data );
		auto newObj = JSON_OBJECT::objType ( vmAllocator<std::pair<JSON_OBJECT_NAME, VAR *>> ( instance, 0 ) );

		for ( auto &it : oldObj )
		{
			newObj[it.first] = copy ( instance, it.second, true, copyVar );
		}
		newJobj->data = newObj;
	} else
	{
		throw errorNum::scINTERNAL;
	}
}

//******************  pack routines

static void cJsonPackCb ( class vmInstance *instance, VAR *val, BUFFER *buff, void *param, void (*packCB) (VAR *var, void *param) )
{
	JSON_OBJECT *obj;
	char name[NAME_LEN];

	obj = (JSON_OBJECT *) classGetCargo ( val );

	bufferPut32 ( buff, 2 );		// version number
	if ( std::holds_alternative<JSON_OBJECT::vecType> ( obj->data ) )
	{
		auto &elems  = std::get<JSON_OBJECT::vecType> ( obj->data );
		bufferPut32 ( buff, (uint32_t)elems.size() );
		bufferPut32 ( buff, 1 );
		auto ctr = 1;
		for ( auto &it : elems )
		{
			_itoa_s ( ctr++, name, sizeof ( name ), 10 );
			buff->write ( name, sizeof ( name ) );
			packCB ( it, param );
		}
	} else if ( std::holds_alternative<JSON_OBJECT::objType> ( obj->data ) )
	{
		auto &elems  = std::get<JSON_OBJECT::objType> ( obj->data );
		bufferPut32 ( buff, (uint32_t)elems.size() );
		bufferPut32 ( buff, 0 );
		for ( auto &it : elems )
		{
			strcpy_s ( name, sizeof ( name ), it.first.c_str () );
			buff->write ( name, sizeof ( name ) );
			packCB ( it.second, param );
		}
	}
}

void cJsonUnPackCb ( class vmInstance *instance, struct VAR *val, unsigned char **buff, uint64_t *len, void *param, struct VAR *(*unPack)(unsigned char **buff, uint64_t *len, void *param) )
{
	auto obj = (JSON_OBJECT *) classGetCargo ( val );
	//ver = *((unsigned long *)*buff);
	*buff += sizeof ( unsigned long );		// version number
	*len -= sizeof ( unsigned long );

	auto numVars = *((unsigned long *) *buff);
	*buff += sizeof ( unsigned long );
	*len -= sizeof ( unsigned long );

	// isArray
	auto isArray = *((unsigned long *) *buff);
	*buff += sizeof ( unsigned long );
	*len -= sizeof ( unsigned long );

	if ( isArray )
	{
		auto newVec = JSON_OBJECT::vecType ( vmAllocator<VAR *> ( instance, 0 ) );

		newVec.reserve ( numVars );
		while ( numVars-- )
		{
			*buff += NAME_LEN;
			*len -= NAME_LEN;
			newVec.push_back ( unPack ( buff, len, param ) );
		}
		obj->data = newVec;
	} else
	{
		auto newObj = JSON_OBJECT::objType ( vmAllocator<std::pair<JSON_OBJECT_NAME, VAR *>> ( instance, 0 ) );

		while ( numVars-- )
		{
			JSON_OBJECT_NAME name ( reinterpret_cast<char const *>(*buff) );
			*buff += NAME_LEN;
			*len -= NAME_LEN;
			newObj[name] = unPack ( buff, len, param );
		}
		obj->data = newObj;
	}
}

static vmInspectList *cJsonInspector ( class vmInstance *instance, bcFuncDef *func, VAR *obj, uint64_t  start, uint64_t  end )
{
	auto vars = new vmInspectList ( );
	auto jobj = (JSON_OBJECT *) classGetCargo ( obj );

	if ( std::holds_alternative<JSON_OBJECT::vecType> ( jobj->data ) )
	{
		auto &elems  = std::get<JSON_OBJECT::vecType> ( jobj->data );
		int64_t ctr = 1;
		for ( auto &it : elems )
		{
			vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, (int64_t) ctr, it )) );
		}
	} else if ( std::holds_alternative<JSON_OBJECT::objType> ( jobj->data ) )
	{
		auto &elems  = std::get<JSON_OBJECT::objType> ( jobj->data );
		for ( auto &it : elems )
		{
			vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, it.first.c_str(), it.second )) );
		}
	}
	return vars;
}

void jsonSetValue ( vmInstance *instance, VAR *obj, std::string &name , VAR *var )
{
	auto jobj = (JSON_OBJECT *) classGetCargo ( obj );

	if ( name.empty () )
	{
		if ( std::holds_alternative<std::monostate> ( jobj->data ) )
		{
			jobj->data = JSON_OBJECT::vecType ( vmAllocator<VAR *> ( instance, 0 ) );
		}

		if ( std::holds_alternative<JSON_OBJECT::vecType> ( jobj->data ) )
		{
			auto &elems  = std::get<JSON_OBJECT::vecType> ( jobj->data );
			elems.push_back ( var );
		} else
		{
			throw errorNum::scINVALID_PARAMETER;
		}
	} else
	{
		if ( std::holds_alternative<std::monostate> ( jobj->data ) )
		{
			jobj->data = JSON_OBJECT::objType ( vmAllocator<std::pair<JSON_OBJECT_NAME, VAR *>> ( instance, 0 ) );
		}

		if ( std::holds_alternative<JSON_OBJECT::objType> ( jobj->data ) )
		{
			auto &elems = std::get<JSON_OBJECT::objType> ( jobj->data );

			elems[name.c_str()] = var;
			name.clear ();
		} else
		{
			throw errorNum::scINVALID_PARAMETER;
		}
	}
}

struct JSON_STACK
{
	vmInstance			*instance;
	std::stack<VAR *>	 vars;
	bool				 first = true;
	std::string			 name;

	JSON_STACK ( vmInstance *instance ) : instance ( instance )
	{}
};

static int print ( void *ctx, JSON_type type, const JSON_value *value )
{
	JSON_STACK *stack;
	VAR *val;

	stack = (JSON_STACK *) ctx;
	vmInstance *instance = stack->instance;

	switch ( type )
	{
		case JSON_T_ARRAY_BEGIN:
		case JSON_T_OBJECT_BEGIN:
			if ( !stack->first )
			{
				classNew ( instance, "JSON", 0 );
				auto newVar = new (instance->om->allocVar ( 1 )) VAR ( instance->result );
				jsonSetValue ( instance, stack->vars.top (), stack->name, newVar );
				stack->vars.push ( newVar );
			} else
			{
				stack->vars.push ( stack->vars.top ( ) );
				stack->first = false ;
			}
			break;
		case JSON_T_ARRAY_END:
		case JSON_T_OBJECT_END:
			stack->vars.pop ( );
			break;
		case JSON_T_INTEGER:
			val = instance->om->allocVar ( 1 );
			val->type = slangType::eLONG;
			val->dat.l = value->vu.integer_value;
			jsonSetValue ( instance, stack->vars.top (), stack->name, val );
			break;
		case JSON_T_FLOAT:
			val = instance->om->allocVar ( 1 );
			val->type = slangType::eDOUBLE;
			val->dat.d = value->vu.float_value;
			jsonSetValue ( instance, stack->vars.top (), stack->name, val );
			break;
		case JSON_T_NULL:
			val = instance->om->allocVar ( 1 );
			val->type = slangType::eNULL;
			jsonSetValue ( instance, stack->vars.top (), stack->name, val );
			break;
		case JSON_T_TRUE:
			val = instance->om->allocVar ( 1 );
			val->type = slangType::eLONG;
			val->dat.l = 1;
			jsonSetValue ( instance, stack->vars.top (), stack->name, val );
			break;
		case JSON_T_FALSE:
			val = instance->om->allocVar ( 1 );
			val->type = slangType::eLONG;
			val->dat.l = 0;
			jsonSetValue ( instance, stack->vars.top (), stack->name, val );
			break;
		case JSON_T_KEY:
			stack->name = value->vu.str.value;
			break;
		case JSON_T_STRING:
			val = instance->om->allocVar ( 1 );
			*val = VAR_STR ( instance, value->vu.str.value, value->vu.str.length);
			jsonSetValue ( instance, stack->vars.top (), stack->name, val );
			break;
		default:
			break;
	}
	return 1;
}

void do_json ( vmInstance *instance, VAR *obj, unsigned char *jStr )
{
	JSON_config config;
	struct JSON_parser_struct *jc = NULL;
	JSON_STACK	 stack ( instance );
	stack.vars.push ( obj );

	init_JSON_config ( &config );

	config.callback = &print;
	config.callback_ctx = (void *) &stack;
	config.allow_comments = 1;
	config.handle_floats_manually = 0;

	jc = new_JSON_parser ( &config );

	for ( ; *jStr; jStr++ )
	{
		if ( !JSON_parser_char ( jc, *jStr ) )
		{
			delete_JSON_parser ( jc );
			throw errorNum::scINVALID_PARAMETER;
		}
	}
	JSON_parser_done ( jc );
	delete_JSON_parser ( jc );
}

VAR_REF cJasonArrayAccess ( vmInstance *instance, VAR_OBJ *var, VAR *index )
{
	auto json = var->getObj<JSON_OBJECT> ( );

	var->touch ( );

	if ( std::holds_alternative<JSON_OBJECT::objType> ( json->data ) )
	{
		auto &obj = std::get<JSON_OBJECT::objType> ( json->data );
		switch ( index->type )
		{
			case slangType::eSTRING:
				{
					auto it = obj.find ( index->dat.str.c );
					if ( it != obj.end () )
					{
						return VAR_REF ( var, it->second );
					}
					it = obj.find ( JSON_OBJECT_NAME ( index->dat.str.c, true ) );
					if ( it != obj.end () )
					{
						return VAR_REF ( var, it->second );
					}
					std::string name ( index->dat.str.c );
					VAR *value = instance->om->allocVar ( 1 );
					value->type = slangType::eNULL;
					jsonSetValue ( instance, var, name, value );
					return VAR_REF ( var, value );
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else if ( std::holds_alternative<JSON_OBJECT::vecType> ( json->data ) )
	{
		auto &vec= std::get<JSON_OBJECT::vecType> ( json->data );
		switch ( index->type )
		{
			case slangType::eLONG:
				{
					if ( (index->dat.l > 0) && (index->dat.l <= (int64_t) vec.size ()) )
					{
						return VAR_REF ( var, vec[index->dat.l - 1] );
					}
					throw errorNum::scINVALID_PARAMETER;
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}
}

VAR cJasonDefaultAssign ( vmInstance *instance, VAR_OBJ *var, VAR_STR *index, VAR *value )
{
	auto json = var->getObj<JSON_OBJECT> ( );

	var->touch ( );

	if ( std::holds_alternative<JSON_OBJECT::objType> ( json->data ) )
	{
		auto &obj = std::get<JSON_OBJECT::objType> ( json->data );
		switch ( index->type )
		{
			case slangType::eSTRING:
				{
					auto it = obj.find ( index->dat.str.c );
					if ( it != obj.end () )
					{
						it->second = new (instance->om->allocVar ( 1 )) VAR ( *value );
						assert ( it->second );
						return VAR_REF ( var, it->second );
					}
					it = obj.find ( JSON_OBJECT_NAME ( index->dat.str.c, true ) );
					if ( it != obj.end () )
					{
						it->second = new (instance->om->allocVar ( 1 )) VAR ( *value );
						assert ( it->second );
						return VAR_REF ( var, it->second );
					}
					std::string name ( index->dat.str.c );
					VAR *newValue = new (instance->om->allocVar ( 1 )) VAR (*value);
					jsonSetValue ( instance, var, name, newValue );
					return VAR_REF ( var, newValue );
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else if ( std::holds_alternative<JSON_OBJECT::vecType> ( json->data ) )
	{
		auto &vec= std::get<JSON_OBJECT::vecType> ( json->data );
		switch ( index->type )
		{
			case slangType::eLONG:
				{
					if ( (index->dat.l > 0) && (index->dat.l <= (int64_t)vec.size ()) )
					{
						assert ( value );
						VAR *newValue = new (instance->om->allocVar ( 1 )) VAR (*value);
						vec[index->dat.l - 1] = newValue;
						return VAR_REF ( var, newValue );
					}
					throw errorNum::scINVALID_PARAMETER;
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}
}

void cJasonNew ( vmInstance *instance, VAR_OBJ *obj, char const *jStr )
{
	obj->makeObj<JSON_OBJECT>( instance );
	do_json ( instance, obj, (unsigned char *) jStr );
}

static void toXml ( JSON_OBJECT *jobj, char const *name, BUFFER &buff );

static void toXml ( VAR *var, char const *name, BUFFER &buff )
{
	switch ( var->type )
	{
		case slangType::eLONG:
			if ( name )
			{
				buff.write ( "<" );
				buff.write ( name );
				buff.write ( ">" );
			}
			buff.printf ( "%I64i", var->dat.l );
			if ( name )
			{
				buff.write ( "</" );
				buff.write ( name );
				buff.write ( ">\r\n" );
			}
			break;
		case slangType::eDOUBLE:
			if ( name )
			{
				buff.write ( "<" );
				buff.write ( name );
				buff.write ( ">" );
			}
			buff.printf ( "%d", var->dat.d );
			if ( name )
			{
				buff.write ( "</" );
				buff.write ( name );
				buff.write ( ">\r\n" );
			}
			break;
		case slangType::eSTRING:
			if ( name )
			{
				buff.write ( "<" );
				buff.write ( name );
				buff.write ( ">" );
			}
			buff.printf ( "%s", var->dat.str.c );
			if ( name )
			{
				buff.write ( "</" );
				buff.write ( name );
				buff.write ( ">\r\n" );
			}
			break;
		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT_ROOT:
		case slangType::eREFERENCE:
			toXml ( var->dat.ref.v, name, buff );
			break;
		case slangType::eARRAY_FIXED:
			for ( int64_t index = 0; index < var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex; index++ )
			{
				toXml ( &var[index], name, buff );
			}
			break;
		case slangType::eARRAY_SPARSE:
			var = var->dat.aSparse.v;
			while ( var )
			{
				toXml ( var->dat.aElem.var, name, buff );
				var = var->dat.aElem.next;
			}
			break;
		case slangType::eOBJECT:
			{
				auto classDef = var->dat.obj.classDef;

				if ( !strccmp ( classDef->name, "json" ) )
				{
					auto jobj = VAR_OBJ(var).getObj<JSON_OBJECT> ( );

					toXml ( jobj, name, buff );
				} else
				{
					buff.write ( "<" );
					buff.write ( classDef->name );
					buff.write ( ">\r\n" );
					for ( size_t loop = 0; loop < classDef->nElements; loop++ )
					{
						auto elem = &classDef->elements[loop];
						switch ( classDef->elements[loop].type )
						{
							case fgxClassElementType::fgxClassType_iVar:
							case fgxClassElementType::fgxClassType_static:
								if ( elem->isStatic )
								{
									toXml ( classDef->loadImage->globals[elem->methodAccess.globalEntry], elem->name, buff );
								} else
								{
									if ( elem->isVirtual )
									{
										toXml ( &var[var->dat.obj.vPtr[elem->methodAccess.vTableEntry].delta], elem->name, buff );
									} else
									{
										toXml ( &var[elem->methodAccess.iVarIndex], elem->name, buff );
									}
								}
								break;
							default:
								break;
						}
					}
					buff.write ( "</" );
					buff.write ( classDef->name );
					buff.write ( ">\r\n" );
				}
			}
			break;
		default:
			break;
	}
}

static void toXml ( JSON_OBJECT *jobj, char const *name, BUFFER &buff )
{
	if ( std::holds_alternative<JSON_OBJECT::vecType> ( jobj->data ) )
	{
		auto &vec= std::get<JSON_OBJECT::vecType> ( jobj->data );
		for ( auto const &it : vec )
		{
			if ( name )
			{
				buff.write ( "<" );
				buff.write ( name );
				buff.write ( ">" );
			}
			toXml ( it, 0, buff );
			if ( name )
			{
				buff.write ( "</" );
				buff.write ( name );
				buff.write ( ">\r\n" );
			}
		}
	} else if ( std::holds_alternative<JSON_OBJECT::objType> ( jobj->data ) )
	{
		auto &obj = std::get<JSON_OBJECT::objType> ( jobj->data );
		if ( name )
		{
			buff.write ( "<" );
			buff.write ( name );
			buff.write ( ">" );
		}
		for ( auto const &it : obj )
		{
			toXml ( it.second, it.first.c_str(), buff );
		}
		if ( name )
		{
			buff.write ( "</" );
			buff.write ( name );
			buff.write ( ">" );
		}
	}
}

static VAR_STR cJsonToXml ( vmInstance *instance, VAR_OBJ *obj )
{
	auto jobj = obj->getObj<JSON_OBJECT> ( );

	BUFFER	buff;

	toXml ( jobj, 0, buff );

	return VAR_STR ( instance, buff );
}

void builtinJSONInit ( class vmInstance *instance, opFile *file )
{
	/************************** NODE ****************************/

	REGISTER ( instance, file );
		FUNC ( "objToJson", vmToJson, DEF ( 2, "false" ), DEF ( 3, "false" ), DEF ( 4, "false" ) );

		// stupid class to maintain backwards compatibility with old engine... no need for this as we support name/value pairs to define an associative array and [] to declare a variable length array so we
		// support json definitions directly
		CLASS ( "json" );
			METHOD ( "new", cJasonNew );
			ACCESS ( "default", cJasonArrayAccess );
			ASSIGN ( "default", cJasonDefaultAssign );
			METHOD ( "toXml", cJsonToXml );
			OP ( "[", cJasonArrayAccess );
			OP ( "[", cJasonDefaultAssign );
			GCCB ( cJsonGcCb, cJsonCopyCb );
			PACKCB ( cJsonPackCb, cJsonUnPackCb );
			INSPECTORCB ( cJsonInspector );
		END;
	END;
}
