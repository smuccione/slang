#include <iostream>
#include <set>
#include <filesystem>
#include <string>
#include <map>
#include <list>
#include <tuple>
#include <stack>
#include <sstream>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <chrono>
#include <mutex>
#include <atomic>
#include <queue>
#include <chrono>
#include <future>
#include <semaphore>

#include "stdlib.h"

#include "Utility/stringi.h"
#include "Utility/util.h"
#include "Utility/buffer.h"
#include "Utility/sourceFile.h"

#include "smake.h"

static LARGE_INTEGER perfFreq;
static BOOL configured = (QueryPerformanceFrequency ( &perfFreq ), perfFreq.QuadPart = perfFreq.QuadPart / 1000, true);

maker::maker ()
{
	// postive numbers indicate exactly that number of parameters, negative indicate at least that number, but may be more (absolute value of negative)
	functionList = {
		{ stringi ( "subst" ),		{ 3,	&maker::macroFuncSubst, true } },
		{ stringi ( "patsubst" ),	{ 3,	&maker::macroFuncPatsubst, true } },
		{ stringi ( "strip" ),		{ 1,	&maker::macroFuncStrip, true } },
		{ stringi ( "findstring" ), { 2,	&maker::macroFuncFindstring, true } },
		{ stringi ( "filter" ),		{ -2,	&maker::macroFuncFilter, true } },
		{ stringi ( "filter-out" ), { -2,	&maker::macroFuncFilterOut, true } },
		{ stringi ( "sort" ),		{ 1,	&maker::macroFuncSort, true } },
		{ stringi ( "word" ),		{ 2,	&maker::macroFuncWord, true } },
		{ stringi ( "wordlist" ),	{ 3,	&maker::macroFuncWordlist, true } },
		{ stringi ( "words" ),		{ 1,	&maker::macroFuncWords, true } },
		{ stringi ( "firstword" ),	{ -1,	&maker::macroFuncFirstword, true } },
		{ stringi ( "lastword" ),	{ -1,	&maker::macroFuncLastword, true } },
		{ stringi ( "dir" ),		{ -1,	&maker::macroFuncDir, true } },
		{ stringi ( "notdir" ),		{ -1,	&maker::macroFuncNotdir, true } },
		{ stringi ( "suffix" ),		{ -1,	&maker::macroFuncSuffix, true } },
		{ stringi ( "basename" ),	{ -1,	&maker::macroFuncBasename, true } },
		{ stringi ( "addsuffix" ),	{ -2,	&maker::macroFuncAddsuffix, true } },
		{ stringi ( "addprefix" ),	{ -2,	&maker::macroFuncAddprefix, true } },
		{ stringi ( "join" ),		{ 2,	&maker::macroFuncJoin, true } },
		{ stringi ( "wildcard" ),	{ 1,	&maker::macroFuncWildcard, true } },
		{ stringi ( "realpath" ),	{ -1,	&maker::macroFuncRealpath, true } },
		{ stringi ( "abspath" ),	{ -1,	&maker::macroFuncAbspath, true } },
		{ stringi ( "if" ),			{ -2,	&maker::macroFuncIf, false } },
		{ stringi ( "and" ),		{ -1,	&maker::macroFuncAnd, false } },
		{ stringi ( "or" ),			{ -1,	&maker::macroFuncOr, false } },
		{ stringi ( "foreach" ),	{ 3,	&maker::macroFuncForeach, false } },
		{ stringi ( "file" ),		{ 2,	&maker::macroFuncFile, true } },
		{ stringi ( "call" ),		{ -1,	&maker::macroFuncCall, true } },
		{ stringi ( "value" ),		{ 1,	&maker::macroFuncValue, false } },
		{ stringi ( "eval" ),		{ 1,	&maker::macroFuncEval, true } },
		{ stringi ( "origin" ),		{ 1,	&maker::macroFuncOrigin, true } },
		{ stringi ( "flavor" ),		{ 1,	&maker::macroFuncFlavor, true } },
		{ stringi ( "error" ),		{ 1,	&maker::macroFuncError, true } },
		{ stringi ( "warning" ),	{ 1,	&maker::macroFuncWarning, true } },
		{ stringi ( "info" ),		{ 1,	&maker::macroFuncInfo, true } },
		{ stringi ( "shell" ),		{ 1,	&maker::macroFuncShell, true } },
	};
};

stringi getGuid ( void )
{
	stringi ret;
	GUID guid;
	CoCreateGuid ( &guid );

	BYTE *str;
	UuidToString ( (UUID *) &guid, &str );

	ret = stringi ( (char const *) str );

	RpcStringFree ( &str );

	return ret;
}

