/*! \file triangulation.c
 * \author M. Sainlot
 * \date 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "triangulation.h"
#include "math_fn.h"
#include "print_fn.h"


/*----------------------------------------------------------------------------------*/
//                                  Triangulation functions
/*----------------------------------------------------------------------------------*/

Simplex* add_new_simp( Grid *grid, Vertex *v0, Vertex *v1, Vertex *v2 )
{
	Simplex *simp = grid->table_of_simp[grid->nb_simp];
	grid->nb_simp++;

	simp->sommet[0] = v0;
	simp->sommet[1] = v1;
	simp->sommet[2] = v2;

	compute_plan(simp);

	return simp;
}

Grid* create_grid( const int nb_pts, const int nb_simp, const PGMData *pic, const int ZUNIF )
{
	Grid *grid = (Grid *) malloc(sizeof(Grid));
	grid->table_of_simp = (Simplex **)malloc(nb_simp*sizeof(Simplex *));

	for (int i = 0; i < nb_simp; ++i)
	{
		grid->table_of_simp[i] = create_empty_simplex();
	}
	grid->nb_simp = 0;
	// 4 corners vertex
	Vertex *ul = create_vertex(0.0, 1.0, 0.0);
	Vertex *ur = create_vertex(1.0, 1.0, 0.0);
	Vertex *dr = create_vertex(1.0, 0.0, 0.0);
	Vertex *dl = create_vertex(0.0, 0.0, 0.0);

	// 2 init triangles
	Simplex *simp[2];
	simp[0] = add_new_simp(grid, ur, ul, dl);
	simp[1] = add_new_simp(grid, dl, dr, ur);
	simp[0]->voisin[1] = simp[1];
	simp[1]->voisin[1] = simp[0];

	// init vertex
	Vertex *new_vert;
	int i, j;
	for (i = 0; i < nb_pts; i++)
	{
		if (pic->matrix != NULL)
		{
			new_vert = create_vertex_img_based(pic);
		}
		else if (ZUNIF)
		{
			double x = randf();
			double y = randf();
			double z = randf();
			new_vert = create_vertex(x,y,z);
		}
		else
		{
			double x = randf();
			double y = randf();
			double xsc = (x*16)-8;
			double ysc = (y*16)-8;
			double z = sin(xsc*xsc+ysc*ysc) / (xsc*xsc+ysc*ysc);
			new_vert = create_vertex(x,y,(z+2)/8);			
		}
		for (j = 0; j < 2; j++)
		{
			if ( inside_simplex( simp[j], new_vert ) )
				break;
		}
		new_vert->zdist = compute_zdist( simp[j], new_vert );
		if ( simp[j]->candidates->length > 0 && is_superior_vertex( new_vert, simp[j]->candidates->root->links[STD][FWD]) )
			add_begin_dll( simp[j]->candidates, new_vert, STD );
		else
			add_end_dll( simp[j]->candidates, new_vert, STD );
	}

	
	grid->fdp = create_fdp( nb_simp );

	grid->candidates_to_redistribute = create_dll();

	for (int i = 0; i < NB_STK; i++)
	{
		grid->top_of_stack[i] = NULL;
		grid->stack_size[i] = 0;
		grid->stack_max_size[i] = 0;
	}

	insert_in_fdp(grid->fdp, simp[0]);
	insert_in_fdp(grid->fdp, simp[1]);

	return grid;
}

void redistribute_candidates( Grid *grid )
{
	int nb_candidates = grid->candidates_to_redistribute->length;
	Vertex *current_vert = grid->candidates_to_redistribute->root->links[STD][FWD];
	Vertex *next_vert = NULL;
	Simplex *current_simp = NULL;

	// redistributes candidates through all new triangles and update zdist
	for (int i = 0; i < nb_candidates; i++)
	{
		current_simp = grid->top_of_stack[NEW];
		while (1)
		{
			if ( inside_simplex( current_simp, current_vert ) ) break;
			current_simp = current_simp->next_stk[NEW];
		}

		current_vert->zdist = compute_zdist( current_simp, current_vert );
		next_vert = current_vert->links[STD][FWD];
		if ( current_simp->candidates->length > 0 && is_superior_vertex( current_vert, current_simp->candidates->root->links[STD][FWD]) )
			add_begin_dll( current_simp->candidates, current_vert, STD );
		else
			add_end_dll( current_simp->candidates, current_vert, STD );
		current_vert = next_vert;
	}
}

void stack( Grid *grid, Simplex *simp, int LNK )
{
	simp->next_stk[LNK] = grid->top_of_stack[LNK];
	grid->top_of_stack[LNK] = simp;
	grid->stack_size[LNK]++;
	if (grid->stack_size[LNK] > grid->stack_max_size[LNK])
		grid->stack_max_size[LNK] = grid->stack_size[LNK];
}

Simplex* unstack( Grid *grid, int LNK )
{
	Simplex *top = grid->top_of_stack[LNK];
	grid->top_of_stack[LNK] = top->next_stk[LNK];
	top->next_stk[LNK] = NULL;
	grid->stack_size[LNK]--;
	return top;
}

