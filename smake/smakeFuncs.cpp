#include <iostream>
#include <set>
#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include "stdlib.h"

#include "Utility/stringi.h"
#include "Utility/buffer.h"
#include "smake.h"

bool maker::targetMatch ( char const *str, char const *match, std::string &base )
{
	while ( *match )
	{
		if ( *match == '%' )
		{
			if ( targetMatch ( str, match + 1, base ) ) return true;
			base += *str;
			if ( *str && targetMatch ( str + 1, match, base ) ) return true;
			base.pop_back ( );
			return false;
		} else
		{
			if ( caseInsenstiveTable[(uint8_t)*(str++)] != caseInsenstiveTable[(uint8_t) *(match++)] ) return false;
		}
	}
	return !*str;
}

std::vector<stringi> maker::getParamVec ( char const *&ptr, char const *delim, char const *stop )
{
	std::vector<stringi> result;
	stringi paramStr;
	size_t perenCount = 0;

	while ( isspace ( ptr) )
	{
		ptr++;
	}

	while ( *ptr )
	{
		switch ( *ptr )
		{
			case '"':
				paramStr += *ptr++;
				do
				{
					if ( ptr[0] == '\\' && ptr[1] == '"' )
					{
						ptr += 2;
						paramStr += '"';
					} else
					{
						paramStr += *ptr++;
					}
				} while ( *ptr && *ptr != '"' );
				break;
			case '(':
				paramStr += *ptr++;
				perenCount++;
				break;
			case ')':
				if ( !perenCount )
				{
					goto finish_parse;
				}
				paramStr += *ptr++;
				perenCount--;
				break;
			default:
				if ( !perenCount && stop && strrchr ( stop, *ptr ) )
				{
					goto finish_parse;
				} else if ( !perenCount && strrchr ( delim, *ptr ) )
				{
					result.push_back ( paramStr.trim ( ) );
					paramStr = "";
				} else
				{
					paramStr += *ptr;
				}
				ptr++;
				break;
		}
	}
finish_parse:
	if ( paramStr.size() )
	{
		result.push_back ( paramStr.trim ( ) );
	}
	return result;
}

std::vector<stringi> maker::getParamVec (  stringi const &ptr, char const *delim )
{
	auto strPtr = ptr.c_str ( );
	return getParamVec ( strPtr, delim, nullptr );
}

std::vector<stringi> maker::getParamVec ( char const *&ptr, char const *delim )
{
	return getParamVec ( ptr, delim, nullptr );
}

std::pair<std::vector<stringi>, std::vector<stringi>> maker::getParamVec ( stringi const& ptr, char const* delim, char const *split )
{
	auto strPtr = ptr.c_str ();
	std::vector<stringi> first = getParamVec ( strPtr, delim, split );
	std::vector<stringi> second;
	if ( *strPtr )
	{
		strPtr++;
		second = getParamVec ( strPtr, delim, nullptr );
	}
	return { first, second };
}

/* ------------------------------------- start of macro functions ---------------------------------------------- */
/*
		NOTE: the parameter count validation is done by the caller... maker::functionList contains the exact or minimum number of parameters and will not call a function unless this is met
*/
stringi maker::macroFuncSubst ( std::vector<stringi> const &params )
{
	stringi ret;
	auto &pattern = params[0];
	auto &replacement = params[1];
	return stringi ( params[2] ).findAndReplace ( pattern, replacement );
}

