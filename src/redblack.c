/*
 * Copyright 2003,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "redblack.h"

#ifdef PROTO
typedef struct RBtree RBtree;
#endif

/* --- Red/Black tree support routines --- */

#define	RED	1
#define	BLACK	2

typedef struct RBnode RBnode;

struct RBtree {
	RBnode	*root;
	int	datasize;
	int	(*compare)(void *a, void *b);
};

struct RBnode {
	RBnode	*left, *right;
	RBnode	*parent;
	int	color;
	char	data[0];
};

/*
 * Create a new red/black tree structure.  It stores an arbitrary block
 * of data of a fixed size (size).  The tree is balanced with a generic
 * comparison function that is passed.
 */
RBtree *
RBtree_new(int size, int (*compare)(void *a, void *b))
{
	RBtree	*ret = malloc(sizeof(RBtree));

	ret->root = 0;
	ret->datasize = size;
	ret->compare = compare;

	return (ret);
}

private void
freenode(RBnode *n)
{
	if (n->left) freenode(n->left);
	if (n->right) freenode(n->right);
	free(n);
}

/*
 * Free a balanced redblack tree.
 */
void
RBtree_free(RBtree *t)
{
	if (t->root) freenode(t->root);
	free(t);
}

/*
 * Return data block of first node in tree.  NULL if tree is empty.
 */
void *
RBtree_first(RBtree *t)
{
	RBnode	*x = t->root;

	unless (x) return (0);
	while (x->left) x = x->left;

	return (x->data);
}

/*
 * Return data block on last node in tree, or NULL if empty.
 */
void *
RBtree_last(RBtree *t)
{
	RBnode	*x = t->root;

	unless (x) return (0);
	while (x->right) x = x->right;

	return (x->data);
}

/*
 * Find a data block in the tree that compares equal to the
 * data block on the command line, or NULL if none match.
 */
void *
RBtree_find(RBtree *t, void *data)
{
	RBnode	*x = t->root;
	int	cmp;

	while (x) {
		cmp = t->compare(data, x->data);
		if (cmp == 0) return (x->data);
		if (cmp < 0) {
			x = x->left;
		} else {
			x = x->right;
		}
	}
	return (0);
}

/*
 * Give a data block that exists in the tree, return the next data
 * block in the tree in insertion order. Or NULL at end.
 */
void *
RBtree_next(RBtree *t, void *data)
{
	RBnode	*n = (RBnode *)((char *)data - sizeof(RBnode));
	RBnode	*p;

	if (n->right) {
		p = n->right;
		while (p->left) p = p->left;
	} else {
		p = n->parent;
		while (p && n == p->right) {
			n = p;
			p = p->parent;
		}
	}
	unless (p) return (0);
	return (p->data);
}

/*
 * Give a data block that exists in the tree, return the previous data
 * block in the tree in insertion order. Or NULL at end.
 */
void *
RBtree_prev(RBtree *t, void *data)
{
	RBnode	*n = (RBnode *)((char *)data - sizeof(RBnode));
	RBnode	*p;

	if (n->left) {
		p = n->left;
		while (p->right) p = p->right;
	} else {
		p = n->parent;
		while (p && n == p->left) {
			n = p;
			p = p->parent;
		}
	}
	unless (p) return (0);
	return (p->data);
}

private void
left_rotate(RBtree *t, RBnode *x)
{
	RBnode	*y;

	y = x->right;
	/* Turn y's left sub-tree into x's right sub-tree */
	x->right = y->left;
	if (y->left) y->left->parent = x;
	/* y's new parent was x's parent */
	y->parent = x->parent;
	/* Set the parent to point to y instead of x */
	/* First see whether we're at the root */
	if (!x->parent) {
		t->root = y;
	} else if (x == x->parent->left) {
		/* x was on the left of its parent */
		x->parent->left = y;
	} else {
		/* x must have been on the right */
		x->parent->right = y;
	}
	/* Finally, put x on y's left */
	y->left = x;
	x->parent = y;
}

private void
right_rotate(RBtree *t, RBnode *x)
{
	RBnode	*y;

	y = x->left;
	/* Turn y's right sub-tree into x's left sub-tree */
	x->left = y->right;
	if (y->right) y->right->parent = x;
	/* y's new parent was x's parent */
	y->parent = x->parent;
	/* Set the parent to point to y instead of x */
	/* First see whether we're at the root */
	if (!x->parent) {
		t->root = y;
	} else if (x == x->parent->right) {
		/* x was on the right of its parent */
		x->parent->right = y;
	} else {
		/* x must have been on the left */
		x->parent->left = y;
	}
	/* Finally, put x on y's right */
	y->right = x;
	x->parent = y;
}

/*
 * insert a new data block in tree
 */
