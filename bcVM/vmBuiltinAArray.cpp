/*
	aArray() class

	NOTE: until we can find an effective way to know a-prioi the total size of the memory allocated by a map and each node we resort to using the avl tree code

	aArrayNew is the problem... this is highly optimized for speed as we use this a LOT for all the dynamic arrays in the engine.

*/

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

#include <span>

/* An PAVL tree node. */
struct pavl_node
{
	struct	pavl_node	*pavl_link[2];		/* Subtrees.		*/
	struct	pavl_node	*pavl_parent;		/* Parent node.		*/
	VAR					*index;				/* index value		*/
	VAR					*data;				/* data value		*/
	signed	char		 pavl_balance;      /* Balance factor.	*/
};


/* Tree data structure. */
struct pavl_table
{
	pavl_node			 *pavl_root;			/* Tree's root.							*/
	pavl_node			 *pavl_current;			/* traverser for first/last/next/prev	*/
	size_t				  pavl_count;			/* Number of items in tree.				*/
	VAR					 *cbFunc;
	int64_t				(*varCompare)		( vmInstance *instance, pavl_table *table, VAR *var1, VAR *var2 );
};

/* PAVL traverser structure. */
struct pavl_traverser
{
	struct pavl_table	*pavl_table;        /* Tree being traversed. */
	struct pavl_node	*pavl_node;          /* Current node in tree. */
};

//****************** object comparison

static int64_t varCompareUser ( vmInstance *instance, pavl_table *table, VAR *var1, VAR *var2 )
{
	bcFuncDef	*func;
	VAR			*stackSave;

	stackSave = instance->stack;

	*(instance->stack++) = *var2;
	*(instance->stack++) = *var1;

	if ( table->cbFunc->type == slangType::eATOM )
	{
		func = instance->atomTable->atomGetFunc ( table->cbFunc->dat.atom );
	} else
	{
		// TODO: functor dispatch
		/*
		func = table->cbFunc->dat.ref.v->dat.closure.func;
		// copy closed over values to the stack
		memcpy ( instance->stack, table->cbFunc->dat.ref.v->dat.closure.detach, sizeof ( VAR ) * func->nClosures );
		instance->stack += func->nClosures;
		*/
		throw errorNum::scUNSUPPORTED;
	}


	switch ( func->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( func, 0, 3 );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( func->atom, 3 );
			break;
		default:
			throw errorNum::scINTERNAL;
	}

	instance->stack = stackSave;
	VAR *tmpVar = &instance->result;
	while ( TYPE ( tmpVar ) == slangType::eREFERENCE )
	{
		tmpVar = tmpVar->dat.ref.v;
	}
	if ( TYPE ( tmpVar ) != slangType::eLONG )
	{
		throw errorNum::scINTERNAL;
	}
	return tmpVar->dat.l;
}