stringi maker::macroFuncPatsubst ( std::vector<stringi> const &params )
{
	stringi ret;
	auto &pattern = params[0];
	auto &replacement= params[1];
	auto textVec = getParamVec ( params[2], " " );
	for ( size_t index = 2; index < textVec.size ( ); index++ )
	{
		std::string stem;
		stringi val = textVec[index];
		if ( targetMatch ( textVec[index], pattern, stem ) )
		{
			val = replacement;
			val = val.findAndReplace ( "%", stem );
		}
		if ( ret.size ( ) )
		{
			ret += " ";
		}
		ret += val;
	}
	return ret;
}
stringi maker::macroFuncStrip ( std::vector<stringi> const &params )
{
	stringi ret;
	for ( stringi it : params )
	{
		if ( ret.size ( ) )
		{
			ret += " ";
		}
		ret += it.trim ( );
	}
	return "";
}
stringi maker::macroFuncFindstring ( std::vector<stringi> const &params )
{
	if ( params[1].find ( params[0] ) != stringi::npos )
	{
		return params[0].c_str ( );
	}
	return "";
}
stringi maker::macroFuncFilter ( std::vector<stringi> const &params )
{
	stringi result;
	auto patVec = getParamVec ( params[0], " " );
	auto textVec = getParamVec ( params[1], " " );
	for ( auto &textIt : textVec )
	{
		for ( auto &patIt : patVec )
		{
			std::string stem;
			if ( targetMatch ( textIt, patIt, stem ) )
			{
				if ( result.size ( ) )
				{
					result += " ";
				}
				result += textIt;
				break;
			}
		}
	}
	return result;
}
stringi maker::macroFuncFilterOut ( std::vector<stringi> const &params )
{
	stringi result;
	auto patVec = getParamVec ( params[0], " " );
	auto textVec = getParamVec ( params[1], " " );
	for ( auto &textIt : textVec )
	{
		bool found = false;
		for ( auto &patIt : patVec )
		{
			std::string stem;
			if ( targetMatch ( textIt, patIt, stem ) )
			{
				found = true;
				break;
			}
		}
		if ( !found )
		{
			if ( result.size ( ) )
			{
				result += " ";
			}
			result += textIt;
		}
	}
	return result;
}
stringi maker::macroFuncSort ( std::vector<stringi> const &params )
{
	std::vector<stringi> x = params;
	std::sort ( x.begin ( ), x.end ( ) );
	stringi ret;
	for ( auto &it : x )
	{
		if ( ret.size ( ) )
		{
			ret += " ";
		}
		ret += it;
	}
	return ret;
}
stringi maker::macroFuncWord ( std::vector<stringi> const &params )
{
	auto index = atoi ( params[0] );
	auto vec = getParamVec ( params[1], " \t" );
	if ( index && index - 1 < (int)vec.size ( ) )
	{
		return vec[(size_t)(index - 1)];
	}
	return "";
}
stringi maker::macroFuncWordlist ( std::vector<stringi> const &params )
{
	auto start = atoi ( params[0] );
	auto stop = atoi ( params[1] );
	auto words = getParamVec ( params[2], " \t" );
	stringi ret;
	for ( auto loop = start - 1; loop < words.size ( ) && loop < stop; loop++ )
	{
		if ( ret.size ( ) )
		{
			ret += " ";
		}
		ret += words[loop];
	}
	return ret;
}
stringi maker::macroFuncWords ( std::vector<stringi> const &params )
{
	char ret[65];
	_ui64toa_s ( params.size ( ), ret, sizeof (ret ), 10 );
	return ret;
}
stringi maker::macroFuncFirstword ( std::vector<stringi> const &params )
{
	return params[0];
}
stringi maker::macroFuncLastword ( std::vector<stringi> const &params )
{
	return params[params.size ( ) - 1];
}
stringi maker::macroFuncDir ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &it : params )
	{
		std::filesystem::path t ( it.c_str () );
		if ( res.size ( ) )
		{
			res += " ";
		}
		res += stringi ( std::filesystem::path ( t ).remove_filename ().string ().c_str () );
	}
	return res;
}
stringi maker::macroFuncNotdir ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &it : params )
	{
		std::filesystem::path t ( it.c_str () );
		if ( res.size ( ) )
		{
			res += " ";
		}
		res += stringi ( t.filename().string().c_str () );
	}
	return res;
}
stringi maker::macroFuncSuffix ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &it : params )
	{
		std::filesystem::path t ( it.c_str () );
		if ( res.size () )
		{
			res += " ";
		}
		res += stringi ( t.extension().string ().c_str () );
	}
	return res;
}
stringi maker::macroFuncBasename ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &it : params )
	{
		std::filesystem::path t ( it.c_str () );
		t.replace_extension ();
		if ( res.size () )
		{
			res += " ";
		}
		res += stringi ( t.string ().c_str () );
	}
	return res;
}

