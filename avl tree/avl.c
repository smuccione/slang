/* Produced by texiweb from libavl.w on 2002/08/24 at 13:21. */

/* libavl - library for manipulation of binary trees.
Copyright (C) 1998-2002 Free Software Foundation, Inc.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the GNU General Public License for more details.

	  You should have received a copy of the GNU General Public License
	  along with this program; if not, write to the Free Software
	  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	  02111-1307, USA.

		The author may be contacted at <blp@gnu.org> on the Internet, or
		write to Ben Pfaff, Stanford University, Computer Science Dept., 353
		Serra Mall, Stanford CA 94305, USA.
*/
#define _MERGE_DATA_
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avl.h"
#include "assert.h"

static	CRITICAL_SECTION	 avlNodeFreeProtect;
static  long				 avlNodeFreeInit = 0;
static	struct avl_node *avlNodeFree = 0;

struct avl_node *avl_node_allocate ( void )
{
	struct avl_node *p;

	EnterCriticalSection ( &avlNodeFreeProtect );

	if ( avlNodeFree )
	{
		p = avlNodeFree;
		avlNodeFree = avlNodeFree->avl_link[0];
		LeaveCriticalSection ( &avlNodeFreeProtect );
		return (p);
	} else
	{
		LeaveCriticalSection ( &avlNodeFreeProtect );
		return (malloc ( sizeof ( *p ) ));
	}
}

void avl_node_free ( struct avl_node *p )
{
	EnterCriticalSection ( &avlNodeFreeProtect );

	p->avl_link[0] = avlNodeFree;
	avlNodeFree = p;

	LeaveCriticalSection ( &avlNodeFreeProtect );
}

/* Creates and returns a new table
with comparison function |compare| using parameter |param|
and memory allocator |allocator|.
Returns |NULL| if memory allocation failed. */
struct avl_table *
	avl_create ( avl_comparison_func *compare, void *param,
				struct libavl_allocator *allocator )
{
	struct avl_table *tree;

	tree = malloc ( sizeof ( *tree ) );
	assert( tree );

	tree->avl_root = NULL;
	tree->avl_compare = compare;
	tree->avl_param = param;
	tree->avl_alloc = allocator;
	tree->avl_count = 0;
	tree->avl_generation = 0;

	if ( !avlNodeFreeInit )
	{
		avlNodeFreeInit = 1;
		int r = InitializeCriticalSectionAndSpinCount ( &avlNodeFreeProtect, 4000 );
		assert( r );
	}

	return tree;
}

/* Search |tree| for an item matching |item|, and return it if found.
Otherwise return |NULL|. */
void *
avl_find ( const struct avl_table *tree, const void *item )
{
	const struct avl_node *p;

	for ( p = tree->avl_root; p != NULL; )
	{
		int cmp = tree->avl_compare ( item, p->avl_data, tree->avl_param );

		if ( cmp < 0 )
			p = p->avl_link[0];
		else if ( cmp > 0 )
			p = p->avl_link[1];
		else /* |cmp == 0| */
			return p->avl_data;
	}

	return NULL;
}

