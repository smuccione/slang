// compilerTest.cpp : Defines the entry point for the console application.
//

#include "Utility/settings.h"

#include <process.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>
#include <stdarg.h>

#include "Utility/buffer.h"
#include "compilerPreprocessor.h"
#include "Target/fileCache.h"

#if !defined ( __clang__ )
#pragma inline_depth(255)
#pragma inline_recursion(on)
#endif

#define BOOST_WAVE_SUPPORT_THREADING 1
#define BOOST_FILESYSTEM_VERSION 3

#pragma warning (disable: 4503)
#pragma warning (disable: 4355) // 'this' used in base member initializer list
#pragma warning (disable: 4800) // forcing value to bool 'true' or 'false'
#pragma warning (disable: 4996) // forcing value to bool 'true' or 'false'

#define BOOST_WAVE_USE_STRICT_LEXER	1
#define BOOST_WAVE_SUPPORT_LONGLONG_INTEGER_LITERALS 1

#include <boost/config.hpp>     //  global configuration information

///////////////////////////////////////////////////////////////////////////////
//  include required boost libraries
#include <boost/assert.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/filesystem/config.hpp>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class

using namespace boost;

using std::string;
using std::getline;
using std::ifstream;
using std::cout;
using std::cerr;
using std::endl;
using std::ostream;

// change the load context to add the file to the global file cache for monitoring
struct my_load_file_to_string
{
	template <typename IterContextT>
	class inner
	{
		public:
		template <typename PositionT>
		static void init_iterators ( IterContextT &iter_ctx, PositionT const &act_pos, wave::language_support language )
		{
			static std::map<std::string, std::string> theFiles;

			typedef typename IterContextT::iterator_type iterator_type;

			std::streambuf* buffer = nullptr;

			// read in the file
			std::ifstream instream ( iter_ctx.filename.c_str ( ) );
			if ( !instream.is_open ( ) )
			{
				//BOOST_WAVE_THROW_CTX(iter_ctx.ctx, wave::preprocess_exception,
				//  wave::bad_include_file, iter_ctx.filename.c_str(), act_pos);
				std::cout << "error: not open" << std::endl;
				return;
			}
			instream.unsetf ( std::ios::skipws );
			buffer = instream.rdbuf ( );

			iter_ctx.instring.assign (
				std::istreambuf_iterator<char> ( buffer ),
				std::istreambuf_iterator<char> ( ) );

			iter_ctx.first = iterator_type (
				iter_ctx.instring.begin ( ), iter_ctx.instring.end ( ),
				PositionT ( iter_ctx.filename ), language );
			iter_ctx.last = iterator_type ( );

			// monitor the file for changes so we can rebuild if necessary
			globalFileCache.monitor ( iter_ctx.filename.c_str ( ) );
		}

		private:
		std::string instring;
	};
};

class custom_directives_hooks : public wave::context_policies::default_preprocessing_hooks
{
public:

	template <typename ContextT, typename TokenT>
	bool may_skip_whitespace ( ContextT const &ctx, TokenT &token, bool &skipped_newline )
	{
		return false;
	}
};

char *compPreprocessor ( char const *fName, char const *src, bool isSlang )
{
	BUFFER			buff ( 1024*1024*16 );
	if ( 0 && isSlang )
	{
		boost::wave::util::file_position_type current_position;

		std::istringstream instream ( src );
		std::string instring;

//		instream.unsetf ( std::ios::skipws );
		instring = std::string ( std::istreambuf_iterator<char> ( instream.rdbuf ( ) ), std::istreambuf_iterator<char> ( ) );

		typedef boost::wave::cpplexer::lex_token<> token_type;

		typedef boost::wave::cpplexer::lex_iterator<token_type> lex_iterator_type;
		typedef boost::wave::context<std::string::iterator, lex_iterator_type, my_load_file_to_string, custom_directives_hooks> context_type;

		context_type ctx ( instring.begin ( ), instring.end ( ), fName );

		ctx.set_language (
			boost::wave::language_support (
				boost::wave::support_cpp0x
				| boost::wave::support_option_long_long
//				| boost::wave::support_option_emit_line_directives
				| boost::wave::support_option_include_guard_detection
				| boost::wave::support_option_emit_pragma_directives
				| boost::wave::support_option_insert_whitespace
				| boost::wave::support_option_preserve_comments
			) );
//    ctx.set_language(boost::wave::support_cpp0x);
		ctx.set_language ( boost::wave::enable_prefer_pp_numbers ( ctx.get_language ( ) ) );
	//	ctx.set_language ( boost::wave::enable_emit_contnewlines ( ctx.get_language ( ) ) );
	//   ctx.set_language(boost::wave::support_option_include_guard_detection);
	//   ctx.set_language(boost::wave::support_option_insert_whitespace);
	//	 ctx.remove_macro_definition(std::string ("__cplusplus"), true );
		ctx.add_macro_definition ( "_SLANG_=TRUE", true );

		#define REGISTER_ERROR( errNum, errTxt ) \
		{	\
			char tmp[256];	\
			sprintf_s ( tmp, sizeof ( tmp ), "%s=%u", #errNum, int(errorNum::errNum) ); \
			ctx.add_macro_definition ( std::string (tmp), true ); \
		}
		ERROR_TABLE;


		context_type::iterator_type first = ctx.begin ( );
		context_type::iterator_type last = ctx.end ( );
		context_type::token_type current_token;

		try
		{
			current_position = (*first).get_position ();

			std::size_t lastLine = first->get_position ().get_line();
			char const *file = first->get_position().get_file().c_str();
			auto x = first->get_expand_position ();

			while ( !(first == last) )
			{
				current_token = *first;
				current_position = (*first).get_expand_position ( );
				if ( current_token == boost::wave::T_NEWLINE )
				{
					bufferWrite ( &buff, (*first).get_value ().c_str (), (*first).get_value ().length () );
					++first;
					lastLine = current_position.get_line () + 1;
				} else
				{
					if ( current_position.get_file().c_str() != file || current_position.get_line () != lastLine )
					{
						buff.printf ( "\r\n[[pos: %zu %zu %s]]", current_position.get_column (), current_position.get_line (), current_position.get_file().c_str () );
					}
					bufferWrite ( &buff, (*first).get_value ().c_str (), (*first).get_value ().length () );
					lastLine = current_position.get_line ();
					file = current_position.get_file().c_str();
					++first;
				}
				bufferWrite ( &buff, (*first).get_value ().c_str (), (*first).get_value ().length () );
				++first;
			}
		} catch ( boost::wave::cpp_exception const& e )
		{
// some preprocessing error
			cerr
				<< e.file_name ( ) << "(" << e.line_no ( ) << "): "
				<< e.description ( ) << endl;
			return _strdup ( "" );
		} catch ( std::exception const& e )
		{
// use last recognized token to retrieve the error position
			cerr
				<< current_token.get_position ( ).get_file ( )
				<< "(" << current_token.get_position ( ).get_line ( ) << "): "
				<< "unexpected exception: " << e.what ( )
				<< endl;
			return _strdup ( "" );
		} catch ( ... )
		{
// use last recognized token to retrieve the error position
			cerr
				<< current_token.get_position ( ).get_file ( )
				<< "(" << current_token.get_position ( ).get_line ( ) << "): "
				<< "unexpected exception." << endl;
			return _strdup ( "" );
		}
		bufferPut8 ( &buff, 0 );

		return (char *) bufferDetach ( &buff );
	} else
	{
		buff.write ( src, strlen ( src ) );
		bufferPut8 ( &buff, 0 );
		return (char *) bufferDetach ( &buff );
	}
}