void maker::runBat ( stringi const &target, bool isPhonyTarget, stringi const &shCode, bool asBatch, bool queuePrints )
{
	int			retCode = 0;
	stringi		cmdToRun;
	bool		ignoreErrors = false;

	if ( jobServer )
	{
		WaitForSingleObject ( jobRunSemaphore, INFINITE );
	}

	// if build failed, just exit immediately
	if ( fatalBuild )
	{
		if ( jobServer )
		{
			// cleanup any semaphores we've 
			ReleaseSemaphore ( jobRunSemaphore, 1, 0 );
		}
		return;
	}

//	printf ( "making: %s\r\n", target.c_str () );

	try
	{
		BUFFER buff;

		if ( asBatch )
		{
			cmdToRun = cmdToRun + "smake." + getGuid () + ".bat";

			FILE *file = fopen ( cmdToRun, "w+" );
			if ( !file )
			{
				throw "unable to open temporary file";
			}
			fwrite ( shCode.c_str (), shCode.size (), 1, file );
			fclose ( file );
		} else
		{
			if ( shCode.c_str ()[0] == ':' )
			{
				cmdToRun = shCode.c_str () + 1;
				ignoreErrors = true;
			} else
			{
				cmdToRun = shCode;
			}
		}
		
		{
			auto close = [&retCode]( FILE *file )
			{
				retCode = _pclose ( file );
			};

			std::unique_ptr<FILE, decltype(close)> pipe ( _popen ( cmdToRun, "r" ), close );
			if ( !pipe )
			{
				throw std::runtime_error ( "popen() failed!" );
			}

			if ( !asBatch )
			{
				if ( cmdToRun[0] != '@' )
				{
					buff.printf ( "%s", cmdToRun.c_str () );
				}
			}
			buff.makeRoom ( 8192 );
			bool first = true;
			while ( fgets ( bufferBuff ( &buff ) + buff.size (), (int) bufferFree ( &buff ), pipe.get () ) != nullptr )
			{
				buff.assume ( strlen ( bufferBuff ( &buff ) + buff.size () ) );
				if ( queuePrints )
				{
					buff.makeRoom ( 8192 );
				} else
				{
					auto start = bufferBuff ( &buff );
					auto len = buff.size ();
					if ( first )
					{
						while ( len && (start[0] == '\r' || start[0] == '\n') )
						{
							start++;
							len--;
						}
						first = false;
					}
					printf ( "%.*s", (int) len, start );
					buff.reset ();
				}
			}

			if ( queuePrints )
			{
				auto start = bufferBuff ( &buff );
				auto len = buff.size ();
				while ( len && (start[0] == '\r' || start[0] == '\n') )
				{
					start++;
					len--;
				}
				// no previous errors from other tasks, OR if we are an error as well, print out the output.   just don't polute with normal output (to the best of our ability)
				if ( !fatalBuild || !retCode )
				{
					WaitForSingleObject ( printSemaphore, INFINITE );
					printf ( "%.*s", (int) len, start );
					ReleaseSemaphore ( printSemaphore, 1, 0 );
				}
			}
		}

		if ( retCode && !ignoreErrors && !this->ignoreErrors )
		{
			if ( !isPhonyTarget )
			{
				std::error_code ec;
				std::filesystem::remove ( target.c_str (), ec );
			}
			fatalBuild = true;
		}
	} catch ( ... )
	{
		if ( !silent ) printf ( "*********************************error: %u %i %lu  %s\r\n", __LINE__, retCode, GetLastError (), target.c_str() );
		fatalBuild = true;
		std::error_code ec;
		std::filesystem::remove ( target.c_str (), ec );
	}

	if ( jobServer )
	{
		ReleaseSemaphore ( jobRunSemaphore, 1, 0 );
	}
	
	if ( asBatch )
	{
		std::error_code ec;
		std::filesystem::remove ( cmdToRun.c_str (), ec );
	}
}

void maker::exec ( stringi const &target, bool phonyTarget, std::vector<stringi> const &cmds )
{
	bool		singleShell = this->singleShell;

	if ( fatalBuild ) return;

	for ( auto &it : cmds )
	{
		if ( !memcmpi ( it, "cd ", 2 ) || it == "cd" )
		{
			singleShell = true;
			break;
		} else if ( !memcmpi ( it, makeExeName, makeExeName.size () ) )
		{
			singleShell = outputSync;
		}
	}

	if ( singleShell )
	{
		stringi		shCode;
		for ( auto &it : cmds )
		{
			auto c = it.c_str ()[0] == ':' ? it.c_str () + 1 : it.c_str ();

			if ( !memcmpi ( it, makeExeName, makeExeName.size () ) )
			{
				shCode += "@";
				shCode += c;
				if ( jobServer )
				{
					shCode = shCode + " -jSem " + jobRunSemaphoreName;
				}
			} else
			{
				shCode += c;
			}
			shCode += " 2>&1";
			shCode += "\r\n";
			if ( it.c_str ()[0] != ':' )
			{
				shCode += "@if errorlevel 1 goto _smake_error_exit\r\n";
			}
		}
		shCode += ":_smake_exit\r\n";
		shCode += "@exit 0\r\n";
		shCode += ":_smake_error_exit\r\n";
		shCode += "@exit 2\r\n";
		try
		{
			runBat ( target, phonyTarget, shCode, true, true );
		} catch ( ... )
		{
			fatalBuild = true;
		}
	} else
	{
		for ( auto &it : cmds )
		{
			stringi		shCode;
			auto c = it.c_str ()[0] == ':' ? it.c_str () + 1 : it.c_str ();

			if ( !memcmpi ( c, makeExeName, makeExeName.size () ) )
			{
				shCode += c;
				if ( jobServer )
				{
					shCode = shCode + " -jSem " + jobRunSemaphoreName;
				}
				if ( outputSync )
				{
				}
				shCode += " 2>&1\r\n";
				try
				{
					runBat ( target, phonyTarget, shCode, false, outputSync );
				} catch ( ... )
				{
					if ( it.c_str ()[0] != ':' )
					{
						fatalBuild = true;
					}
				}
			} else
			{
				shCode += c;
				shCode += " 2>&1";
				shCode += "\r\n";
				try
				{
					runBat ( target, phonyTarget, shCode, false, true );
				} catch ( ... )
				{
					if ( it.c_str ()[0] != ':' )
					{
						fatalBuild = true;
					}
				}
			}
		}
	}
}