/* Inserts |item| into |tree| and returns a pointer to |item|'s address.
If a duplicate item is found in the tree,
returns a pointer to the duplicate without inserting |item|.
Returns |NULL| in case of memory allocation failure. */
void **
avl_probe ( struct avl_table *tree, void *item )
{
	struct avl_node *y, *z; /* Top node to update balance factor, and parent. */
	struct avl_node *p, *q; /* Iterator, and parent. */
	struct avl_node *n;     /* Newly inserted node. */
	struct avl_node *w;     /* New root of rebalanced subtree. */
	int dir;                /* Direction to descend. */

	unsigned char da[AVL_MAX_HEIGHT] = {0}; /* Cached comparison results. */
	int k = 0;              /* Number of cached results. */

	z = (struct avl_node *) &tree->avl_root;
	y = tree->avl_root;
	dir = 0;
	for ( q = z, p = y; p != NULL; q = p, p = p->avl_link[dir] )
	{
		int cmp = tree->avl_compare ( item, p->avl_data, tree->avl_param );
		if ( cmp == 0 )
			return &p->avl_data;

		if ( p->avl_balance != 0 )
			z = q, y = p, k = 0;
		da[k++] = dir = cmp > 0;
	}

	n = q->avl_link[dir] = avl_node_allocate ();;

	tree->avl_count++;
	n->avl_data = item;
	n->avl_link[0] = n->avl_link[1] = NULL;
	n->avl_balance = 0;
	if ( y == NULL )
		return &n->avl_data;

	for ( p = y, k = 0; p != n; p = p->avl_link[da[k]], k++ )
		if ( da[k] == 0 )
			p->avl_balance--;
		else
			p->avl_balance++;

	if ( y->avl_balance == -2 )
	{
		struct avl_node *x = y->avl_link[0];
		if ( x->avl_balance == -1 )
		{
			w = x;
			y->avl_link[0] = x->avl_link[1];
			x->avl_link[1] = y;
			x->avl_balance = y->avl_balance = 0;
		} 			else
		{
			w = x->avl_link[1];
			assert( w );
			x->avl_link[1] = w->avl_link[0];
			w->avl_link[0] = x;
			y->avl_link[0] = w->avl_link[1];
			w->avl_link[1] = y;
			if ( w->avl_balance == -1 )
				x->avl_balance = 0, y->avl_balance = +1;
			else if ( w->avl_balance == 0 )
				x->avl_balance = y->avl_balance = 0;
			else /* |w->avl_balance == +1| */
				x->avl_balance = -1, y->avl_balance = 0;
			w->avl_balance = 0;
		}
	} 		else if ( y->avl_balance == +2 )
	{
		struct avl_node *x = y->avl_link[1];
		if ( x->avl_balance == +1 )
		{
			w = x;
			y->avl_link[1] = x->avl_link[0];
			x->avl_link[0] = y;
			x->avl_balance = y->avl_balance = 0;
		} else
		{
			w = x->avl_link[0];
			assert( w );
			x->avl_link[0] = w->avl_link[1];
			w->avl_link[1] = x;
			y->avl_link[1] = w->avl_link[0];
			w->avl_link[0] = y;
			if ( w->avl_balance == +1 )
				x->avl_balance = 0, y->avl_balance = -1;
			else if ( w->avl_balance == 0 )
				x->avl_balance = y->avl_balance = 0;
			else /* |w->avl_balance == -1| */
				x->avl_balance = +1, y->avl_balance = 0;
			w->avl_balance = 0;
		}
	} 		else
		return &n->avl_data;
	z->avl_link[y != z->avl_link[0]] = w;

	tree->avl_generation++;
	return &n->avl_data;
}

/* Inserts |item| into |table|.
Returns |NULL| if |item| was successfully inserted
or if a memory allocation error occurred.
Otherwise, returns the duplicate item. */
void *
avl_insert ( struct avl_table *table, void *item )
{
	void **p = avl_probe ( table, item );
	return p == NULL || *p == item ? NULL : *p;
}

/* Inserts |item| into |table|, replacing any duplicate item.
Returns |NULL| if |item| was inserted without replacing a duplicate,
or if a memory allocation error occurred.
Otherwise, returns the item that was replaced. */
void *
avl_replace ( struct avl_table *table, void *item )
{
	void **p = avl_probe ( table, item );
	if ( p == NULL || *p == item )
		return NULL;
	else
	{
		void *r = *p;
		*p = item;
		return r;
	}
}