stringi maker::macroFuncAddsuffix ( std::vector<stringi> const &params )
{  
	auto vec = getParamVec ( params[1], " " );
	stringi result;
	for ( auto &it : vec )
	{
		if ( result.size ( ) )
		{
			result += " ";
		}
		result += it;
		result += params[0];
	}
	return result;
}
stringi maker::macroFuncAddprefix ( std::vector<stringi> const &params )
{
	auto vec = getParamVec ( params[1], " " );
	stringi result;
	for ( auto &it : vec )
	{
		if ( result.size ( ) )
		{
			result += " ";
		}
		result += params[0];
		result += it;
	}
	return result;
}
stringi maker::macroFuncJoin ( std::vector<stringi> const &params )
{
	stringi res;
	auto lVec = getParamVec ( applySubstitutions ( params[0] ), " " );
	auto rVec = getParamVec ( applySubstitutions ( params[1] ), " " );
	size_t index = 0;
	while ( (index < lVec.size ( )) || (index < rVec.size ( )) )
	{
		stringi s;
		if ( index < lVec.size ( ) )
		{
			s += lVec[index];
		}
		if ( index < rVec.size ( ) )
		{
			s += rVec[index];
		}
		if ( res.size ( ) )
		{
			res += " ";
		}
		res += s;
	}
	return res;
}
stringi maker::macroFuncWildcard ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &it : params )
	{
		for ( auto &entry : std::filesystem::directory_iterator ( it.c_str() ) )
		{
			if ( entry.is_regular_file ( ) )
			{
				if ( res.size ( ) )
				{
					res += " ";
				}
				res += entry.path ( ).string ( ).c_str ( );
			}
		}
	}
	return res;
}
stringi maker::macroFuncRealpath ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &entry : params )
	{
		if ( res.size ( ) )
		{
			res += " ";
		}
		res += std::filesystem::canonical ( std::filesystem::path ( entry.c_str ( ) ) ).string ( ).c_str ( );
	}
	return res;
}
stringi maker::macroFuncAbspath ( std::vector<stringi> const &params )
{
	stringi res;
	for ( auto &entry : params )
	{
		if ( res.size ( ) )
		{
			res += " ";
		}
		res += std::filesystem::canonical ( std::filesystem::path ( entry.c_str ( ) ) ).string ( ).c_str ( );
	}
	return res;
}
stringi maker::macroFuncIf ( std::vector<stringi> const &params )
{
	stringi cond = params[0];
	cond.trim ( );
	cond = applySubstitutions ( cond ).trim ( );
	if ( cond.size ( ) )
	{
		return applySubstitutions ( params[1] ).trim ( );
	} else
	{
		return applySubstitutions ( params[2] ).trim ( );
	}
	return "";
}
stringi maker::macroFuncAnd ( std::vector<stringi> const &params )
{
	for ( auto &it : params )
	{
		auto var = applySubstitutions ( it );
		if ( var.size ( ) )
		{
			return var;
		}
	}
	return "";
}
stringi maker::macroFuncOr ( std::vector<stringi> const &params )
{
	stringi var;
	for ( auto &it : params )
	{
		var = applySubstitutions ( it ).trim ( );
		if ( !var.size ( ) )
		{
			break;
		}
	}
	return var;
}
stringi maker::macroFuncForeach ( std::vector<stringi> const &params )
{
	stringi var = stringi ( "$(" ) + applySubstitutions ( params[0] ).trim ( ) + ")";
	auto list = getParamVec ( applySubstitutions ( params[1] ), " \t" );

	stringi res;
	for ( auto &it : list )
	{
		stringi cmd = params[2];
		cmd = cmd.findAndReplace ( var, it );
		if ( res.size ( ) )
		{
			res += " ";
		}
		res += applySubstitutions ( cmd ).trim ( );
	}
	return res;
}

static class fileCleanup
{
	std::vector<stringi>	files;

public:
	~fileCleanup ()
	{
		for ( auto &it : files )
		{
			std::error_code ec;
			std::filesystem::remove ( it.c_str(), ec );
		}
	}
	void add ( stringi const &fName )
	{
		files.push_back ( fName );
	}
} cleaner;

stringi maker::macroFuncFile ( std::vector<stringi> const &params )
{
	stringi fName = params[0];
	fName = applySubstitutions ( fName );
	auto fileName = fName.c_str ();
	bool isRead = false;
	bool isAppend = false;
	bool isTemp = false;

	if ( fileName[0] == '>' )
	{
		fileName++;
		if ( fileName[0] == '>' )
		{
			isAppend = true;
			fileName++;
		}
		if ( fileName[0] == '^' )
		{
			isTemp = true;
			fileName++;
		}
	} else if ( fileName[0] == '<' )
	{
		fileName++;
		isRead = true;
	} else
	{
		throw "invalid file mode";
	}

	stringi trimmedFileName = stringi ( fileName ).trim ();

	if ( isRead )
	{
		FILE *file = fopen ( trimmedFileName, "rt" );
		if ( !file )
		{
			throw "unable to open input file for reading";
		}
		BUFFER buff;

		fseek ( file, 0, SEEK_END );
		auto size = ftell ( file );
		fseek ( file, 0, SEEK_SET );
		 
		buff.makeRoom ( size );
		auto nRead = fread ( buff.data<char *> (), 1, size, file );
		buff.assume ( nRead );
		fclose ( file );
		return buff.data<char *> ();
	} else
	{
		char const *mode = "wt";
		if ( isAppend )
		{
			mode = "at";
		}
		FILE *file = fopen ( trimmedFileName, mode );
		if ( !file )
		{
			throw "unable to file to write";
		}
		fwrite ( params[1].c_str(), params[1].size(), 1, file );
		fclose ( file );
		if ( isTemp )
		{
			cleaner.add ( trimmedFileName );
		}
	}

	return "";
}

