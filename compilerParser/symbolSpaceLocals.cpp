/*
	symbol space implementation

*/

#include "stdlib.h"
#include <list>
#include <string>
#include "Target\common.h"
#include "symbolSpace.h"
#include "expressionParser.h"
#include "compilerBCGenerator\compilerBCGenerator.h"
#include "classParser.h"
#include "fileParser.h"
#include "avl tree\avl.h"
#include "bcInterpreter\bcInterpreter.h"
#include "bcVM\funcky.h"
#include "_symbolSpace.h"
#include "symbolSpaceLocals.h"