/* Deletes from |tree| and returns an item matching |item|.
Returns a null pointer if no matching item found. */
void *avl_delete ( struct avl_table *tree, const void *item )
{
	/* Stack of nodes. */
	struct avl_node *pa[AVL_MAX_HEIGHT]; /* Nodes. */
	unsigned char da[AVL_MAX_HEIGHT];    /* |avl_link[]| indexes. */
	int k;                               /* Stack pointer. */

	struct avl_node *p;   /* Traverses tree to find node to delete. */
	int cmp;              /* Result of comparison between |item| and |p|. */

	k = 0;
	p = (struct avl_node *) &tree->avl_root;
	for ( cmp = -1; cmp != 0;
	cmp = tree->avl_compare ( item, p->avl_data, tree->avl_param ) )
	{
		int dir = cmp > 0;

		pa[k] = p;
		da[k++] = dir;

		p = p->avl_link[dir];
		if ( p == NULL )
			return NULL;
	}
	item = p->avl_data;

	if ( p->avl_link[1] == NULL )
		pa[k - 1]->avl_link[da[k - 1]] = p->avl_link[0];
	else
	{
		struct avl_node *r = p->avl_link[1];
		if ( r->avl_link[0] == NULL )
		{
			r->avl_link[0] = p->avl_link[0];
			r->avl_balance = p->avl_balance;
			pa[k - 1]->avl_link[da[k - 1]] = r;
			da[k] = 1;
			pa[k++] = r;
		} 		else
		{
			struct avl_node *s;
			int j = k++;

			for ( ;;)
			{
				da[k] = 0;
				pa[k++] = r;
				s = r->avl_link[0];
				if ( s->avl_link[0] == NULL )
					break;

				r = s;
			}

			s->avl_link[0] = p->avl_link[0];
			r->avl_link[0] = s->avl_link[1];
			s->avl_link[1] = p->avl_link[1];
			s->avl_balance = p->avl_balance;

			pa[j - 1]->avl_link[da[j - 1]] = s;
			da[j] = 1;
			pa[j] = s;
		}
	}

	avl_node_free ( p );

	while ( --k > 0 )
	{
		struct avl_node *y = pa[k];

		if ( da[k] == 0 )
		{
			y->avl_balance++;
			if ( y->avl_balance == +1 )
				break;
			else if ( y->avl_balance == +2 )
			{
				struct avl_node *x = y->avl_link[1];
				if ( x->avl_balance == -1 )
				{
					struct avl_node *w;
					w = x->avl_link[0];
					x->avl_link[0] = w->avl_link[1];
					w->avl_link[1] = x;
					y->avl_link[1] = w->avl_link[0];
					w->avl_link[0] = y;
					if ( w->avl_balance == +1 )
						x->avl_balance = 0, y->avl_balance = -1;
					else if ( w->avl_balance == 0 )
						x->avl_balance = y->avl_balance = 0;
					else /* |w->avl_balance == -1| */
						x->avl_balance = +1, y->avl_balance = 0;
					w->avl_balance = 0;
					pa[k - 1]->avl_link[da[k - 1]] = w;
				} 				else
				{
					y->avl_link[1] = x->avl_link[0];
					x->avl_link[0] = y;
					pa[k - 1]->avl_link[da[k - 1]] = x;
					if ( x->avl_balance == 0 )
					{
						x->avl_balance = -1;
						y->avl_balance = +1;
						break;
					} 					else
						x->avl_balance = y->avl_balance = 0;
				}
			}
		} 		else
		{
			y->avl_balance--;
			if ( y->avl_balance == -1 )
				break;
			else if ( y->avl_balance == -2 )
			{
				struct avl_node *x = y->avl_link[0];
				if ( x->avl_balance == +1 )
				{
					struct avl_node *w;
					w = x->avl_link[1];
					x->avl_link[1] = w->avl_link[0];
					w->avl_link[0] = x;
					y->avl_link[0] = w->avl_link[1];
					w->avl_link[1] = y;
					if ( w->avl_balance == -1 )
						x->avl_balance = 0, y->avl_balance = +1;
					else if ( w->avl_balance == 0 )
						x->avl_balance = y->avl_balance = 0;
					else /* |w->avl_balance == +1| */
						x->avl_balance = -1, y->avl_balance = 0;
					w->avl_balance = 0;
					pa[k - 1]->avl_link[da[k - 1]] = w;
				} 				else
				{
					y->avl_link[0] = x->avl_link[1];
					x->avl_link[1] = y;
					pa[k - 1]->avl_link[da[k - 1]] = x;
					if ( x->avl_balance == 0 )
					{
						x->avl_balance = +1;
						y->avl_balance = -1;
						break;
					} 					else
						x->avl_balance = y->avl_balance = 0;
				}
			}
		}
	}

	tree->avl_count--;
	tree->avl_generation++;
	return (void *) item;
}

