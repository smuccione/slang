#if DO_THREADED
#define DEF_OP(name) &&op_##name, 
void *opsTable[] =	{
						SLANG_OPS
					};
#undef DEF_OP		
#endif

for ( ;; )
{

#if _DEBUG 
	memset ( stack, uint8_t(slangType::eDEBUG_MARKER), sizeof ( *stack ) * 8 );
	stack->type = slangType::eDEBUG_MARKER;
	// 	validateHeap();
	//	_CrtCheckMemory ( ); 
#endif
#if defined ( MURPHY )
	{
		this->stack = stack;
		GRIP g1 ( this );
	}
#endif
#if DO_THREADED
	#define nextOp	{ ops++; goto *(opsTable[int(ops->op)]); }
	#define goOp goto *(opsTable[int(ops->op)]);;
	#define OP_HANDLER(opcode) op_##opcode:
#else
	#define nextOp	{ ops++; continue; }
	#define goOp continue;
	#define OP_HANDLER(opcode) case fglOpcodes::##opcode:

//	_m_prefetch ( ops + 1 );

	switch ( ops->op )
	{
#endif

		OP_HANDLER ( reserve )
			memset ( stack, 0, ops->imm.index * sizeof ( VAR ) );	// ideally we wouldn't need to memset, but since the garbage collector needs to have valid values...   
			stack += ops->imm.index;
			nextOp;
		// integer ops
		OP_HANDLER ( addi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l += stack->dat.l;
			nextOp;
		OP_HANDLER ( subi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l -= stack->dat.l;
			nextOp;
		OP_HANDLER ( muli )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l *= stack->dat.l;
			nextOp;
		OP_HANDLER ( divi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.d = (double) (stack - 1)->dat.l / (double) stack->dat.l;
			(stack - 1)->type = slangType::eDOUBLE;
			nextOp;
		OP_HANDLER ( modi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l %= stack->dat.l;
			nextOp;
		OP_HANDLER ( powi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (int64_t) pow ( (stack - 1)->dat.l, stack->dat.l );
			nextOp;
		OP_HANDLER ( maxi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			if ( stack->dat.l > (stack - 1)->dat.l )
			{
				(stack - 1)->dat.l = stack->dat.l;
			}
			nextOp;
		OP_HANDLER ( mini )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			if ( stack->dat.l < (stack - 1)->dat.l )
			{
				(stack - 1)->dat.l = stack->dat.l;
			}
			nextOp;
		OP_HANDLER ( negi )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = -(stack - 1)->dat.l;
			nextOp;
		OP_HANDLER ( noti )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = !(stack - 1)->dat.l;
			nextOp;
		OP_HANDLER ( twoci )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = ~(stack - 1)->dat.l;
			nextOp;

		// integer immediate
		OP_HANDLER ( addiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l += (long) ops->imm.intValue;
			nextOp;
		OP_HANDLER ( subiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l -= (long) ops->imm.intValue;
			nextOp;
		OP_HANDLER ( muliImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l *= (long) ops->imm.intValue;
			nextOp;
		OP_HANDLER ( diviImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.d = (double) (stack - 1)->dat.l / ops->imm.intValue;
			(stack - 1)->type = slangType::eDOUBLE;
			nextOp;
		OP_HANDLER ( modiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l %= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( maxiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( ops->imm.intValue > (stack - 1)->dat.l )
			{
				(stack - 1)->dat.l = ops->imm.intValue;
			}
			nextOp;
		OP_HANDLER ( miniImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( ops->imm.intValue < (stack - 1)->dat.l )
			{
				(stack - 1)->dat.l = ops->imm.intValue;
			}
			nextOp;
		OP_HANDLER ( shliImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l <<= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( shriImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l >>= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( bworiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l |= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( bwandiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l &= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( bwxoriImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l ^= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( powiImm )
			throw errorNum::scINTERNAL;
			nextOp;

		// double ops
		OP_HANDLER ( addd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			(stack - 1)->dat.d += stack->dat.d;
			nextOp;
		OP_HANDLER ( subd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			(stack - 1)->dat.d -= stack->dat.d;
			nextOp;
		OP_HANDLER ( muld )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			(stack - 1)->dat.d *= stack->dat.d;
			nextOp;
		OP_HANDLER ( divd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			(stack - 1)->dat.d /= stack->dat.d;
			nextOp;
		OP_HANDLER ( powd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			(stack - 1)->dat.d = pow ( (stack - 1)->dat.d, stack->dat.d );
			nextOp;
		OP_HANDLER ( maxd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			if ( stack->dat.d > (stack - 1)->dat.d )
			{
				(stack - 1)->dat.d = stack->dat.d;
			}
			nextOp;
		OP_HANDLER ( mind )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			stack--;
			if ( stack->dat.d < (stack - 1)->dat.d )
			{
				(stack - 1)->dat.d = stack->dat.d;
			}
			nextOp;
		OP_HANDLER ( negd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->dat.d = -(stack - 1)->dat.d;
			nextOp;
		OP_HANDLER ( notd )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = !(bool) (stack - 1)->dat.d;
			nextOp;

	// double immediate
		OP_HANDLER ( adddImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->dat.d += ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( subdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->dat.d -= ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( muldImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->dat.d *= ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( divdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->dat.d = (stack - 1)->dat.d / ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( maxdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			if ( ops->imm.doubleValue > (stack - 1)->dat.l )
			{
				(stack - 1)->dat.d = ops->imm.doubleValue;
			}
			nextOp;
		OP_HANDLER ( mindImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			if ( ops->imm.doubleValue < (stack - 1)->dat.l )
			{
				(stack - 1)->dat.d = ops->imm.doubleValue;
			}
			nextOp;

		// integer comparison
		OP_HANDLER ( eqi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (stack - 1)->dat.l == stack->dat.l;
			nextOp;
		OP_HANDLER ( eqi1 )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l == (stack - 2)->dat.l;
			nextOp;
		OP_HANDLER ( neqi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (stack - 1)->dat.l != stack->dat.l;
			nextOp;
		OP_HANDLER ( gti )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (stack - 1)->dat.l > stack->dat.l;
			nextOp;
		OP_HANDLER ( gteqi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (stack - 1)->dat.l >= stack->dat.l;
			nextOp;
		OP_HANDLER ( lti )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (stack - 1)->dat.l < stack->dat.l;
			nextOp;
		OP_HANDLER ( lteqi )
			assert ( (stack - 1)->type == slangType::eLONG );
			stack--;
			(stack - 1)->dat.l = (stack - 1)->dat.l <= stack->dat.l;
			nextOp;

		// integer immediate comparison
		OP_HANDLER ( eqiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l == ops->imm.intValue;
			nextOp;
		OP_HANDLER ( neqiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l != ops->imm.intValue;
			nextOp;
		OP_HANDLER ( gtiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l > ops->imm.intValue;
			nextOp;
		OP_HANDLER ( gteqiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l >= ops->imm.intValue;
			nextOp;
		OP_HANDLER ( ltiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l < ops->imm.intValue;
			nextOp;
		OP_HANDLER ( lteqiImm )
			assert ( (stack - 1)->type == slangType::eLONG );
			(stack - 1)->dat.l = (stack - 1)->dat.l <= ops->imm.intValue;
			nextOp;


		// double comparison
		OP_HANDLER ( ltd )
			stack--;
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d < stack->dat.d;
			nextOp;
		OP_HANDLER ( lteqd )
			stack--;
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d <= stack->dat.d;
			nextOp;
		OP_HANDLER ( gtd )
			stack--;
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d > stack->dat.d;
			nextOp;
		OP_HANDLER ( gteqd )
			stack--;
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d >= stack->dat.d;
			nextOp;
		OP_HANDLER ( eqd )
			stack--;
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d == stack->dat.d;
			nextOp;
		OP_HANDLER ( eqd1 )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d == (stack - 2)->dat.d;
			nextOp;
		OP_HANDLER ( neqd )
			stack--;
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d != stack->dat.d;
			nextOp;

		// double immediate comparison
		OP_HANDLER ( eqdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d == ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( neqdImm )
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d != ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( gtdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d > ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( gteqdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d >= ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( ltdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d < ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( lteqdImm )
			assert ( (stack - 1)->type == slangType::eDOUBLE );
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = (stack - 1)->dat.d <= ops->imm.doubleValue;
			nextOp;

		OP_HANDLER ( aeqs )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( !strccmp ( (stack - 1)->dat.str.c, stack->dat.str.c ) )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			nextOp;
		OP_HANDLER ( eqs )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( (*(stack - 1)->dat.str.c == *stack->dat.str.c) && !memcmp ( (stack - 1)->dat.str.c + 1, stack->dat.str.c + 1, stack->dat.str.len ) )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			nextOp;
		OP_HANDLER ( eqs1 )
			assert ( (stack - 1)->type == slangType::eSTRING );
			if ( !strccmp ( (stack - 1)->dat.str.c, (stack - 2)->dat.str.c ) )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			(stack - 1)->type = slangType::eLONG;
			nextOp;
		OP_HANDLER ( lts )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( strccmp ( (stack - 1)->dat.str.c, stack->dat.str.c ) < 0 )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			nextOp;
		OP_HANDLER ( lteqs )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( strccmp ( (stack - 1)->dat.str.c, stack->dat.str.c ) <= 0 )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			nextOp;
		OP_HANDLER ( gts )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( strccmp ( (stack - 1)->dat.str.c, stack->dat.str.c ) > 0 )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			nextOp;
		OP_HANDLER ( gteqs )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( strccmp ( (stack - 1)->dat.str.c, stack->dat.str.c ) >= 0 )
			{
				(stack - 1)->dat.l = 1;
			} else
			{
				(stack - 1)->dat.l = 0;
			}
			nextOp;
		OP_HANDLER ( neqs )
			stack--;
			assert ( (stack - 1)->type == slangType::eSTRING );
			(stack - 1)->type = slangType::eLONG;
			if ( !strccmp ( (stack - 1)->dat.str.c, stack->dat.str.c ) )
			{
				(stack - 1)->dat.l = 0;
			} else
			{
				(stack - 1)->dat.l = 1;
			}
			nextOp;

		// variant comparison
		OP_HANDLER ( ltv )
			this->stack = stack;
			op_ltv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( lteqv )
			this->stack = stack;
			op_lteqv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( gtv )
			this->stack = stack;
			op_gtv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( gteqv )
			this->stack = stack;
			op_gteqv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( aeqv )
			this->stack = stack;
			op_aeqv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( eqv )
			this->stack = stack;
			op_eqv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( eqv1 )
			this->stack = stack;
			op_eqv1 ( this, ops );
			nextOp;
		OP_HANDLER ( neqv )
			this->stack = stack;
			op_neqv ( this, ops, false );
			stack--;
			nextOp;

		// safe variant comparison
		OP_HANDLER ( ltSafev )
			this->stack = stack;
			op_ltv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( lteqSafev )
			this->stack = stack;
			op_lteqv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( gtSafev )
			this->stack = stack;
			op_gtv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( gteqSafev )
			this->stack = stack;
			op_gteqv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( aeqSafev )
			this->stack = stack;
			op_aeqv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( eqSafev )
			this->stack = stack;
			op_eqv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( neqSafev )
			this->stack = stack;
			op_neqv ( this, ops, true );
			stack--;
			nextOp;

		// program flow
		OP_HANDLER ( once )
			if ( (funcDef->loadImage->bssBase)[ops->imm.dual.val2] )
			{
				ops = &(funcDef->loadImage->csBase)[ops->imm.dual.val1];
				goOp;
			} else
			{
				(funcDef->loadImage->bssBase)[ops->imm.dual.val2] = 1;
				nextOp;
			}
		OP_HANDLER ( jmp )
			ops = &(funcDef->loadImage->csBase)[ops->imm.index];
			goOp;
		OP_HANDLER ( jmpc )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( (stack - 1)->dat.l )
			{
				ops = &(funcDef->loadImage->csBase)[ops->imm.index];
				goOp;
			} else
			{
				nextOp;
			}
		OP_HANDLER ( jmpcpop )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( (--stack)->dat.l )
			{
				ops = &(funcDef->loadImage->csBase)[ops->imm.index];
				goOp;
			} else
			{
				nextOp;
			}
		OP_HANDLER ( jmpcpop2 )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( (--stack)->dat.l )
			{
				stack--;
				ops = &(funcDef->loadImage->csBase)[ops->imm.index];
				goOp;
			} else
			{
				nextOp;
			}
		OP_HANDLER ( jmpcv )
			varTmp = stack - 1;
			if ( !varTmp->isTrue() ) nextOp;
			ops = &(funcDef->loadImage->csBase)[ops->imm.index];
			goOp;
		OP_HANDLER ( jmpcpopv )
			varTmp = --stack;
			if ( !varTmp->isTrue() ) nextOp;
			ops = &(funcDef->loadImage->csBase)[ops->imm.index];
			goOp;
		OP_HANDLER ( jmpnc )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( !(stack - 1)->dat.l )
			{
				ops = &(funcDef->loadImage->csBase)[ops->imm.index];
				goOp;
			} else
			{
				nextOp;
			}
		OP_HANDLER ( jmpncpop )
			assert ( (stack - 1)->type == slangType::eLONG );
			if ( !(--stack)->dat.l )
			{
				ops = &(funcDef->loadImage->csBase)[ops->imm.index];
				goOp;
			} else
			{
				nextOp;
			}
		OP_HANDLER ( jmpncv )
			varTmp = stack - 1;
			if ( varTmp->isTrue() ) nextOp;
			ops = &(funcDef->loadImage->csBase)[ops->imm.index];
			goOp;
		OP_HANDLER ( jmpncpopv )
			varTmp = --stack;
			if ( varTmp->isTrue() ) nextOp;
			ops = &(funcDef->loadImage->csBase)[ops->imm.index];
			goOp;
		OP_HANDLER ( jmpnnull )
			varTmp = --stack;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			switch ( TYPE ( varTmp ) )
			{
				case slangType::eNULL:
					// null so just continue exection
					nextOp;
				default:
					break;
			}
			// reached if it's not null... so do a jump
			ops = &(funcDef->loadImage->csBase)[ops->imm.index];
			goOp;
		OP_HANDLER ( jmpGlobalNoInit )
			if ( funcDef->loadImage->globals[ops->imm.dual.val2]->type != slangType::eNULL )
			{
				ops = &(funcDef->loadImage->csBase)[ops->imm.index];
				goOp;
			} else
			{
				nextOp;
			}
		OP_HANDLER ( castRef )
			varTmp = stack - 1;
			if ( TYPE ( varTmp ) != slangType::eREFERENCE )
			{
				throw errorNum::scNOT_A_REF;
			}
			nextOp;

		OP_HANDLER ( castObject )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( (TYPE ( varTmp ) != slangType::eOBJECT_ROOT) && !varTmp->dat.ref.v )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}
			{
				auto len = stack->dat.str.len = *(uint32_t *) ((funcDef->loadImage->dsBase) + ops->imm.index);
				auto name = (char *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t ));

				if ( len )
				{
					if ( memcmpi ( varTmp->dat.ref.v->dat.obj.classDef->name, name, len + 1 ) )
					{
						throw errorNum::scWRONG_OBJECT;
					}
				}
			}
			nextOp;
		OP_HANDLER ( castArray )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_ROOT )
			{
				throw errorNum::scNOT_AN_ARRAY;
			}
			nextOp;

		OP_HANDLER ( castFunction )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eATOM )
			{
				if ( TYPE ( varTmp ) != slangType::eOBJECT_ROOT || TYPE ( varTmp ) != slangType::eOBJECT )
				{
					if ( !varTmp->dat.ref.v->dat.obj.classDef->ops[int(fgxOvOp::ovFuncCall)] )
					{
						throw errorNum::scNOT_A_FUNCTION;
					}
				} else
				{
					throw errorNum::scNOT_A_FUNCTION;
				}
			}
			nextOp;

		OP_HANDLER ( castBool )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			switch ( varTmp->type )
			{
				case slangType::eNULL:
					(stack - 1)->dat.l = 0;
					break;
				case slangType::eDOUBLE:
					(stack - 1)->dat.l = (bool) ((long) varTmp->dat.d);
					break;
				case slangType::eLONG:
					(stack - 1)->dat.l = (bool) (long) varTmp->dat.l;
					break;
				case slangType::eSTRING:
					(stack - 1)->dat.l = varTmp->dat.str.len ? 1 : 0;
					break;
				case slangType::eARRAY_ROOT:
					switch ( varTmp->dat.ref.v->type )
					{
						case slangType::eARRAY_FIXED:
							(stack - 1)->dat.l = varTmp->dat.ref.v->dat.arrayFixed.endIndex >= varTmp->dat.ref.v->dat.arrayFixed.startIndex ? 1 : 0;
							break;
						case slangType::eARRAY_SPARSE:
							(stack - 1)->dat.l = varTmp->dat.ref.v->dat.aSparse.v ? 1 : 0;
							break;
						default:
							break;
					}
					break;
				case slangType::eOBJECT_ROOT:
					if ( varTmp->dat.ref.v )
					{
						(stack - 1)->dat.l = 1;
					} else
					{
						(stack - 1)->dat.l = 0;
					}
					break;
				case slangType::eCODEBLOCK_ROOT:
				case slangType::eATOM:
					(stack - 1)->dat.l = 1;
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			(stack - 1)->type = slangType::eLONG;
			nextOp;
		OP_HANDLER ( castInt )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			switch ( varTmp->type )
			{
				case slangType::eNULL:
					(stack - 1)->dat.l = 0;
					break;
				case slangType::eDOUBLE:
					(stack - 1)->dat.l = (long) varTmp->dat.d;
					break;
				case slangType::eLONG:
					*(stack - 1) = *varTmp;
					break;
				case slangType::eSTRING:
					(stack - 1)->dat.l = atol ( varTmp->dat.str.c );
					break;
				case slangType::eARRAY_ROOT:
					switch ( varTmp->dat.ref.v->type )
					{
						case slangType::eARRAY_FIXED:
							(stack - 1)->dat.l = varTmp->dat.ref.v->dat.arrayFixed.endIndex >= varTmp->dat.ref.v->dat.arrayFixed.startIndex ? 1 : 0;
							break;
						case slangType::eARRAY_SPARSE:
							(stack - 1)->dat.l = varTmp->dat.ref.v->dat.aSparse.v ? 1 : 0;
							break;
						default:
							break;
					}
					break;
				case slangType::eOBJECT_ROOT:
					if ( varTmp->dat.ref.v )
					{
						(stack - 1)->dat.l = 1;
					} else
					{
						(stack - 1)->dat.l = 0;
					}
					break;
				case slangType::eCODEBLOCK_ROOT:
				case slangType::eATOM:
					(stack - 1)->dat.l = 1;
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			(stack - 1)->type = slangType::eLONG;
			nextOp;
		OP_HANDLER ( castDouble )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			switch ( varTmp->type )
			{
				case slangType::eNULL:
					(stack - 1)->dat.d = 0;
					break;
				case slangType::eDOUBLE:
					*(stack - 1) = *varTmp;
					break;
				case slangType::eLONG:
					(stack - 1)->dat.d = (double) varTmp->dat.l;
					break;
				case slangType::eSTRING:
					(stack - 1)->dat.d = atof ( varTmp->dat.str.c );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			(stack - 1)->type = slangType::eDOUBLE;
			nextOp;
		OP_HANDLER ( castString )
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			switch ( varTmp->type )
			{
				case slangType::eSTRING:
					*(stack - 1) = *varTmp;
					break;
				case slangType::eLONG:
					{
						this->stack = stack;	// necessary because garbage collection may occur and instance->stack MUST be valid
						GRIP g1 ( this );
						char tmpStr[_MAX_I64TOSTR_BASE10_COUNT + 1];
						_i64toa ( varTmp->dat.l, tmpStr, 10 );
						(stack - 1)->type = slangType::eSTRING;
						(stack - 1)->dat.str.c = om->strDup ( tmpStr );
						(stack - 1)->dat.str.len = strlen ( tmpStr );
					}
					break;
				case slangType::eDOUBLE:
					{
						this->stack = stack;	// necessary because garbage collection may occur and instance->stack MUST be valid
						GRIP g1 ( this );
						char tmpStr[64];
						sprintf_s ( tmpStr, sizeof ( tmpStr ), "%f", varTmp->dat.d );
						(stack - 1)->dat.str.c = om->strDup ( tmpStr );
						(stack - 1)->dat.str.len = strlen ( tmpStr );
						(stack - 1)->type = slangType::eSTRING;
					}
					break;
				case slangType::eNULL:
					(stack - 1)->type = slangType::eSTRING;
					(stack - 1)->dat.str.c = "";
					(stack - 1)->dat.str.len = 0;
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			nextOp;
		OP_HANDLER ( castBooli )
			(stack - 1)->dat.l = (bool) (stack - 1)->dat.l;
			nextOp;

		// object ivar
		OP_HANDLER ( pushObjIVar )
			assert ( basePtr[ops->imm.objOp.stackIndex].type == slangType::eOBJECT_ROOT );
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = varTmp[ops->imm.objOp.objectIndex];
			nextOp;
		OP_HANDLER ( pushObjIVarRef )
			assert ( basePtr[ops->imm.objOp.stackIndex].type == slangType::eOBJECT_ROOT );
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = std::move ( VAR_REF (varTmp, &varTmp[ops->imm.objOp.objectIndex] ) );
			nextOp;
		OP_HANDLER ( pushObjIVarRel )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = varTmp[ops->imm.objOp.objectIndex];
			nextOp;
		OP_HANDLER ( pushObjIVarRelRef )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = std::move ( VAR_REF (varTmp, &varTmp[ops->imm.objOp.objectIndex] ) );
			nextOp;

			// object virtual ivar
		OP_HANDLER ( pushObjVirtIVar )
			assert ( basePtr[ops->imm.objOp.stackIndex].type == slangType::eOBJECT_ROOT );
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];
			nextOp;
		OP_HANDLER ( pushObjVirtIVarRef )
			assert ( basePtr[ops->imm.objOp.stackIndex].type == slangType::eOBJECT_ROOT );
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = std::move ( VAR_REF (varTmp, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] ) );
			nextOp;
		OP_HANDLER ( pushObjVirtIVarRel )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];
			nextOp;
		OP_HANDLER ( pushObjVirtIVarRelRef )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = std::move ( VAR_REF (varTmp, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] ) );
			nextOp;

		OP_HANDLER ( pushContext )
			assert ( basePtr[ops->imm.objOp.stackIndex].type == slangType::eOBJECT_ROOT );
			varTmp = &basePtr[ops->imm.objOp.stackIndex];
			if ( varTmp->dat.ref.v->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = std::move ( VAR_OBJ ( varTmp, &varTmp->dat.ref.v[ops->imm.objOp.objectIndex] ) );
			nextOp;
		OP_HANDLER ( pushContextVirt )
			assert ( basePtr[ops->imm.objOp.stackIndex].type == slangType::eOBJECT_ROOT );
			varTmp = &basePtr[ops->imm.objOp.stackIndex];
			if ( varTmp->dat.ref.v->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			*(stack++) = std::move ( VAR_OBJ ( varTmp, &varTmp->dat.ref.v[varTmp->dat.ref.v->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] ) );
			nextOp;

		OP_HANDLER ( modifyContext )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.obj;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			(stack - ops->imm.objOp.stackIndex)->dat.ref.v = &varTmp[ops->imm.objOp.objectIndex];
			nextOp;
		OP_HANDLER ( modifyContextVirt )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.obj;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			(stack - ops->imm.objOp.stackIndex)->dat.ref.v = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];
			nextOp;

		OP_HANDLER ( pushSender )
			assert ( (stack - ops->imm.objOp.stackIndex)->type == slangType::eOBJECT_ROOT );
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			*(stack++) = varTmp[1];										// most derived is always at offset 1 from any object
			nextOp;
		OP_HANDLER ( pushSymLocal )
			*(stack++) = basePtr[ops->imm.index];
			nextOp;
		OP_HANDLER ( pushSymLocalRef )
			*(stack++) = std::move ( VAR_REF ( &basePtr[ops->imm.index], &basePtr[ops->imm.index] ) );
			nextOp;
		OP_HANDLER ( pushNull )
			stack++->type = slangType::eNULL;
			nextOp;
		OP_HANDLER ( pushSymGlobal )
			*(stack++) = *(funcDef->loadImage->globals)[ops->imm.index];
			nextOp;
		OP_HANDLER ( pushSymGlobalRef )
			varTmp = (funcDef->loadImage->globals)[ops->imm.index];
			while ( varTmp->type == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			*(stack++) = std::move ( VAR_REF (varTmp, varTmp ) );
			nextOp;
		OP_HANDLER ( pushNumLong )
			stack->type = slangType::eLONG;
			(stack++)->dat.l = (long) ops->imm.intValue;
			nextOp;
		OP_HANDLER ( pushAtom )
			stack->type = slangType::eATOM;
			(stack++)->dat.l = (long) ops->imm.index;
			nextOp;
		OP_HANDLER ( pushNumDouble )
			stack->type = slangType::eDOUBLE;
			(stack++)->dat.d = ops->imm.doubleValue;
			nextOp;
		OP_HANDLER ( pushString )
			stack->type = slangType::eSTRING;
			stack->dat.str.len = *(uint32_t *) ((funcDef->loadImage->dsBase) + ops->imm.index);
			stack->dat.str.c = (char *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t ));
			stack++;
			nextOp;
		OP_HANDLER ( pushCodeblock )
			{
				auto cb = (vmCodeBlock *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t ));
				stack->type = slangType::eCODEBLOCK_ROOT;
				stack->dat.ref.v = om->allocVar ( cb->nSymbols + 1 );
				stack->dat.ref.obj = stack->dat.ref.v;
				memset ( &stack->dat.ref.v[1], 0, sizeof ( VAR ) * (cb->nSymbols - cb->nParams + 1) );
				stack->dat.ref.v->type = slangType::eCODEBLOCK;
				stack->dat.ref.v->dat.cb.cb = cb;
		
				stack++;
				this->stack = stack;
				op_cbFixup ( this, funcDef, basePtr );
			}
			nextOp;
		OP_HANDLER ( makeParamPack )
			{
				uint32_t nVarPack = (nParams - (funcDef->nParams - 1));

				if ( (nVarPack != 1) || (basePtr[(int32_t) ops->imm.index].type != slangType::eNULL) )
				{
					this->stack = stack;
					auto arr = om->allocVar ( nVarPack + 1 );

					// save off the array first as we're going to blow out the first element next
					for ( uint64_t loop = 0; loop < nVarPack; loop++ )
					{
						arr[loop + 1] = basePtr[(int32_t) ops->imm.index - loop];
					}

					basePtr[ops->imm.index].type = slangType::eARRAY_ROOT;
					basePtr[ops->imm.index].dat.ref.v = arr;
					basePtr[ops->imm.index].dat.ref.obj = arr;
					basePtr[ops->imm.index].dat.ref.v->type = slangType::eARRAY_FIXED;
					basePtr[ops->imm.index].dat.ref.v->dat.arrayFixed.startIndex = 1;
					basePtr[ops->imm.index].dat.ref.v->dat.arrayFixed.endIndex = nVarPack;
				} else
				{
					this->stack = stack;
					basePtr[ops->imm.index].dat.ref.v = om->allocVar ( 1 );
					basePtr[ops->imm.index].dat.ref.obj = basePtr[ops->imm.index].dat.ref.v;
					basePtr[ops->imm.index].type = slangType::eARRAY_ROOT;

					basePtr[ops->imm.index].dat.ref.v->type = slangType::eARRAY_SPARSE;
					basePtr[ops->imm.index].dat.ref.v->dat.aSparse.lastAccess = 0;
					basePtr[ops->imm.index].dat.ref.v->dat.aSparse.v = 0;
					basePtr[ops->imm.index].dat.ref.v->dat.aSparse.maxI = 0;
				}
			}
			nextOp;
		OP_HANDLER ( pushFixedArray )
			{
				this->stack = stack;

				auto arr = om->allocVar ( ops->imm.index + 1 );

				stack->type = slangType::eARRAY_ROOT;
				stack->dat.ref.v = arr;
				stack->dat.ref.obj = arr;
				stack->dat.ref.v->type = slangType::eARRAY_FIXED;
				stack->dat.ref.v->dat.arrayFixed.startIndex = 1;
				stack->dat.ref.v->dat.arrayFixed.endIndex = ops->imm.index;
				memcpy ( arr + 1, stack - ops->imm.index, sizeof ( VAR ) * ops->imm.index );

				varTmp = stack;

				stack -= ops->imm.index;

				(*stack++) = *varTmp;
			}
			nextOp;
		OP_HANDLER ( pushNullArray )
			this->stack = stack;
			// we're safe doing this... stack is NOT collected as it's not a live.  alloc will return a valid value even if collection occurs and we just reuse this value.
			stack->dat.ref.v = om->allocVar ( 1 );
			stack->dat.ref.obj = stack->dat.ref.v;
			stack->type = slangType::eARRAY_ROOT;

			stack->dat.ref.v->type = slangType::eARRAY_SPARSE;
			stack->dat.ref.v->dat.aSparse.lastAccess = 0;
			stack->dat.ref.v->dat.aSparse.v = 0;
			stack->dat.ref.v->dat.aSparse.maxI = 0;
			stack++;
			nextOp;
		OP_HANDLER ( pushAArray )
			this->stack = stack;
			aArrayNew ( this, ops->imm.index );
			stack = this->stack;
			nextOp;
		OP_HANDLER ( pushVariableArray )
			this->stack = stack;
			op_vArrayNew ( this, ops->imm.index );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( pushClassGlobal )
			this->stack = stack;
			op_pushValue ( this, ops, (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val1 + sizeof ( uint32_t ), (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val2 + sizeof ( uint32_t ) );
			stack++;
			nextOp;
		OP_HANDLER ( pushClassGlobalRef )
			this->stack = stack;
			op_pushValueRef ( this, ops, (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val1 + sizeof ( uint32_t ), (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val2 + sizeof ( uint32_t ) );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( makeEnumerator )
			varTmp = stack - 1;
			while ( varTmp->type == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			switch ( varTmp->type )
			{
				case slangType::eARRAY_ROOT:
					switch ( varTmp->dat.ref.v->type )
					{
						case slangType::eARRAY_FIXED:
							this->stack = stack;
							classNew ( this, "fixedArrayEnumerator", 1 );
							break;
						case slangType::eARRAY_SPARSE:
							this->stack = stack;
							classNew ( this, "sparseArrayEnumerator", 1 );
							break;
						default:
							throw errorNum::scINTERNAL;
					}
					break;
				case slangType::eSTRING:
					this->stack = stack;
					classNew ( this, "stringEnumerator", 1 );
					break;
				case slangType::eOBJECT_ROOT:
					this->stack = stack;
					classCallMethod ( this, stack - 1, "getEnumerator", 0 );
					break;
				default:
					throw errorNum::scNOT_ENUMERABLE;
			}
			*(stack - 1) = result;
			nextOp;

		OP_HANDLER ( store )
			this->stack = stack;
			op_assign ( this, ops, stack - 1, stack - 2 );
			stack--;
			varTmp = stack - 1;
			while ( varTmp->type == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			*(stack - 1) = *varTmp;
			nextOp;
		OP_HANDLER ( storePop )
			this->stack = stack;
			op_assign ( this, ops, stack - 1, stack - 2 );
			stack -= 2;
			nextOp;

		OP_HANDLER ( storeClassGlobal )
			this->stack = stack;
			op_storeValue ( this, ops, (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val1 + sizeof ( uint32_t ), (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val2 + sizeof ( uint32_t ) );
			varTmp = stack - 1;
			while ( varTmp->type == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			*(stack - 1) = *varTmp;
			break;
		OP_HANDLER ( storeClassGlobalPop )
			this->stack = stack;
			op_storeValue ( this, ops, (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val1 + sizeof ( uint32_t ), (char *) (funcDef->loadImage->dsBase) + ops->imm.dual.val2 + sizeof ( uint32_t ) );
			stack -= 1;
			break;

		OP_HANDLER ( storeLocalAdd )
			this->stack = stack;
			op_assignAdd ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalAdd )
			this->stack = stack;
			op_assignAdd ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarAdd )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignAdd ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarAdd )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignAdd ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefAdd )
			this->stack = stack;
			op_assignAdd ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalSub )
			this->stack = stack;
			op_assignSub ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalSub )
			this->stack = stack;
			op_assignSub ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarSub )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignSub ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarSub )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignSub ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefSub )
			this->stack = stack;
			op_assignSub ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalMul )
			this->stack = stack;
			op_assignMul ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalMul )
			this->stack = stack;
			op_assignMul ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarMul )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignMul ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarMul )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignMul ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefMul )
			this->stack = stack;
			op_assignMul ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalDiv )
			this->stack = stack;
			op_assignDiv ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalDiv )
			this->stack = stack;
			op_assignDiv ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarDiv )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignDiv ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarDiv )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignDiv ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefDiv )
			this->stack = stack;
			op_assignDiv ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalPow )
			this->stack = stack;
			op_assignPow ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalPow )
			this->stack = stack;
			op_assignPow ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarPow )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignPow ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarPow )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignPow ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefPow )
			this->stack = stack;
			op_assignPow ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalMod )
			this->stack = stack;
			op_assignMod ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalMod )
			this->stack = stack;
			op_assignMod ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarMod )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignMod ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarMod )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignMod ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefMod )
			this->stack = stack;
			op_assignMod ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalBwAnd )
			this->stack = stack;
			op_assignBwAnd ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalBwAnd )
			this->stack = stack;
			op_assignBwAnd ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarBwAnd )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignBwAnd ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarBwAnd )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignBwAnd ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefBwAnd )
			this->stack = stack;
			op_assignBwAnd ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalBwOr )
			this->stack = stack;
			op_assignBwOr ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalBwOr )
			this->stack = stack;
			op_assignBwOr ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarBwOr )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignBwOr ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarBwOr )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignBwOr ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefBwOr )
			this->stack = stack;
			op_assignBwOr ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalBwXor )
			this->stack = stack;
			op_assignBwXor ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalBwXor )
			this->stack = stack;
			op_assignBwXor ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarBwXor )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignBwXor ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarBwXor )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignBwXor ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefBwXor )
			this->stack = stack;
			op_assignBwXor ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalShLeft )
			this->stack = stack;
			op_assignShLeft ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalShLeft )
			this->stack = stack;
			op_assignShLeft ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarShLeft )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignShLeft ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarShLeft )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignShLeft ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefShLeft )
			this->stack = stack;
			op_assignShLeft ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		OP_HANDLER ( storeLocalShRight )
			this->stack = stack;
			op_assignShRight ( this, ops, &basePtr[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeGlobalShRight )
			this->stack = stack;
			op_assignShRight ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( storeClassIVarShRight )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignShRight ( this, ops, &varTmp[ops->imm.objOp.objectIndex], stack - 1 );
			nextOp;
		OP_HANDLER ( storeClassVirtIVarShRight )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			this->stack = stack;
			op_assignShRight ( this, ops, &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta], stack - 1 );
			nextOp;
		OP_HANDLER ( storeRefShRight )
			this->stack = stack;
			op_assignShRight ( this, ops, stack - 2, stack - 1 );
			stack -= ops->imm.dual.val2;
			nextOp;

		// string operations
		OP_HANDLER ( adds )
			{
				char *newString;

				size_t size = (stack - 1)->dat.str.len + (stack - 2)->dat.str.len;

				this->stack = stack;

				newString = (char *) om->alloc ( size + 1 );

				memcpy ( newString, (stack - 2)->dat.str.c, (stack - 2)->dat.str.len );
				memcpy ( newString + (stack - 2)->dat.str.len, (stack - 1)->dat.str.c, (stack - 1)->dat.str.len );
				newString[size] = 0;

				stack--;
				(stack - 1)->dat.str.len = size;
				(stack - 1)->dat.str.c = newString;
			}
			nextOp;
		OP_HANDLER ( addsmulti )
			{
				char *newString;
				size_t size = 0;
				for ( size_t loop = ops->imm.index; loop; loop-- )
				{
					size += (stack - loop)->dat.str.len;
				}

				this->stack = stack;

				newString = (char *) om->alloc ( size + 1 );

				size = 0;
				for ( size_t loop = ops->imm.index; loop; loop-- )
				{
					memcpy ( newString + size, (stack - loop)->dat.str.c, (stack - loop)->dat.str.len );
					size += (stack - loop)->dat.str.len;
				}
				stack -= ops->imm.index - 1;
				newString[size] = 0;

				(stack - 1)->dat.str.len = size;
				(stack - 1)->dat.str.c = newString;
			}
			nextOp;
		OP_HANDLER ( nots )
			(stack - 1)->type = slangType::eLONG;
			(stack - 1)->dat.l = !(stack - 1)->dat.str.len;
			nextOp;

		OP_HANDLER ( cmps )
			(stack - 2)->type = slangType::eLONG;
			(stack - 2)->dat.l = strcmp ( (stack - 1)->dat.str.c, (stack - 2)->dat.str.c );
			nextOp;

		// variant operations
		OP_HANDLER ( addv )
			this->stack = stack;
			op_addv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( subv )
			this->stack = stack;
			op_subv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( mulv )
			this->stack = stack;
			op_mulv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( divv )
			this->stack = stack;
			op_divv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( modv )
			this->stack = stack;
			op_modv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( powv )
			this->stack = stack;
			op_powv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( maxv )
			this->stack = stack;
			op_maxv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( minv )
			this->stack = stack;
			op_minv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( negv )
			this->stack = stack;
			op_negv ( this, ops, stack - 1, false );
			nextOp;
		OP_HANDLER ( notv )
			this->stack = stack;
			op_notv ( this, ops, stack - 1, false );
			nextOp;
		OP_HANDLER ( twocv )
			this->stack = stack;
			op_twocv ( this, ops, stack - 1, false );
			nextOp;

		OP_HANDLER ( bworv )
			this->stack = stack;
			op_bwOrv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( bwandv )
			this->stack = stack;
			op_bwAndv ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( bwxorv )
			this->stack = stack;
			op_bwXor ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( shlv )
			this->stack = stack;
			op_bwShLeft ( this, ops, false );
			stack--;
			nextOp;
		OP_HANDLER ( shrv )
			this->stack = stack;
			op_bwShRight ( this, ops, false );
			stack--;
			nextOp;

		// safe variant operations
		OP_HANDLER ( addSafev )
			this->stack = stack;
			op_addv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( subSafev )
			this->stack = stack;
			op_subv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( mulSafev )
			this->stack = stack;
			op_mulv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( divSafev )
			this->stack = stack;
			op_divv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( modSafev )
			this->stack = stack;
			op_modv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( powSafev )
			this->stack = stack;
			op_powv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( maxSafev )
			this->stack = stack;
			op_maxv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( minSafev )
			this->stack = stack;
			op_minv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( negSafev )
			this->stack = stack;
			op_negv ( this, ops, stack - 1, true );
			nextOp;
		OP_HANDLER ( notSafev )
			this->stack = stack;
			op_notv ( this, ops, stack - 1, true );
			nextOp;
		OP_HANDLER ( twocSafev )
			this->stack = stack;
			op_twocv ( this, ops, stack - 1, true );
			nextOp;

		OP_HANDLER ( bworSafev )
			this->stack = stack;
			op_bwOrv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( bwandSafev )
			this->stack = stack;
			op_bwAndv ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( bwxorSafev )
			this->stack = stack;
			op_bwXor ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( shlSafev )
			this->stack = stack;
			op_bwShLeft ( this, ops, true );
			stack--;
			nextOp;
		OP_HANDLER ( shrSafev )
			this->stack = stack;
			op_bwShRight ( this, ops, true );
			stack--;
			nextOp;

		OP_HANDLER ( callBC )
			{
				this->stack = stack;
				auto callerFDef = atomTable->atomGetFunc ( ops->imm.dual.val1 );
				interpretBC ( callerFDef, ops + 1, ops->imm.dual.val2 );
				stack -= ops->imm.dual.val2;
				*(stack++) = result;
			}
			nextOp;
		OP_HANDLER ( callBCPop )
			{
				this->stack = stack;
				auto callerFDef = atomTable->atomGetFunc ( ops->imm.dual.val1 );
				interpretBC ( callerFDef, ops + 1, ops->imm.dual.val2 );
				stack -= ops->imm.dual.val2;
			}
			nextOp;
		OP_HANDLER ( callBCVirt )
			{
				varTmp = (stack - 1)->dat.ref.v;
				if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
				auto callerFDef = atomTable->atomGetFunc ( varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].atom );		// find our atom using the virtual table
	//					(stack - 1)->dat.ref.v = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];		// adjust our object using the offset in the virtual table - already an objRoot so just need to adjust dat.ref.v
				this->stack = stack;
				interpretBC ( callerFDef, ops + 1, ops->imm.objOp.nParams );
				stack -= ops->imm.objOp.nParams;
				*(stack++) = result;
			}
			nextOp;
		OP_HANDLER ( callBCVirtPop )
			{
				varTmp = (stack - 1)->dat.ref.v;
				if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
				auto callerFDef = atomTable->atomGetFunc ( varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].atom );		// find our atom using the virtual table
	//					(stack - 1)->dat.ref.v = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];		// adjust our object using the offset in the virtual table
				this->stack = stack;
				interpretBC ( callerFDef, ops + 1, ops->imm.objOp.nParams );
				stack -= ops->imm.objOp.nParams;
			}
			nextOp;
		OP_HANDLER ( callC )
			this->stack = stack;
			debug->IP = ops + 1;
			funcCall ( ops->imm.dual.val1, ops->imm.dual.val2 );
			stack -= ops->imm.dual.val2;
			*(stack++) = result;
			nextOp;
		OP_HANDLER ( callCPop )
			this->stack = stack;
			debug->IP = ops + 1;
			funcCall ( ops->imm.dual.val1, ops->imm.dual.val2 );
			stack -= ops->imm.dual.val2;
			nextOp;
		OP_HANDLER ( callCVirt )
			{
				varTmp = (stack - 1)->dat.ref.v;
				if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
				auto callerFDef = atomTable->atomGetFunc ( varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].atom );		// find our atom using the virtual table
	//				(stack - 1)->dat.ref.v = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];		// adjust our object using the offset in the virtual table - already an objRoot so just need to adjust dat.ref.v
				this->stack = stack;
				funcCall ( callerFDef, ops->imm.objOp.nParams );
				stack -= ops->imm.objOp.nParams;
				*(stack++) = result;
			}
			nextOp;
		OP_HANDLER ( callCVirtPop )
			{
				varTmp = (stack - 1)->dat.ref.v;
				if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
				auto callerFDef = atomTable->atomGetFunc ( varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].atom );		// find our atom using the virtual table
	//					(stack - 1)->dat.ref.v = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];		// adjust our object using the offset in the virtual table - already an objRoot so just need to adjust dat.ref.v
				this->stack = stack;
				funcCall ( callerFDef, ops->imm.objOp.nParams );
				stack -= ops->imm.objOp.nParams;
			}
			nextOp;
		OP_HANDLER ( callBCTail )
			// move basePtr to the start of the callstack, including any variant parameters
			basePtr = basePtr - (nParams - funcDef->nParams);

			// copy over our parameters
			memcpy ( basePtr, stack - ops->imm.dual.val2, sizeof ( VAR ) * ops->imm.dual.val2 );

			// TODO: verify this logic
			nParams = ops->imm.dual.val2;
			stack = basePtr + ops->imm.dual.val2;

			funcDef = atomTable->atomGetFunc ( ops->imm.dual.val1 );

			basePtr = stack - funcDef->nParams;

			stack->type = slangType::ePCALL;
			stack->dat.pCall.op = ops;
			stack->dat.pCall.func = funcDef;
			stack->dat.pCall.nParams = nParams;
			stack++;

			ops = funcDef->cs;
			goOp;

		OP_HANDLER ( callDefValue )
			if ( ops->imm.index < destFunc->nParams )
			{
				if ( destFunc->defaultParams && destFunc->defaultParams[ops->imm.index] )
				{
					stack = this->interpretBCParam ( destFunc, destFunc, destFunc->loadImage->csBase + destFunc->defaultParams[ops->imm.index], 0, stack, basePtr );
					this->stack = stack;
				} else
				{
					stack++->type = slangType::eNULL;
				}
			} else
			{
				stack++->type = slangType::eNULL;
			}
			nextOp;

		OP_HANDLER ( result )
			this->result = *(--stack);
			nextOp;

		OP_HANDLER ( resultv )
			varTmp = --stack;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			this->result = *varTmp;
			nextOp;

		OP_HANDLER ( pushResult )
			*(stack++) = this->result;
			nextOp;

		OP_HANDLER ( ret )
	#if _DEBUG
			if ( basePtr + funcDef->nParams != stack - 1 )
			{
	//			throw errorNum::scINTERNAL;
			}
	#endif
			this->stack = stack - 1;
			RETURN ( nullptr );

		OP_HANDLER ( pRet )
			RETURN ( stack );

		OP_HANDLER ( callV )
			this->stack = stack;
			ops = op_call ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			goOp;
		OP_HANDLER ( callVPop )
			this->stack = stack;
			ops = op_call ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			goOp;
		OP_HANDLER ( callVSafe )
			this->stack = stack;
			ops = op_callSafe ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			goOp;
		OP_HANDLER ( callVSafePop )
			this->stack = stack;
			ops = op_callSafe ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			goOp;

		OP_HANDLER ( callPPack )
			this->stack = stack;
			ops = op_callPPack ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			goOp;
		OP_HANDLER ( callPPackPop )
			this->stack = stack;
			ops = op_callPPack ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			goOp;
		OP_HANDLER ( callPPackSafe )
			this->stack = stack;
			ops = op_callPPackSafe ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			goOp;
		OP_HANDLER ( callPPackSafePop )
			this->stack = stack;
			ops = op_callPPackSafe ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			goOp;

		// code block ops
		OP_HANDLER ( callCB )
			this->stack = stack;
			op_cbCall ( this, ops, funcDef, basePtr, ops->imm.index );
			stack -= ops->imm.index + 1;
			*(stack++) = result;
			nextOp;
		OP_HANDLER ( callCBPop )
			this->stack = stack;
			op_cbCall ( this, ops, funcDef, basePtr, ops->imm.index );
			stack -= ops->imm.index + 1;
			nextOp;
		OP_HANDLER ( evaluate )
			this->stack = stack;
			op_compile ( this, funcDef, true );
			op_cbFixup ( this, funcDef, basePtr );
			op_cbCall ( this, ops, funcDef, basePtr, 0 );
			*(stack - 1) = result;
			nextOp;

		// variable access
		OP_HANDLER ( storeLocalNull )
			basePtr[ops->imm.index].type = slangType::eNULL;
			nextOp;
		OP_HANDLER ( storeLocal )
			basePtr[ops->imm.index] = *(stack - 1);
			nextOp;
		OP_HANDLER ( storeLocalPop )
			basePtr[ops->imm.index] = *(--stack);
			nextOp;
		OP_HANDLER ( storeLocalv )
			this->stack = stack;
			op_assign ( this, ops, &basePtr[ops->imm.index], stack - 1 );
			nextOp;
		OP_HANDLER ( storeLocalPopv )
			this->stack = stack;
			op_assign ( this, ops, &basePtr[ops->imm.index], --stack );
			nextOp;
		OP_HANDLER ( storeGlobal )
			varTmp = funcDef->loadImage->globals[ops->imm.index];
			while ( varTmp->type == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			*varTmp = *(stack - 1);
			nextOp;
		OP_HANDLER ( storeGlobalPop )
			varTmp = funcDef->loadImage->globals[ops->imm.index];
			while ( varTmp->type == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			*varTmp = *(--stack);
			nextOp;
		OP_HANDLER ( storeGlobalv )
			this->stack = stack;
			op_assign ( this, ops, (funcDef->loadImage->globals)[ops->imm.index], stack - 1 );
			nextOp;
		OP_HANDLER ( storeGlobalPopv )
			this->stack = stack;
			op_assign ( this, ops, (funcDef->loadImage->globals)[ops->imm.index], --stack );
			nextOp;

		OP_HANDLER ( storeClassIVar )
			if ( !basePtr[ops->imm.objOp.stackIndex].dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;
			basePtr[ops->imm.objOp.stackIndex].dat.ref.v[ops->imm.objOp.objectIndex] = *(stack - 1);
			nextOp;
		OP_HANDLER ( storeClassIVarPop )
			if ( !basePtr[ops->imm.objOp.stackIndex].dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;
			basePtr[ops->imm.objOp.stackIndex].dat.ref.v[ops->imm.objOp.objectIndex] = *(--stack);
			nextOp;
		OP_HANDLER ( storeClassIVarRel )
			if ( !(stack - ops->imm.objOp.stackIndex)->dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;
			(stack - ops->imm.objOp.stackIndex)->dat.ref.v[ops->imm.objOp.objectIndex] = *(stack - 2);
			stack--;
			nextOp;
		OP_HANDLER ( storeClassIVarRelPop )
			if ( !(stack - ops->imm.objOp.stackIndex)->dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;
			(stack - 1)->dat.ref.v[ops->imm.objOp.objectIndex] = *(stack - 2);
			stack -= 2;
			nextOp;

		OP_HANDLER ( storeClassVirtIVar )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] = *(stack - 1);
			nextOp;
		OP_HANDLER ( storeClassVirtIVarPop )
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] = *(--stack);
			nextOp;
		OP_HANDLER ( storeClassVirtIVarRel )
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] = *(stack - 1);
			nextOp;
		OP_HANDLER ( storeClassVirtIVarRelPop )
			varTmp = (stack - ops->imm.objOp.stackIndex)->dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta] = *(--stack);
			nextOp;

		OP_HANDLER ( incLocali )
			basePtr[ops->imm.index].dat.l++;
			nextOp;
		OP_HANDLER ( incLocald )
			basePtr[ops->imm.index].dat.d++;
			nextOp;
		OP_HANDLER ( incLocalv )
			this->stack = stack;
			op_incv ( this, ops, &basePtr[ops->imm.dual.val1], (fgxOvOp) ops->imm.dual.val2 );
			nextOp;
		OP_HANDLER ( incGlobalv )
			this->stack = stack;
			op_incv ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], (fgxOvOp) ops->imm.dual.val2 );
			nextOp;
		OP_HANDLER ( incRef )
			varTmp = stack - 1;
			if ( TYPE ( varTmp ) != slangType::eREFERENCE )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			this->stack = stack;
			op_incv ( this, ops, varTmp, (fgxOvOp) ops->imm.dual.val2 );
			nextOp;
		OP_HANDLER ( incClassIVar )
			// TODO: needs to be an op_ function to handle overloaded operations
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp = &varTmp[ops->imm.objOp.objectIndex];
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			varTmp->dat.l++;
			nextOp;
			throw errorNum::scINTERNAL;
		OP_HANDLER ( incClassVirtIVar )
			// TODO: needs to be an op_ function to handle overloaded operations
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			varTmp->dat.l++;
			nextOp;

		OP_HANDLER ( decLocali )
			basePtr[ops->imm.index].dat.l--;
			nextOp;
		OP_HANDLER ( decLocald )
			basePtr[ops->imm.index].dat.d--;
			nextOp;
		OP_HANDLER ( decLocalv )
			this->stack = stack;
			op_decv ( this, ops, &basePtr[ops->imm.dual.val1], (fgxOvOp) ops->imm.dual.val2 );
			nextOp;
		OP_HANDLER ( decGlobalv )
			this->stack = stack;
			op_decv ( this, ops, (funcDef->loadImage->globals)[ops->imm.dual.val1], (fgxOvOp) ops->imm.dual.val2 );
			nextOp;
		OP_HANDLER ( decRef )
			varTmp = stack - 1;
			if ( TYPE ( varTmp ) != slangType::eREFERENCE )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			this->stack = stack;
			op_decv ( this, ops, varTmp, (fgxOvOp) ops->imm.dual.val2 );
			nextOp;
		OP_HANDLER ( decClassIVar )
			throw errorNum::scINTERNAL;	// needs to handle types
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp = &varTmp[ops->imm.objOp.objectIndex];
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			varTmp->dat.l--;
			nextOp;
		OP_HANDLER ( decClassVirtIVar )
			throw errorNum::scINTERNAL;	// needs to handle types
			varTmp = basePtr[ops->imm.objOp.stackIndex].dat.ref.v;
			if ( varTmp->type != slangType::eOBJECT ) throw errorNum::scSEND_TO_NONOBJECT;
			varTmp = &varTmp[varTmp->dat.obj.vPtr[ops->imm.objOp.objectIndex].delta];
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			varTmp->dat.l--;
			nextOp;


		OP_HANDLER ( move )
			basePtr[ops->imm.index] = *(--stack);
			nextOp;
		OP_HANDLER ( pop )
			stack -= ops->imm.index;
			nextOp;
		OP_HANDLER ( dupv )
			*(stack) = *(stack - ops->imm.index);
			stack++;
			nextOp;
		OP_HANDLER ( deref )
			while ( (stack - ops->imm.index)->type == slangType::eREFERENCE )
			{
				*(stack - ops->imm.index) = *((stack - ops->imm.index)->dat.ref.v);
			}
			nextOp;
		OP_HANDLER ( popadup )
			varTmp = stack - 1;
			stack -= ops->imm.index;
			*(stack - 1) = *varTmp;
			nextOp;

		// array manipulation
		OP_HANDLER ( arrFixedDeref )
			stack--;
			if ( TYPE ( stack ) != slangType::eLONG )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = stack - 1;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_ROOT )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = varTmp->dat.ref.v;
			if ( (stack->dat.l < varTmp->dat.arrayFixed.startIndex) || (stack->dat.l > varTmp->dat.arrayFixed.endIndex) )
			{
				throw errorNum::scFIXED_ARRAY_OUT_OF_BOUNDS;
			} else
			{
				*(stack - 1) = varTmp[stack->dat.l - varTmp->dat.arrayFixed.startIndex + 1];
			}
			nextOp;

		OP_HANDLER ( arrFixedDerefRef )
			if ( TYPE ( stack - 1 ) != slangType::eLONG )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = stack - 2;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_ROOT )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = varTmp->dat.ref.v;
			if ( ((stack - 1)->dat.l < varTmp->dat.arrayFixed.startIndex) || ((stack - 1)->dat.l > varTmp->dat.arrayFixed.endIndex) )
			{
				throw errorNum::scFIXED_ARRAY_OUT_OF_BOUNDS;
			} else
			{
				*(stack - 2) = std::move ( VAR_REF ( varTmp, &varTmp[(stack - 1)->dat.l - varTmp->dat.arrayFixed.startIndex + 1] ) );
			}
			stack--;
			nextOp;
		OP_HANDLER ( arrDeref )
			this->stack = stack;
			op_arrDeref ( this, ops );
			stack--;
			nextOp;
		OP_HANDLER ( arrDerefRef )
			this->stack = stack;
			op_arrDerefRef ( this, ops );
			stack--;
			nextOp;
		OP_HANDLER ( storeArray )
			this->stack = stack;
			op_arrStore ( this, ops );
			stack -= 2;
			nextOp;
		OP_HANDLER ( storeArrayPop )
			this->stack = stack;
			op_arrStore ( this, ops );
			stack -= 3;
			nextOp;

		OP_HANDLER ( storeMulti )
			// these "prep" for the store... they ensure that we have exactly the right number of values on the stack for "symbol pack" store
			varTmp = stack;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_ROOT )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_FIXED )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			if ( varTmp->dat.arrayFixed.startIndex != 1 )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			if ( (int32_t) ops->imm.index > varTmp->dat.arrayFixed.endIndex )
			{
				// we want more than we have
				memcpy ( stack, &varTmp[1], sizeof ( VAR ) * varTmp->dat.arrayFixed.endIndex );
				stack += varTmp->dat.arrayFixed.endIndex;
				for ( auto index = varTmp->dat.arrayFixed.endIndex; index < ops->imm.index; index++ )
				{
					(stack++)->type = slangType::eNULL;
				}
			} else
			{
				// we want less or equal to what we have
				memcpy ( stack, &varTmp[1], sizeof ( VAR ) * ops->imm.index );
				stack += ops->imm.index;
			}
			nextOp;

		OP_HANDLER ( storeMultiPop )
			// these "prep" for the store... they ensure that we have exactly the right number of values on the stack for "symbol pack" store
			varTmp = --stack;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_ROOT )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_FIXED )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			if ( varTmp->dat.arrayFixed.startIndex != 1 )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			if ( (int32_t) ops->imm.index > varTmp->dat.arrayFixed.endIndex )
			{
				// we want more than we have
				memcpy ( stack, &varTmp[1], sizeof ( VAR ) * varTmp->dat.arrayFixed.endIndex );
				stack += varTmp->dat.arrayFixed.endIndex;
				for ( auto index = varTmp->dat.arrayFixed.endIndex; index < ops->imm.index; index++ )
				{
					(stack++)->type = slangType::eNULL;
				}
			} else
			{
				// we want less or equal to what we have
				memcpy ( stack, &varTmp[1], sizeof ( VAR ) * ops->imm.index );
				stack += ops->imm.index;
			}
			nextOp;

		// object manipulation
		OP_HANDLER ( objStore )
			this->stack = stack;
			op_objStore ( this, ops, funcDef, basePtr, funcDef->loadImage->dsBase );
			stack--;
			nextOp;
		OP_HANDLER ( objStorePop )
			this->stack = stack;
			op_objStore ( this, ops, funcDef, basePtr, funcDef->loadImage->dsBase );
			stack -= 2;
			nextOp;

		OP_HANDLER ( objStoreInd )
			this->stack = stack;
			op_objStoreInd ( this, ops, basePtr );
			stack -= 2;
			nextOp;
		OP_HANDLER ( objStoreIndPop )
			this->stack = stack;
			op_objStoreInd ( this, ops, basePtr );
			stack -= 3;
			nextOp;

		// object manipulation
		OP_HANDLER ( objAccess )
			this->stack = stack;
			op_accessObj ( this, ops, funcDef, basePtr, funcDef->loadImage->dsBase );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( objAccessInd )
			this->stack = stack;
			op_accessObjInd ( this, ops, basePtr );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( objAccessIndSafe )
			this->stack = stack;
			op_accessObjIndSafe ( this, ops, basePtr );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( objAccessRef )
			this->stack = stack;
			op_accessObjRef ( this, ops, funcDef, basePtr, funcDef->loadImage->dsBase );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( objAccessRefInd )
			this->stack = stack;
			op_accessObjRefInd ( this, ops, basePtr );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( objCall )
			this->stack = stack;
			ops = op_objCall ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			goOp;

		OP_HANDLER ( objCallPop )
			this->stack = stack;
			ops = op_objCall ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			goOp;

		OP_HANDLER ( objCallPPack )
			this->stack = stack;
			ops = op_objCallPPack ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			goOp;

		OP_HANDLER ( objCallPPackPop )
			this->stack = stack;
			ops = op_objCallPPack ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			goOp;

		OP_HANDLER ( objCallFuncOv )
			this->stack = stack;
			op_objCallFuncOv ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			*(stack++) = result;
			nextOp;

		OP_HANDLER ( objCallFuncOvPop )
			this->stack = stack;
			op_objCallFuncOv ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			nextOp;

		OP_HANDLER ( pushUndefinedObject )
			stack->type = slangType::eOBJECT_ROOT;
			stack->dat.ref.v = nullptr;
			stack->dat.ref.obj = nullptr;
			stack++;
			nextOp;
		OP_HANDLER ( objBuild )
			this->stack = stack;
			op_objBuild ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			nextOp;
		OP_HANDLER ( objConstruct )
			this->stack = stack;
			op_objNew ( this, ops, funcDef, nParams, basePtr );
			stack = this->stack;
			nextOp;
		OP_HANDLER ( objConstructV )
			this->stack = stack;
			ops = op_objNewV ( this, ops, funcDef, nParams, basePtr, false );
			stack = this->stack;
			goOp;

		OP_HANDLER ( objRelease )
			this->stack = stack;
			op_objRelease ( this, ops, basePtr );
			--stack;
			nextOp;
		// workarea management
		OP_HANDLER ( waStarts )
			workareaTable->push ( (char *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t )) );
			nextOp;
		OP_HANDLER ( waStarti )
			workareaTable->push ( (int32_t) ops->imm.intValue );
			nextOp;
		OP_HANDLER ( waStartInd )
			workareaTable->push ( --stack );
			nextOp;
		OP_HANDLER ( waStartIndi )
			workareaTable->push ( (--stack)->dat.l );
			nextOp;
		OP_HANDLER ( waStartInds )
			workareaTable->push ( (--stack)->dat.str.c );
			nextOp;
		OP_HANDLER ( waEnd )
			workareaTable->pop ();
			nextOp;

		// workarea field access
		OP_HANDLER ( waPushField )
			workareaTable->read ( (char *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t )), stack++ );
			nextOp;
		OP_HANDLER ( waPushFieldInd )
			workareaTable->read ( stack - 1, stack - 1 );
			nextOp;
		OP_HANDLER ( waStoreField )
			workareaTable->write ( (char *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t )), stack - 1 );
			nextOp;
		OP_HANDLER ( waStoreFieldPop )
			workareaTable->write ( (char *) ((funcDef->loadImage->dsBase) + ops->imm.index + sizeof ( uint32_t )), --stack );
			nextOp;
		OP_HANDLER ( waStoreFieldInd )
			workareaTable->write ( stack - 1, stack - 2 );
			stack--;
			nextOp;
		OP_HANDLER ( waStoreFieldIndPop )
			workareaTable->write ( stack - 1, stack - 2 );
			stack -= 2;
			nextOp;

		OP_HANDLER ( promote )
			switch ( basePtr[ops->imm.index].type )
			{
				case slangType::eREFERENCE:
					if ( !om->isInGCMemory ( basePtr[ops->imm.index].dat.ref.v ) )
					{
						// duplicate the referred to element in om memory
						this->stack = stack;
						varTmp = om->allocVar ( 1 );
						*varTmp = *basePtr[ops->imm.index].dat.ref.v;

						// point original element into om memory
						basePtr[ops->imm.index] = std::move ( VAR_REF ( varTmp, varTmp ) );

						// point our variable to the element in om memory
						basePtr[ops->imm.index].dat.ref.v = varTmp;
					}
					break;
				default:
					// it's not already a reference, so move it into object memory and make our symbol a pointer to the moved object
					this->stack = stack;
					varTmp = om->allocVar ( 1 );
					*varTmp = basePtr[ops->imm.index];

					basePtr[ops->imm.index] = std::move ( VAR_REF ( varTmp, varTmp ) );
					break;
			}
			*(stack++) = basePtr[ops->imm.index];
			nextOp;

		OP_HANDLER ( pushParamPack )
			varTmp = --stack;
			while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_ROOT )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			varTmp = varTmp->dat.ref.v;
			if ( TYPE ( varTmp ) != slangType::eARRAY_FIXED )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			if ( varTmp->dat.arrayFixed.startIndex != 1 )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			{
				uint64_t count = varTmp->dat.arrayFixed.endIndex;
				while ( count )
				{
					*(stack++) = varTmp[count];
					count--;
				}
			}
			nextOp;

		OP_HANDLER ( debugFuncStart )
			if ( debug->stepInto || debug->halt )
			{
				this->stack = stack;
				auto debugOp = debugSignal ( this, vmStateType::vmTrace, ops, funcDef, stack );
				if ( debugOp != ops )
				{
					if ( debug->stackHeight != stack - eval )
					{
						if ( &eval[debug->stackHeight] > stack )
						{
							memset ( stack, 0, sizeof ( VAR ) * (&eval[debug->stackHeight] - stack) );
						}
						stack = basePtr + debug->stackHeight;
					}
					ops = debugOp;
					goOp;
				}
			}
			nextOp;
		OP_HANDLER ( debugCheckHalt )
			if ( debug->halt )
			{
				this->stack = stack;
				auto debugOp = debugSignal ( this, vmStateType::vmDebugBreak, ops, funcDef, stack );
				if ( debugOp != ops )
				{
					if ( debug->stackHeight != stack - eval )
					{
						if ( &eval[debug->stackHeight] > stack )
						{
							memset ( stack, 0, sizeof ( VAR ) * (&eval[debug->stackHeight] - stack) );
						}
						stack = basePtr + debug->stackHeight;
					}
					ops = debugOp;
					goOp;
				}
			}
			nextOp;
		OP_HANDLER ( debugBreak )
			this->stack = stack;
			{
				auto debugOp = debugSignal ( this, vmStateType::vmDebugBreak, ops, funcDef, stack );
				if ( debugOp != ops )
				{
					if ( debug->stackHeight != stack - eval )
					{
						if ( &eval[debug->stackHeight] > stack )
						{
							memset ( stack, 0, sizeof ( VAR ) * (&eval[debug->stackHeight] - stack) );
						}
						stack = basePtr + debug->stackHeight;
					}
					ops = debugOp;
					goOp;
				}
			}
			nextOp;
		OP_HANDLER ( debugTrace )
			this->stack = stack;
			{
				auto debugOp = debugSignal ( this, vmStateType::vmTrace, ops, funcDef, stack );
				if ( debugOp != ops )
				{
					if ( debug->stackHeight != stack - eval )
					{
						if ( &eval[debug->stackHeight] > stack )
						{
							memset ( stack, 0, sizeof ( VAR ) * (&eval[debug->stackHeight] - stack) );
						}
						stack = basePtr + debug->stackHeight;
					}
					ops = debugOp;
					goOp;
				}
			}
			// this is a CONTINUE... we WANT to reexecute the same opcode as it will have been replaced by the trap and the debugger will have restored it to the correct opcode to execute!
			goOp;
		OP_HANDLER ( debugTrap )
			this->stack = stack;
			{
				auto debugOp = debugSignal ( this, vmStateType::vmTrap, ops, funcDef, stack );
				if ( debugOp != ops )
				{
					if ( debug->stackHeight != stack - eval )
					{
						if ( &eval[debug->stackHeight] > stack )
						{
							memset ( stack, 0, sizeof ( VAR ) * (&eval[debug->stackHeight] - stack) );
						}
						stack = basePtr + debug->stackHeight;
					}
					ops = debugOp;
					goOp;
				}
			}
			nextOp;
		OP_HANDLER ( profFuncStart )
			funcDef->execNCalls++;
			debug->nCalls++;
			profTimeStart = getTicks ();
			profTimeCalls = 0;
			nextOp;
		OP_HANDLER ( profFuncEnd )
			funcDef->execTimeTotal += getTicks () - profTimeStart;
			funcDef->execTimeLocal += getTicks () - profTimeStart - profTimeCalls;
			nextOp;
		OP_HANDLER ( profCallStart )
			profTimeCallStart = getTicks ();
			nCallsSave = debug->nCalls;
			nextOp;
		OP_HANDLER ( profCallEnd )
			if ( nCallsSave != debug->nCalls )
			{
				profTimeCalls = getTicks () - profTimeCallStart;
			}
			nextOp;
		OP_HANDLER ( profReset )
			debug->SetProfiling ( this );
			nextOp;
		OP_HANDLER ( prefix )
		OP_HANDLER ( noop )
			nextOp;

#if defined ( NDEBUG )
		OP_HANDLER ( chkstk )
			nextOp;
		// don't do this for debug builds... this lets the compiler tell us if we have any missing opcodes that we don't have a handler for
		// do it for release builds for safety so we can terminate if the code is corrupted in some way
#if !DO_THREADED
		default:
			__assume ( false );
#endif
#else
		OP_HANDLER ( chkstk )
			if ( basePtr + ops->imm.dual.val1 != stack )
			{
				DebugBreak ();
			}
			nextOp;
			// do it for release builds for safety so we can terminate if the code is corrupted in some way
#if !DO_THREADED
		default:
			__assume ( false );
			throw errorNum::scINTERNAL;
			break;
#endif
#endif
		

#if !DO_THREADED
	}
#endif
}