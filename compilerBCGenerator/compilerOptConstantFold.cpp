
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "compilerOptimizer.h"

// this is an AST based expression evaluator.   It's a bit twitchy and has some limitations because it works on the AST.
// it would be better if this actually TRUELY evaluated the expressions rather than the semi-crappy method we have going on now.
// im heartened as LLVM even did AST evaluation apparently for a long portion of its existance and only switched to an interpreter
// later in it's life
//
// TODO: write an interpreter for the AST rather than the method we have now

// get VALUE returns constants
static astNode *getValue ( astNode *node, symbolStack *sym )
{
	if ( !node ) return node;
	node->left = getValue ( node->left, sym );
	node->right = getValue ( node->right, sym );

	switch ( node->getOp() )
	{
		case astOp::symbolValue:
			switch (sym->getScope (node->symbolValue(), true))
			{
				case symbolSpaceScope::symLocalConst:
				case symbolSpaceScope::symClassConst:
					{
						auto newNode = sym->getInitializer ( node->symbolValue() );

						if ( newNode->getOp () == astOp::assign )
						{
							newNode = newNode->right;
						}
						if ( newNode->isValueType() )
						{
							delete node;
							return getValue ( new astNode ( *newNode ), sym );
						}
					}
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
	return node;
}

static astNode *precompute ( astNode *node, symbolStack *sym, astNode *parent )
{
	astNode	*tmpNode;

	errorLocality e ( &sym->file->errHandler, node->location );

	if ( node->getOp() == astOp::dummy )
	{
		switch ( parent->getOp () )
		{
			case astOp::funcCall:
			case astOp::sendMsg:
			case astOp::workAreaStart:
				return node;
			default:
				break;
		}
	}

	if ( node->left )
	{
		node->left = precompute ( node->left, sym, node );

		// left only handlers
		switch ( node->getOp ( ) )
		{
			case astOp::dummy:
				while ( node->getOp ( ) == astOp::dummy )
				{
					if ( node->left )
					{
						tmpNode = node;
						switch ( node->left->getOp ( ) )
						{
							case astOp::funcCall:
							case astOp::sendMsg:
							case astOp::workAreaStart:
								tmpNode = nullptr;
								break;
							default:
								break;
						}
						if ( tmpNode )
						{
							node = node->left;
							tmpNode->left = nullptr;
							delete tmpNode;
						} else
						{
							break;
						}
					} else
					{
						break;
					}
				}
				break;
			default:
				break;
		}
		if ( node->right )
		{
			node->right = precompute ( node->right, sym, node );
			switch ( node->getOp() )
			{
				case astOp::dummy:
					while ( node->getOp() == astOp::dummy )
					{
						if ( node->left )
						{
							tmpNode = node;
							switch ( node->left->getOp ( ) )
							{
								case astOp::funcCall:
								case astOp::sendMsg:
								case astOp::workAreaStart:
									tmpNode = nullptr;
									break;
								default:
									break;
							}
							if ( tmpNode )
							{
								node = node->left;
								tmpNode->left = nullptr;
								delete tmpNode;
							} else
							{
								break;
							}
						} else
						{
							break;
						}
					}
					break;
				case astOp::add:
					switch ( node->left->getOp() )
					{
						case astOp::add:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->intValue() = node->left->right->intValue() + node->right->intValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = node->left->right->doubleValue() + (double) node->right->intValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								case astOp::doubleValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = (double) node->left->right->intValue() + node->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = node->left->right->doubleValue() + node->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								default:
									break;
							}
							break;
						case astOp::subtract:			// 1-2+3
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->intValue() = -node->left->right->intValue() + node->right->intValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = -node->left->right->doubleValue() + (double) node->left->right->intValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								case astOp::doubleValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = -(double) node->left->right->intValue() + (double)node->right->intValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = -node->left->right->doubleValue() + node->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								default:
									break;
							}
							break;
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() + node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = (double) node->left->intValue() + node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() + _atoi64 ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() + (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() + node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() + atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							switch ( node->right->getOp() )
							{
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::stringValue );
									node->stringValue() += node->left->stringValue() + node->right->stringValue();
									break;
								case astOp::intValue:
									{
										char tmp[65];
										_i64toa_s( node->right->intValue(), tmp, sizeof ( tmp ), 10 );
										node->setOp( sym->file->sCache, astOp::stringValue );
										node->stringValue() += node->left->stringValue() + tmp;
									}
									break;
								case astOp::doubleValue:
									{
										char tmp[_CVTBUFSIZE];
										_gcvt_s( tmp, node->right->doubleValue(), 16 );
										node->setOp( sym->file->sCache, astOp::stringValue );
										node->stringValue() += node->left->stringValue() + tmp;
									}
									break;
								default:
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::subtract:
					switch ( node->left->getOp() )
					{
						case astOp::add:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->intValue() = node->left->right->intValue() - node->right->intValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = node->left->right->doubleValue() - (double) node->right->intValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								case astOp::doubleValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = (double) node->left->right->intValue() - node->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = node->left->right->doubleValue() - node->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								default:
									break;
							}
							break;
						case astOp::subtract:			// 1-2+3
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->intValue() = -node->left->right->intValue() - node->right->intValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = -node->left->right->doubleValue() - (double) node->left->right->intValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								case astOp::doubleValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = -(double) node->left->right->intValue() - (double)node->right->intValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->setOp ( sym->file->sCache, astOp::add );
											node->right->doubleValue() = -node->left->right->doubleValue() - node->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								default:
									break;
							}
							break;
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() - node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = (double) node->left->intValue() - node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = (double)node->left->intValue() - atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = (double)node->left->doubleValue() - (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() - node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() - atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::multiply:
					switch ( node->left->getOp() )
					{
						case astOp::multiply:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->right->intValue() *= node->left->right->intValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->right->doubleValue() = (double) node->left->right->intValue() * node->left->right->doubleValue();
											node->right->setOp ( sym->file->sCache, astOp::doubleValue );
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								case astOp::doubleValue:
									switch ( node->left->right->getOp() )
									{
										case astOp::intValue:
											node->right->doubleValue() *= (double) node->left->right->intValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::doubleValue:
											node->right->doubleValue() *= node->left->right->doubleValue();
											tmpNode = node->left;
											node->left = node->left->left;
											tmpNode->left = 0;
											delete tmpNode;
											break;
										case astOp::stringValue:
										default:
											break;
									}
									break;
								default:
									break;
							}
							break;
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() * node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = (double) node->left->intValue() * node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = (double)node->left->intValue() * atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() * (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() * node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = node->left->doubleValue() * atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::divide:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									if ( !node->right->intValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else if ( node->right->intValue() == 1 )
									{
										node->setOp ( sym->file->sCache, astOp::intValue );
										node->intValue() = node->left->intValue();
									} else
									{
										node->setOp ( sym->file->sCache, astOp::doubleValue );
										node->doubleValue() = (double) node->left->intValue() / (double) node->right->intValue();
									}
									break;
								case astOp::doubleValue:
									if ( !(bool)node->right->doubleValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else if ( node->right->doubleValue() == 1 )
									{
										node->setOp ( sym->file->sCache, astOp::intValue );
										node->intValue() = node->left->intValue();
									} else
									{
										node->setOp ( sym->file->sCache, astOp::doubleValue );
										node->doubleValue() = (double) node->left->intValue() / node->right->doubleValue();
									}
									break;
								case astOp::stringValue:
									if ( !(bool) atof ( node->right->stringValue() ) )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else if ( atof ( node->right->stringValue() ) == 1 )
									{
										node->setOp ( sym->file->sCache, astOp::intValue );
										node->intValue() = node->left->intValue();
									} else
									{
										node->setOp ( sym->file->sCache, astOp::doubleValue );
										node->doubleValue() = (double) node->left->intValue() / atof ( node->right->stringValue() );
									}
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									if ( !node->right->intValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else
									{
										node->setOp ( sym->file->sCache, astOp::doubleValue );
										node->doubleValue() = node->left->doubleValue() / (double) node->right->intValue();
									}
									break;
								case astOp::doubleValue:
									if ( !(bool) node->right->doubleValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else
									{
										node->setOp ( sym->file->sCache, astOp::doubleValue );
										node->doubleValue() = node->left->doubleValue() / node->right->doubleValue();
									}
									break;
								case astOp::stringValue:
									if ( !(bool) atof ( node->right->stringValue() ) )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else
									{
										node->setOp ( sym->file->sCache, astOp::doubleValue );
										node->doubleValue() = node->left->doubleValue() / atof ( node->right->stringValue() );
									}
									break;
								default:
									break;
							}
							break;
						default:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									if ( !node->right->intValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									}
									break;
								case astOp::doubleValue:
									if ( !(bool) node->right->doubleValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									}
									break;
								case astOp::stringValue:
									if ( !(bool) atof ( node->right->stringValue() ) )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									}
									break;
								default:
									break;
							}
							break;
					}
					break;
				case astOp::modulus:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									if ( !node->right->intValue() )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else
									{
										node->setOp ( sym->file->sCache, astOp::intValue );
										node->intValue() = node->left->intValue() % node->right->intValue();
									}
									break;
								case astOp::doubleValue:
									throw errorNum::scILLEGAL_OPERAND;
								case astOp::stringValue:
									if ( !_atoi64 ( node->right->stringValue() ) )
									{
										throw errorNum::scDIVIDE_BY_ZERO;
									} else
									{
										node->setOp ( sym->file->sCache, astOp::intValue );
										node->intValue() = node->left->intValue() % _atoi64 ( node->right->stringValue() );
									}
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							throw errorNum::scILLEGAL_OPERAND;
						default:
							break;
					}
					break;
				case astOp::power:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = pow ( (double) node->left->intValue(), (double) node->right->intValue() );
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = pow ( (double) node->left->intValue(), node->right->doubleValue() );
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = pow ( (double) node->left->intValue(), atof ( node->right->stringValue() ) );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = pow ( node->left->doubleValue(), (double) node->right->intValue() );
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = pow ( node->left->doubleValue(), node->right->doubleValue() );
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::doubleValue );
									node->doubleValue() = pow ( node->left->doubleValue(), atof ( node->right->stringValue() ) );
									break;
								default:
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::bitAnd:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() & node->right->intValue();
									break;
								case astOp::doubleValue:
								case astOp::stringValue:
								case astOp::nullValue:
								case astOp::varrayValue:
								case astOp::arrayValue:
								case astOp::symbolPack:
								case astOp::nilValue:
									throw errorNum::scILLEGAL_OPERATION;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
						case astOp::stringValue:
						case astOp::nullValue:
						case astOp::varrayValue:
						case astOp::arrayValue:
						case astOp::symbolPack:
						case astOp::nilValue:
							throw errorNum::scILLEGAL_OPERATION;
						default:
							break;
					}
					break;
				case astOp::bitOr:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() | node->right->intValue();
									break;
								case astOp::doubleValue:
								case astOp::stringValue:
								case astOp::nullValue:
								case astOp::varrayValue:
								case astOp::arrayValue:
								case astOp::symbolPack:
								case astOp::nilValue:
									throw errorNum::scILLEGAL_OPERATION;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
						case astOp::stringValue:
						case astOp::nullValue:
						case astOp::varrayValue:
						case astOp::arrayValue:
						case astOp::symbolPack:
						case astOp::nilValue:
							throw errorNum::scILLEGAL_OPERATION;
						default:
							break;
					}
					break;
				case astOp::bitXor:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() ^ node->right->intValue();
									break;
								case astOp::doubleValue:
								case astOp::stringValue:
								case astOp::nullValue:
								case astOp::varrayValue:
								case astOp::arrayValue:
								case astOp::symbolPack:
								case astOp::nilValue:
									throw errorNum::scILLEGAL_OPERATION;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
						case astOp::stringValue:
						case astOp::nullValue:
						case astOp::varrayValue:
						case astOp::arrayValue:
						case astOp::symbolPack:
						case astOp::nilValue:
							throw errorNum::scILLEGAL_OPERATION;
						default:
							break;
					}
					break;
				case astOp::shiftLeft:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->intValue() = node->left->intValue() << node->right->intValue();
									node->setOp ( sym->file->sCache, astOp::intValue );
									break;
								case astOp::doubleValue:
								case astOp::stringValue:
								case astOp::nullValue:
								case astOp::varrayValue:
								case astOp::arrayValue:
								case astOp::symbolPack:
								case astOp::nilValue:
									throw errorNum::scILLEGAL_OPERATION;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
						case astOp::stringValue:
						case astOp::nullValue:
						case astOp::varrayValue:
						case astOp::arrayValue:
						case astOp::symbolPack:
						case astOp::nilValue:
							throw errorNum::scILLEGAL_OPERATION;
						default:
							break;
					}
					break;
				case astOp::shiftRight:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() >> node->right->intValue();
									break;
								case astOp::doubleValue:
								case astOp::stringValue:
								case astOp::nullValue:
								case astOp::varrayValue:
								case astOp::arrayValue:
								case astOp::symbolPack:
								case astOp::nilValue:
									throw errorNum::scILLEGAL_OPERATION;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
						case astOp::stringValue:
						case astOp::nullValue:
						case astOp::varrayValue:
						case astOp::arrayValue:
						case astOp::symbolPack:
						case astOp::nilValue:
							throw errorNum::scILLEGAL_OPERATION;
						default:
							break;
					}
					break;
				case astOp::equal:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() == node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double)node->left->intValue() == node->right->doubleValue();
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() == (double)node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() == node->right->doubleValue();
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							switch ( node->right->getOp() )
							{
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() =  node->left->stringValue() ==  node->right->stringValue()  ? 1 : 0;
									break;
								default:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 0;
									break;
							}
							break;
						case astOp::nullValue:
							switch ( node->right->getOp() )
							{
								case astOp::nullValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1;
									break;
								default:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 0;
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::notEqual:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() != node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double)node->left->intValue() == node->right->doubleValue();
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() != (double)node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() != node->right->doubleValue();
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							switch ( node->right->getOp() )
							{
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() =  node->left->stringValue() ==  node->right->stringValue()  ? 0 : 1;
									break;
								default:
									break;
							}
							break;
						case astOp::nullValue:
							switch ( node->right->getOp() )
							{
								case astOp::nullValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 0;
									break;
								default:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1;
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::less:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() < node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double)node->left->intValue() < node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() < atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() < (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() < node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() < atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							throw errorNum::scILLEGAL_OPERATION;
							break;
						case astOp::nullValue:
							switch ( node->right->getOp() )
							{
								case astOp::nullValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 0;
									break;
								default:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1;
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::lessEqual:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() <= node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() <= node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() <= atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() <= (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() <= node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() <= atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							throw errorNum::scILLEGAL_OPERATION;
							break;
						case astOp::nullValue:
							node->setOp ( sym->file->sCache, astOp::intValue );
							node->intValue() = 1;
							break;
						default:
							break;
					}
					break;
				case astOp::greater:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() > node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() > node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() > atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() > (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() > node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() > atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							throw errorNum::scILLEGAL_OPERATION;
							break;
						case astOp::nullValue:
							node->setOp ( sym->file->sCache, astOp::intValue );
							node->intValue() = 0;
							break;
						default:
							break;
					}
					break;
				case astOp::greaterEqual:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() >= node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() >= node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (double) node->left->intValue() >= atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() >= (double) node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() >= node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->doubleValue() >= atof ( node->right->stringValue() );
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							throw errorNum::scILLEGAL_OPERATION;
							break;
						case astOp::nullValue:
							switch ( node->right->getOp() )
							{
								case astOp::nullValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1;
									break;
								default:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 0;
									break;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::logicalAnd:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() && node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() && (bool)node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() && *node->right->stringValue().c_str();
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (bool)node->left->doubleValue() && node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (bool) node->left->doubleValue() && (bool) node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (bool) node->left->doubleValue() && *node->right->stringValue().c_str();
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = *node->left->stringValue().c_str() && node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = *node->left->stringValue().c_str() && (bool) node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = *node->left->stringValue().c_str ( ) && *node->right->stringValue().c_str ( );
									break;
								default:
									break;
							}
							break;
						case astOp::nullValue:
							node->setOp ( sym->file->sCache, astOp::intValue );
							node->intValue() = 0;
							break;
						default:
							break;
					}
					break;
				case astOp::logicalOr:
					switch ( node->left->getOp() )
					{
						case astOp::intValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() || node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() || (bool) node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = node->left->intValue() || *node->right->stringValue().c_str();
									break;
								default:
									break;
							}
							break;
						case astOp::doubleValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (bool) node->left->doubleValue() || node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (bool)node->left->doubleValue() || (bool) node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = (bool)node->left->doubleValue() || *node->right->stringValue().c_str ( );
									break;
								default:
									break;
							}
							break;
						case astOp::stringValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = *node->left->stringValue().c_str() || node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() =*node->left->stringValue().c_str() || (bool) node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = *node->left->stringValue().c_str() || *node->right->stringValue().c_str ( );
									break;
								default:
									break;
							}
							break;
						case astOp::nullValue:
							switch ( node->right->getOp() )
							{
								case astOp::intValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1 || node->right->intValue();
									break;
								case astOp::doubleValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1 || (bool) node->right->doubleValue();
									break;
								case astOp::stringValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 1 || *node->right->stringValue().c_str ( );
									break;
								case astOp::nullValue:
									node->setOp ( sym->file->sCache, astOp::intValue );
									node->intValue() = 0;
									break;
								default:
									break;
							}
							break;
						default:
							break;

					}
					break;
				default:
					break;
			}
		}
	} else
	{
		switch ( node->getOp() )
		{
			case astOp::conditional:
				node->conditionData().trueCond = precompute ( node->conditionData().trueCond, sym, node );
				node->conditionData().falseCond = precompute ( node->conditionData().falseCond, sym, node );
				break;
			case astOp::symbolValue:
				node = getValue ( node, sym );
				break;
			default:
				break;
		}

	}
	if ( node )
	{
		switch ( node->getOp () )
		{
			case astOp::stringValue:
			case astOp::intValue:
			case astOp::doubleValue:
				if ( node->left ) delete node->left;
				if ( node->right ) delete node->right;
				node->left = 0;
				node->right = 0;
				break;
			default:
				break;
		}
	}
	return node;
}