static int64_t varCompare ( vmInstance *instance, pavl_table *table, VAR *var1, VAR *var2 )
{
	int64_t		r;
	double	dTmp;

	while ( TYPE ( var1 ) == slangType::eREFERENCE )
	{
		var1 = var1->dat.ref.v;
	}

	while ( TYPE ( var2 ) == slangType::eREFERENCE )
	{
		var2 = var2->dat.ref.v;
	}

	switch ( TYPE ( var1 ) )
	{
		case slangType::eLONG:
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
					r = var1->dat.l - var2->dat.l;
					r = r < 0 ? -1 : (r > 0 ? 1 : 0);
					break;
				case slangType::eDOUBLE:
					dTmp = (double)var1->dat.l - var2->dat.d;
					r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
					break;
				case slangType::eSTRING:
					r = 1;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
					dTmp = var1->dat.d - (double)var2->dat.l;
					r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
					break;
				case slangType::eDOUBLE:
					dTmp = var1->dat.d - var2->dat.d;
					r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
					break;
				case slangType::eSTRING:
					r = 1;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
					r = -1;
					break;
				case slangType::eSTRING:
					r = strcmp ( var1->dat.str.c, var2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return r;
}

static int64_t varCompareI ( vmInstance *instance, pavl_table *table, VAR *var1, VAR *var2 )
{
	int64_t		r;
	double	dTmp;

	while ( TYPE ( var1 ) == slangType::eREFERENCE )
	{
		var1 = var1->dat.ref.v;
	}

	while ( TYPE ( var2 ) == slangType::eREFERENCE )
	{
		var2 = var2->dat.ref.v;
	}

	switch ( TYPE ( var1 ) )
	{
		case slangType::eLONG:
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
					r = var1->dat.l - var2->dat.l;
					r = r < 0 ? -1 : (r > 0 ? 1 : 0);
					break;
				case slangType::eDOUBLE:
					dTmp = (double)var1->dat.l - var2->dat.d;
					r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
					break;
				case slangType::eSTRING:
					r = 1;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
					dTmp = var1->dat.d - (double)var2->dat.l;
					r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
					break;
				case slangType::eDOUBLE:
					dTmp = var1->dat.d - var2->dat.d;
					r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
					break;
				case slangType::eSTRING:
					r = 1;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
					r = -1;
					break;
				case slangType::eSTRING:
					r = strccmp ( var1->dat.str.c, var2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return r;
}

//****************** modified pavl code

/* Inserts |item| into |tree| and returns a pointer to |item|'s address.
If a duplicate item is found in the tree,
returns a pointer to the duplicate without inserting |item|.
Returns |NULL| in case of memory allocation failure. */
static pavl_node *pavl_probe ( vmInstance *instance, VAR *obj, VAR *index )
{
	pavl_node		*y;     /* Top node to update balance factor, and parent. */
	pavl_node		*p, *q; /* Iterator, and parent. */
	pavl_node		*n;     /* Newly inserted node. */
	pavl_node		*w;     /* New root of rebalanced subtree. */
	pavl_table		*tree;
	int64_t			 dir{};                 /* Direction to descend. */
	VAR				*iVar;
	VAR				*newVar;

	iVar = index;

	tree = (pavl_table *)classGetCargo ( obj );

	y = tree->pavl_root;
	for (q = NULL, p = tree->pavl_root; p != NULL; q = p, p = p->pavl_link[dir])
	{
		auto cmp = tree->varCompare ( instance, tree, iVar, p->index );
		if (cmp == 0)
		{
			tree->pavl_current = p;
			return p;
		}
		dir = cmp > 0;

		if (p->pavl_balance != 0)
		{
			y = p;
		}
	}

	n = (pavl_node *)instance->om->alloc ( sizeof ( pavl_node ) );
	newVar = instance->om->allocVar( 2 );

	tree = (pavl_table *) classGetCargo ( obj );

	tree->pavl_count++;

	n->pavl_link[0] = n->pavl_link[1] = NULL;
	n->pavl_parent	= q;
	n->data			= &newVar[0];
	n->data->type	= slangType::eNULL;
	n->index		= &newVar[1];
	*n->index		= *iVar;

	if (q != NULL)
	{
		q->pavl_link[dir] = n;
	} else
	{
		tree->pavl_root = n;
	}
	n->pavl_balance = 0;
	if (tree->pavl_root == n)
	{
		tree->pavl_current = n;
		return n;
	}

	for (p = n; p != y; p = q)
	{
		q = p->pavl_parent;
		dir = q->pavl_link[0] != p;
		if (dir == 0)
		{
			q->pavl_balance--;
		} else
		{
			q->pavl_balance++;
		}
	}

	if (y->pavl_balance == -2)
	{
		struct pavl_node *x = y->pavl_link[0];
		assert ( x );
		if (x->pavl_balance == -1)
		{
			w = x;
			y->pavl_link[0] = x->pavl_link[1];
			x->pavl_link[1] = y;
			x->pavl_balance = y->pavl_balance = 0;
			x->pavl_parent = y->pavl_parent;
			y->pavl_parent = x;
			if (y->pavl_link[0] != NULL)
				y->pavl_link[0]->pavl_parent = y;
		} else
		{
			w = x->pavl_link[1];
			assert ( w );
			x->pavl_link[1] = w->pavl_link[0];
			w->pavl_link[0] = x;
			y->pavl_link[0] = w->pavl_link[1];
			w->pavl_link[1] = y;
			if (w->pavl_balance == -1)
			{
				x->pavl_balance = 0, y->pavl_balance = +1;
			} else if (w->pavl_balance == 0)
			{
				x->pavl_balance = y->pavl_balance = 0;
			} else /* |w->pavl_balance == +1| */
			{
				x->pavl_balance = -1, y->pavl_balance = 0;
			}
			w->pavl_balance = 0;
			w->pavl_parent = y->pavl_parent;
			x->pavl_parent = y->pavl_parent = w;
			if (x->pavl_link[1] != NULL)
			{
				x->pavl_link[1]->pavl_parent = x;
			}
			if (y->pavl_link[0] != NULL)
			{
				y->pavl_link[0]->pavl_parent = y;
			}
		}
	} else if (y->pavl_balance == +2)
	{
		struct pavl_node *x = y->pavl_link[1];
		assert ( x );
		if (x->pavl_balance == +1)
		{
			w = x;
			y->pavl_link[1] = x->pavl_link[0];
			x->pavl_link[0] = y;
			x->pavl_balance = y->pavl_balance = 0;
			x->pavl_parent = y->pavl_parent;
			y->pavl_parent = x;
			if (y->pavl_link[1] != NULL)
				y->pavl_link[1]->pavl_parent = y;
		} else
		{
			w = x->pavl_link[0];
			assert ( w );
			x->pavl_link[0] = w->pavl_link[1];
			w->pavl_link[1] = x;
			y->pavl_link[1] = w->pavl_link[0];
			w->pavl_link[0] = y;
			if (w->pavl_balance == +1)
			{
				x->pavl_balance = 0, y->pavl_balance = -1;
			} else if (w->pavl_balance == 0)
			{
				x->pavl_balance = y->pavl_balance = 0;
			} else /* |w->pavl_balance == -1| */
			{
				x->pavl_balance = +1, y->pavl_balance = 0;
			}
			w->pavl_balance = 0;
			w->pavl_parent = y->pavl_parent;
			x->pavl_parent = y->pavl_parent = w;
			if (x->pavl_link[0] != NULL)
			{
				x->pavl_link[0]->pavl_parent = x;
			}
			if (y->pavl_link[1] != NULL)
			{
				y->pavl_link[1]->pavl_parent = y;
			}
		}
	} else
	{
		tree->pavl_current = n;
		return n;
	}

	if (w->pavl_parent != NULL)
	{
		w->pavl_parent->pavl_link[y != w->pavl_parent->pavl_link[0]] = w;
	} else
	{
		tree->pavl_root = w;
	}

	tree->pavl_current = n;
	return n;
}

static char *pseudoAlloc ( char ** ptr, size_t size )
{
	char *ret;
	ret = *ptr;
	(*ptr) += size;

	return ret;
}

//****************** modified pavl code

/* Inserts |item| into |tree| and returns a pointer to |item|'s address.
If a duplicate item is found in the tree,
returns a pointer to the duplicate without inserting |item|.
Returns |NULL| in case of memory allocation failure. */
static pavl_node *pavl_probe_pseudo ( vmInstance *instance, VAR *obj, VAR *index, char ** ptr, VAR **newVar )
{
	pavl_node		*y;     /* Top node to update balance factor, and parent. */
	pavl_node		*p, *q; /* Iterator, and parent. */
	pavl_node		*n;     /* Newly inserted node. */
	pavl_node		*w;     /* New root of rebalanced subtree. */
	pavl_table		*tree;
	int64_t			dir = 0;                 /* Direction to descend. */
	VAR				*iVar;

	iVar = index;
	while ( TYPE ( iVar ) == slangType::eREFERENCE )
	{
		iVar = iVar->dat.ref.v;
	}

	tree = (pavl_table *)classGetCargo ( obj );

	y = tree->pavl_root;
	for (q = NULL, p = tree->pavl_root; p != NULL; q = p, p = p->pavl_link[dir])
	{
		auto cmp = tree->varCompare ( instance, tree, iVar, p->index );
		if (cmp == 0)
		{
			tree->pavl_current = p;
			return p;
		}
		dir = cmp > 0;

		if (p->pavl_balance != 0)
		{
			y = p;
		}
	}

	// allocate the new node

	n = (pavl_node *)pseudoAlloc ( ptr, sizeof ( pavl_node ) );

	tree->pavl_count++;

	n->pavl_link[0] = n->pavl_link[1] = NULL;
	n->pavl_parent = q;
	n->index		= index;
	n->data			= *newVar;
	(*newVar)++;
	n->data->type	= slangType::eNULL;
	if (q != NULL)
	{
		q->pavl_link[dir] = n;
	} else
	{
		tree->pavl_root = n;
	}
	n->pavl_balance = 0;
	if (tree->pavl_root == n)
	{
		tree->pavl_current = n;
		return n;
	}

	for (p = n; p != y; p = q)
	{
		q = p->pavl_parent;
		dir = q->pavl_link[0] != p;
		if (dir == 0)
		{
			q->pavl_balance--;
		} else
		{
			q->pavl_balance++;
		}
	}

	if (y->pavl_balance == -2)
	{
		struct pavl_node *x = y->pavl_link[0];
		assert ( x );
		if (x->pavl_balance == -1)
		{
			w = x;
			y->pavl_link[0] = x->pavl_link[1];
			x->pavl_link[1] = y;
			x->pavl_balance = y->pavl_balance = 0;
			x->pavl_parent = y->pavl_parent;
			y->pavl_parent = x;
			if (y->pavl_link[0] != NULL)
				y->pavl_link[0]->pavl_parent = y;
		} else
		{
			w = x->pavl_link[1];
			assert ( w );
			x->pavl_link[1] = w->pavl_link[0];
			w->pavl_link[0] = x;
			y->pavl_link[0] = w->pavl_link[1];
			w->pavl_link[1] = y;
			if (w->pavl_balance == -1)
			{
				x->pavl_balance = 0, y->pavl_balance = +1;
			} else if (w->pavl_balance == 0)
			{
				x->pavl_balance = y->pavl_balance = 0;
			} else /* |w->pavl_balance == +1| */
			{
				x->pavl_balance = -1, y->pavl_balance = 0;
			}
			w->pavl_balance = 0;
			w->pavl_parent = y->pavl_parent;
			x->pavl_parent = y->pavl_parent = w;
			if (x->pavl_link[1] != NULL)
			{
				x->pavl_link[1]->pavl_parent = x;
			}
			if (y->pavl_link[0] != NULL)
			{
				y->pavl_link[0]->pavl_parent = y;
			}
		}
	} else if (y->pavl_balance == +2)
	{
		struct pavl_node *x = y->pavl_link[1];
		assert ( x );
		if (x->pavl_balance == +1)
		{
			w = x;
			y->pavl_link[1] = x->pavl_link[0];
			x->pavl_link[0] = y;
			x->pavl_balance = y->pavl_balance = 0;
			x->pavl_parent = y->pavl_parent;
			y->pavl_parent = x;
			if (y->pavl_link[1] != NULL)
				y->pavl_link[1]->pavl_parent = y;
		} else
		{
			w = x->pavl_link[0];
			assert ( w );
			x->pavl_link[0] = w->pavl_link[1];
			w->pavl_link[1] = x;
			y->pavl_link[1] = w->pavl_link[0];
			w->pavl_link[0] = y;
			if (w->pavl_balance == +1)
			{
				x->pavl_balance = 0, y->pavl_balance = -1;
			} else if (w->pavl_balance == 0)
			{
				x->pavl_balance = y->pavl_balance = 0;
			} else /* |w->pavl_balance == -1| */
			{
				x->pavl_balance = +1, y->pavl_balance = 0;
			}
			w->pavl_balance = 0;
			w->pavl_parent = y->pavl_parent;
			x->pavl_parent = y->pavl_parent = w;
			if (x->pavl_link[0] != NULL)
			{
				x->pavl_link[0]->pavl_parent = x;
			}
			if (y->pavl_link[1] != NULL)
			{
				y->pavl_link[1]->pavl_parent = y;
			}
		}
	} else
	{
		tree->pavl_current = n;
		return n;
	}

	if (w->pavl_parent != NULL)
	{
		w->pavl_parent->pavl_link[y != w->pavl_parent->pavl_link[0]] = w;
	} else
	{
		tree->pavl_root = w;
	}

	tree->pavl_current = n;
	return n;
}

/* finds an item... if not found then NULL is returned */
static pavl_node *pavl_find ( vmInstance *instance, VAR *obj, VAR *index, int64_t soft )
{
	pavl_node		*p; /* Iterator, and parent. */
	pavl_table		*tree;
	int64_t			 dir = 0;                /* Direction to descend. */
	VAR				*iVar;

	iVar = index;
	while ( TYPE ( iVar ) == slangType::eREFERENCE )
	{
		iVar = iVar->dat.ref.v;
	}

	tree = (pavl_table *)classGetCargo ( obj );

	for ( p = tree->pavl_root; p != NULL; p = p->pavl_link[dir])
	{
		int64_t cmp = tree->varCompare ( instance, tree, iVar, p->index );
		if (cmp == 0)
		{
			tree->pavl_current = p;
			return p;
		}
		dir = cmp > 0;
	}

	return 0;
}


/* Deletes from |tree| and returns an item matching |item|.
Returns a null pointer if no matching item found. */
static int64_t pavl_delete ( vmInstance *instance, struct pavl_table *tree, VAR *index)

{
	pavl_node		*p;			/* Traverses tree to find node to delete. */
	pavl_node		*q;			/* Parent of |p|. */
	int				 dir;       /* Side of |q| on which |p| is linked. */
	VAR				*iVar;


	if (tree->pavl_root == NULL)
	{
		return ( 0 );
	}

	iVar = index;
	while ( TYPE ( iVar ) == slangType::eREFERENCE )
	{
		iVar = iVar->dat.ref.v;
	}

	dir = 0;
	p = tree->pavl_root;
	for (;;)
	{
		int64_t cmp = tree->varCompare ( instance, tree, iVar, p->index );
		if (cmp == 0)
		{
			break;
		}

		dir = cmp > 0;
		p = p->pavl_link[dir];
		if (p == NULL)
		{
			return ( 0 );
		}
	}

	if ( p == tree->pavl_current )
	{
		tree->pavl_current = 0;
	}

	q = p->pavl_parent;
	if (q == NULL)
	{
		q = (struct pavl_node *) &tree->pavl_root;
		dir = 0;
	}

	if (p->pavl_link[1] == NULL)
	{
		q->pavl_link[dir] = p->pavl_link[0];
		if (q->pavl_link[dir] != NULL)
		{
			q->pavl_link[dir]->pavl_parent = p->pavl_parent;
		}
	} else
	{
		struct pavl_node *r = p->pavl_link[1];
		if (r->pavl_link[0] == NULL)
		{
			r->pavl_link[0] = p->pavl_link[0];
			q->pavl_link[dir] = r;
			r->pavl_parent = p->pavl_parent;
			if (r->pavl_link[0] != NULL)
			{
				r->pavl_link[0]->pavl_parent = r;
			}
			r->pavl_balance = p->pavl_balance;
			q = r;
			dir = 1;
		} else
		{
			struct pavl_node *s = r->pavl_link[0];
			while (s->pavl_link[0] != NULL)
			{
				s = s->pavl_link[0];
			}
			r = s->pavl_parent;
			r->pavl_link[0] = s->pavl_link[1];
			s->pavl_link[0] = p->pavl_link[0];
			s->pavl_link[1] = p->pavl_link[1];
			q->pavl_link[dir] = s;
			if (s->pavl_link[0] != NULL)
			{
				s->pavl_link[0]->pavl_parent = s;
			}
			s->pavl_link[1]->pavl_parent = s;
			s->pavl_parent = p->pavl_parent;
			if (r->pavl_link[0] != NULL)
			{
				r->pavl_link[0]->pavl_parent = r;
			}
			s->pavl_balance = p->pavl_balance;
			q = r;
			dir = 0;
		}
	}

	while (q != (struct pavl_node *) &tree->pavl_root)
	{
		pavl_node *y = q;

		if (y->pavl_parent != NULL)
		{
			q = y->pavl_parent;
		} else
		{
			q = (struct pavl_node *) &tree->pavl_root;
		}

		if (dir == 0)
		{
			dir = q->pavl_link[0] != y;
			y->pavl_balance++;
			if (y->pavl_balance == +1)
			{
				break;
			} else if (y->pavl_balance == +2)
			{
				struct pavl_node *x = y->pavl_link[1];
				if (x->pavl_balance == -1)
				{
					struct pavl_node *w;

					w = x->pavl_link[0];
					x->pavl_link[0] = w->pavl_link[1];
					w->pavl_link[1] = x;
					y->pavl_link[1] = w->pavl_link[0];
					w->pavl_link[0] = y;
					if (w->pavl_balance == +1)
					{
						x->pavl_balance = 0, y->pavl_balance = -1;
					} else if (w->pavl_balance == 0)
					{
						x->pavl_balance = y->pavl_balance = 0;
					} else /* |w->pavl_balance == -1| */
					{
						x->pavl_balance = +1, y->pavl_balance = 0;
					}
					w->pavl_balance = 0;
					w->pavl_parent = y->pavl_parent;
					x->pavl_parent = y->pavl_parent = w;
					if (x->pavl_link[0] != NULL)
					{
						x->pavl_link[0]->pavl_parent = x;
					}
					if (y->pavl_link[1] != NULL)
					{
						y->pavl_link[1]->pavl_parent = y;
					}
					q->pavl_link[dir] = w;
				}
				else
				{
					y->pavl_link[1] = x->pavl_link[0];
					x->pavl_link[0] = y;
					x->pavl_parent = y->pavl_parent;
					y->pavl_parent = x;
					if (y->pavl_link[1] != NULL)
					{
						y->pavl_link[1]->pavl_parent = y;
					}
					q->pavl_link[dir] = x;
					if (x->pavl_balance == 0)
					{
						x->pavl_balance = -1;
						y->pavl_balance = +1;
						break;
					} else
					{
						x->pavl_balance = y->pavl_balance = 0;
					}
				}
			}
		} else
		{
			dir = q->pavl_link[0] != y;
			y->pavl_balance--;
			if (y->pavl_balance == -1)
			{
				break;
			} else if (y->pavl_balance == -2)
			{
				struct pavl_node *x = y->pavl_link[0];
				assert ( x );
				if (x->pavl_balance == +1)
				{
					struct pavl_node *w;
					w = x->pavl_link[1];
					x->pavl_link[1] = w->pavl_link[0];
					w->pavl_link[0] = x;
					y->pavl_link[0] = w->pavl_link[1];
					w->pavl_link[1] = y;
					if (w->pavl_balance == -1)
					{
						x->pavl_balance = 0, y->pavl_balance = +1;
					} else if (w->pavl_balance == 0)
					{
						x->pavl_balance = y->pavl_balance = 0;
					} else /* |w->pavl_balance == +1| */
					{
						x->pavl_balance = -1, y->pavl_balance = 0;
					}
					w->pavl_balance = 0;
					w->pavl_parent = y->pavl_parent;
					x->pavl_parent = y->pavl_parent = w;
					if (x->pavl_link[1] != NULL)
					{
						x->pavl_link[1]->pavl_parent = x;
					}
					if (y->pavl_link[0] != NULL)
					{
						y->pavl_link[0]->pavl_parent = y;
					}
					q->pavl_link[dir] = w;
				} else
				{
					y->pavl_link[0] = x->pavl_link[1];
					x->pavl_link[1] = y;
					x->pavl_parent = y->pavl_parent;
					y->pavl_parent = x;
					if (y->pavl_link[0] != NULL)
					{
						y->pavl_link[0]->pavl_parent = y;
					}
					q->pavl_link[dir] = x;
					if (x->pavl_balance == 0)
					{
						x->pavl_balance = +1;
						y->pavl_balance = -1;
						break;
					} else
					{
						x->pavl_balance = y->pavl_balance = 0;
					}
				}
			}
		}
	}

	tree->pavl_count--;
	return ( 1 );
}

static pavl_node *pavl_find_first ( struct pavl_table *tree )
{
	pavl_node		*p;			/* Traverses tree to find node to delete. */

	p = tree->pavl_root;
	if ( p != NULL )
	{
		while ( p->pavl_link[0] != NULL )
		{
			p = p->pavl_link[0];
		}
		return ( p );
	} else
	{
		return NULL;
	}
}

static pavl_node *pavl_find_last ( struct pavl_table *tree )
{
	pavl_node		*p;			/* Traverses tree to find node to delete. */

	p = tree->pavl_root;
	if ( p != NULL )
	{
		while ( p->pavl_link[1] != NULL )
		{
			p = p->pavl_link[1];
		}
		return ( p );
	} else
	{
		return NULL;
	}
}


/* Returns the next data item in inorder
within the tree being traversed with |trav|,
or if there are no more data items returns |NULL|. */
static pavl_node *pavl_find_next ( struct pavl_table *tree, struct pavl_node *node )
{
	if ( node == NULL )
	{
		return ( pavl_find_first ( tree ) );
	} else if ( node->pavl_link[1] == NULL)
	{
		struct pavl_node *q, *p; /* Current node and its child. */

		for (p = node, q = p->pavl_parent;; p = q, q = q->pavl_parent )
		{
			if (q == NULL || p == q->pavl_link[0])
			{
				return ( q != NULL ? q : NULL );
			}
		}
	} else
	{
		node = node->pavl_link[1];
		while ( node->pavl_link[0] != NULL )
		{
			node = node->pavl_link[0];
		}
		return ( node );
	}
}

/* Returns the previous data item in inorder
within the tree being traversed with |trav|,
or if there are no more data items returns |NULL|. */
static pavl_node *pavl_find_prev ( struct pavl_table *tree, struct pavl_node *node )
{
	if ( node == NULL)
	{
		return pavl_find_last (tree );
	} else if ( node->pavl_link[0] == NULL)
	{
		struct pavl_node *q, *p; /* Current node and its child. */
		for ( p = node, q = p->pavl_parent;;p = q, q = q->pavl_parent )
		{
			if (q == NULL || p == q->pavl_link[1])
			{
				return ( q != NULL ? q : NULL );
			}
		}
	} else
	{
		node = node->pavl_link[0];
		while ( node->pavl_link[1] != NULL)
		{
			node = node->pavl_link[1];
		}
		return ( node );
	}
}

//******************  traverser

/* Initializes |trav| for use with |tree|
   and selects the null node. */
static void pavl_t_init (struct pavl_traverser *trav, struct pavl_table *tree)
{
  trav->pavl_table = tree;
  trav->pavl_node = NULL;
}

/* Initializes |trav| for |tree|.
   Returns data item in |tree| with the least value,
   or |NULL| if |tree| is empty. */
static VAR *pavl_t_first (struct pavl_traverser *trav, struct pavl_table *tree)
{
  assert (tree != NULL && trav != NULL);

  trav->pavl_table = tree;
  trav->pavl_node = tree->pavl_root;
  if (trav->pavl_node != NULL)
    {
      while (trav->pavl_node->pavl_link[0] != NULL)
        trav->pavl_node = trav->pavl_node->pavl_link[0];
	  return trav->pavl_node->data;
    }
  else
    return NULL;
}

/* Returns the next data item in inorder
   within the tree being traversed with |trav|,
   or if there are no more data items returns |NULL|. */
static VAR *pavl_t_next (struct pavl_traverser *trav)
{
  assert (trav != NULL);

  if (trav->pavl_node == NULL)
    return pavl_t_first (trav, trav->pavl_table);
  else if (trav->pavl_node->pavl_link[1] == NULL)
    {
      struct pavl_node *q, *p; /* Current node and its child. */
      for (p = trav->pavl_node, q = p->pavl_parent; ;
           p = q, q = q->pavl_parent)
        if (q == NULL || p == q->pavl_link[0])
          {
            trav->pavl_node = q;
            return trav->pavl_node != NULL ? trav->pavl_node->data : NULL;
          }
    }
  else
    {
      trav->pavl_node = trav->pavl_node->pavl_link[1];
      while (trav->pavl_node->pavl_link[0] != NULL)
        trav->pavl_node = trav->pavl_node->pavl_link[0];
      return trav->pavl_node->data;
    }
}

/* Returns |trav|'s current item. */
static VAR *pavl_t_cur (struct pavl_traverser *trav)
{
  assert (trav != NULL);

  return trav->pavl_node != NULL ? trav->pavl_node->data : NULL;
}

/* Returns |trav|'s current item. */
static VAR *pavl_t_index( struct pavl_traverser *trav )
{
	assert( trav != NULL );

	return trav->pavl_node != NULL ? trav->pavl_node->index : NULL;
}

//******************  garbage collection routines

static pavl_node *cBtreeMove ( vmInstance *instance, pavl_table *table, pavl_node *oldNode, objectMemory::collectionVariables *col )
{
	pavl_node	*newNode;

	if ( col->doCopy( oldNode ) )
	{
		// move the node into the new GC generation
		newNode = (pavl_node *) instance->om->allocGen ( sizeof( pavl_node ) * 1, col->getAge() );
		std::swap ( *newNode, *oldNode );
		if ( table->pavl_current == oldNode )
		{
			table->pavl_current = newNode;
		}
		newNode->index	= instance->om->move ( instance, &newNode->index, newNode->index, false, col );
		newNode->data	= instance->om->move ( instance, &newNode->data, newNode->data, false, col );

	} else
	{
		newNode = oldNode;
		newNode->index	= instance->om->move ( instance, &newNode->index, newNode->index, false, col );
		newNode->data	= instance->om->move ( instance, &newNode->data, newNode->data, false, col );
	}

	// move the left and right links
	if ( newNode->pavl_link[0] )
	{
		newNode->pavl_link[0] = cBtreeMove ( instance, table, newNode->pavl_link[0], col );
		newNode->pavl_link[0]->pavl_parent = newNode;
	}
	if ( newNode->pavl_link[1] )
	{
		newNode->pavl_link[1] = cBtreeMove ( instance, table, newNode->pavl_link[1], col );
		newNode->pavl_link[1]->pavl_parent = newNode;
	}

	return ( newNode );
}

static void cBTreeGcCb ( vmInstance *instance, VAR_OBJ *val, objectMemory::collectionVariables *col )
{
	auto tableOld = val->getObj<pavl_table>( );
	if ( col->doCopy( tableOld ) )
	{
		auto table = val->makeObj<pavl_table>( col->getAge(), instance );
		std::swap ( *table, *tableOld );

		if ( table->pavl_root )
		{
			table->pavl_root = cBtreeMove( instance, table, table->pavl_root, col );
		}
	} else
	{
		if ( tableOld->pavl_root )
		{
			tableOld->pavl_root = cBtreeMove( instance, tableOld, tableOld->pavl_root, col );
		}
	}
}

static pavl_node *cBtreeCopy ( vmInstance *instance, pavl_table *table, pavl_node *oldNode, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	pavl_node	*newNode;

	// move the node into the new GC universe
	newNode = (pavl_node *)instance->om->alloc ( sizeof ( pavl_node ) );
	*newNode = *oldNode;

	if ( table->pavl_current == oldNode )
	{
		table->pavl_current = newNode;
	}

	// move the left and right links
	if ( newNode->pavl_link[0] )
	{
		newNode->pavl_link[0] = cBtreeCopy ( instance, table, newNode->pavl_link[0], copy, copyVar );
		newNode->pavl_link[0]->pavl_parent = newNode;
	}
	if ( newNode->pavl_link[1] )
	{
		newNode->pavl_link[1] = cBtreeCopy ( instance, table, newNode->pavl_link[1], copy, copyVar );
		newNode->pavl_link[1]->pavl_parent = newNode;
	}

	newNode->index = copy ( instance, oldNode->index, true, copyVar );
	newNode->data = copy ( instance, oldNode->data, true, copyVar );

	return newNode;
}

static void cBTreeCopyCb ( vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto table = VAR_OBJ ( val ).getObj<pavl_table> ( );
	auto newTable = VAR_OBJ ( val ).makeObj<pavl_table> ( instance );

	*newTable = *table;

	if ( newTable && newTable->pavl_root )
	{
		newTable->pavl_root = cBtreeCopy ( instance, newTable, newTable->pavl_root, copy, copyVar );
	}
}

//******************  pack routines

static void cBTreePack ( vmInstance *instance, BUFFER *buff, pavl_node *node, void *param, void (*packCB)( VAR *var, void *param ) )

{
	if ( node->pavl_link[0] )
	{
		cBTreePack ( instance, buff, node->pavl_link[0], param, packCB );
	}

	packCB ( node->index, param );
	packCB ( node->data, param );

	if ( node ->pavl_link[1] )
	{
		cBTreePack ( instance, buff, node->pavl_link[1], param, packCB );
	}
}

static void cBTreePackCb ( vmInstance *instance, VAR *val, BUFFER *buff, void *param, void (*packCB) ( VAR *var, void *param ) )
{
	pavl_table	*table;

	table = (pavl_table *)classGetCargo ( val );

	if ( table->varCompare == varCompare )
	{
		bufferPut32 ( buff, 1 );
	} else
	{
		bufferPut32 ( buff, 0 );
	}
	bufferPut32 ( buff, (uint32_t)table->pavl_count );

	if ( table->pavl_root )
	{
		cBTreePack ( instance, buff, table->pavl_root, param, packCB );
	}
}

static void cBTreeUnPackCB	( vmInstance *instance, struct VAR *val, unsigned char ** buff, uint64_t  *len, void *param, struct VAR *(*unPack)( unsigned char ** buff, uint64_t  *len, void *param ) )
{
	pavl_node		*node;
	unsigned long	 loop;
	VAR				*index;
	VAR				*data;

	auto table = VAR_OBJ ( val ).getObj<pavl_table> ( );

	index	= 0;

	data = 0;

	if ( *((uint32_t *)*buff) )
	{
		table->varCompare = varCompare;
	} else
	{
		table->varCompare = varCompareI;
	}
	*buff += sizeof ( uint32_t );
	*len -= sizeof ( uint32_t );

	loop = *((uint32_t *)*buff);
	*buff += sizeof ( uint32_t );
	*len -= sizeof ( uint32_t );

	for (; loop; loop-- )
	{
		data	= 0;
		index	= 0;

		try {
			index	= unPack ( buff, len, param );
			data	= unPack ( buff, len, param );
		} catch ( ... )
		{

			throw errorNum::scINTERNAL;
		}
		if ( !(node = pavl_probe ( instance, val, index )) )
		{
			throw errorNum::scINTERNAL;
		}

		*node->data = *data;
	}
}

static int64_t cBTreeNew ( vmInstance *instance, VAR_OBJ *object, VAR *p1 )
{
	auto table = object->makeObj<pavl_table> ( instance );

	memset ( table, 0, sizeof ( pavl_table ) );

	while ( TYPE ( p1 ) == slangType::eREFERENCE ) p1 = p1->dat.ref.v;

	switch ( TYPE ( p1 ) )
	{
		case slangType::eATOM:
			table->cbFunc = p1;
			table->varCompare = varCompareUser;
			break;
		case slangType::eCODEBLOCK_ROOT:
			// TODO: support this
			throw errorNum::scUNSUPPORTED;
		case slangType::eLONG:
			table->varCompare = p1->dat.l ? varCompare : varCompareI;
			break;
		case slangType::eNULL:
			table->varCompare = varCompareI;
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	return ( 1 );
}

static VAR_REF cBTreeOpArray ( vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	pavl_node		*node;

	if ( obj->type == slangType::eNULL ) throw errorNum::scINVALID_PARAMETER;
	obj->touch( );

	if ( !(node = pavl_probe ( instance, obj->dat.ref.v, index )) )
	{
		throw errorNum::scINTERNAL;
	}

	return VAR_REF ( obj, node->data );
}

static VAR cBTreeOpArrayAssign ( vmInstance *instance, VAR_OBJ *obj, VAR *index, VAR *value )
{
	pavl_node		*node;

	obj->touch ( );

	if ( !(node = pavl_probe ( instance, obj->dat.ref.v, index )) )
	{
		throw errorNum::scINTERNAL;
	}

	*node->data = *value;

	return VAR_REF ( obj, value );
}

static VAR cBTreeDefAssign ( vmInstance *instance, VAR_OBJ *obj, VAR *index, VAR *val )
{
	pavl_node		*node;

	obj->touch ( );

	if ( !(node = pavl_probe ( instance, obj->dat.ref.v, index )) )
	{
		throw errorNum::scINTERNAL;
	}

	*node->data = *val;

	return VAR_REF ( obj, val );
}

static int64_t cBTreeIsPresent ( vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	if ( pavl_find ( instance, obj->dat.ref.v, index, 0 ) )
	{
		return ( 1 );
	}

	return ( 0 );
}

static int64_t cBTreeDelete ( vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	auto table = obj->getObj<pavl_table> ( );

	obj->touch( );

	return ( pavl_delete ( instance, table, index ) );
}

static size_t cBTreeLen ( vmInstance *instance, VAR_OBJ *obj )
{
	auto table = obj->getObj<pavl_table> ( );

	return table->pavl_count;
}

static void cBTreeInspectTrav ( vmInstance *instance, vmInspectList *vars, pavl_node *node )
{
	VAR		*index;

	if ( !node ) return;

	if ( node->pavl_link[0] )
	{
		cBTreeInspectTrav ( instance, vars, node->pavl_link[0] );
	}

	index = node->index;
	while ( TYPE ( index ) == slangType::eREFERENCE ) index = index->dat.ref.v;

	switch ( TYPE ( node->index ) )
	{
		case slangType::eSTRING:
			vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, index->dat.str.c, node->data )) );
			break;
		case slangType::eLONG:
			vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, index->dat.l, node->data )) );
			break;
		case slangType::eDOUBLE:
			vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, index->dat.d, node->data )) );
			break;
		default:
			break;
	}

	if ( node->pavl_link[1] )
	{
		cBTreeInspectTrav ( instance, vars, node->pavl_link[1] );
	}
}