stringi maker::findAndReplace ( stringi s, std::vector < std::pair<stringi, stringi>> &replaceList )
{
	for ( auto &it : replaceList )
	{
		s = s.findAndReplace ( std::get<0> ( it ), std::get<1> ( it ) );
	}
	return applySubstitutions ( s ).trim ();
}

stringi maker::macroFuncCall ( std::vector<stringi> const &params )
{
	auto &target = params[0];
	stringi s;

	std::vector < std::pair<stringi, stringi>> replaceList;

	replaceList.push_back ( { "$(0)", target } );

	for ( size_t index = 1; index < params.size (); index++ )
	{
		stringi rep = stringi ( "$(" ) + index + ")";
		replaceList.push_back ( { rep, params[index] } );
	}

	auto subs = variables.find ( target );
	if ( subs != variables.end () )
	{
		s = subs->second.subst;
	} else
	{
		throw "variable not found";
	}
	s = findAndReplace ( s, replaceList );
	return s;
}

stringi maker::macroFuncValue ( std::vector<stringi> const &params )
{
	auto subs = variables.find ( params[0] );
	if ( subs != variables.end ( ) )
	{
		return subs->second.subst;
	}
	return "";
}

stringi maker::macroFuncEval ( std::vector<stringi> const &params )
{
	std::string str = params[0].operator const std::string &();
	std::stringstream stream ( str );
	processStream ( stream );
	return "";
}

stringi maker::macroFuncOrigin ( std::vector<stringi> const &params )
{
	auto var = variables.find ( params[0] );
	if ( var != variables.end ( ) )
	{
		switch ( var->second.orig )
		{
			case maker::origin::o_default:
				return "default";
			case maker::origin::o_environment:
				return "environment";
			case maker::origin::o_environment_override:
				return "environment override";
			case maker::origin::o_file:
				return "file";
			case maker::origin::o_command_line:
				return "command line";
			case maker::origin::o_override:
				return "override";
			case maker::origin::o_automatic:
				return "automatic";
			default:
				return "undefined";
		}
	}
	return "undefined";
}
stringi maker::macroFuncFlavor ( std::vector<stringi> const &params )
{
	auto var = variables.find ( params[0] );
	if ( var != variables.end () )
	{
		auto ptr = var->second.subst.c_str ();
		while ( *ptr )
		{
			if ( ptr[0] == '\n' && ptr[1] == '\t' )
			{
				// recipe entry... this is deferred processing so don't handle it right now
				ptr++;
				while ( *ptr && *ptr != '\n' )
				{
					ptr++;
				}
			} else
			{
				if ( !memcmp ( ptr, "$(", 2 ) )
				{
					return "recursive";
				} else if ( !memcmp ( ptr, "$$", 2 ) )
				{
					ptr += 2;
				} else
				{
					ptr++;
				}
			}
		}
		return "simple";
	}
	return "undefined";
}
stringi maker::macroFuncError ( std::vector<stringi> const &params )
{
	for ( auto &it : params )
	{
		std::cout << it.c_str ( );
	}
	std::cout << std::endl;
	return "";
}
stringi maker::macroFuncWarning ( std::vector<stringi> const &params )
{
	for ( auto &it : params )
	{
		std::cout << it.c_str ( );
	}
	std::cout << std::endl;
	return "";
}
stringi maker::macroFuncInfo ( std::vector<stringi> const &params )
{
	for ( auto &it : params )
	{
		std::cout << it.c_str ( );
	}
	std::cout << std::endl;
	return "";
}
stringi maker::macroFuncShell ( std::vector<stringi> const &params )
{
	int			retCode = 0;
	stringi		tmpCmd;

	tmpCmd = "cmd /C ";
	for ( auto &it : params )
	{
		tmpCmd = tmpCmd + it;
	}
	auto close = [&retCode]( FILE *file )
	{
		retCode = _pclose ( file );
	};

	std::unique_ptr<FILE, decltype(close)> pipe ( _popen ( tmpCmd, "r" ), close );
	if ( !pipe )
	{
		throw std::runtime_error ( "popen() failed!" );
	}

	BUFFER buff;
	buff.makeRoom ( 8192 );
	while ( fgets ( bufferBuff ( &buff ) + buff.size (), (int) bufferFree ( &buff ), pipe.get () ) != nullptr )
	{
		buff.assume ( strlen ( bufferBuff ( &buff ) + buff.size () ) );
		buff.makeRoom ( 8192 );
	}
	return stringi ( bufferBuff ( &buff ) );
}