/* Refreshes the stack of parent pointers in |trav|
and updates its generation number. */
static void
trav_refresh ( struct avl_traverser *trav )
{
	trav->avl_generation = trav->avl_table->avl_generation;

	if ( trav->avl_node != NULL )
	{
		avl_comparison_func *cmp = trav->avl_table->avl_compare;
		void *param = trav->avl_table->avl_param;
		struct avl_node *node = trav->avl_node;
		struct avl_node *i;

		trav->avl_height = 0;
		for ( i = trav->avl_table->avl_root; i != node; )
		{
			trav->avl_stack[trav->avl_height++] = i;
			i = i->avl_link[cmp ( node->avl_data, i->avl_data, param ) > 0];
		}
	}
}

/* Initializes |trav| for use with |tree|
and selects the null node. */
void
avl_t_init ( struct avl_traverser *trav, struct avl_table *tree )
{
	trav->avl_table = tree;
	trav->avl_node = NULL;
	trav->avl_height = 0;
	trav->avl_generation = tree->avl_generation;
}

/* Initializes |trav| for |tree|
and selects and returns a pointer to its least-valued item.
Returns |NULL| if |tree| contains no nodes. */
void *
avl_t_first ( struct avl_traverser *trav, struct avl_table *tree )
{
	struct avl_node *x;

	trav->avl_table = tree;
	trav->avl_height = 0;
	trav->avl_generation = tree->avl_generation;

	x = tree->avl_root;
	if ( x != NULL )
		while ( x->avl_link[0] != NULL )
		{
			trav->avl_stack[trav->avl_height++] = x;
			x = x->avl_link[0];
		}
	trav->avl_node = x;

	return x != NULL ? x->avl_data : NULL;
}

/* Initializes |trav| for |tree|
and selects and returns a pointer to its greatest-valued item.
Returns |NULL| if |tree| contains no nodes. */
void *
avl_t_last ( struct avl_traverser *trav, struct avl_table *tree )
{
	struct avl_node *x;

	trav->avl_table = tree;
	trav->avl_height = 0;
	trav->avl_generation = tree->avl_generation;

	x = tree->avl_root;
	if ( x != NULL )
		while ( x->avl_link[1] != NULL )
		{
			trav->avl_stack[trav->avl_height++] = x;
			x = x->avl_link[1];
		}
	trav->avl_node = x;

	return x != NULL ? x->avl_data : NULL;
}

/* Searches for |item| in |tree|.
If found, initializes |trav| to the item found and returns the item
as well.
If there is no matching item, initializes |trav| to the null item
and returns |NULL|. */
void *
avl_t_find ( struct avl_traverser *trav, struct avl_table *tree, void *item )
{
	struct avl_node *p, *q;

	trav->avl_table = tree;
	trav->avl_height = 0;
	trav->avl_generation = tree->avl_generation;
	for ( p = tree->avl_root; p != NULL; p = q )
	{
		int cmp = tree->avl_compare ( item, p->avl_data, tree->avl_param );

		if ( cmp < 0 )
			q = p->avl_link[0];
		else if ( cmp > 0 )
			q = p->avl_link[1];
		else /* |cmp == 0| */
		{
			trav->avl_node = p;
			return p->avl_data;
		}

		trav->avl_stack[trav->avl_height++] = p;
	}

	trav->avl_height = 0;
	trav->avl_node = NULL;
	return NULL;
}