void split_in_3(Grid *grid, Simplex *simp, Vertex *vert)
{
	// define new vertices of new simplex
	Simplex *new_simp1 = add_new_simp(grid, vert, simp->sommet[1], simp->sommet[2]); 
	Simplex *new_simp2 = add_new_simp(grid, vert, simp->sommet[2], simp->sommet[0]);
	simp->sommet[2] = vert;

	// define new neighbors
	new_simp1->voisin[0] = simp->voisin[0];
	new_simp1->voisin[1] = new_simp2;
	new_simp1->voisin[2] = simp;

	new_simp2->voisin[0] = simp->voisin[1];
	new_simp2->voisin[1] = simp;
	new_simp2->voisin[2] = new_simp1;

	simp->voisin[0] = new_simp1;
	simp->voisin[1] = new_simp2;

	int ind_neighbor;
	if (new_simp1->voisin[0] !=NULL)
	{
		ind_neighbor = get_neighbor_index( simp, new_simp1->voisin[0]);
		new_simp1->voisin[0]->voisin[ind_neighbor] = new_simp1;
	}

	if (new_simp2->voisin[0] !=NULL)
	{
		ind_neighbor = get_neighbor_index( simp, new_simp2->voisin[0]);
		new_simp2->voisin[0]->voisin[ind_neighbor] = new_simp2;
	}

	compute_plan(simp);

	insert_in_fdp(grid->fdp, new_simp1);
	insert_in_fdp(grid->fdp, new_simp2);

	stack( grid, new_simp1, DEL );
	stack( grid, new_simp2, DEL );
	stack( grid, simp, DEL );
}

void flip(Grid *grid, Simplex *current, int ind_vert, Simplex *opp, int ind_opp)
{
	int ind_neighbor;

	current->voisin[ind_vert] = opp->voisin[(ind_opp+1)%3];
	if (opp->voisin[(ind_opp+1)%3] !=NULL)
	{
		ind_neighbor = get_neighbor_index( opp, opp->voisin[(ind_opp+1)%3]);
		opp->voisin[(ind_opp+1)%3]->voisin[ind_neighbor] = current;
	}
	
	opp->voisin[ind_opp] = current->voisin[(ind_vert+1)%3];
	if (current->voisin[(ind_vert+1)%3] != NULL)
	{
		ind_neighbor = get_neighbor_index( current, current->voisin[(ind_vert+1)%3] );
		current->voisin[(ind_vert+1)%3]->voisin[ind_neighbor] = opp;
	}

	current->voisin[(ind_vert+1)%3] = opp;
	opp->voisin[(ind_opp+1)%3] = current;

	current->sommet[(ind_vert+2)%3] = opp->sommet[ind_opp];
	opp->sommet[(ind_opp+2)%3] = current->sommet[ind_vert];

	compute_plan(current);
	compute_plan(opp);

	if (current->index_in_fdp <= 0)
		insert_in_fdp(grid->fdp, current);

	if (opp->index_in_fdp <= 0)
		insert_in_fdp(grid->fdp, opp);

	stack( grid, current, DEL );
	stack( grid, opp, DEL );
}

void delauney( Grid *grid )
{
	Simplex *simp =	grid->fdp->table[1] ;
	
	// Vertex to insert (first candidat)
	Vertex *vert = simp->candidates->root->links[STD][FWD];
	grid->nb_vertex_inserted++;

	vert->zdist = 0;

	rm_begin_dll(simp->candidates, STD);

	add_dll_end_dll(grid->candidates_to_redistribute, simp->candidates->root->links[STD][FWD], simp->candidates->root->links[STD][BWD], simp->candidates->length, STD );

	// reinit dll
	init_dll(simp->candidates, simp->candidates->root);

	split_in_3(grid, simp, vert);

	Simplex *current = NULL;
	Simplex *opp = NULL;
	int ind_vert;
	int ind_opp;

	while (grid->stack_size[DEL] > 0)
	{
		current = unstack(grid, DEL);
		ind_vert = get_vertex_index(vert, current);
		opp = current->voisin[ind_vert];

		if ( opp !=NULL && in_circle( opp->sommet[0], opp->sommet[1], opp->sommet[2], vert ) )
		{
			ind_opp = (get_vertex_index(current->sommet[(ind_vert + 1)%3], opp) + 1) %3 ;
			add_dll_end_dll(grid->candidates_to_redistribute, opp->candidates->root->links[STD][FWD], opp->candidates->root->links[STD][BWD], opp->candidates->length, STD );
			init_dll(opp->candidates, opp->candidates->root);

			flip(grid, current, ind_vert, opp, ind_opp);
		}
		else
		{
			stack( grid, current, NEW);
		}
	}

	redistribute_candidates(grid);

	int n = grid->stack_size[NEW];
	for (int i = 0; i < n; i++)
	{
		current = unstack(grid, NEW);
		if (current->index_in_fdp > 0)
		{
			if (is_empty(current) )
			{
				rm_of_fdp(grid->fdp, current);
			}
			else
			{
				up_heap( grid->fdp, current->index_in_fdp, current->index_in_fdp/2 );
				down_heap( grid->fdp, current->index_in_fdp*2, current->index_in_fdp );
			}
		}
	}

	// reinit
	init_dll(grid->candidates_to_redistribute, grid->candidates_to_redistribute->root);
}