bool maker::makeTarget ( stringi const &target, std::promise<bool> &&run, bool sharedState )
{
	// this chunk of code ensures that we only build something once and that we properly wait for the build to complete before any dependant targets can continue
	targetScheduledAccess.lock ();
	auto buildStatus = targetScheduled.find ( target );

	if ( fatalBuild )
	{
		if ( buildStatus != targetScheduled.end () )
		{
			targetScheduledAccess.unlock ();
			SetEvent ( std::get<0> ( buildStatus->second ) );
		} else
		{
			targetScheduledAccess.unlock ();
		}
		return false;
	}

	// this chunk of code ensures that we only build something once and that we properly wait for the build to complete before any dependant targets can continue
	if ( buildStatus != targetScheduled.end () )
	{
		// the target is already in our scheduled build list... (some other dependency launched it).   Therefore we simply wait for it to complete (on the build completion event)
		targetScheduledAccess.unlock ();
//		printf ( "waiting for target target: '%s'\r\n", target.c_str () );
		WaitForSingleObject ( std::get<0> ( buildStatus->second ), INFINITE );
		if ( sharedState ) run.set_value ( *const_cast<bool *>(&std::get<1> ( buildStatus->second )) );
		return *const_cast<bool *>(&std::get<1> ( buildStatus->second ));

	}

	// no one else has yet scheduled this, but we still hold the lock to the list, so create the completion event, add it the the scheduled list and release the lock
//	printf ( "building target: '%s'\r\n", target.c_str () );
	auto depCompleteEvent = CreateEvent ( nullptr, true, false, nullptr );
	buildStatus = targetScheduled.emplace ( target, std::pair<HANDLE,bool>{ depCompleteEvent, false } ).first;
	targetScheduledAccess.unlock ();

	try 
	{
		for ( auto const &rule : rules )
		{
			std::string stem;
			for ( auto const &targetIt : rule.targets )
			{
				if ( targetMatch ( target, targetIt, stem ) )
				{
					std::filesystem::file_time_type targetTime;
					bool targetTimeValid = false;
					bool isPhonyTarget = true;
					if ( phonyTargets.find ( target ) == phonyTargets.end () )
					{
						isPhonyTarget = false;
						if ( std::filesystem::exists ( target.c_str () ) )
						{
							targetTime = std::filesystem::last_write_time ( target.c_str () );
							targetTimeValid = true;
						}
					}

					stringi preReq;
					stringi preReqNewer;
					bool runRecipe = true;

					std::vector<std::thread> builderThreads;
					std::vector<std::pair<std::future<bool>, bool>> builderFutures;

					// build the dependencies
					std::vector<stringi> deps;
					for ( auto const &dep : rule.dependants )
					{
						auto prereqName = std::get<0> ( dep ).findAndReplace ( "%", stem );
						prereqName = applySubstitutions ( prereqName );
						auto dep2 = getParamVec ( prereqName, " " );

						if ( !std::get<1> ( dep ) )
						{
							// we have at least one non-order-only dependent (a real dependency), so our rule execution therefore depends upon our dependency
							// that said, we REALLY set it to alwaysMake.  if alwasyMake is true then we ALWAYS want to run the recipe...
							runRecipe = alwaysMake;
						}

						// see if we need to run our recipe
						for ( auto const &d : dep2 )
						{
							if ( jobServer && rule.dependants.size () > 1 )
							{
								std::promise<bool>	p;
								builderFutures.push_back ( { p.get_future (), std::get<1> ( dep ) } );
								builderThreads.emplace_back ( std::thread ( &maker::makeTarget, this, d, std::move ( p ), true ) );			// TODO: speed this up by using worker queues
							} else
							{
								std::promise<bool>	p;
								if ( std::get<1> ( dep ) )
								{
									makeTarget ( d, std::move ( p ), false );
								} else
								{
									runRecipe |= makeTarget ( d, std::move ( p ), false );
								}
								if ( fatalBuild )
								{
									SetEvent ( std::get<0> ( buildStatus->second ) );
									if ( sharedState ) run.set_value ( false );
									return false;
								}
							}
							if ( preReq.size () )
							{
								preReq += " ";
							}
							preReq += d;
							deps.push_back ( d );
						}
					}
					// all our dependencies must be done before we can build our rules
					// MUST be done before we get any future... if not we'll abort() if a thread throws 
					for ( auto &thread : builderThreads )
					{
						if ( thread.joinable () )
						{
							thread.join ();
						}
					}

					for ( auto &f : builderFutures )
					{
						if ( std::get<1> ( f ) )
						{
							std::get<0> ( f ).get ();
						} else
						{
							runRecipe |= std::get<0> ( f ).get ();
						}
					}

					if ( fatalBuild )
					{
						SetEvent ( std::get<0> ( buildStatus->second ) );
						if ( sharedState ) run.set_value ( false );
						return false;
					}

					// now check built dependencies to see if we need to run our recipes
					for ( auto const &d : deps )
					{
						if ( std::filesystem::exists ( d.c_str () ) )
						{
							auto depTime = std::filesystem::last_write_time ( d.c_str () );
							if ( !targetTimeValid || (targetTimeValid && (depTime > targetTime)) )
							{
								// one of our dependencies is newer, so run our recipes
								runRecipe = true;
								if ( preReqNewer.size () )
								{
									preReqNewer += " ";
								}
								preReqNewer += d;
							}
						}
					}

					if ( runRecipe && !fatalBuild )
					{
						// do we have anything to actually run?
						if ( rule.rules.size () )
						{
							std::filesystem::path t ( target.c_str () );

							stringi fileName = t.filename ().string ();
							stringi path = std::filesystem::path ( t ).remove_filename ().string ();

							std::vector<stringi> cmds;

							auto prereqName = preReq.trim ().size () ? getParamVec ( preReq, " " )[0].findAndReplace ( "%", stem ) : stringi ( "" );
							for ( auto const &ruleIt : rule.rules )
							{
								stringi r = ruleIt.findAndReplace ( "$@", target );
								r = r.findAndReplace ( "$<", prereqName );
								r = r.findAndReplace ( "$^", preReq );
								r = r.findAndReplace ( "$?", preReqNewer );
								r = r.findAndReplace ( "$*", stem );
								r = r.findAndReplace ( "$(@D)", path.c_str() );
								r = r.findAndReplace ( "$(@F)", fileName );
								auto &c = applySubstitutions ( r ).trim ();
								if ( c.size () )
								{
									cmds.push_back ( c );
								}
							}
							exec ( target, isPhonyTarget, cmds );
						}
						if ( sharedState ) run.set_value ( true );
						std::get<1> ( buildStatus->second ) = true;
						SetEvent ( std::get<0> ( buildStatus->second ) );
						return true;
					}
					if ( sharedState )run.set_value ( false );
					SetEvent ( std::get<0> ( buildStatus->second ) );
					return false;
				}
			}
		}
		if ( !std::filesystem::exists ( target.c_str () ) )
		{
			// will be caught and converted to a promise exception
			throw "no rule to build target";
		}
	} catch ( ... )
	{
		if ( !silent ) printf ( "error building: %s\r\n", target.c_str () );
		fatalBuild = true;
		SetEvent ( std::get<0> ( buildStatus->second ) );
		if ( sharedState )
		{
			run.set_exception_at_thread_exit ( std::current_exception () );
			return false;
		} else
		{
			throw;
		}
	}
	SetEvent ( std::get<0> ( buildStatus->second ) );
	if ( sharedState ) run.set_value ( false );
	return false;
}