static vmInspectList *cBTreeInspector ( vmInstance *instance, bcFuncDef *func, VAR *obj, uint64_t  start, uint64_t  end )
{
	auto table = VAR_OBJ(obj).getObj<pavl_table> ( );
	auto vars = new vmInspectList();

	cBTreeInspectTrav ( instance, vars, table->pavl_root );

	return vars;
}

static VAR cBTreeIndicieAccess ( VAR_OBJ *obj, vmInstance *instance )
{
	auto table = obj->getObj<pavl_table> ( );

	if ( table->pavl_current )
	{
		return VAR_REF ( obj, table->pavl_current->index );
	} else
	{
		return VAR_NULL ();
	}
}

static VAR *cBTreeAdd ( vmInstance *instance, VAR_OBJ *obj, VAR *val )
{
	pavl_node		*node;
	int64_t			 indicie[2]{};
	VAR				*index;
	VAR				*value;

	instance->result = *obj;
	obj->touch( );

	while ( TYPE ( val ) == slangType::eREFERENCE )
	{
		val = val->dat.ref.v;
	}

	if ( TYPE ( val ) == slangType::eNULL )
	{
		return obj;
	}

	if ( TYPE ( val ) != slangType::eARRAY_ROOT ) throw errorNum::scINVALID_PARAMETER;

	val = val->dat.ref.v;

	switch ( TYPE ( val ) )
	{
		case slangType::eARRAY_SPARSE:
			if ( val->dat.aSparse.v )
			{
				indicie[0] = val->dat.aSparse.v->dat.aElem.elemNum;
				while ( indicie[0] <= (int)val->dat.aSparse.maxI )
				{
					indicie[1] = 1;

					if ( _arrayIsElem ( instance, val, 2, indicie ) )
					{
						index = _arrayGet ( instance, val, 2, indicie );
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					indicie[1] = 2;
					if ( _arrayIsElem ( instance, val, 2, indicie ) )
					{
						if ( !(node = pavl_probe ( instance, obj->dat.ref.v, index )) )
						{
							throw errorNum::scINTERNAL;
						}

						value = _arrayGet ( instance, val, 2, indicie );
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					*node->data = *value;
					indicie[0]++;
				}
			}
			break;
		case slangType::eARRAY_FIXED:
			indicie[0] = val->dat.arrayFixed.startIndex;
			while ( indicie[0] <= val->dat.arrayFixed.endIndex )
			{
				indicie[1] = 1;

				if ( _arrayIsElem ( instance, val, 2, indicie ) )
				{
					index = _arrayGet ( instance, val, 2, indicie );
				} else
				{
					break;
				}

				indicie[1] =2;
				if ( _arrayIsElem ( instance, val, 2, indicie ) )
				{
					if ( !(node = pavl_probe ( instance, obj->dat.ref.v, index )) )
					{
						throw errorNum::scINTERNAL;
					}

					value = _arrayGet ( instance, val, 2, indicie );
				} else
				{
					break;
				}

				*node->data = *value;

				indicie[0]++;
			}
			break;
		default:
			break;
	}

	return obj;
}

static auto findRecurse ( vmInstance *instance, pavl_table *tree, pavl_node *p, VAR *index, int64_t soft )
{
	int64_t	 cmp;

	while ( p )
	{
		cmp = tree->varCompare ( instance, tree, index, p->index );

		if (cmp < 0)
		{
			p = p->pavl_link[0];
		} else if (cmp > 0)
		{
			p = p->pavl_link[1];
		} else
		{
			return p;
		}
	}
	return (pavl_node *)nullptr;
}

static VAR cBTreeFind ( vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	pavl_node *res;
	if ( (res = findRecurse ( instance, table, table->pavl_root, index, 1 )) )
	{
		return VAR_REF ( obj, res->data );
	}
	return VAR_NULL ( );
}

static VAR cBTreeFirstAccess ( vmInstance *instance, VAR_OBJ *obj )
{
	pavl_node *node;

	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	if ( (node = pavl_find_first ( table )) )
	{
		table->pavl_current = node;
		return VAR_REF ( obj, node->data );
	}
	return VAR_NULL();
}

static VAR cBTreeLastAccess ( vmInstance *instance, VAR_OBJ *obj )
{
	pavl_node *node;

	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	if ( (node = pavl_find_last ( table )) )
	{
		table->pavl_current = node;
		return VAR_REF ( obj, node->data );
	}
	return VAR_NULL ( );
}

static VAR cBTreeNextAccess ( vmInstance *instance, VAR_OBJ *obj )
{
	pavl_node *node;

	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	if ( (node = pavl_find_next ( table, table->pavl_current )) )
	{
		table->pavl_current = node;
		return VAR_REF ( obj, node->data );
	}
	return VAR_NULL ( );
}

static VAR cBTreePrevAccess ( vmInstance *instance, VAR_OBJ *obj )
{
	pavl_node *node;

	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	if ( (node = pavl_find_prev ( table, table->pavl_current )) )
	{
		return VAR_REF ( obj, node->data );
	}
	return VAR_NULL ( );
}

static VAR cBTreeCurrentAccess ( vmInstance *instance, VAR_OBJ *obj )
{
	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	if ( table->pavl_current )
	{
		return VAR_REF ( obj, table->pavl_current->data );
	}
	return VAR_NULL ( );
}

static VAR cBTreeRename ( vmInstance *instance, VAR_OBJ *obj, VAR *oldIndex, VAR *newIndex )
{
	auto table = obj->getObj<pavl_table> ( );
	obj->touch( );

	if ( findRecurse ( instance, table, table->pavl_root, oldIndex, 1 ) )
	{
		pavl_delete ( instance, table, oldIndex );

		pavl_node *node = pavl_probe ( instance, obj, newIndex );
		*node->data = instance->result;
		return *obj;
	}
	return VAR_NULL ( );
}

void aArrayNew ( vmInstance *instance, int64_t nPairs )
{
	pavl_table			 *table;
	pavl_node			 *node;
	bcClass				 *callerClass;
	VAR					 *varTmp;
	VAR					 *obj;
	VAR					 *params;
	VAR					 *nodeVal;
	char				 *tmpPtr;
	VAR					 *newVar;
	GRIP				  g1 ( instance );

	VAR *destAddr = instance->stack - nPairs * 2;

	callerClass = instance->atomTable->atomGetClass ( "aArray" );
	
	tmpPtr = (char *)instance->om->alloc ( sizeof ( pavl_table ) + 16 + sizeof ( pavl_node ) * nPairs );
	newVar = instance->om->allocVar( nPairs * 2 + 2 + callerClass->numVars );

	obj = newVar++;

	// SPECIAL CASE: this object is guaranteed to have no instance variables and does not inherit.   we could use the 
	obj->type    = slangType::eOBJECT;
	obj->dat.obj.classDef = callerClass;
	obj->dat.obj.vPtr = callerClass->vTable;

	newVar->type = slangType::eOBJECT_ROOT;
	newVar->dat.ref.v = obj;
	newVar->dat.ref.obj = obj;
	newVar++;

	varTmp				= instance->stack++;
	varTmp->type		= slangType::eOBJECT_ROOT;
	varTmp->dat.ref.v	= obj;
	varTmp->dat.ref.obj = obj;

	obj->dat.obj.cargo		= pseudoAlloc ( &tmpPtr, sizeof ( pavl_table )  + 16 );
	*(size_t *)obj->dat.obj.cargo = sizeof ( pavl_table );

	table = (pavl_table *)classGetCargo ( varTmp );
	memset ( table, 0, sizeof ( pavl_table ) );

	table->varCompare		= varCompare;

	params = instance->stack - 2 * nPairs - 1;
	for (; nPairs; nPairs-- )
	{
		nodeVal		= newVar++;
		*nodeVal	= (*params++);
		if ( !(node = pavl_probe_pseudo ( instance, varTmp, nodeVal, &tmpPtr, &newVar )) )
		{
			throw errorNum::scINTERNAL;
		}
		*node->data	= *(params++);;
	}
	*destAddr = *(instance->stack - 1);
	instance->stack = destAddr + 1;
}

static VAR_OBJ_TYPE<"aArrayEnumerator"> cBTreeGetEnumerator ( vmInstance *instance, VAR *object )
{
	PUSH ( *object );
	instance->result = *classNew ( instance, "aArrayEnumerator", 1 );
	POP(1);
	return VAR_OBJ ( instance->result );
}

static void aArrayEnumeratorNew ( vmInstance *instance, VAR_OBJ *object, VAR_OBJ *arr )
{
	auto table = arr->getObj<pavl_table> ();
	auto trav = object->makeObj<pavl_traverser> ( instance );

	pavl_t_init ( trav, table );

	*(*object)["$array"] = *arr;
}

static VAR aArrayEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *object )
{
	auto trav = object->getObj<pavl_traverser> ( );

	return *pavl_t_cur ( trav );
}

static VAR aArrayEnumeratorIndex( vmInstance *instance, VAR_OBJ *object )
{
	auto trav = object->getObj<pavl_traverser> ( );

	return *pavl_t_index( trav );
}

static uint64_t  aArrayEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *object )
{
	auto trav = object->getObj<pavl_traverser> ( );
	object->touch( );

	return pavl_t_next ( trav ) ? 1 : 0;
}

