/*
	FGL Internal error codes
*/


#pragma once

#define ERROR_TABLE \
REGISTER_ERROR ( scINTERNAL, "internal" )   \
REGISTER_ERROR ( scMEMORY, "out of memory" )   \
REGISTER_ERROR ( scSYNTAX_MISSING_OPEN_PEREN, "missing (" )   \
REGISTER_ERROR ( scSYNTAX_MISSING_CLOSE_PEREN, "unbalanced parentheses, missing )" )   \
REGISTER_ERROR ( scSYNTAX_MISSING_MULTI_LINE_STRING_OPENER, "missing ( in a multi-line string" )   \
REGISTER_ERROR ( scSYNTAX_EXTRA_ON_LINE, "extra on line" )   \
REGISTER_ERROR ( scSYNTAX_MISSING_SEMICOLON, "missing ;" )   \
REGISTER_ERROR ( scSYNTAX_MISSING_IN, "missing IN" )   \
REGISTER_ERROR ( scMISSING_CONTROL_STRUCTURE, "try without catch or finally is not allowed" )   \
REGISTER_ERROR ( scNESTING, "nested functions not allowed" )   \
REGISTER_ERROR ( scEXTRA_END, "END without matching block start" )   \
REGISTER_ERROR ( scUNSUPPORTED, "feature is unsupported" )   \
REGISTER_ERROR ( scUNEXPECTED_STATEMENT, "an unexpected statement was encountered" )   \
REGISTER_ERROR ( scMISSING_END, "no matching end found" )   \
REGISTER_ERROR ( scMISSING_WHILE, "no matching while found for do" )   \
REGISTER_ERROR ( scMISSING_UNTIL, "no matching until found for repeat" )   \
REGISTER_ERROR ( scILLEGAL_STRING_EXPANSION, "strings cannot span lines" )   \
REGISTER_ERROR ( scNONARRAY, "attempting to dereference something that is not an array" )   \
REGISTER_ERROR ( scARRAY_ENUMERATION, "error in definition of array" )   \
REGISTER_ERROR ( scUNBALANCED_PERENS, "unbalanced parentheses" )   \
REGISTER_ERROR ( scMISSING_CLOSE_BRACE, "missing close brace" )   \
REGISTER_ERROR ( scMISSING_OPEN_BRACE, "missing open brace" )   \
REGISTER_ERROR ( scEXTRA_BRACE, "close brace } without matching open {" )   \
REGISTER_ERROR ( scEXTRA_PEREN, "close peren ) without matching open (" )   \
REGISTER_ERROR ( scINVALID_OPERATOR, "invalid operator" )   \
REGISTER_ERROR ( scILLEGAL_OPERATION, "illegal operation" )   \
REGISTER_ERROR ( scINVALID_INDICE, "invalid index" )   \
REGISTER_ERROR ( scDIVIDE_BY_ZERO, "divide by zero" )   \
REGISTER_ERROR ( scILLEGAL_OPERAND, "illegal operand" )   \
REGISTER_ERROR ( scILLEGAL_ASSIGNMENT, "illegal assignment" )   \
REGISTER_ERROR ( scILLEGAL_EXPRESSION, "illegal expression" )   \
REGISTER_ERROR ( scINVALID_PARAMETER, "invalid parameter" )   \
REGISTER_ERROR ( scUNKNOWN_FUNCTION, "unknown function" )   \
REGISTER_ERROR ( scUNKNOWN_SYMBOL, "unknown symbol" )   \
REGISTER_ERROR ( scDUPLICATE_SYMBOL, "duplicate symbol" )   \
REGISTER_ERROR ( scDUPLICATE_INITIALIZER, "duplicate initializer" )   \
REGISTER_ERROR ( scALSO_SEEN, "also seen here" )   \
REGISTER_ERROR ( scFIELD_INITIALIZER, "fields cannot have initializers" )   \
REGISTER_ERROR ( scINVALID_KEY, "invalid key" )   \
REGISTER_ERROR ( scMISSING_QUOTE, "missing quote" )   \
REGISTER_ERROR ( scILLEGAL_IDENTIFIER, "illegal identifier" )   \
REGISTER_ERROR ( scBAD_VARIABLE_DEF, "invalid variable" )   \
REGISTER_ERROR ( scCODEBLOCK_INVALID, "invalid codeblock, missing parameters section" )   \
REGISTER_ERROR ( scCODEBLOCK_PARAMETERS, "invalid codeblock, unterminated parameters section" )   \
REGISTER_ERROR ( scILLEGAL_REFERENCE, "illegal reference" )   \
REGISTER_ERROR ( scSEND_TO_NONOBJECT, "not an object" )   \
REGISTER_ERROR ( scWRONG_OBJECT, "unable to cast too desired object" )   \
REGISTER_ERROR ( scINVALID_MESSAGE, "the expression does not evaluate to something that can be deduced as an object member name" )   \
REGISTER_ERROR ( scMISSING_SYMBOL, "missing symbol" )   \
REGISTER_ERROR ( scINVALID_ASSIGN, "does not evaluate to an assignable entity or method " )   \
REGISTER_ERROR ( scINVALID_ACCESS, "does not evaluate to an accessible entity or method" )   \
REGISTER_ERROR ( scUNKNOWN_METHOD, "unknown method or instance variable" )   \
REGISTER_ERROR ( scDUPLICATE_METHOD, "duplicate method" )   \
REGISTER_ERROR ( scINVALID_CATCH_EXPR, "invalid catch expression" )   \
REGISTER_ERROR ( scCLASS_NOT_FOUND, "class definition not found" )   \
REGISTER_ERROR ( scNOFREE_WORKAREAS, "no free workareas" )   \
REGISTER_ERROR ( scTYPE_CONVERSION, "invalid type conversion" )   \
REGISTER_ERROR ( scINVALID_WORKAREA, "invalid workarea" )   \
REGISTER_ERROR ( scWORKAREA_NOT_IN_USE, "workarea not in use" )   \
REGISTER_ERROR ( scUNKNOWN_IVAR, "unknown instance variable" )   \
REGISTER_ERROR ( scERROR_IN_INHERIT, "inherited class not found" )   \
REGISTER_ERROR ( scBAD_COMPILED_CODE, "bad compiled code" )   \
REGISTER_ERROR ( scDUPLICATE_CLASS, "duplicate class" )   \
REGISTER_ERROR ( scARRAY_NOT_OF_CONSISTENT_TYPE, "array not of consistent type/dimension" )   \
REGISTER_ERROR ( scINVALID_XML_DOCUMENT, "invalid xml document" )   \
REGISTER_ERROR ( scDUPLICATE_FUNCTION, "duplicate function" )\
REGISTER_ERROR ( scLIBRARY_ALREADY_DEFINED, "library already defined" ) \
REGISTER_ERROR ( scMODULE_ALREADY_DEFINED, "module already defined" ) \
REGISTER_ERROR ( scDUPLICATE_LABEL, "duplicate label" )   \
REGISTER_ERROR ( scLABEL_NOT_FOUND, "label not found" )   \
REGISTER_ERROR ( scILLEGAL_LABEL, "illegal label" )   \
REGISTER_ERROR ( scDUPLICATE_IDENTIFIER, "duplicate identifier" )   \
REGISTER_ERROR ( scMULTIPLE_DEFAULTS, "multiple defaults in switch statement" )   \
REGISTER_ERROR ( scEMPTY_SWITCH, "switch contains no case or default statements" )   \
REGISTER_ERROR ( scDUPLICATE_PARAMETER, "duplicate parameter" )   \
REGISTER_ERROR ( scNOT_LEGAL_INSIDE_CLASS, "not legal inside a class definition" )   \
REGISTER_ERROR ( scNOT_LEGAL_OUTSIDE_PROPERTY, "not legal outside a property definition" )   \
REGISTER_ERROR ( scNOT_LEGAL_INSIDE_PROPERTY, "not legal inside a property definition" )   \
REGISTER_ERROR ( scINCORRECT_NUMBER_OF_PARAMETERS, "" )   \
REGISTER_ERROR ( scILLEGAL_METHOD_INITIALIZER, "initilizer must contain target to initialize" )   \
REGISTER_ERROR ( scILLEGAL_METHOD_IVAR_INITIALIZER, "initializer in incorrect format for instance variable initialization" )   \
REGISTER_ERROR ( scPARAMETER_COUNT_MISMATCH, "incorrect number of parameters to codeblock call, variant arguments are not allowed in a codeblock" )   \
REGISTER_ERROR ( scILLEGAL_INITIALIZER, "illegal initializer" )   \
REGISTER_ERROR ( scNO_OPERATOR_OVERLOAD, "object does not supply overload for operation") \
REGISTER_ERROR ( scMULTIPLE_OVERRIDE, "multiple overrides of same operator" )   \
REGISTER_ERROR ( scMISSING_CLASSNAME, "missing class name in new" )   \
REGISTER_ERROR ( scNEW_RETURN_TYPE_NOT_ALLOWED, "return values from new are not allowed" )   \
REGISTER_ERROR ( scNO_RETURN_VALUE, "no return value" )   \
REGISTER_ERROR ( scNESTED_VIRTUAL_INHERITANCE, "nested virtual inheritance is not allowed" )   \
REGISTER_ERROR ( scASSIGN_TO_CONST, "attempting to assign to a constant" )   \
REGISTER_ERROR ( scNO_SUCH_OPERATOR_SIGNATURE, "no such operator signature" )   \
REGISTER_ERROR ( scFIXED_ARRAY_OUT_OF_BOUNDS, "fixed array access out of bounds" )   \
REGISTER_ERROR ( scSTRING_REFERNCE_OUT_OF_BOUNDS, "attempting to dereference a character in a string that is out of bounds" )   \
REGISTER_ERROR ( scNOT_ALLOWED_WITH_PROP_METHODS, "illegal usage of a property" )   \
REGISTER_ERROR ( scNOT_ALLOWED_WITH_METHODS, "taking value of a method is not allowed" )   \
REGISTER_ERROR ( scNO_CONTINUE_POINT, "attempting to continue outside of a looping construct" )   \
REGISTER_ERROR ( scNO_BREAK_POINT, "attempting to break outside of a construct that supports break" )   \
REGISTER_ERROR ( scVIRTUAL_RELEASE, "virtual destructors not allowed" )   \
REGISTER_ERROR ( scINVALID_FIRST_CLASS, "anonymous classes must be contained within containing function or method" )   \
REGISTER_ERROR ( scNOT_AN_LVALUE, "illegal left hand side of assignment" )   \
REGISTER_ERROR ( scWORKAREA_STACK_DEPTH_EXCEEDED, "workarea stack depth exceeded" )   \
REGISTER_ERROR ( scNOT_A_METHOD, "not a method" )   \
REGISTER_ERROR ( scCANT_RESOLVE_NAMESPACE, "can't resolve namespace" )   \
REGISTER_ERROR ( scUSER_TERMINATE, "debugger initiated termination" )   \
REGISTER_ERROR ( scNOT_A_FUNCTION, "attemtping to call something that is not a function" )   \
REGISTER_ERROR ( scFIELD_REF, "field references not allowed" )   \
REGISTER_ERROR ( scCOROUTINE_CONTINUE, "cannot yield to a dead coroutine" )   \
REGISTER_ERROR ( scCOROUTINE_NORETURN, "not in a coroutine" )   \
REGISTER_ERROR ( scCOROUTINE_NONVARIANT, "coroutines cannot have non-variant parameters" )   \
REGISTER_ERROR ( scITERATOR_RETURN, "iterators can only have variant return type" )   \
REGISTER_ERROR ( scITERATOR_MAIN, "main() must be a function" )   \
REGISTER_ERROR ( scILLEGAL_GOTO, "goto around local block definitions not allowed" )   \
REGISTER_ERROR ( scNO_RETURN, "no valid return value" )   \
REGISTER_ERROR ( scDUPLICATE_INTERFACE, "duplicate interface definition" )   \
REGISTER_ERROR ( scCLASS_MEMBER_NOT_FOUND, "class member not found" )   \
REGISTER_ERROR ( scINTERFACE_NOT_FOUND, "interface not found" )   \
REGISTER_ERROR ( scINTERFACE_CONTRACT, "Unfulfilled interface contract" )   \
REGISTER_ERROR ( scUNKNOWN_NAMESPACE, "Unknown Namespace" )   \
REGISTER_ERROR ( scLAMBDA_PARAMETER, "invalid lambda expression parameter list" )   \
REGISTER_ERROR ( scDEFINITION, "Error in definition" )   \
REGISTER_ERROR ( scSTATIC_VIRTUAL, "Identifier cannot be simultaneously virtual and static" )   \
REGISTER_ERROR ( scILLEGAL_EXTENSION, "illegal extension specificier" )   \
REGISTER_ERROR ( scILLEGAL_STATIC_PROP, "properties cannot be static" )   \
REGISTER_ERROR ( scNOT_STATIC_MEMBER, "member is not static or constant" )   \
REGISTER_ERROR ( scNOT_ENUMERABLE, "operand is not enumerable" )   \
REGISTER_ERROR ( scINVALID_SYMBOL_PACK, "invalid symbol pack" )   \
REGISTER_ERROR ( scINVALID_STORE_MULTI, "invalid destructured assignment" )   \
REGISTER_ERROR ( scASORT_TYPE, "asort requires a consistent type unless a comparator is provided" )   \
REGISTER_ERROR ( scLINQ_EXPRESSION, "linq expression is invalid" )   \
REGISTER_ERROR ( scLINQ_TERMINATE, "linq expression must terminate with either a select or a group" )   \
REGISTER_ERROR ( scLINQ_MISSING_IN, "linq expression missing \"in\" clause" )   \
REGISTER_ERROR ( scLINQ_MISSING_BY, "linq expression missing \"by\" clause" )   \
REGISTER_ERROR ( scLINQ_INVALID_LET, "linq let expression invalid - must be an assignment" )   \
REGISTER_ERROR ( scLINQ_SYMBOL_EXPECTED, "linq expression expecting a symbol" )   \
REGISTER_ERROR ( scLINQ_FROM_AFTER_INTO, "linq expression expecting a from clause after an into" )   \
REGISTER_ERROR ( scFOR_EACH_RETURN, "must have consistent name/value or value return" )   \
REGISTER_ERROR ( scNOT_FOUND, "not found" )   \
REGISTER_ERROR ( scDOUBLE_INITIALIZE, "instance variable cannot have both a default value and an initializer" )   \
REGISTER_ERROR ( scASSERT, "assertion thrown" )   \
REGISTER_ERROR ( scNO_LIBRARY, "library not found" )   \
REGISTER_ERROR ( scNOT_A_REF, "attempting to assign to a value to an r-value" )   \
REGISTER_ERROR ( scNOT_AN_ARRAY, "not an array" )   \
REGISTER_ERROR ( scFIND_FILE_OUT_OF_ORDER, "findFileNext was called before findFileFirst" )   \
REGISTER_ERROR ( scCIRCULAR_REFERENCE, "objects containing circular references not allowed" ) \
REGISTER_ERROR ( scMISSING_IS_IN_MESSAGE, "missing IS in message statement" ) \
REGISTER_ERROR ( scMISSING_IN_IN_MESSAGE, "missing IN in message statement" ) \
REGISTER_ERROR ( scMESSAGE_REANAME_ILLEGAL, "cannot rename new, _pack, _unpack, or default" ) \
REGISTER_ERROR ( scILLEGAL_EXPANSION, "expansions can only occur inside function calls" ) \
REGISTER_ERROR ( scRESET_NOT_APPLICABLE, "reset not valid for this iterator") \
REGISTER_ERROR ( scILLEGAL_FINAL, "FINAL property is only applicable to class definitions") \
REGISTER_ERROR ( scINHERIT_FINAL, "inheriting from a class marked as FINAL") \
REGISTER_ERROR ( scCUSTOM, "user defined error") \
REGISTER_ERROR ( scPSUEDO_OBJ_NEW, "pseudo-object call to new not allowed") \
REGISTER_ERROR ( scINITIALIZER_DECLARING_VARIABLE, "initializers cannot declare variables") \
REGISTER_ERROR ( scMULTIPLE_INITIALZERS, "multiple initializers for same global not allowed" )   \
REGISTER_ERROR ( scMISSING_ENTRY_POINT, "executable does not have an entry point (no main() function)" )   \
REGISTER_ERROR ( scNO_CONSTRUCTOR, "attempting to call a defaulted constructor with parameters" )   \
REGISTER_ERROR ( scINVALID_ITERATOR, "invalid iteration" )   \
REGISTER_ERROR ( scLAMBDA_NOT_ALLOWED_IN_CODEBLOCK, "lambda's not allowed within codeblock" ) \
REGISTER_ERROR ( scEXTENSION_NOT_CLASS_MEMBER, "attempting to call an extension method directly.  Extension methods must be called using <obj>.<extension method> syntax" ) \
REGISTER_ERROR ( scGET_ENUMERATOR_NO_CLASS, "getEnumerator must return a class type" ) \
REGISTER_ERROR ( scINVALID_COMPARATOR_RESULT, "the return value from an overloaded comparitor must be either a long or a boolean" ) \
REGISTER_ERROR ( scNOT_ALLOWED_IN_FGL, "not allowed in FGL source files" ) \
REGISTER_ERROR ( scINVALID_ANONYMOUS_TYPE_FIELD_NAME, "fields in anonymous types take their name from either an assignment or the name of a property used as an initializer" ) \
REGISTER_ERROR ( scINVALID_ANONYMOUS_TYPE_EMPTY, "anonymous types can not be empty" ) \
REGISTER_ERROR ( scDATABASE_REQUEST_TIMEOUT, "database timeout" )   \
REGISTER_ERROR ( scDATABASE_HARD_CLOSE, "database hard close" )   \
REGISTER_ERROR ( scDATABASE_INTEGRITY, "database integrity" )   \
REGISTER_ERROR ( scDB_CONNECTION, "error in database connection" )   \
REGISTER_ERROR ( scTO_MANY_DB_CONNECTIONS, "database has exhausted available connections" )   \
REGISTER_ERROR ( scDATABASE_RECORD_LOCKED_BY_OTHER, "operation cannot be performed, record locked by another" )   \
REGISTER_ERROR ( scINVALID_ALIAS, "invalid alias" )   \
REGISTER_ERROR ( scINVALID_FIELD, "invalid field" )   \
REGISTER_ERROR ( scFIELD_NOT_FOUND, "field not found" )   \
REGISTER_ERROR ( scALIAS_NOT_FOUND, "alias not found" )   \
REGISTER_ERROR ( scPROPERTY_NO_METHOD, "a property statement must include at least a set or get method" )   \
REGISTER_ERROR ( scSAFE_CALL, "safe call violation" )   \
REGISTER_ERROR ( scEMAX, 0 )   \