stringi maker::getRecipeToken ( char const *&line )
{
	while ( isspace ( line ) )
	{
		line++;
	}
	stringi result;
	while ( *line && !isspace ( line ) && (*line != ':') && (*line != '?') && (*line != ')') && (*line != '=') && (*line != '+') )
	{
		if ( *line == '(' )
		{
			while ( *line && *line != ')' )
			{
				result += *line;
				line++;
			}
		} else
		{
			result += *line;
			line++;
		}
	}
	while ( isspace ( line ) )
	{
		line++;
	}
	return result;
}

stringi maker::getToken ( char const *&line )
{
	while ( isspace ( line ) )
	{
		line++;
	}
	stringi result;

	while ( *line && !isspace ( line ) && !((line[0] == ':') && ((line[1] == ' ') || !line[1]))  && (*line != '?') && (*line != ')') && (*line != '=') && (*line != '+') )
	{
		if ( *line == '(' )
		{
			while ( *line && *line != ')'  )
			{
				result += *line;
				line++;
			}
		} else
		{
			result += *line;
			line++;
		}
	}
	while ( isspace ( line ) )
	{
		line++;
	}
	return result;
}

stringi maker::applySubstitutions ( stringi const &line )
{
	stringi res;

	auto ptr = line.c_str ();
	while ( *ptr )
	{
		if ( ptr[0] == '\n' && ptr[1] == '\t' )
		{
			// recipe entry... this is deferred processing so don't handle it right now
			res += *(ptr++);
			while ( *ptr && *ptr != '\n' )
			{
				res += *(ptr++);
			}
		} else
		{
			if ( !memcmp ( ptr, "$(", 2 ) )
			{
				ptr += 2;
				stringi token;
				while ( *ptr && (*ptr != ')' && *ptr != ':') && !isspace ( ptr ) )
				{
					token += *ptr;
					ptr++;
				}
				while ( isspace ( ptr ) ) ptr++;

				auto func = functionList.find ( token );
				if ( functionList.find ( token ) != functionList.end () )
				{
					auto params = getParamVec ( ptr, "," );
					if ( std::get<0> ( func->second ) > 0 )
					{
						if ( params.size () != std::get<0> ( func->second ) )
						{
							throw "incorrect number of parameters to function";
						}
					} else
					{
						if ( params.size () < -std::get<0> ( func->second ) )
						{
							throw "not enough parameters for function";
						}
					}

					if ( std::get<2> ( func->second ) )
					{
						for ( auto &it : params )
						{
							it = applySubstitutions ( it );
						}
					}
					res += (this->*(std::get<1> ( func->second ))) (params);
					while ( isspace ( ptr ) ) ptr++;
					if ( *ptr != ')' )
					{
						throw "missing )";
					}
					ptr++;
				} else if ( *ptr == ':' )
				{
					// text substitution
					ptr++;
					auto subs = variables.find ( token );
					if ( subs != variables.end () )
					{
						auto pats = getParamVec ( ptr, "," );
						stringi var = subs->second.subst;
						for ( auto &pat : pats )
						{
							auto params = getParamVec ( pat, "=" );
							for ( auto &it : params )
							{
								it = applySubstitutions ( it ).trim ();
							}
							if ( params.size () != 2 )
							{
								throw "invalid replacement";
							}
							var = var.findAndReplace ( params[0], params[1] );
							while ( isspace ( ptr ) ) ptr++;
							if ( *ptr != ')' )
							{
								throw "missing )";
							}
						}
						res += var;
						ptr++;
					} else
					{
						throw "unknown variable";
					}
				} else if ( *ptr == ')' )
				{
					ptr++;
					auto subs = variables.find ( token );
					if ( subs != variables.end () )
					{
						res += applySubstitutions ( subs->second.subst );
					} else
					{
						//						throw "unknown variable";
					}
				}
			} else if ( !memcmp ( ptr, "$$", 2 ) )
			{
				// $$ is emitted as a single $ and both $$ are skipped
				res += *(ptr++);
				ptr++;
			} else
			{
				res += *(ptr++);
			}
		}
	}
	return res;
}

