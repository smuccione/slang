#include <iostream>
#include <set>
#include <filesystem>
#include <string>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <future>
#include <semaphore>

#include "stdlib.h"
#include "Utility/stringi.h"
#include "Utility/jsonParser.h"

class maker
{
public:
	enum class origin
	{
		o_undefined,
		o_default,
		o_environment,
		o_environment_override,
		o_file,
		o_command_line,
		o_override,
		o_automatic
	};
private:
	struct buildEvent
	{
		std::mutex m;
		std::condition_variable cv;
		volatile bool built = false;

		buildEvent ()
		{}

		void signal ()
		{
			{
				std::lock_guard<std::mutex> lk ( m );
				built = true;
			}
			cv.notify_all ();
		}
		void wait ()
		{
			std::unique_lock<std::mutex> lk ( m );
			cv.wait ( lk, [this] {return built; } );
		}
	};
	struct variable
	{
		stringi subst;

		maker::origin orig = maker::origin::o_undefined;

		variable ()
		{};

		variable ( variable const &old )
		{
			subst = old.subst;
			orig = old.orig;
		}

		variable ( variable &&old )
		{
			std::swap ( subst, old.subst );
			std::swap ( orig, old.orig );
		}
		variable &operator = ( variable const &old )
		{
			subst = old.subst;
			orig = old.orig;
			return *this;
		}
		variable &operator = ( variable &&old )
		{
			std::swap ( subst, old.subst );
			std::swap ( orig, old.orig );
			return *this;
		}

		variable ( stringi const &line, origin o );
		variable &add ( char const *line );
	};

	struct rule
	{
		std::vector<stringi>					targets;
		std::vector<std::pair<stringi, bool>>	dependants;		// dependant name, order only flag (true if order only)
		std::vector<stringi>					rules;

		rule ( std::vector<stringi> const &targets, std::vector<std::pair<stringi, bool>> const &dependants, std::vector<stringi> const &rules ) : targets ( targets ), dependants ( dependants ), rules ( rules )
		{}

		rule ()
		{}
	};

	template<typename T>
	class guardedQueue
	{
	public:
		void push ( const T &value )
		{
			std::lock_guard<std::mutex> lock ( m );
			queue.push ( value );
		}

		T pop ()
		{
			T ret;
			std::lock_guard<std::mutex> lock ( m );
			ret = queue.front ();
			queue.pop ();
			return ret;
		}

		bool isEmpty ()
		{
			return queue.size () ? false : true;
		}
	private:
		std::queue<T> queue;
		std::mutex m;
	};

	std::mutex													targetScheduledAccess;
	std::map<stringi, std::pair<HANDLE, volatile bool>>			targetScheduled;	// handle is an event that can be waiting on to signal build complete

	friend struct variable;
	friend struct rule;

	typedef stringi ( maker:: *macroFunc ) (std::vector<stringi> const &params );

#if _DEBUG
	std::map <stringi, std::tuple<int32_t, macroFunc, bool>>	functionList;
	std::map<stringi, variable>									variables;
	std::map<stringi, std::vector<stringi>>						defines;
	std::set<stringi>											phonyTargets;
#else
	std::unordered_map <stringi, std::tuple<int32_t, macroFunc, bool>>	functionList;
	std::unordered_map<stringi, variable>								variables;
	std::unordered_map<stringi, std::vector<stringi>>					defines;
	std::unordered_set<stringi>											phonyTargets;
#endif
	std::vector<rule>													rules;

	std::mutex															targetBuildAccess;
	std::unordered_map<stringi, buildEvent>								targetBuild;

	std::vector<stringi>									getParamVec ( char const *&ptr, char const *delim, char const *stop );

	void													exec ( stringi const &target, bool phonyTarget, std::vector<stringi> const &cmds );
	stringi													getToken ( char const *&line );
	stringi													getRecipeToken ( char const *&line );
	bool													targetMatch ( char const *str, char const *match, std::string &base );
	std::vector<stringi>									getParamVec ( char const *&ptr, char const *delim );
	std::vector<stringi>									getParamVec ( stringi const &ptr, char const *delim );
	std::pair<std::vector<stringi>, std::vector<stringi>>	getParamVec ( stringi const &ptr, char const *delim, char const *split );
	stringi													applySubstitutions ( stringi const &line );
	bool													testConditional ( char const *&linePtr );
	stringi													findAndReplace ( stringi s, std::vector < std::pair<stringi, stringi>> &replaceList );