// warnings

#define WARN_TABLE \
REGISTER_WARN ( scwNO_WARNING, "no warning" )   \
REGISTER_WARN ( scwVARIABLE_USED_BEFORE_INITIALIZED, "variable used before being assigned \"%s\"" )   \
REGISTER_WARN ( scwDEPRECATED_INSTANTIATION, "usage of className %s() to instantate a class is deprecated" )   \
REGISTER_WARN ( scwINCORRECT_NUM_PARAMS, "too many parameters passed to a non-variant function" )   \
REGISTER_WARN ( scwMISSING_PARAMS, "no value passed and parameter does not have a default" )   \
REGISTER_WARN ( scwINCORRECT_NULL_PARAM, "parameter with no default and no passed value being automatically defaulted to null" )   \
REGISTER_WARN ( scwUNDEFINED_FUNCTION, "undefined function \"%s\"" )   \
REGISTER_WARN ( scwUNDEFINED_CLASS, "undefined class \"%s\"" )   \
REGISTER_WARN ( scwUNDEFINED_CLASS_ENTRY, "undefined class entry \"%s::%s\"" )   \
REGISTER_WARN ( scwCONSTANT_ORDERING, "assigning a non-constant expression to \"%s\" has potential ordering side effects") \
REGISTER_WARN ( scwILLEGAL_ESCAPE_SEQUENCE, "unrecognized escape character '%c'") \
REGISTER_WARN ( scwPOSSIBLY_INCORRECT_ASSIGNMENT, "possibly incorrect assignment, did you mean ==") \
REGISTER_WARN ( scwAUTOMATIC_VARIABLE, "automatic local being generated \"%s\"") \
REGISTER_WARN ( scwAUTOMATIC_VARIABLE_MASK, "automatic local masking global symbol \"%s\"") \
REGISTER_WARN ( scwVARIABLE_ASSIGNED_BUT_NEVER_USED, "variable \"%s\" is assigned but never used") \
REGISTER_WARN ( scwVARIABLE_ASSIGNED_BUT_NEVER_USED_HERE, "used here") \
REGISTER_WARN ( scwUNUSED_VARIABLE, "variable \"%s\" is never used") \
REGISTER_WARN ( scwUSELESS_FUNCTION_CALL, "calling const function \"%s\" and ignoring return value") \
REGISTER_WARN ( scwCOMPARISON_COMBINING, "possibly incorrect logic combining, consider using () to enforce order of operations") \
REGISTER_WARN ( scwINCORRECT_COMPARISON, "possibly incorrect comparison, did you mean =") \
REGISTER_WARN ( scwINCORRECT_MUTUAL_EXCLUSION, "Incorrect operator: mutual exclusion over || is always a non-zero constant. Did you intend to use && instead?") \
REGISTER_WARN ( scwINCORRECT_LOGIC, "possibly incorrect ordering of logic statements, use () to enforce order of operations") \
REGISTER_WARN ( scwNO_EFFECT, "statement has no effect") \
REGISTER_WARN ( scwUNABLE_TO_DETERMINE_RETURN_TYPE, "unable to determine return type of function \"%s\"") \
REGISTER_WARN ( scwUNREACHABLE_CODE, "unreachable code") \
REGISTER_WARN ( scwUNKNOWN_SYMBOL, "no definition for symbol \"%s\"") \
REGISTER_WARN ( scwUNKNOWN_PRAGMA, "unknown pragma \"%s\"") \
REGISTER_WARN ( scwUNKNOWN_FUNCTION, "unknown function \"%s\"" )   \
REGISTER_WARN ( scwMAX, 0 )   \