/* Attempts to insert |item| into |tree|.
If |item| is inserted successfully, it is returned and |trav| is
initialized to its location.
If a duplicate is found, it is returned and |trav| is initialized to
its location.  No replacement of the item occurs.
If a memory allocation failure occurs, |NULL| is returned and |trav|
is initialized to the null item. */
void *
avl_t_insert ( struct avl_traverser *trav, struct avl_table *tree, void *item )
{
	void **p;

	p = avl_probe ( tree, item );
	if ( p != NULL )
	{
		trav->avl_table = tree;
		trav->avl_node =
			((struct avl_node *)
			((char *) p - offsetof ( struct avl_node, avl_data )));
		trav->avl_generation = tree->avl_generation - 1;
		return *p;
	} 	else
	{
		avl_t_init ( trav, tree );
		return NULL;
	}
}

/* Initializes |trav| to have the same current node as |src|. */
void *
avl_t_copy ( struct avl_traverser *trav, const struct avl_traverser *src )
{
	if ( trav != src )
	{
		trav->avl_table = src->avl_table;
		trav->avl_node = src->avl_node;
		trav->avl_generation = src->avl_generation;
		if ( trav->avl_generation == trav->avl_table->avl_generation )
		{
			trav->avl_height = src->avl_height;
			memcpy ( trav->avl_stack, (const void *) src->avl_stack, sizeof( *trav->avl_stack) * trav->avl_height );
		}
	}

	return trav->avl_node != NULL ? trav->avl_node->avl_data : NULL;
}

/* Returns the next data item in inorder
within the tree being traversed with |trav|,
or if there are no more data items returns |NULL|. */
void *
avl_t_next ( struct avl_traverser *trav )
{
	struct avl_node *x;

	if ( trav->avl_generation != trav->avl_table->avl_generation )
		trav_refresh ( trav );

	x = trav->avl_node;
	if ( x == NULL )
	{
		return avl_t_first ( trav, trav->avl_table );
	} 	else if ( x->avl_link[1] != NULL )
	{
		trav->avl_stack[trav->avl_height++] = x;
		x = x->avl_link[1];

		while ( x->avl_link[0] != NULL )
		{
			trav->avl_stack[trav->avl_height++] = x;
			x = x->avl_link[0];
		}
	} 	else
	{
		struct avl_node *y;

		do
		{
			if ( trav->avl_height == 0 )
			{
				trav->avl_node = NULL;
				return NULL;
			}

			y = x;
			x = trav->avl_stack[--trav->avl_height];
		} 		while ( y == x->avl_link[1] );
	}
	trav->avl_node = x;

	return x->avl_data;
}

/* Returns the previous data item in inorder
within the tree being traversed with |trav|,
or if there are no more data items returns |NULL|. */
void *
avl_t_prev ( struct avl_traverser *trav )
{
	struct avl_node *x;

	if ( trav->avl_generation != trav->avl_table->avl_generation )
		trav_refresh ( trav );

	x = trav->avl_node;
	if ( x == NULL )
	{
		return avl_t_last ( trav, trav->avl_table );
	} 	else if ( x->avl_link[0] != NULL )
	{
		trav->avl_stack[trav->avl_height++] = x;
		x = x->avl_link[0];

		while ( x->avl_link[1] != NULL )
		{
			trav->avl_stack[trav->avl_height++] = x;
			x = x->avl_link[1];
		}
	} 	else
	{
		struct avl_node *y;

		do
		{
			if ( trav->avl_height == 0 )
			{
				trav->avl_node = NULL;
				return NULL;
			}

			y = x;
			x = trav->avl_stack[--trav->avl_height];
		} 		while ( y == x->avl_link[0] );
	}
	trav->avl_node = x;

	return x->avl_data;
}