static astNode *getValueCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool )
{
	switch ( node->getOp ( ) )
	{
		case astOp::symbolValue:
			if ( isAccess )
			{
				switch ( sym->getScope ( node->symbolValue(), isAccess ) )
				{
					case symbolSpaceScope::symLocalConst:
					case symbolSpaceScope::symClassConst:
						{
							auto newNode = sym->getInitializer ( node->symbolValue() );

							if ( newNode->isValueType ( ) )
							{
								delete node;
								return getValue ( new astNode ( *newNode ), sym );
							}
						}
						break;
					default:
						break;
				}
			}
			break;
		default:
			break;
	}
	return node;
}

static astNode*constantFoldBlockCB (astNode* node, astNode* parent, symbolStack* sym, bool isAccess, bool )
{
	return precompute (node, sym, parent );
}

void optimizer::constantFoldFunction ( opFunction *func )
{
	if ( func->isInterface || !func->codeBlock ) return;
	
	symbolStack	sym ( file, func );
	errorLocality e ( &file->errHandler, func->location );

	func->codeBlock = astNodeWalk ( func->codeBlock, &sym, getValueCB );
	func->codeBlock = astNodeWalk (func->codeBlock, &sym, constantFoldBlockCB );
}


void optimizer::constantFold ()
{
	// compile the functions
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		try
		{
			errorLocality e ( &file->errHandler, (*it).second->location );
			constantFoldFunction ( (*it).second );
		} catch ( errorNum err )
		{
			file->errHandler.throwFatalError ( false, err );
		}
	}
}

void optimizer::constantFoldGlobals ()
{
	symbolStack	sym ( file );
	opUsingList	useList;

	sym.ns = symbolSpaceNamespace ( file, &file->ns, &useList );
	sym.push ( &sym.ns );

	auto tSave = transformMade;

	for ( auto it = file->symbols.begin (); it != file->symbols.end (); it++ )
	{
		do
		{
			transformMade = false;
			if ( it->second.loadTimeInitializable && it->second.initializer )
			{
				if ( it->second.initializer->getOp () == astOp::assign )
				{
					it->second.initializer->right = getValue ( it->second.initializer->right, &sym );
					precompute ( it->second.initializer->right, &sym, nullptr );
				} else
				{
					it->second.initializer = getValue ( it->second.initializer, &sym );
					precompute ( it->second.initializer, &sym, nullptr );
				}
			}
			tSave |= transformMade;
		} while ( transformMade );
	}
	transformMade = tSave;
}