void
RBtree_insert(RBtree *t, void *data)
{
	RBnode	*y;
	RBnode	*x, *n = 0;

	x = malloc(sizeof(RBnode) + t->datasize);
	memset(x, 0, sizeof(RBnode));
	memcpy(x->data, data, t->datasize);

	y = t->root;
	while (y) {
		n = y;
		if (t->compare(x->data, y->data) < 0) {
			y = y->left;
		} else {
			y = y->right;
		}
	}
	if (n) {
		/* Insert in the tree in the usual way */
		if (t->compare(x->data, n->data) < 0) {
			n->left = x;
		} else {
			n->right = x;
		}
		x->parent = n;
	} else {
		t->root = x;
	}

	/* Now restore the red-black property */
	x->color = RED;

	while (x->parent && x->parent->color == RED) {
		if (x->parent == x->parent->parent->left) {
			/* If x's parent is a left, y is x's right 'uncle' */
			y = x->parent->parent->right;
			if (y && y->color == RED) {
				/* case 1 - change the colors */
				x->parent->color = BLACK;
				y->color = BLACK;
				x->parent->parent->color = RED;
				/* Move x up the tree */
				x = x->parent->parent;
			} else {
				/* y is a black node */
				if (x == x->parent->right) {
					/* and x is to the right */
					/* case 2 - move x up and rotate */
					x = x->parent;
					left_rotate(t, x);
				}
				/* case 3 */
				x->parent->color = BLACK;
				x->parent->parent->color = RED;
				right_rotate(t, x->parent->parent);
			}
		} else {
			/* If x's parent is a right, y is x's left 'uncle' */
			y = x->parent->parent->left;
			if (y && y->color == RED) {
				/* case 1 - change the colors */
				x->parent->color = BLACK;
				y->color = BLACK;
				x->parent->parent->color = RED;
				/* Move x up the tree */
				x = x->parent->parent;
			} else {
				/* y is a black node */
				if (x == x->parent->left) {
					/* and x is to the right */
					/* case 2 - move x up and rotate */
					x = x->parent;
					right_rotate(t, x);
				}
				/* case 3 */
				x->parent->color = BLACK;
				x->parent->parent->color = RED;
				left_rotate(t, x->parent->parent);
			}
		}
	}
	/* Color the root black */
	t->root->color = BLACK;
}

/*
 * delete an existing data block in tree
 */
void
RBtree_delete(RBtree *t, void *data)
{
	RBnode	*z = (RBnode *)((char *)data - sizeof(RBnode));
	RBnode	*y = z;
	RBnode	*x = 0;
	RBnode	*x_parent = 0;
	int	tmp;

	/* z has at most one non-null child. y == z. */
	if (y->left == 0) {
		x = y->right;			/* x might be null. */
	} else {
		/* z has exactly one non-null child.  y == z. */
		if (y->right == 0) {
			x = y->left;		/* x is not null. */
		} else {
			/*
			 * z has two non-null children.  Set y to z's
			 * successor.  x might be null.
			 */
			y = y->right;
			while (y->left != 0) y = y->left;
			x = y->right;
		}
	}
	/* relink y in place of z.  y is z's successor */
	if (y != z) {
		z->left->parent = y;
		y->left = z->left;
		if (y != z->right) {
			x_parent = y->parent;
			if (x) x->parent = y->parent;
			y->parent->left = x;      /* y must be a left child */
			y->right = z->right;
			z->right->parent = y;
		} else {
			x_parent = y;
		}
		if (t->root == z) {
			t->root = y;
		} else if (z->parent->left == z) {
			z->parent->left = y;
		} else {
			z->parent->right = y;
		}
		y->parent = z->parent;
		tmp = y->color;
		y->color = z->color;
		z->color = tmp;
		y = z;
		// y now points to node to be actually deleted
	} else {                        // y == z
		x_parent = y->parent;
		if (x) x->parent = y->parent;
		if (t->root == z) {
			t->root = x;
		} else {
			if (z->parent->left == z) {
				z->parent->left = x;
			} else {
				z->parent->right = x;
			}
		}
	}
	if (y->color != RED) {
		while (x != t->root && (x == 0 || x->color == BLACK)) {
			if (x == x_parent->left) {
				RBnode	*w = x_parent->right;

				if (w->color == RED) {
					w->color = BLACK;
					x_parent->color = RED;
					left_rotate(t, x_parent);
					w = x_parent->right;
				}
				if ((w->left == 0 ||
					w->left->color == BLACK) &&
				    (w->right == 0 ||
					w->right->color == BLACK)) {
					w->color = RED;
					x = x_parent;
					x_parent = x_parent->parent;
				} else {
					if (w->right == 0 ||
					    w->right->color == BLACK) {
						if (w->left) {
							w->left->color = BLACK;
						}
						w->color = RED;
						right_rotate(t, w);
						w = x_parent->right;
					}
					w->color = x_parent->color;
					x_parent->color = BLACK;
					if (w->right) w->right->color = BLACK;
					left_rotate(t, x_parent);
					break;
				}
			} else {
				/*  same as above, with right <-> left. */
				RBnode *w = x_parent->left;

				if (w->color == RED) {
					w->color = BLACK;
					x_parent->color = RED;
					right_rotate(t, x_parent);
					w = x_parent->left;
				}
				if ((w->right == 0 ||
					w->right->color == BLACK) &&
				    (w->left == 0 ||
					w->left->color == BLACK)) {
					w->color = RED;
					x = x_parent;
					x_parent = x_parent->parent;
				} else {
					if (w->left == 0 ||
					    w->left->color == BLACK) {
						if (w->right) {
							w->right->color =
								BLACK;
						}
						w->color = RED;
						left_rotate(t, w);
						w = x_parent->left;
					}
					w->color = x_parent->color;
					x_parent->color = BLACK;
					if (w->left) w->left->color = BLACK;
					right_rotate(t, x_parent);
					break;
				}
			}
		}
		if (x) x->color = BLACK;
	}
	free(y);
}