	stringi macroFuncSubst ( std::vector<stringi> const &params );
	stringi macroFuncPatsubst ( std::vector<stringi> const &params );
	stringi macroFuncStrip ( std::vector<stringi> const &params );
	stringi macroFuncFindstring ( std::vector<stringi> const &params );
	stringi macroFuncFilter ( std::vector<stringi> const &params );
	stringi macroFuncFilterOut ( std::vector<stringi> const &params );
	stringi macroFuncSort ( std::vector<stringi> const &params );
	stringi macroFuncWord ( std::vector<stringi> const &params );
	stringi macroFuncWordlist ( std::vector<stringi> const &params );
	stringi macroFuncWords ( std::vector<stringi> const &params );
	stringi macroFuncFirstword ( std::vector<stringi> const &params );
	stringi macroFuncLastword ( std::vector<stringi> const &params );
	stringi macroFuncDir ( std::vector<stringi> const &params );
	stringi macroFuncNotdir ( std::vector<stringi> const &params );
	stringi macroFuncSuffix ( std::vector<stringi> const &params );
	stringi macroFuncBasename ( std::vector<stringi> const &params );
	stringi macroFuncAddsuffix ( std::vector<stringi> const &params );
	stringi macroFuncAddprefix ( std::vector<stringi> const &params );
	stringi macroFuncJoin ( std::vector<stringi> const &params );
	stringi macroFuncWildcard ( std::vector<stringi> const &params );
	stringi macroFuncRealpath ( std::vector<stringi> const &params );
	stringi macroFuncAbspath ( std::vector<stringi> const &params );
	stringi macroFuncIf ( std::vector<stringi> const &params );
	stringi macroFuncAnd ( std::vector<stringi> const &params );
	stringi macroFuncOr ( std::vector<stringi> const &params );
	stringi macroFuncForeach ( std::vector<stringi> const &params );
	stringi macroFuncFile ( std::vector<stringi> const &params );
	stringi macroFuncCall ( std::vector<stringi> const &params );
	stringi macroFuncValue ( std::vector<stringi> const &params );
	stringi macroFuncEval ( std::vector<stringi> const &params );
	stringi macroFuncOrigin ( std::vector<stringi> const &params );
	stringi macroFuncFlavor ( std::vector<stringi> const &params );
	stringi macroFuncError ( std::vector<stringi> const &params );
	stringi macroFuncWarning ( std::vector<stringi> const &params );
	stringi macroFuncInfo ( std::vector<stringi> const &params );
	stringi macroFuncShell ( std::vector<stringi> const &params );

	bool isspace ( char const *ptr ) const
	{
		if ( *ptr == ' ' || *ptr == '\t' ) return true;
		return false;
	}

	void runBat ( stringi const &target, bool phonyTarget, stringi const &shCode, bool asBatch, bool queuePrints );
public:
	maker ();
	bool makeTarget ( stringi const &target, std::promise<bool> &&run, bool sharedState );
	void addVariable ( stringi const &name, stringi const &value, origin orig );
	void addVariable ( stringi const &assign, origin orig );
	void processStream ( std::istream &inFile );
	
	stringi				makeExeName;
	bool				printExecTimes = false;
	bool				jobServer = false;
	bool				silent = false;
	bool				ignoreErrors = false;
	bool				LSParse = false;
	bool				alwaysMake = false;
	bool				singleShell = false;
	bool				outputSync = false;

	std::atomic<bool>	fatalBuild = false;

	jsonElement			LSOutput;

#if defined ( WIN32 )
	HANDLE				jobRunSemaphore = INVALID_HANDLE_VALUE;		// NOLINT (performance-no-int-to-ptr)
	stringi				jobRunSemaphoreName;

	HANDLE				printSemaphore = INVALID_HANDLE_VALUE;		// NOLINT (performance-no-int-to-ptr)
	stringi				printSemaphoreName;

	bool				subMake = false;
#else
#endif

};