static void aArrayEnumeratorGcCb ( vmInstance *instance, VAR_OBJ *object, objectMemory::collectionVariables *col )
{
	auto travOld = object->getObj<pavl_traverser> ( );
	pavl_traverser *trav;

	if ( col->doCopy( travOld ) )
	{
		trav = object->makeObj<pavl_traverser>( col->getAge(), instance );
		std::swap ( *trav, *travOld );
	} else
	{
		trav = travOld;
	}

	std::vector<bool> moveLeft;
	auto node = trav->pavl_node;
	if ( node )
	{
		// get our left-right pattern to get back to our node
		while ( node->pavl_parent )
		{
			if ( node->pavl_parent->pavl_link[0] == node )
			{
				moveLeft.push_back ( true );
			} else
			{
				moveLeft.push_back ( false );
			}
			node = node->pavl_parent;
		}

		// --- ivar's will have already been collected by garbage collector before calling the C callback
		auto arr = (*(VAR_OBJ *)object)["$array"];
		auto table = VAR_OBJ ( arr ).getObj<pavl_table> ( );
		trav->pavl_table = table;
		
		auto node2 = table->pavl_root;
		for ( auto it = moveLeft.crbegin ( ); it != moveLeft.crend ( ); it++ )
		{
			if ( *it )
			{
				node2 = node2->pavl_link[0];
				assert ( node2 );
			} else
			{
				node2 = node2->pavl_link[1];
				assert ( node2 );
			}
		}
		assert ( node2 );
		trav->pavl_node = node2;
	}
}

