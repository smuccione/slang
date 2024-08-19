/*
bTreeClass

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"

#define MULTI_LINE_STRING(...) #__VA_ARGS__

void builtinQueryable ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CODE ( R"sl( 
						class queryable
						{
							extension iterator select( source, projection )
							{
								foreach( item in source )
								{
									yield projection( item );
								}
							}
							extension iterator selectMany( source, collectionSelector, resultSelector = ((x, y) => y) )
							{
								local index = 0;
								foreach( item in source )
								{
									foreach( collectionItem in collectionSelector( item, index++ ) )
									{
										yield resultSelector( item, collectionItem );
									}
								}
							}
							extension iterator where( source, predicate )
							{
								foreach( item in source )
								{
									if ( predicate( item ) )
									{
										yield item;
									}
								}
							}
							extension method toLookup( source, keySelector, elementSelector = (elem => elem), keyCompare = ((a, b) => a < b) )
							{
								local lkup = new lookup( keyCompare );

								foreach( item in source )
								{
									lkup.add( keySelector( item ), elementSelector( item ) );
								}
								return lkup;
							}
							extension iterator join( outer, inner, outerKeySelector, innerKeySelector, resultSelector, comparer = ((a, b) => a < b) )
							{
								local lkup = (from inner).toLookup( innerKeySelector, (x) => x, comparer );
								local results = outer.SelectMany( outerElement => lkup[outerKeySelector( outerElement )], resultSelector );
								foreach( result in results )
								{
									yield result;
								}
							}
							extension iterator groupJoin( outer, inner, outerKeySelector, innerKeySelector, resultSelector, comparer  = ((a, b) => a < b))
							{
								local lkup = (from inner).toLookup( innerKeySelector, (x) => x, comparer );
								foreach( outerElement in outer )
								{
									yield resultSelector( outerElement, lkup[outerKeySelector( outerElement )] );
								}
							}
							extension iterator groupBy( source, keySelector, elementSelector, resultSelector = ((key, group) => group), comparer = ((a, b) => a < b) )
							{
								foreach( item in source.toLookup( keySelector, elementSelector, comparer ) )
								{
									yield item;
								}
							}

							extension method orderBy( source, keyCompare )
							{
								return new orderable( source, keyCompare, 1 );
							}
							extension method orderByDescending( source, keyCompare )
							{
								return new orderable( source, keyCompare, -1 );
							}
							extension method sum( source )
							{
								local total = 0;
								foreach( item in source )
								{
									total += item
								}
								return total;
							}
							extension method average( source )
							{
								local total = 0;
								local count = 0;
								foreach( item in source )
								{
									count++;
									total += item
								}
								return total / count;
							}
							extension method all( source, condition )
							{
								foreach( item in source )
								{

									if ( !condition ( item ) )
									{
										return true;
									}
								}
								return false;
							}
							extension method any( source, condition )
							{
								foreach( item in source )
								{
									if ( condition ( item ) )
									{
										return true;
									}
								}
								return false;
							}
							extension iterator append( source, newItem )
							{
								foreach( item in source )
								{
									yield item;
								}
								return newItem;
							}
							extension iterator transform ( source, func )
							{
								foreach( item in source )
								{
									yield func(item);
								}
							}
							extension iterator max ( source )
							{
								local max = null
								foreach( item in source )
								{
									if ( max )
									{
										if ( item > max ) 
										{
											max = item;
										}
									} else
									{
										max = item;
									}
								}
								return max;
							}
							extension iterator min ( source )
							{
								local min = null
								foreach( item in source )
								{
									if ( min )
									{
										if ( item < min ) 
										{
											min = item;
										}
									} else
									{
										min = item;
									}
								}
								return min;
							}
							extension iterator last ( source )
							{
								local value
								foreach( item in source )
								{
									value = item;
								}
								return value;
							}
							extension iterator first ( source )
							{
								foreach( item in source )
								{
									return item;
								}
								return null;
							}
							extension iterator firstOrDefault ( source, def )
							{
								foreach( item in source )
								{
									return item;
								}
								return def;
							}
							extension iterator elementAt ( source, count)
							{
								foreach( item in source )
								{
									if ( !--count )
									{
										return item;
									}
								}
								return null;
							}
							extension iterator elementAtOrDefault ( source, count, def )
							{
								foreach( item in source )
								{
									if ( !--count )
									{
										return item;
									}
								}
								return def;
							}
						}

						class lookup
						{
							local map;
							local keys;

							iterator GetEnumerator()
							{
								foreach( key in keys )
								{
									yield new grouping( key, map[key].elements );
								}
							}
							method new (keyCompare)
							{
								map = new map( keyCompare );
								keys = {};
							}
							method add( key, elem )
							{
								if ( map.has( key ) )
								{
									local group = map.find( key ).current;
									group.elements[len( group.elements ) + 1] = elem;
								} else
								{
									local group = new grouping( key, {} );
									group.elements[len( group.elements ) + 1] = elem;
									map.insert( key, group );
									keys[len( keys ) + 1] = key;
								}
							}
							method contains( key )
							{
								if ( map.has( key ) )
								{
									return true;
								} else
								{
									return false;
								}
							}
							method count()
							{
								return map.size();
							}
							operator [ (key)
							{
								if ( map.has( key ) )
								{
									return from map[key];
								}
								return from {};
							}
						}

						class grouping
						{
							local key;
							local elements;
							method new (key, elements)
							{
								self.key = key;
								self.elements = elements;
							}
							method getEnumerator()
							{
								return from elements;
							}
						}

						class orderable
						{
							inherit queryable;

							local source;
							local compList = {};

							method new (source, expression, direction)
							{
								self.source = source;
								compList[len( compList ) + 1] = { expression, direction };
							}

							method getEnumerator()
							{
								local tmp = {};

								foreach( item in source )
								{
									tmp[len( tmp ) + 1] = item;
								}

								if ( len( compList ) == 1 )
								{
									if ( compList[1][2] == 1 )
									{
										tmp = stableSort( tmp, (a, b, cb ) => cb ( a ) - cb ( b ), compList[1][1] );
									} else
									{
										tmp = stableSort( tmp, (a, b, cb ) => cb ( b ) - cb ( a ), compList[1][1] );
									}
								} else
								{
									tmp = stableSort( tmp, function (a, b, cb )
															{
																integer ret;
																foreach( cb in compList )
																{
																	ret = (cb[1]( a ) - cb[1]( b )) * cb[2];
																	if ( ret )
																	{
																		return ret;
																	}
																}
																return 0;
															},
													  compList );
								}

								return (from tmp);
							}
							method ThenBy( expression )
							{
								compList[len( compList ) + 1] = { expression, 1 };
								return self;

							}
							method ThenByDescending( expression )
							{
								compList[len( compList ) + 1] = { expression, -1 };
								return self;
							}
						}
					)sl" );
	END;
}