maker::variable::variable ( stringi const &line, origin o ) : subst ( line ), orig ( o )
{
	subst = line;
	subst.trim ();
}

maker::variable &maker::variable::add ( char const *line )
{
	if ( subst.size () )
	{
		subst += " ";
	}
	subst += line;
	subst.trim ();
	return *this;
}

bool maker::testConditional ( char const *&linePtr )
{
	if ( *linePtr != '(' )
	{
		throw "missing '('";
	}
	linePtr++;
	auto params = getParamVec ( linePtr, "," );
	for ( auto &it : params )
	{
		it = applySubstitutions ( it ).trim ();
	}
	if ( params.size () != 2 )
	{
		throw "incorrect number of parameters to ifeq";
	}
	return params[0] == params[1];
}

void maker::processStream ( std::istream &inFile )
{
	std::stack<std::pair<bool, bool>>	condStack;		// execute, has had positive condition
	std::string line;
	std::string inLine;
	bool execute = true;

	std::unordered_map<stringi, bool *> options = {
		{ ".silent", &silent },
		{ ".printExecTimes", &printExecTimes },
		{ ".alwaysMake", &alwaysMake },
		{ ".singleShell", &singleShell },
		{ ".printDirectory", &silent },
	};

	condStack.push ( { true, true } );
	while ( std::getline ( inFile, inLine ) )
	{
	parse_line:
	// comments start with #
		if ( inLine[0] != '#' )
		{
			if ( !inLine.size () || (inLine.size () && inLine[inLine.size () - 1] != '\\') )
			{
				if ( line.size () )
				{
					line += " ";
				}
				line += stringi ( inLine ).trim ().operator const std::string & ();

				if ( !line.size() ) continue;

				auto linePtr = line.c_str ();

				auto firstToken = getToken ( linePtr );

				if ( condStack.top ().first )
				{
					if ( firstToken == "endif" )
					{
						if ( condStack.size () <= 1 )
						{
							throw "endif without matching if";
						}
						condStack.pop ();
					} else if ( firstToken == "else" )
					{
						if ( condStack.size () <= 1 )
						{
							throw "endif without matching if";
						}
						// don't give a crap about any following conditionals... they will never be executed
						condStack.top () = { false, true };
					} else if ( firstToken == "ifeq" )
					{
						execute = testConditional ( linePtr );
						condStack.push ( { execute, execute } );
					} else if ( firstToken == "ifneq" )
					{
						execute = !testConditional ( linePtr );
						condStack.push ( { execute, execute } );
					} else if ( firstToken == "ifdef" )
					{
						if ( !memcmp ( linePtr, "$(", 2 ) )
						{
							linePtr += 2;
							firstToken = getToken ( linePtr );
							if ( *linePtr != ')' )
							{
								throw "invalid ifdef";
							}
							linePtr++;
							condStack.push ( { variables.find ( firstToken ) != variables.end (), variables.find ( firstToken ) != variables.end () } );
						} else
						{
							throw "invalid ifdef";
						}
					} else if ( firstToken == "ifndef" )
					{
						if ( !memcmp ( linePtr, "$(", 2 ) )
						{
							linePtr += 2;
							firstToken = getToken ( linePtr );
							if ( *linePtr != ')' )
							{
								throw "invalid ifndef";
							}
							linePtr++;
							condStack.push ( { variables.find ( firstToken ) == variables.end (), variables.find ( firstToken ) == variables.end () } );
						} else
						{
							throw "invalid ifndef";
						}
					} else if ( !memcmp ( linePtr, "::=", 3 ) )
					{
						// POSIX simply expanded variable
						linePtr += 3;
						firstToken = applySubstitutions ( firstToken );
						variables[firstToken] = variable ( applySubstitutions ( linePtr ), maker::origin::o_file  );
					} else if ( !memcmp ( linePtr, ":=", 2 ) )
					{
						// short for simply expanded variable
						linePtr += 2;
						firstToken = applySubstitutions ( firstToken );
						variables[firstToken] = variable ( applySubstitutions ( linePtr ), maker::origin::o_file );
					} else if ( !memcmp ( linePtr, "?=", 2 ) )
					{
						// conditional expansion of variable
						linePtr += 2;
						firstToken = applySubstitutions ( firstToken );
						if ( variables.find ( firstToken ) != variables.end () )
						{
							variables[firstToken] = variable ( linePtr, maker::origin::o_file );
						}
					} else if ( !memcmp ( linePtr, "?:=", 3 ) )
					{
						// conditional expansion of variable
						linePtr += 3;
						firstToken = applySubstitutions ( firstToken );
						if ( variables.find ( firstToken ) != variables.end () )
						{
							variables[firstToken] = variable ( applySubstitutions ( linePtr ), maker::origin::o_file );
						}
					} else if ( !memcmp ( linePtr, "+=", 2 ) )
					{
						// append
						linePtr += 2;
						firstToken = applySubstitutions ( firstToken );
						if ( variables.find ( firstToken ) != variables.end () )
						{
							// default to lazy expansion
							variables[firstToken] = variables[firstToken].add ( linePtr );
						} else
						{
							variables[firstToken] = variable ( linePtr, maker::origin::o_file );
						}
					} else if ( !memcmp ( linePtr, "=", 1 ) )
					{
						linePtr += 1;
						firstToken = applySubstitutions ( firstToken );
						variables[firstToken] = variable ( linePtr, maker::origin::o_file );
					} else if ( !memcmp ( linePtr, ":", 1 ) && (firstToken == ".PHONY") )
					{
						linePtr += 1;
						auto targets = getParamVec ( linePtr, " " );
						for ( auto &it : targets )
						{
							phonyTargets.insert ( it );
						}
					} else if ( !memcmp ( linePtr, ":", 1 ) )
					{
						firstToken = applySubstitutions ( firstToken );
						linePtr += 1;
						stringi depLine = linePtr;
						std::pair<std::vector<stringi>, std::vector<stringi>> dependencies;
						depLine.trim ();
						if ( depLine.size () )
						{
							auto dep = applySubstitutions ( depLine.c_str () );
							dependencies = getParamVec ( dep, " \t", "|" );
						}

						std::vector<std::pair<stringi, bool>> ruleDependencies;
						for ( auto &it : std::get<0> ( dependencies ) )
						{
							ruleDependencies.push_back ( { it, false } );
						}
						for ( auto &it : std::get<1> ( dependencies ) )
						{
							ruleDependencies.push_back ( { it, true } );
						}
						std::vector<stringi> recipes;

						std::string recipeLine;
						while ( std::getline ( inFile, recipeLine ) )
						{
							if ( recipeLine[0] == '\t' )
							{
								auto &recipe = stringi ( recipeLine ).trim ();
								if ( recipe.size () ) recipes.push_back ( recipe );
							} else
							{
								rules.push_back ( { {firstToken}, ruleDependencies, recipes } );
								inLine = recipeLine;
								line = "";
								goto parse_line;
							}
						}
						rules.push_back ( { {firstToken}, ruleDependencies, recipes } );
					} else if ( firstToken == "include" )
					{
						firstToken = applySubstitutions ( linePtr );
						auto incVec = getParamVec ( linePtr, " " );
						for ( auto &it : incVec )
						{
							it = applySubstitutions ( it );
							std::ifstream includeFile ( it.c_str () );
							if ( !includeFile.is_open () )
							{
								throw "can not open include file";
							}
							processStream ( includeFile );
						}
					} else if ( firstToken == "-include" )
					{
						firstToken = applySubstitutions ( linePtr );
						auto incVec = getParamVec ( linePtr, " " );
						for ( auto &it : incVec )
						{
							it = applySubstitutions ( it );
							std::ifstream includeFile ( it.c_str () );
							if ( includeFile.is_open () )
							{
								processStream ( includeFile );
							}
						}
					} else if ( firstToken == "define" )
					{
						firstToken = applySubstitutions ( getToken ( linePtr ) );
						auto op = getToken ( linePtr );
						std::string defineLine;
						while ( std::getline ( inFile, inLine ) )
						{
							linePtr = inLine.c_str ();
							if ( getToken ( linePtr ) == "endef" )
							{
								break;
							}
							if ( defineLine.size () )
							{
								defineLine += "\n";
							}
							defineLine += inLine;
						}

						if ( !op.size () || op == "=" )
						{
							// non-expansion assignment
							variables[firstToken] = variable ( defineLine, maker::origin::o_file );
						} else if ( op == "?=" )
						{
							// conditional assignment, non-expansion
							if ( variables.find ( firstToken ) != variables.end () )
							{
								variables[firstToken] = variable ( defineLine, maker::origin::o_file );
							}
						} else if ( op == "::=" || op == ":=" )
						{
							// POSIX simply expanded variable
							variables[firstToken] = variable ( applySubstitutions ( defineLine ), maker::origin::o_file );
						} else if ( op == "?:=" )
						{
							// conditional expansion of variable
							if ( variables.find ( firstToken ) != variables.end () )
							{
								variables[firstToken] = variable ( applySubstitutions ( defineLine ), maker::origin::o_file );
							}
						} else if ( op == "+=" )
						{
							// append
							if ( variables.find ( firstToken ) != variables.end () )
							{
								// default to lazy expansion
								variables[firstToken] = variable ( defineLine, maker::origin::o_file );
							} else
							{
								variables[firstToken].add ( defineLine.c_str () );
							}
						}
					} else if ( firstToken[0] == '.' )
					{
						auto it = options.find ( firstToken );
						if ( it != options.end () )
						{
							*it->second = true;
						}
					} else
					{
						std::stringstream stream ( applySubstitutions ( line ).trim ().operator const std::string &() );
						processStream ( stream );
					}
				} else if ( firstToken == "endif" )
				{
					condStack.pop ();
				} else  if ( firstToken == "else" )
				{
					if ( !condStack.top ().second )
					{
						firstToken = getToken ( linePtr );
						if ( firstToken == "ifeq" )
						{
							execute = testConditional ( linePtr );
							condStack.top () = { execute, execute };
						} else if ( firstToken == "ifneq" )
						{
							execute = !testConditional ( linePtr );
							condStack.top () = { execute, execute };
						} else if ( firstToken == "ifdef" )
						{
							if ( !memcmp ( linePtr, "$(", 2 ) )
							{
								linePtr += 2;
								firstToken = getToken ( linePtr );
								if ( *linePtr != ')' )
								{
									throw "invalid ifdef";
								}
								linePtr++;
								condStack.top () = { variables.find ( firstToken ) != variables.end (), variables.find ( firstToken ) != variables.end () };
							} else
							{
								throw "invalid ifdef";
							}
						} else if ( firstToken == "ifdef" )
						{
							if ( !memcmp ( linePtr, "$(", 2 ) )
							{
								linePtr += 2;
								firstToken = getToken ( linePtr );
								if ( *linePtr != ')' )
								{
									throw "invalid ifdef";
								}
								linePtr++;
								condStack.top () = { variables.find ( firstToken ) == variables.end (), variables.find ( firstToken ) == variables.end () };
							} else
							{
								throw "invalid ifdef";
							}
						} else
						{
							if ( firstToken.size () )
							{
								throw "invalid conditional on else";
							}
							execute = true;
							condStack.top () = { execute, execute };
						}
					}
				}
				line = "";
			} else
			{
				// always has an embedded zero in the getline, so we strip that and the \ off
				line += " ";
				line += stringi ( inLine.substr ( 0, inLine.size () - 1 ) ).trim ().operator const std::string &();
			}
		}
	}
}