/* Returns |trav|'s current item. */
void *
avl_t_cur ( struct avl_traverser *trav )
{
	return trav->avl_node != NULL ? trav->avl_node->avl_data : NULL;
}

/* Replaces the current item in |trav| by |new| and returns the item replaced.
|trav| must not have the null item selected.
The new item must not upset the ordering of the tree. */
void *
avl_t_replace ( struct avl_traverser *trav, void *new )
{
	void *old;

	old = trav->avl_node->avl_data;
	trav->avl_node->avl_data = new;
	return old;
}

static void
copy_error_recovery ( struct avl_node **stack, int height,
					 struct avl_table *new, avl_item_func *destroy )
{
	for ( ; height > 2; height -= 2 )
		stack[height - 1]->avl_link[1] = NULL;
	avl_destroy ( new, destroy );
}

/* Copies |org| to a newly created tree, which is returned.
If |copy != NULL|, each data item in |org| is first passed to |copy|,
and the return values are inserted into the tree,
with |NULL| return values taken as indications of failure.
On failure, destroys the partially created new tree,
applying |destroy|, if non-null, to each item in the new tree so far,
and returns |NULL|.
If |allocator != NULL|, it is used for allocation in the new tree.
Otherwise, the same allocator used for |org| is used. */
struct avl_table *
	avl_copy ( const struct avl_table *org, avl_copy_func *copy,
			  avl_item_func *destroy, struct libavl_allocator *allocator )
{
	struct avl_node *stack[2 * (AVL_MAX_HEIGHT + 1)];
	int height = 0;

	struct avl_table *new;
	const struct avl_node *x;
	struct avl_node *y;

	new = avl_create ( org->avl_compare, org->avl_param,
		allocator != NULL ? allocator : org->avl_alloc );
	if ( new == NULL )
		return NULL;
	new->avl_count = org->avl_count;
	if ( new->avl_count == 0 )
		return new;

	x = (const struct avl_node *) &org->avl_root;
	y = (struct avl_node *) &new->avl_root;
	for ( ;;)
	{
		while ( x->avl_link[0] != NULL )
		{
			y->avl_link[0] = avl_node_allocate ();

			stack[height++] = (struct avl_node *) x;
			stack[height++] = y;
			x = x->avl_link[0];
			y = y->avl_link[0];
		}
		y->avl_link[0] = NULL;

		for ( ;;)
		{
			y->avl_balance = x->avl_balance;
			if ( copy == NULL )
				y->avl_data = x->avl_data;
			else
			{
				y->avl_data = copy ( x->avl_data, org->avl_param );
				if ( y->avl_data == NULL )
				{
					y->avl_link[1] = NULL;
					copy_error_recovery ( stack, height, new, destroy );
					return NULL;
				}
			}

			if ( x->avl_link[1] != NULL )
			{
				y->avl_link[1] = avl_node_allocate ();
				if ( y->avl_link[1] == NULL )
				{
					copy_error_recovery ( stack, height, new, destroy );
					return NULL;
				}

				x = x->avl_link[1];
				y = y->avl_link[1];
				break;
			} 			else
				y->avl_link[1] = NULL;

			if ( height <= 2 )
				return new;

			y = stack[--height];
			x = stack[--height];
		}
	}
}

/* Frees storage allocated for |tree|.
If |destroy != NULL|, applies it to each data item in inorder. */
void avl_destroy ( struct avl_table *tree, avl_item_func *destroy )
{
	struct avl_node *p, *q;

	for ( p = tree->avl_root; p != NULL; p = q )
	{
		if ( p->avl_link[0] == NULL )
		{
			q = p->avl_link[1];
			if ( destroy != NULL && p->avl_data != NULL )
				destroy ( p->avl_data, tree->avl_param );
			free ( p );
		} else
		{
			q = p->avl_link[0];
			p->avl_link[0] = q->avl_link[1];
			q->avl_link[1] = p;
		}
	}
	free ( tree );
}