static VAR_ARRAY arrayAdd( vmInstance *instance, VAR_REF *arr, VAR *newElem )
{
	VAR *value = arr;

	while ( 1 )
	{
		switch ( TYPE ( value ) )
		{
			case slangType::eREFERENCE:
				value = value->dat.ref.v;
				break;
			case slangType::eARRAY_ROOT:
				{
					if ( value->dat.ref.v->type != slangType::eARRAY_FIXED )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					auto newVar = instance->om->allocVar ( value->dat.ref.v->dat.arrayFixed.endIndex - value->dat.ref.v->dat.arrayFixed.startIndex + 1 + 1 + 1 );	// +1 for size, +1 for new item, +1 for array_fixed var

					memcpy ( &newVar[1], &(value->dat.ref.v)[1], (value->dat.ref.v->dat.arrayFixed.endIndex - value->dat.ref.v->dat.arrayFixed.startIndex + 1) * sizeof ( VAR ) );
					newVar[value->dat.ref.v->dat.arrayFixed.endIndex - value->dat.ref.v->dat.arrayFixed.startIndex + 1 + 1] = *newElem;

					newVar->type = slangType::eARRAY_FIXED;
					newVar->dat.arrayFixed.startIndex = value->dat.ref.v->dat.arrayFixed.startIndex;
					newVar->dat.arrayFixed.endIndex = value->dat.ref.v->dat.arrayFixed.endIndex + 1;

					value->dat.ref.v = newVar;

					return VAR_ARRAY ( newVar );
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
}

static VAR_ARRAY arraySize( vmInstance *instance, VAR_REF *param, int64_t size )
{
	VAR *value = param;

	while ( 1 )
	{
		switch ( TYPE ( value ) )
		{
			case slangType::eREFERENCE:
				value = value->dat.ref.v;
				break;
			case slangType::eARRAY_ROOT:
				{
					auto oldSize = value->dat.ref.v->dat.arrayFixed.endIndex - value->dat.ref.v->dat.arrayFixed.startIndex + 1;

					auto nCopy = size;

					if ( size <= 0 ) throw errorNum::scINVALID_PARAMETER;

					if ( size > oldSize )
					{
						nCopy = oldSize;
					}

					auto newVar = instance->om->allocVar ( size + 1 );
					newVar->type = slangType::eARRAY_FIXED;
					newVar->dat.arrayFixed.startIndex = value->dat.ref.v->dat.arrayFixed.startIndex;
					newVar->dat.arrayFixed.endIndex = value->dat.ref.v->dat.arrayFixed.startIndex + size - 1;

					memcpy ( &newVar[1], &(value->dat.ref.v)[1], nCopy * sizeof ( VAR ) );

					for ( int64_t loop = nCopy; loop < size; loop++ )
					{
						newVar[loop + 1].type = slangType::eNULL;
					}

					value->dat.ref.v = newVar;
					return VAR_ARRAY ( newVar );
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
}

static VAR arrayFixed( vmInstance *instance, nParamType num )

{
	int64_t		 start;
	int64_t		 end;
	VAR			*param;
	VAR          defaultValue;

	defaultValue = VAR_NULL ();
	if ( num == 1 )
	{
		start = 1;
		param = num[0];
		switch ( TYPE( param ) )
		{
			case slangType::eLONG:
				end = param->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else if ( num == 2 )
	{
		param = num[0];

		switch ( TYPE( param ) )
		{
			case slangType::eLONG:
				start = param->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}

		param = num[1];

		switch ( TYPE( param ) )
		{
			case slangType::eLONG:
				end = start + param->dat.l - 1;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else if ( num == 3 )
	{
		param = num[2];
		defaultValue = *param;

		param = num[0];

		switch ( TYPE ( param ) )
		{
			case slangType::eLONG:
				start = param->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}

		param = num[1];

		switch ( TYPE ( param ) )
		{
			case slangType::eLONG:
				end = start + param->dat.l - 1;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	return arrayFixed2( instance, start, end, defaultValue );
}

/* performs a quicksort on a single dimensional array (both fixed and variable) */
static VAR_ARRAY vmStableSort ( vmInstance *instance, VAR *param, VAR *expr, VAR *userParam )
{
	VAR			 *var = nullptr;
	VAR			 *var2 = nullptr;
	std::unique_ptr<VAR *[]> arr;			// TODO: create custom allocator/deletor so that arr is allocated from the instance heap!
	uint64_t	  cnt;

	instance->result = *param;
	param = param->dat.ref.v;

	switch ( TYPE ( param ) )
	{
		case slangType::eNULL:
			return VAR_ARRAY ( instance );
		case slangType::eARRAY_SPARSE:
			if ( !(var = param->dat.aSparse.v) )
			{
				return VAR_ARRAY ( instance );
			} else
			{
				cnt = 0;
				/* how many things in this array? */
				while ( var )
				{
					cnt++;
					var = var->dat.aElem.next;
				}

				/* allocate array to hold VAR structures to sort */
				arr = std::make_unique<VAR *[]>( cnt );

				/* build our array to sort */
				cnt = 0;
				var = param->dat.aSparse.v;
				while ( var )
				{
					arr[cnt++] = var->dat.aElem.var;
					var = var->dat.aElem.next;
				}
			}
			break;

		case slangType::eARRAY_FIXED:
			var = param;

			cnt = var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1;

			/* allocate a C array to hold VAR structures to sort */
			arr = std::make_unique<VAR * []> ( cnt );

			for ( uint64_t loop = 0; loop < cnt; loop++ )
			{
				arr[loop] = &var[loop + 1];
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	if ( cnt )
	{
		std::span sArr ( arr.get(), cnt );

		if ( expr->type != slangType::eNULL )
		{
			bcFuncDef	*func;

			for ( uint64_t loop = 0; loop < cnt; loop++ )
			{
				var2 = arr[loop];

				while ( TYPE ( var2 ) == slangType::eREFERENCE )
				{
					var2 = var2->dat.ref.v;
				}

				arr[loop] = var2;
			}

			switch ( TYPE ( expr ) )
			{
				case slangType::eATOM:
					func = instance->atomTable->atomGetFunc ( expr->dat.atom );

					if ( func->nParams != 3 ) throw errorNum::scINVALID_PARAMETER;

					std::stable_sort ( sArr.begin ( ), sArr.end ( ), [=]( auto l, auto r )
									   {
										   *(instance->stack++) = *userParam;
										   *(instance->stack++) = *l;
										   *(instance->stack++) = *r;

										   switch ( func->conv )
										   {
											   case fgxFuncCallConvention::opFuncType_Bytecode:
												   instance->interpretBC ( func, 0, 3 );
												   break;
											   case fgxFuncCallConvention::opFuncType_cDecl:
												   instance->funcCall ( func, 3 );
												   break;
											   default:
												   throw errorNum::scINTERNAL;
										   }

										   instance->stack -= 3;

										   VAR *tmpVar = &instance->result;
										   while ( TYPE ( tmpVar ) == slangType::eREFERENCE )
										   {
											   tmpVar = tmpVar->dat.ref.v;
										   }
										   if ( TYPE ( tmpVar ) != slangType::eLONG )
										   {
											   throw errorNum::scINTERNAL;
										   }
										   return tmpVar->dat.l < 0;
									   }
					);
					break;
				case slangType::eOBJECT_ROOT:
					if ( expr->dat.ref.v->type == slangType::eOBJECT )
					{
						auto  cEntry = &expr->dat.ref.v->dat.obj.classDef->elements[expr->dat.ref.v->dat.obj.classDef->ops[int(fgxOvOp::ovFuncCall)] - 1];

						if ( cEntry->isVirtual )
						{
							func = instance->atomTable->atomGetFunc ( expr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
						} else
						{
							func = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
						}

						if ( func->nParams != 4 ) throw errorNum::scINVALID_PARAMETER;

						std::stable_sort ( sArr.begin (), sArr.end (), [=]( auto l, auto r )
										   {
											   *(instance->stack++) = *userParam;
											   *(instance->stack++) = *l;
											   *(instance->stack++) = *r;
											   *(instance->stack++) = *expr;

											   switch ( func->conv )
											   {
												   case fgxFuncCallConvention::opFuncType_Bytecode:
													   instance->interpretBC ( func, 0, 3 );
													   break;
												   case fgxFuncCallConvention::opFuncType_cDecl:
													   instance->funcCall ( func, 3 );
													   break;
												   default:
													   throw errorNum::scINTERNAL;
											   }

											   instance->stack -= 4;

											   VAR *tmpVar = &instance->result;
											   while ( TYPE ( tmpVar ) == slangType::eREFERENCE )
											   {
												   tmpVar = tmpVar->dat.ref.v;
											   }
											   if ( TYPE ( tmpVar ) != slangType::eLONG )
											   {
												   throw errorNum::scINTERNAL;
											   }
											   return tmpVar->dat.l < 0;
										   }
						);
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}
					break;
				case slangType::eCODEBLOCK_ROOT:
					// TODO: support this
					throw errorNum::scUNSUPPORTED;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		} else
		{
			for ( uint64_t loop = 0; loop < cnt; loop++ )
			{
				var2 = arr[loop];

				while ( TYPE ( var2 ) == slangType::eREFERENCE )
				{
					var2 = var2->dat.ref.v;
				}

				arr[loop] = var2;
				if ( arr[0]->type != arr[loop]->type ) throw errorNum::scASORT_TYPE;
			}

			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), []( auto l, auto r )
									   {
										   return (l->dat.l - r->dat.l);
									   }
					);
					break;
				case slangType::eDOUBLE:
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), []( auto l, auto r )
									   {
										   return (l->dat.d - r->dat.d);
									   }
					);
					break;
				case slangType::eSTRING:
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), []( auto l, auto r )
										{
											return strncmp ( l->dat.str.c, r->dat.str.c, l->dat.str.len + 1 );
										}
					);
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	}

	VAR *a = instance->om->allocVar ( cnt + 1 );
	a->type = slangType::eARRAY_FIXED;
	a->dat.arrayFixed.startIndex = 1;
	a->dat.arrayFixed.endIndex = static_cast<int64_t>(cnt);
	for ( uint64_t loop = 0; loop < cnt; loop++ )
	{
		a[loop + 1] = *arr[loop];
	}

	return VAR_ARRAY ( a );
}

/* performs a quicksort on a single dimensional array (both fixed and variable) */
static VAR_ARRAY vmSort ( vmInstance *instance, VAR *param, VAR *expr, VAR *userParam )
{
	VAR							*var = nullptr;
	VAR							*var2 = nullptr;
	std::unique_ptr<VAR * []>	 arr;			// TODO: create custom allocator/deletor so that arr is allocated from the instance heap!
	int64_t						 cnt;

	instance->result = *param;
	param = param->dat.ref.v;

	switch ( TYPE ( param ) )
	{
		case slangType::eNULL:
			return VAR_ARRAY ( instance );
		case slangType::eARRAY_SPARSE:
			if ( !(var = param->dat.aSparse.v) )
			{
				return VAR_ARRAY ( instance );
			} else
			{
				cnt = 0;
				/* how many things in this array? */
				while ( var )
				{
					cnt++;
					var = var->dat.aElem.next;
				}

				/* allocate array to hold VAR structures to sort */
				arr = std::make_unique<VAR * []> ( cnt );

				/* build our array to sort */
				cnt = 0;
				var = param->dat.aSparse.v;
				while ( var )
				{
					arr[cnt++] = var->dat.aElem.var;
					var = var->dat.aElem.next;
				}
			}
			break;

		case slangType::eARRAY_FIXED:
			var = param;

			cnt = var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1;
			assert ( cnt > 0 );	// by definition all fixed arrays must contain at least one element... start/end are incluisive so if they're equal it's one element;

			/* allocate a C array to hold VAR structures to sort */
			arr = std::make_unique<VAR * []> ( cnt );

			arr[0] = &var[1];
			for ( int64_t loop = 1; loop < cnt; loop++ )
			{
				arr[loop] = &var[loop + 1];
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	if ( cnt )
	{
		std::span sArr ( arr.get ( ), cnt );

		for ( int64_t loop = 0; loop < cnt; loop++ )
		{
			var2 = arr[loop];

			while ( TYPE ( var2 ) == slangType::eREFERENCE )
			{
				var2 = var2->dat.ref.v;
			}

			arr[loop] = var2;
			if ( arr[0]->type != arr[loop]->type ) throw errorNum::scASORT_TYPE;
		}

		if ( expr->type != slangType::eNULL )
		{
			bcFuncDef *func;

			switch ( TYPE ( expr ) )
			{
				case slangType::eATOM:
					func = instance->atomTable->atomGetFunc ( expr->dat.atom );
					break;
				case slangType::eCODEBLOCK_ROOT:
					// TODO: support this
					throw errorNum::scUNSUPPORTED;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			if ( func->nParams != 3 ) throw errorNum::scINVALID_PARAMETER;

			std::sort ( sArr.begin ( ), sArr.end ( ), [=]( auto l, auto r )
							   {
								   *(instance->stack++) = *userParam;
								   *(instance->stack++) = *l;
								   *(instance->stack++) = *r;

								   switch ( func->conv )
								   {
									   case fgxFuncCallConvention::opFuncType_Bytecode:
										   instance->interpretBC ( func, 0, 3 );
										   break;
									   case fgxFuncCallConvention::opFuncType_cDecl:
										   instance->funcCall ( func, 3 );
										   break;
									   default:
										   throw errorNum::scINTERNAL;
								   }

								   instance->stack -= 3;

								   VAR *tmpVar = &instance->result;
								   while ( TYPE ( tmpVar ) == slangType::eREFERENCE )
								   {
									   tmpVar = tmpVar->dat.ref.v;
								   }
								   if ( TYPE ( tmpVar ) != slangType::eLONG )
								   {
									   throw errorNum::scINTERNAL;
								   }
								   return (int64_t) tmpVar->dat.l;
							   }
			);
		} else
		{
			switch ( TYPE ( var2 ) )
			{
				case slangType::eLONG:
					std::sort ( sArr.begin ( ), sArr.end ( ), []( auto l, auto r )
									   {
										   return (l->dat.l - r->dat.l) < 0;
									   }
					);
					break;
				case slangType::eDOUBLE:
					std::sort ( sArr.begin ( ), sArr.end ( ), []( auto l, auto r )
									   {
										   return (l->dat.d - r->dat.d) < 0;
									   }
					);
					break;
				case slangType::eSTRING:
					std::sort ( sArr.begin ( ), sArr.end ( ), []( auto l, auto r )
									   {
										   return strncmp ( l->dat.str.c, r->dat.str.c, l->dat.str.len + 1 ) < 0;
									   }
					);
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	}

	VAR *a = instance->om->allocVar ( cnt + 1 );
	a->type = slangType::eARRAY_FIXED;
	a->dat.arrayFixed.startIndex = 1;
	a->dat.arrayFixed.endIndex = cnt;
	for ( int64_t loop = 0; loop < cnt; loop++ )
	{
		a[loop + 1] = *arr[loop];
	}

	return VAR_ARRAY ( a );
}

/* performs a quicksort on a single dimensional array (both fixed and variable) */
static VAR asort ( vmInstance *instance, VAR *param,  bool isDescend, int64_t column, bool isCaseSensitive )
{
	VAR							*var;
	std::unique_ptr<VAR * []>	 arr;
	int64_t						 cnt;

	instance->result = *param;
	param = param->dat.ref.v;

	switch ( TYPE ( param ) )
	{
		case slangType::eNULL:
			return VAR_NULL ( );
		case slangType::eARRAY_SPARSE:
			if ( !(var = param->dat.aSparse.v) )
			{
				return instance->result;
			} else
			{
				cnt = 0;
				/* how many things in this array? */
				while ( var )
				{
					cnt++;
					var = var->dat.aElem.next;
				}

				/* allocate array to hold VAR structures to sort */
				arr = std::make_unique<VAR * []> ( cnt );

				/* build our array to sort */
				cnt = 0;
				var = param->dat.aSparse.v;
				while ( var )
				{
					arr[cnt] = var->dat.aElem.var;
					while ( TYPE ( arr[cnt] ) == slangType::eREFERENCE ) arr[cnt] = arr[cnt]->dat.ref.v;
					if ( arr[0]->type != arr[cnt]->type ) throw errorNum::scASORT_TYPE;
					cnt++;
					var = var->dat.aElem.next;
				}
			}
			break;

		case slangType::eARRAY_FIXED:
			var = param;

			cnt = var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1;

				/* allocate array to hold VAR structures to sort */
			arr = std::make_unique<VAR * []> ( cnt );

			for ( int64_t loop = 0; loop < cnt; loop++ )
			{
				arr[loop] = &var[loop + 1];
				while ( TYPE ( arr[loop] ) == slangType::eREFERENCE ) arr[loop] = arr[cnt]->dat.ref.v;
				if ( arr[0]->type != arr[loop]->type ) throw errorNum::scASORT_TYPE;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	std::span sArr ( arr.get(), cnt );
	if ( cnt )
	{
		int dir = isDescend ? -1 : 1;

		switch ( TYPE ( arr[0] ) )
		{
			case slangType::eLONG:
				std::stable_sort ( sArr.begin ( ), sArr.end ( ), [dir]( auto l, auto r )
									{
									   return (l->dat.l - r->dat.l) * dir < 0;
									}
				);
				break;
			case slangType::eDOUBLE:
				std::stable_sort ( sArr.begin ( ), sArr.end ( ), [dir]( auto l, auto r )
									{
									   return (l->dat.d - r->dat.d) * dir < 0;
									}
				);
				break;
			case slangType::eSTRING:
				if ( isCaseSensitive )
				{
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), [dir]( auto l, auto r )
										{
										   return strncmp ( l->dat.str.c, r->dat.str.c, l->dat.str.len + 1 ) * dir < 0; // NOLINT ( bugprone-suspicious-string-compare )
										}
					);
				} else
				{
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), [dir]( auto l, auto r )
										{
										   return strncmp ( l->dat.str.c, r->dat.str.c, l->dat.str.len + 1 ) * dir < 0; // NOLINT ( bugprone-suspicious-string-compare )
										}
					);
				}
				break;
			case slangType::eARRAY_ROOT:
				if ( isCaseSensitive )
				{
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), [dir, column]( auto l, auto r )
									   {
										   auto aVal = (*l)[column];
										   auto bVal = (*r)[column];

										   while ( aVal->type == slangType::eREFERENCE ) aVal = aVal->dat.ref.v;
										   while ( bVal->type == slangType::eREFERENCE ) bVal = bVal->dat.ref.v;

										   if ( TYPE ( aVal ) != TYPE ( bVal ) ) throw errorNum::scINVALID_PARAMETER;

										   int64_t ret;
										   switch ( TYPE ( aVal ) )
										   {
											   case slangType::eDOUBLE:
												   ret = (int64_t) (aVal->dat.d - bVal->dat.d) * dir;
												   break;
											   case slangType::eLONG:
												   ret = (int64_t) (aVal->dat.l - bVal->dat.l) * dir;
												   break;
											   case slangType::eSTRING:
												   ret = (int64_t) strncmp ( aVal->dat.str.c, bVal->dat.str.c, aVal->dat.str.len + 1 ) * dir;
												   break;
											   default:
												   throw errorNum::scINVALID_PARAMETER;
										   }
										   return ret < 0;
									   }
									);
				} else
				{
					std::stable_sort ( sArr.begin ( ), sArr.end ( ), [dir, column]( auto l, auto r )
									   {
										   auto aVal = (*l)[column];
										   auto bVal = (*r)[column];

										   while ( aVal->type == slangType::eREFERENCE ) aVal = aVal->dat.ref.v;
										   while ( bVal->type == slangType::eREFERENCE ) bVal = bVal->dat.ref.v;

										   if ( TYPE ( aVal ) != TYPE ( bVal ) ) throw errorNum::scINVALID_PARAMETER;

										   int64_t ret;
										   switch ( TYPE ( aVal ) )
										   {
											   case slangType::eDOUBLE:
												   ret = (int64_t) (aVal->dat.d - bVal->dat.d) * dir;
												   break;
											   case slangType::eLONG:
												   ret = (int64_t) (aVal->dat.l - bVal->dat.l) * dir;
												   break;
											   case slangType::eSTRING:
												   ret = (int64_t) memcmpi ( aVal->dat.str.c, bVal->dat.str.c, aVal->dat.str.len + 1 ) * dir;
												   break;
											   default:
												   throw errorNum::scINVALID_PARAMETER;
										   }
										   return ret < 0;
									   }
					);
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}

#if 1
	switch ( TYPE ( param ) )
	{
		case slangType::eARRAY_SPARSE:
			var = param->dat.aSparse.v;
			cnt = 0;
			while ( var )
			{
				var->dat.aElem.var = arr[cnt];
				cnt++;
				var = var->dat.aElem.next;
			}
			break;
		default:
			break;
	}

	return instance->result;
#else

	instance->result.type = slangType::eARRAY_ROOT;
	instance->result.dat.ref.v = instance->om->allocVar ( cnt + 1 );
	instance->result.dat.ref.obj = instance->result.dat.ref.v;
	instance->result.dat.ref.v->type = slangType::eARRAY_FIXED;
	instance->result.dat.ref.v->dat.arrayFixed.startIndex = 1;
	instance->result.dat.ref.v->dat.arrayFixed.endIndex = cnt;
	for ( uint64_t loop = 0; loop < cnt; loop++ )
	{
		instance->result.dat.ref.v[loop + 1] = *arr[loop];
	}

	return instance->result;
#endif
}

static VAR_OBJ_TYPE<"aArrayEnumerator">  aArrayEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *object )
{
	return *object;
}

void builtinAArrayInit( vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CLASS ( "aArray" );
			METHOD( "new", cBTreeNew, DEF ( 2, "null" ) );

			/* compatability funcions*/
			ACCESS ( "first", cBTreeFirstAccess );
			ACCESS ( "last", cBTreeLastAccess );
			ACCESS ( "next", cBTreeNextAccess );
			ACCESS ( "prev", cBTreePrevAccess );
			ACCESS ( "current", cBTreeCurrentAccess );
			ACCESS ( "value", cBTreeCurrentAccess )  CONST;
			METHOD ( "rename", cBTreeRename );

			/* new flavor functions that match with other container types suporting enumeration */
			METHOD( "add", cBTreeAdd );
			METHOD( "getEnumerator", cBTreeGetEnumerator );
			METHOD( "find", cBTreeFind )  CONST;
			METHOD( "size", cBTreeLen )  CONST;
			METHOD( "len", cBTreeLen )  CONST;
			METHOD( "delete", cBTreeDelete );
			METHOD( "insert", cBTreeOpArrayAssign );
			METHOD( "has", cBTreeIsPresent )  CONST;
			ACCESS( "default", cBTreeOpArray );
			ASSIGN( "default", cBTreeDefAssign );
			ACCESS( "indicie", cBTreeIndicieAccess );
			OP ( "[", cBTreeOpArray );
			OP ( "[", cBTreeOpArrayAssign );

			GCCB( cBTreeGcCb, cBTreeCopyCb );
			PACKCB( cBTreePackCb, cBTreeUnPackCB );
			INSPECTORCB ( cBTreeInspector );
		END;

		CLASS ( "aArrayEnumerator" );
			INHERIT ( "Queryable" );

			METHOD( "new", aArrayEnumeratorNew );
			METHOD( "getEnumerator", aArrayEnumeratorGetEnumerator );
			METHOD( "moveNext", aArrayEnumeratorMoveNext );
			ACCESS( "current", aArrayEnumeratorCurrent ) CONST;
			ACCESS( "index", aArrayEnumeratorIndex ) CONST;
			IVAR ( "$array" );
			GCCB ( aArrayEnumeratorGcCb, nullptr );
		END;


		FUNC( "aMaxI", _amaxi_ );
		FUNC( "aMinI", _amini_ );
		FUNC( "aSort", asort, DEF ( 2, "0" ), DEF ( 3, "1" ), DEF ( 4, "1" ), DEF ( 5, "null" ) );
		FUNC( "aSortFast", asort, DEF ( 2, "0" ), DEF ( 3, "1" ), DEF ( 4, "1" ), DEF ( 5, "null" ) );

		FUNC( "stableSort", vmStableSort, DEF ( 2, "null" ), DEF ( 3, "null" ) );
		FUNC( "sort",		vmSort, DEF ( 2, "null" ), DEF ( 3, "null" ) );

		FUNC( "aNextI", _anexti );

		FUNC( "array", arrayFixed );
		FUNC( "aAdd",  arrayAdd, DEF ( 2, "null" ) );
		FUNC( "aSize", arraySize );

		FUNC( "arrayToNameValue", _arrayToNameValue );
	END;
}