void maker::addVariable ( stringi const &name, stringi const &value, origin orig )
{
	variables[name] = variable ( value, orig );
}

void maker::addVariable ( stringi const &assign, origin orig )
{
	const char *linePtr = assign.c_str ();
	auto firstToken = getToken ( linePtr );
	if ( *linePtr == '=' )
	{
		linePtr++;
	}
	variables[firstToken].add ( linePtr );
}

void pipePrinter ( HANDLE pipe )
{

}

int main ( int argc, char *argv[], char *envp[] )
{
	maker m;

	try
	{
		//	auto outPipe = CreateNamedPipe ( "\\\\.\\pipe\\smake.out.pipe", PIPE_ACCESS_DUPLEX, FILE_FLAG_FIRST_PIPE_INSTANCE, 

		DWORD dwMode = 0;
		if ( GetConsoleMode( GetStdHandle( STD_OUTPUT_HANDLE ), &dwMode ) )
		{
			SetConsoleMode( GetStdHandle( STD_OUTPUT_HANDLE ), dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
		}

		m.makeExeName = argv[0];
		m.addVariable ( "MAKE", argv[0], maker::origin::o_automatic );
		m.addVariable ( "CURDIR", std::filesystem::current_path ().string ().c_str (), maker::origin::o_automatic );

		for ( size_t index = 0; envp[index]; index++ )
		{
			m.addVariable ( envp[index], maker::origin::o_environment );
		}

		stringi fileName = "makefile";
		std::vector<stringi> targets;

		stringi workingDir;
		stringi makeFlags;

		for ( int index = 1; index < argc; index++ )
		{
			if ( argv[index][0] == '-' )
			{
				std::string param = argv[index] + 1;
				if ( param == "f" )
				{
					index++;
					fileName = argv[index];
				} else if ( param == "s" )
				{
					m.silent = true;
					makeFlags += " -s";
				} else if ( param == "i" )
				{
					m.ignoreErrors = true;
					makeFlags += " -i";
				} else if ( param == "B" )
				{
					m.alwaysMake = true;
					makeFlags += " -B";
				} else if ( param == "O" || param == "o" )
				{
					m.outputSync = true;
					makeFlags += " -O";
				} else if ( param == "C" )
				{
					index++;
					std::filesystem::current_path ( argv[index] );
					workingDir = argv[index];
				} else if ( param == "j" )
				{
					index++;
					auto nServers = _atoi64 ( (char *) argv[index] );
					if ( nServers == 1 )
					{
						// do nothing.. just use this thread
					} else
					{
						if ( !nServers )
						{
							SYSTEM_INFO		 systemInfo;
							GetSystemInfo ( &systemInfo );
							nServers = systemInfo.dwNumberOfProcessors;
						}

						GUID guid;
						CoCreateGuid ( &guid );

						BYTE *str;
						UuidToString ( (UUID *) &guid, &str );

						m.jobRunSemaphoreName = m.jobRunSemaphoreName + "smake." + (char const *) str;
						m.printSemaphoreName = m.jobRunSemaphoreName + ".print";

						RpcStringFree ( &str );

						SECURITY_ATTRIBUTES	sec{};

						sec.nLength = sizeof ( sec );
						sec.bInheritHandle = true;
						sec.lpSecurityDescriptor = nullptr;

						// execution semaphore
						m.jobRunSemaphore = CreateSemaphore ( &sec, (LONG) nServers, (LONG) nServers, m.jobRunSemaphoreName );
						if ( m.jobRunSemaphore != INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
						{
							m.jobServer = true;
						}

						// print semaphore
						m.printSemaphore = CreateSemaphore ( &sec, 1, 1, m.printSemaphoreName );
					}
				} else if ( param == "jSem" )
				{
					index++;
					m.jobRunSemaphoreName = argv[index];

					m.jobRunSemaphore = OpenSemaphore ( SEMAPHORE_ALL_ACCESS, true, m.jobRunSemaphoreName );
					if ( m.jobRunSemaphore != INVALID_HANDLE_VALUE )// NOLINT (performance-no-int-to-ptr)
					{
						m.jobServer = true;
						m.subMake = true;

						ReleaseSemaphore ( m.jobRunSemaphore, 1, 0 );
					}

					m.printSemaphoreName = m.jobRunSemaphoreName + ".print";
					m.printSemaphore = OpenSemaphore ( SEMAPHORE_ALL_ACCESS, true, m.printSemaphoreName );
				} else if ( param == "T" )
				{
					m.printExecTimes = true;
					makeFlags += " -T";
				}
			} else
			{
				stringi arg ( argv[index] );
				auto eqPos = arg.find ( "=" );

				if ( eqPos != stringi::npos )
				{
					auto name = arg.substr ( 0, eqPos );
					auto value = arg.substr ( eqPos + 1, stringi::npos );
					m.addVariable ( name, value, maker::origin::o_command_line );
				} else
				{
					targets.push_back ( arg );
				}
			}
		}

		if ( m.printSemaphore == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
		{
			SECURITY_ATTRIBUTES	sec{};

			sec.nLength = sizeof ( sec );
			sec.bInheritHandle = true;
			sec.lpSecurityDescriptor = nullptr;

			m.printSemaphore = CreateSemaphore ( &sec, 1, 1, m.printSemaphoreName );
		}

		if ( workingDir.size () && !m.silent ) printf ( "Entering Directory %s\r\n", workingDir.c_str() );

		m.addVariable ( "MAKEFLAGS", makeFlags, maker::origin::o_automatic );

		if ( !targets.size () )
		{
			targets.push_back ( "all" );
		}

		stringi goals;
		for ( auto &it : targets )
		{
			if ( goals.size () )
			{
				goals += " ";
			}
			goals += it;
		}
		m.addVariable ( "MAKECMDGOALS", goals, maker::origin::o_automatic );

		try
		{
			std::ifstream inFile ( fileName.c_str () );
			if ( !inFile.is_open () )
			{
				throw "can not open makefile";
			}

			// process the makefile and build the internal tables
			m.processStream ( inFile );

			// process the targets and generate the list of things to build
			for ( auto &it : targets )
			{
				std::promise<bool> p;
				m.makeTarget ( it, std::move ( p ), false );
			}

			if ( m.subMake )
			{
				WaitForSingleObject ( m.jobRunSemaphore, INFINITE );
			}
			if ( workingDir.size () )
			{
				if ( !m.silent ) printf ( "Leaving Directory %s\r\n", workingDir.c_str () );
			} else
			{
				if ( m.fatalBuild )
				{
					if ( !m.silent ) printf ( "done with errors: %i %lu\r\n", __LINE__, GetLastError () );
				} else
				{
					if ( !m.silent ) printf ( "done.\r\n" );
				}
			}
			return m.fatalBuild;
		} catch ( char const *err )
		{
			if ( !m.silent ) printf ( "%s\r\n", err );
			if ( m.subMake )
			{
				WaitForSingleObject ( m.jobRunSemaphore, INFINITE );
			}
			if ( workingDir.size () )
			{
				if ( !m.silent ) printf ( "Leaving Directory %s\r\n", workingDir.c_str () );
			} else
			{
				if ( !m.silent ) printf ( "done with errors: %i %lu\r\n", __LINE__, GetLastError () );
			}
			return 1;
		} catch ( ... )
		{
			if ( m.subMake )
			{
				WaitForSingleObject ( m.jobRunSemaphore, INFINITE );
			}
			if ( workingDir.size () )
			{
				if ( !m.silent ) printf ( "Leaving Directory %s\r\n", workingDir.c_str () );
			} else
			{
				if ( !m.silent ) printf ( "done with errors: %i %lu\r\n", __LINE__, GetLastError () );
			}
			return 1;
		}
	} catch ( ... )
	{
		if ( m.subMake )
		{
			WaitForSingleObject ( m.jobRunSemaphore, INFINITE );
		}
		if ( !m.silent ) printf ( "unknown exception\r\n" );
		return 1;
	}
}
