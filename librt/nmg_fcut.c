#define PLOT_BOTH_FACES	1
/*
 *			N M G _ F C U T . C
 *
 *  After two faces have been intersected, cut or join loops crossed
 *  by the line of intersection.  (Formerly nmg_comb.c)
 *
 *  The main external routine here is nmg_face_cutjoin().
 *
 *  The line of intersection ("ray") will divide the face into two sets
 *  of loops.
 *  No one loop may cross the ray after this routine is finished.
 *  (Current optimization may remove this property).
 *
 *  Intersection points of significance to the other face but not yet
 *  part of the current face's geometry are denoted by a vu on the ray
 *  list, which points to a loop of a single vertex.  These points
 *  need to be incorporated into the final face.
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* States of the state machine */
#define NMG_STATE_ERROR		0
#define NMG_STATE_OUT		1
#define NMG_STATE_ON_L		2
#define NMG_STATE_ON_R		3
#define NMG_STATE_ON_B		4
#define NMG_STATE_ON_N		5
#define NMG_STATE_IN		6
static CONST char *nmg_state_names[] = {
	"*ERROR*",
	"out",
	"on_L",
	"on_R",
	"on_both",		/* "hole" crack */
	"on_neither",		/* "non-hole" crack */
	"in",
	"TOOBIG"
};

#define NMG_E_ASSESSMENT_LEFT		0
#define NMG_E_ASSESSMENT_RIGHT		1
#define NMG_E_ASSESSMENT_ON_FORW	2
#define NMG_E_ASSESSMENT_ON_REV		3

#define NMG_V_ASSESSMENT_LONE		16
#define NMG_V_COMB(_p,_n)	(((_p)<<2)|(_n))

/* Extract previous and next assessments from combined version */
#define NMG_V_ASSESSMENT_PREV(_a)	(((_a)>>2)&3)
#define NMG_V_ASSESSMENT_NEXT(_a)	((_a)&3)

static CONST char *nmg_v_assessment_names[32] = {
	"Left,Left",
	"Left,Right",
	"Left,On_Forw",
	"Left,On_Rev",
	"Right,Left",
	"Right,Right",
	"Right,On_Forw",
	"Right,On_Rev",
	"On_Forw,Left",
	"On_Forw,Right",
	"On_Forw,On_Forw",
	"On_Forw,On_Rev",
	"On_Rev,Left",
	"On_Rev,Right",
	"On_Rev,On_Forw",
	"On_Rev,On_Rev",
	"LONE_V",		/* 16 */
	"?17",
	"?18",
	"?19",
	"?20",
	"?21",
	"?22",
	"?23",
	"?24",
	"?25",
	"?26",
	"?27",
	"?28",
	"?29",
	"?30",
	"?31"
};
#define NMG_LEFT_LEFT	NMG_V_COMB(NMG_E_ASSESSMENT_LEFT,NMG_E_ASSESSMENT_LEFT)
#define NMG_LEFT_RIGHT	NMG_V_COMB(NMG_E_ASSESSMENT_LEFT,NMG_E_ASSESSMENT_RIGHT)
#define NMG_LEFT_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_LEFT,NMG_E_ASSESSMENT_ON_FORW)
#define NMG_LEFT_ON_REV	NMG_V_COMB(NMG_E_ASSESSMENT_LEFT,NMG_E_ASSESSMENT_ON_REV)
#define NMG_RIGHT_LEFT	NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT,NMG_E_ASSESSMENT_LEFT)
#define NMG_RIGHT_RIGHT	NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT,NMG_E_ASSESSMENT_RIGHT)
#define NMG_RIGHT_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT,NMG_E_ASSESSMENT_ON_FORW)
#define NMG_RIGHT_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT,NMG_E_ASSESSMENT_ON_REV)
#define NMG_ON_FORW_LEFT NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW,NMG_E_ASSESSMENT_LEFT)
#define NMG_ON_FORW_RIGHT NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW,NMG_E_ASSESSMENT_RIGHT)
#define NMG_ON_FORW_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW,NMG_E_ASSESSMENT_ON_FORW)
#define NMG_ON_FORW_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW,NMG_E_ASSESSMENT_ON_REV)
#define NMG_ON_REV_LEFT	NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV,NMG_E_ASSESSMENT_LEFT)
#define NMG_ON_REV_RIGHT NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV,NMG_E_ASSESSMENT_RIGHT)
#define NMG_ON_REV_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV,NMG_E_ASSESSMENT_ON_FORW)
#define NMG_ON_REV_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV,NMG_E_ASSESSMENT_ON_REV)
#define NMG_LONE	NMG_V_ASSESSMENT_LONE

static CONST char *nmg_e_assessment_names[4] = {
	"LEFT",
	"RIGHT",
	"ON_FORW",
	"ON_REV"
};

/*
 *  Action entries for the state transition tables
 */
#define NMG_ACTION_ERROR		0
#define NMG_ACTION_NONE			1
#define NMG_ACTION_NONE_OPTIM		2
#define NMG_ACTION_VFY_EXT		3
#define NMG_ACTION_VFY_MULTI		4
#define NMG_ACTION_LONE_V_ESPLIT	5
#define NMG_ACTION_LONE_V_JAUNT		6
#define NMG_ACTION_CUTJOIN		7
static CONST char *action_names[] = {
	"*ERROR*",
	"NONE",
	"NONE_OPTIM",
	"VFY_EXT",
	"VFY_MULTI",
	"LONE_V_ESPLIT",
	"LONE_V_JAUNT",
	"CUTJOIN",
	"*TOOBIG*"
};

/* The "ray" here is the intersection line between two faces */
struct nmg_ray_state {
	/* XXX could use a magic number */
	struct vertexuse	**vu;		/* ptr to vu array */
	int			nvu;		/* len of vu[] */
	point_t			pt;		/* The ray */
	vect_t			dir;
	struct edge_g		*eg_p;		/* Edge geom of the ray */
	struct shell		*sA;
	struct shell		*sB;
	struct faceuse		*fu1;
	struct faceuse		*fu2;
	vect_t			left;		/* points left of ray, on face */
	int			state;		/* current (old) state */
	int			last_action;	/* last action taken */
	vect_t			ang_x_dir;	/* x axis for angle measure */
	vect_t			ang_y_dir;	/* y axis for angle measure */
	CONST struct rt_tol	*tol;
};

/*
 *			P T B L _ V S O R T
 *
 *  Sort list of hit points (vertexuse's) in fu1 on plane of fu2,
 *  by increasing distance, vertex ptr, and vu ptr.
 *  Eliminate duplications of vu at same distance.
 *  (Actually, a given vu should show up at exactly 1 distance!)
 *  The line of intersection is pt + t * dir.
 *
 *  For now, a bubble-sort is used, because the list should not have more
 *  than a few hundred entries on it.
 */
static void ptbl_vsort(b, fu1, fu2, pt, dir, mag, dist_tol)
struct nmg_ptbl *b;		/* table of vertexuses on intercept line */
struct faceuse	*fu1;		/* unused? */
struct faceuse	*fu2;		/* unused? */
point_t		pt;
vect_t		dir;
fastf_t		*mag;
fastf_t		dist_tol;
{
	register struct vertexuse	**vu;
	register int i, j;

	vu = (struct vertexuse **)b->buffer;

	if( rt_g.NMG_debug )  {
		/* Ensure that distance from points to ray is reasonable */
		for(i = 0 ; i < b->end ; ++i) {
			fastf_t		dist;

			NMG_CK_VERTEXUSE(vu[i]);
			dist = rt_dist_line_point( pt, dir, vu[i]->v_p->vg_p->coord );
			if( dist > dist_tol )  {
				rt_log("WARNING ptbl_vsort() vu=x%x point off line by %e %g*tol, tol=%e\n",
					vu[i], dist,
					dist/dist_tol, dist_tol);
				VPRINT("  vu", vu[i]->v_p->vg_p->coord);
				VPRINT("  pt", pt);
				VPRINT(" dir", dir);
			}
			if( dist > 100*dist_tol )  {
				rt_log("ERROR ptbl_vsort() vu=x%x point off line by %g > 100*dist_tol\n",
					vu[i], dist);
				rt_bomb("ptbl_vsort()\n");
			}
		}
	}

	/* check vertexuses and compute distance from start of line */
	for(i = 0 ; i < b->end ; ++i) {
		vect_t		vect;
		NMG_CK_VERTEXUSE(vu[i]);

		VSUB2(vect, vu[i]->v_p->vg_p->coord, pt);
		mag[i] = VDOT( vect, dir );

		/* Find previous vu's at "same" distance, within dist_tol */
		for( j = 0; j < i; j++ )  {
			register fastf_t	tmag;

			tmag = mag[i] - mag[j];
			if( tmag < -dist_tol )  continue;
			if( tmag > dist_tol )  continue;
			/* Nearly equal at same vertex */
			if( mag[i] != mag[j] &&
			    vu[i]->v_p == vu[j]->v_p )  {
	rt_log("ptbl_vsort: forcing vu=x%x & vu=x%x mag equal\n", vu[i], vu[j]);
				mag[j] = mag[i]; /* force equal */
			}
		}
	}

	for(i=0 ; i < b->end - 1 ; ++i) {
		for (j=i+1; j < b->end ; ++j) {

			if( mag[i] < mag[j] )  continue;
			if( mag[i] == mag[j] )  {
				if( vu[i]->v_p < vu[j]->v_p )  continue;
				if( vu[i]->v_p == vu[j]->v_p )  {
					if( vu[i] < vu[j] )  continue;
					if( vu[i] == vu[j] )  {
						int	last = b->end - 1;
						/* vu duplication, eliminate! */
	rt_log("ptbl_vsort: vu duplication eliminated\n");
						if( j >= last )  {
							/* j is last element */
							b->end--;
							break;
						}
						/* rewrite j with last element */
						vu[j] = vu[last];
						mag[j] = mag[last];
						b->end--;
						/* Repeat this index */
						j--;
						continue;
					}
					/* vu[i] > vu[j], fall through */
				}
				/* vu[i]->v_p > vu[j]->v_p, fall through */
			}
			/* mag[i] > mag[j] */

			/* exchange [i] and [j] */
			{
				register struct vertexuse *tvu;
				tvu = vu[i];
				vu[i] = vu[j];
				vu[j] = tvu;
			}

			{
				register fastf_t	tmag;
				tmag = mag[i];
				mag[i] = mag[j];
				mag[j] = tmag;
			}
		}
	}
}

/*
 *			N M G _ C K _ V U _ P T B L
 *
 *  As an automatic check for the intersector failing to find
 *  all intersections, check all the vertices on the intersection line.
 *  For each one, find all the other uses in this faceuse, and
 *  if they are not also listed on the line, they were overlooked.
 *
 *  This does not catch _all_ possible mistakes, but does catch some.
 */
int
nmg_ck_vu_ptbl( p, fu )
struct nmg_ptbl	*p;
struct faceuse	*fu;
{
	struct vertex		*v;
	struct vertexuse	*vu;
	struct vertexuse	*tvu;
	struct faceuse		*tfu;
	int			i;
	int			ret = 0;

	NMG_CK_PTBL(p);
	NMG_CK_FACEUSE(fu);

top:
	for( i=0; i < NMG_TBL_END(p); i++ )  {
		vu = (struct vertexuse *)NMG_TBL_GET(p, i);
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		tfu = nmg_find_fu_of_vu(vu);
		if( tfu != fu )  {
			rt_log("ERROR: vu=x%x v=x%x up_fu=x%x != arg_fu=x%x\n",
				vu, v, tfu, fu );
			rt_bomb("nmg_ck_vu_ptbl() intersect list is confused about which face it belongs to.\n");
		}
		for( RT_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
			NMG_CK_VERTEXUSE(tvu);
			if( tvu == vu )  continue;
			if( (tfu = nmg_find_fu_of_vu( tvu )) == (struct faceuse *)NULL )
				continue;
			if( tfu != fu )  continue;
			/* tvu is in fu.  Is tvu on the line? */
			if( nmg_tbl(p, TBL_LOC, &tvu->l.magic ) >= 0 )  continue;
			/* No, not on list */
			rt_log("ERROR: vu=x%x v=x%x is on isect line, tvu=x%x eu=x%x isn't.\n",
				vu, v, tvu, tvu->up.magic_p );
			/* XXX bomb here? */
/* XXXXXXXXXXXXXXXXXXXXXXXX Horrible temporizing measure */
			/* Another strategy:  add it in! */
			(void)nmg_tbl( p, TBL_INS, &tvu->l.magic );
			ret++;
			goto top;
		}
	}
	if(ret)rt_log("nmg_ck_vu_ptbl() ret=%d\n", ret);
	return ret;
}

/*
 *			N M G _ V U _ A N G L E _ M E A S U R E
 *
 *  Given a vertexuse from a loop which lies in a plane,
 *  compute the vector 'vec' from the previous vertex to this one.
 *  Using two perpendicular vectors (x_dir and y_dir) which both lie
 *  in the plane of the loop, return the angle (in radians) of 'vec'
 *  from x_dir, going CCW around the perpendicular x_dir CROSS y_dir.
 *
 *  x_dir is -ray_dir
 *  y_dir points right.
 *
 *  Returns -
 *	vec == x_dir returns 0,
 *	vec == y_dir returns pi/2,
 *	vec == -x_dir returns pi,
 *	vec == -y_dir returns 3*pi/2.
 *	0.0 if unable to compute 'vec'
 */
double
nmg_vu_angle_measure( vu, x_dir, y_dir, assessment, in )
struct vertexuse	*vu;
vect_t			x_dir;
vect_t			y_dir;
int			assessment;
int			in;	/* 1 = inbound edge, 0 = outbound edge */
{	
	struct loopuse	*lu;
	struct edgeuse	*this_eu;
	struct edgeuse	*prev_eu;
	vect_t		vec;
	fastf_t		ang;
	int		this_ass;

	NMG_CK_VERTEXUSE( vu );
	if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
		return 0;		/* Unable to compute 'vec' */
	}

	/*
	 *  For consistency, if entry edge is ON the ray,
	 *  force the angles to be exact, don't compute them.
	 */
	if( in )
		this_ass = NMG_V_ASSESSMENT_PREV( assessment );
	else
		this_ass = NMG_V_ASSESSMENT_NEXT( assessment );
	if( this_ass == NMG_E_ASSESSMENT_ON_FORW )  {
		if( in )  ang = 0;	/* zero angle */
		else	ang = rt_pi;	/* 180 degrees */
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("nmg_vu_angle_measure:  NMG_E_ASSESSMENT_ON_FORW, ang=%g\n", ang);
		return ang;
	}
	if( this_ass == NMG_E_ASSESSMENT_ON_REV )  {
		if( in )  ang = rt_pi;	/* 180 degrees */
		else	ang = 0;	/* zero angle */
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("nmg_vu_angle_measure:  NMG_E_ASSESSMENT_ON_REV, ang=%g\n", ang);
		return ang;
	}

	/*
	 *  Compute the angle
	 */
	lu = nmg_find_lu_of_vu(vu);
	NMG_CK_LOOPUSE(lu);
	this_eu = nmg_find_eu_with_vu_in_lu( lu, vu );
	prev_eu = this_eu;
	do {
		prev_eu = in ? RT_LIST_PLAST_CIRC( edgeuse, prev_eu ) :
			RT_LIST_PNEXT_CIRC( edgeuse, prev_eu );
		if( prev_eu == this_eu )  {
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("nmg_vu_angle_measure: prev eu is this eu, ang=0\n");
			return 0;	/* Unable to compute 'vec' */
		}
		/* Skip any edges that stay on this vertex */
	} while( prev_eu->vu_p->v_p == this_eu->vu_p->v_p );

	/* in==1 Get vec for inbound edge, but pointing away from vert. */
	/* in==0 "prev" is really next, so this is departing vec */
	VSUB2( vec, prev_eu->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord );

	ang = rt_angle_measure( vec, x_dir, y_dir );
	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("nmg_vu_angle_measure:  measured angle=%e\n", ang*rt_radtodeg);

	/*
	 *  Since the entry edge is not on the ray, ensure the
	 *  angles are not exactly 0 or pi.
	 */
#define RADIAN_TWEEK	1.0e-14	/* low bits of double prec., re: 6.28... */
	if( ang == 0 )  {
		if( this_ass == NMG_E_ASSESSMENT_RIGHT )  {
			ang = RADIAN_TWEEK;
		} else {
			/* Assuming NMG_E_ASSESSMENT_LEFT */
			ang = rt_twopi - RADIAN_TWEEK;
		}
	} else if( ang == rt_pi )  {
		if( this_ass == NMG_E_ASSESSMENT_RIGHT )  {
			ang = rt_pi - RADIAN_TWEEK;
		} else {
			ang = rt_pi + RADIAN_TWEEK;
		}
	}

	/*
	 *  Also, ensure computed angle and topological assessment agree
	 *  about which side of the ray this edge is on.
	 */
	if( ang > rt_pi )  {
		if( this_ass != NMG_E_ASSESSMENT_LEFT )  {
			rt_log("*** ERROR topology/geometry conflict, ang=%e, ass=%s\n",
				ang*rt_radtodeg,
				nmg_e_assessment_names[this_ass] );
		}
	} else if( ang < rt_pi )  {
		if( this_ass != NMG_E_ASSESSMENT_RIGHT )  {
			rt_log("*** ERROR topology/geometry conflict, ang=%e, ass=%s\n",
				ang*rt_radtodeg,
				nmg_e_assessment_names[this_ass] );
		}
	}
	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("  final ang=%g (%e), vec=(%g,%g,%g)\n", ang*rt_radtodeg, ang*rt_radtodeg, V3ARGS(vec) );
	return ang;
}

/*
 *			N M G _ A S S E S S _ E U
 *
 *  The current vertex (eu->vu_p) is on the line of intersection.
 *  Assess the indicated edge, to see if it lies on the line of
 *  intersection, or departs towards the left or right.
 *
 *  There is no need to look more than one edge forward or backward.
 *  Even if there are edges which loop around to the same vertex
 *  (with a different vertexuse), that (0-length) edge is ON the ray.
 */
int
nmg_assess_eu( eu, forw, rs, pos )
struct edgeuse		*eu;
int			forw;
struct nmg_ray_state	*rs;
int			pos;
{
	struct vertex		*v;
	struct vertex		*otherv = (struct vertex *)0;
	struct edgeuse		*othereu;
	vect_t			heading;
	int			ret;
	int			ret2;
	register int		i;

	NMG_CK_EDGEUSE(eu);
	RT_CK_TOL(rs->tol);
	v = eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
	othereu = eu;
	if( forw )  {
		othereu = RT_LIST_PNEXT_CIRC( edgeuse, othereu );
	} else {
		othereu = RT_LIST_PLAST_CIRC( edgeuse, othereu );
	}
	if( othereu == eu )  {
		/* Back to where search started */
		if(rt_g.NMG_debug) nmg_pr_eu(eu, NULL);
		rt_bomb("nmg_assess_eu() no edges leave the vertex!\n");
	}
	otherv = othereu->vu_p->v_p;
	if( otherv == v )  {
		/* Edge stays on this vertex -- can't tell if forw or rev! */
		if(rt_g.NMG_debug) nmg_pr_eu(eu, NULL);
		rt_bomb("nmg_assess_eu() edge runs from&to same vertex!\n");
	}

	/*  If the other vertex is mentioned anywhere on the ray's vu list,
	 *  then the edge is "on" the ray.
	 *  Match against vertex (rather than vertexuse) because cut/join
	 *  operations may have changed the particular vertexuse pointer.
	 */
	for( i=rs->nvu-1; i >= 0; i-- )  {
		if( rs->vu[i]->v_p != otherv )  continue;
		/* Edge is on the ray.  Which way does it go? */
/* XXX How to detect leaving the current vertex groups? */
		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log("eu ON ray: vu[%d]=x%x, other:vu[%d]=x%x\n",
				pos, rs->vu[pos], i, otherv );

		/* Compute edge vector, for purposes of orienting answer */
		if( forw )  {
			/* Edge goes from v to otherv */
			VSUB2( heading, otherv->vg_p->coord, v->vg_p->coord );
		} else {
			/* Edge goes from otherv to v */
			VSUB2( heading, v->vg_p->coord, otherv->vg_p->coord );
		}
		if( MAGSQ(heading) < SMALL_FASTF )  rt_bomb("nmg_assess_eu() null heading\n");
		if( MAGSQ(heading) < rs->tol->dist_sq )  rt_bomb("nmg_assess_eu() edge len < dist tol\n");
		if( VDOT( heading, rs->dir ) < 0 )  {
			ret = NMG_E_ASSESSMENT_ON_REV;
		} else {
			ret = NMG_E_ASSESSMENT_ON_FORW;
		}
		/*
		 *  There are 2 ways of determining the assessment:
		 *  edge direction DOT ray direction, above, and
		 *  comparing the subscripts of the two vertexuses
		 *  (which translates to comparing dists along ray).
		 */
		if( i > pos )  {
			if( forw )
				ret2 = NMG_E_ASSESSMENT_ON_FORW;
			else
				ret2 = NMG_E_ASSESSMENT_ON_REV;
		} else {
			/* i < pos  (They can't be equal) */
			if( forw )
				ret2 = NMG_E_ASSESSMENT_ON_REV;
			else
				ret2 = NMG_E_ASSESSMENT_ON_FORW;
		}
		if( ret != ret2 )  {
			rt_log("ret=%s, ret2=%s?\n",
				nmg_e_assessment_names[ret],
				nmg_e_assessment_names[ret2]);
			rt_bomb("nmg_assess_eu() assessment inconsistency\n");
		}
		goto out;
	}

	/*
	 *  Since other vertex does not lie anywhere on line of intersection,
	 *  the edge must lie to one side or the other of the ray.
	 *  Check vector from v to otherv against "left" vector.
	 */
	VSUB2( heading, otherv->vg_p->coord, v->vg_p->coord );
	if( MAGSQ(heading) < SMALL_FASTF )  rt_bomb("nmg_assess_eu() null heading 2\n");
	if( VDOT( heading, rs->left ) < 0 )  {
		ret = NMG_E_ASSESSMENT_RIGHT;
	} else {
		ret = NMG_E_ASSESSMENT_LEFT;
	}
out:
	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("nmg_assess_eu(x%x, fw=%d, pos=%d) v=x%x otherv=x%x: %s\n",
			eu, forw, pos, v, otherv,
			nmg_e_assessment_names[ret] );
		rt_log(" v(%g, %g, %g) other(%g, %g, %g)\n",
			V3ARGS(v->vg_p->coord), V3ARGS(otherv->vg_p->coord) );
		rt_log(" rs->left(%g, %g, %g) heading(%g, %g, %g)\n",
			V3ARGS(rs->left), V3ARGS(heading) );
	}
	return ret;
}

/*
 *			N M G _ A S S E S S _ V U
 */
int
nmg_assess_vu( rs, pos )
struct nmg_ray_state	*rs;
int			pos;
{
	struct vertexuse	*vu;
	struct loopuse	*lu;
	struct edgeuse	*this_eu;
	int		next_ass;
	int		prev_ass;
	int		ass;

	vu = rs->vu[pos];
	NMG_CK_VERTEXUSE( vu );
	if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
		return NMG_V_ASSESSMENT_LONE;
	}
	if( (lu = nmg_find_lu_of_vu(vu)) == (struct loopuse *)0 )
		rt_bomb("nmg_assess_vu: no lu\n");
	this_eu = nmg_find_eu_with_vu_in_lu( lu, vu );
	prev_ass = nmg_assess_eu( this_eu, 0, rs, pos );
	next_ass = nmg_assess_eu( this_eu, 1, rs, pos );
	ass = NMG_V_COMB( prev_ass, next_ass );

	/*
	 *  If the vu assessment is
	 *  NMG_ON_REV_ON_FORW or NMG_ON_FORW_ON_REV,
	 *  ensure that other end of both eu's is same vertex.
	 *  If not, it's an intersector error, and will confuse our caller.
	 */
	if( ass==NMG_ON_REV_ON_FORW || ass==NMG_ON_FORW_ON_REV )  {
		struct edgeuse	*prev;
		struct edgeuse	*next;
		prev = RT_LIST_PLAST_CIRC( edgeuse, this_eu );
		next = RT_LIST_PNEXT_CIRC( edgeuse, this_eu );
		if( prev->vu_p->v_p != next->vu_p->v_p )  {
			rt_log("nmg_assess_vu() %s, v=x%x, prev_v=x%x, next_v=x%x\n",
				nmg_v_assessment_names[ass],
				this_eu->vu_p->v_p,
				prev->vu_p->v_p, next->vu_p->v_p );
			rt_log("nmg_assess_vu() ON/ON edgeuse ends on different vertices.\n");
			VPRINT("vu  ", this_eu->vu_p->v_p->vg_p->coord);
			VPRINT("prev", prev->vu_p->v_p->vg_p->coord);
			VPRINT("next", next->vu_p->v_p->vg_p->coord);
/* See how far off the line they are */
rt_log("vu dist=%e, next dist=%e, tol=%e\n",
rt_dist_line_point( rs->pt, rs->dir, this_eu->vu_p->v_p->vg_p->coord ),
rt_dist_line_point( rs->pt, rs->dir, prev->vu_p->v_p->vg_p->coord ),
rs->tol->dist );
			if( nmg_break_long_edges( nmg_find_s_of_eu(this_eu), rs->tol ) > 0 )
				rt_log("\tnmg_break_long_edges succeeded\n");
			rt_bomb("nmg_assess_vu() ON/ON edgeuse ends on different vertices.\n");
		}
	}
	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("nmg_assess_vu() vu[%d]=x%x, v=x%x: %s\n",
			pos, vu, vu->v_p, nmg_v_assessment_names[ass] );
	}
	return ass;
}

struct nmg_vu_stuff {
	struct vertexuse	*vu;
	int			loop_index;
	struct nmg_loop_stuff	*lsp;
	fastf_t			in_vu_angle;
	fastf_t			out_vu_angle;
	fastf_t			min_vu_dot;
	fastf_t			lo_ang;		/* small if RIGHT, large if LEFT */
	fastf_t			hi_ang;
	int			seq;		/* seq # after lsp->min_vu */
	int			wedge_class;	/* WEDGE_LEFT, etc */
};
struct nmg_loop_stuff {
	struct loopuse		*lu;
	fastf_t			min_dot;
	struct vertexuse	*min_vu;
	int			n_vu_in_loop;	/* # ray vu's in this loop */
};

#define WEDGE_LEFT	0
#define WEDGE_CROSS	1
#define WEDGE_RIGHT	2
#define WEDGE_ON	3
#define WEDGECLASS2STR(_cl)	nmg_wedgeclass_string[(_cl)]
static CONST char *nmg_wedgeclass_string[] = {
	"LEFT",
	"CROSS",
	"RIGHT",
	"ON",
	"???"
};
/*
 *			N M G _ W E D G E _ C L A S S
 *
 *  0 degrees is to the rear (ON_REV), 90 degrees is to the RIGHT,
 *  180 is ON_FORW, 270 is to the LEFT.
 *  Determine if the given wedge is entirely to the left or right of
 *  the ray, or if it crosses.
 *
 *  "halfway X" (ha, hb) have these properties:
 *	< 0	( ==> X < 180 ) RIGHT
 *	> 0	( ==> X > 180 )	LEFT
 *	==0	( ==> X == 180 ) ON_FORW
 *
 *  Returns -
 *	WEDGE_LEFT
 *	WEDGE_CROSSING
 *	WEDGE_RIGHT
 *	WEDGE_ON
 */
int
nmg_wedge_class(ass, a,b)
int	ass;			/* assessment of two edges forming wedge */
double	a;
double	b;
{
	double	ha, hb;
	register int	ret;

	ha = a - 180;
	hb = b - 180;

	if( ass == NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_ON_FORW) )  {
		ret = WEDGE_ON;
		goto out;
	}
	if( ass == NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV, NMG_E_ASSESSMENT_ON_REV) )  {
		ret = WEDGE_ON;
		goto out;
	}

	if( NEAR_ZERO( ha, .01 ) )  {
		/* A is on the ray */
		if( NEAR_ZERO( hb, .01 ) )  {
			ret = WEDGE_CROSS;
			goto out;
		}
		if( hb < 0 )  {
			ret = WEDGE_RIGHT;
			goto out;
		}
		ret = WEDGE_LEFT;
		goto out;
	}
	if( ha < 0 )  {
		/* A is to the right */
		if( hb <= 0 )  {
			ret = WEDGE_RIGHT;
			goto out;
		}
		ret = WEDGE_CROSS;
		goto out;
	}
	/* ha is > 0, A is to the left */
	if( NEAR_ZERO( hb, .01 ) )  {
		/* A is left, B is ON_FORW (180) */
		ret = WEDGE_LEFT;
		goto out;
	}
	if( hb >= 0 )  {
		/* A is left, B is LEFT */
		ret = WEDGE_LEFT;
		goto out;
	}
	/* A is left, B is RIGHT */
	ret = WEDGE_CROSS;
out:
	if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
		rt_log("nmg_wedge_class(%g,%g) = %s\n",
			a, b, WEDGECLASS2STR(ret) );
	}
	return ret;
}

static CONST char *nmg_wedge2_string[] = {
	"WEDGE2_OVERLAP",
	"WEDGE2_NO_OVERLAP",
	"WEDGE2_AB_IN_CD",
	"WEDGE2_CD_IN_AB",
	"WEDGE2_IDENTICAL",
	"WEDGE2_TOUCH_AT_BC",
	"WEDGE2_TOUCH_AT_DA",
	"WEDGE2_???"
};
#define WEDGE2_TO_STRING(_class2)	(nmg_wedge2_string[(_class2)+2])

#define WEDGE2_OVERLAP		-2
#define WEDGE2_NO_OVERLAP	-1
#define WEDGE2_AB_IN_CD		0
#define WEDGE2_CD_IN_AB		1
#define WEDGE2_IDENTICAL	2
#define WEDGE2_TOUCH_AT_BC	3
#define WEDGE2_TOUCH_AT_DA	4

/*
 *			N M G _ C O M P A R E _ 2 _W E D G E S
 *
 *  Returns -
 *	WEDGE2_OVERLAP		AB partially overlaps CD (error)
 *	WEDGE2_NO_OVERLAP	AB does not overlap CD
 *	WEDGE2_AB_IN_CD		AB is inside CD
 *	WEDGE2_CD_IN_AB		CD is inside AB
 *	WEDGE2_IDENTICAL	AB == CD
 *	WEDGE2_TOUCH_AT_BC	AB touches CD at BC, but does not overlap
 *	WEDGE2_TOUCH_AT_DA	CD touches AB at DA, but does not overlap
 */
static int
nmg_compare_2_wedges( a, b, c, d )
double	a,b,c,d;
{
	double	t;
	int	a_in_cd = 0;
	int	b_in_cd = 0;
	int	c_in_ab = 0;
	int	d_in_ab = 0;
	int	a_eq_b = 0;
	int	a_eq_c = 0;
	int	a_eq_d = 0;
	int	b_eq_c = 0;
	int	b_eq_d = 0;
	int	c_eq_d = 0;
	int	ret;

#define WEDGE_ANG_TOL	0.001	/* XXX Angular tolerance, in degrees */
#define	ANG_SMASH(_a)	{\
	if( _a <= WEDGE_ANG_TOL )  _a = 0; \
	else if( NEAR_ZERO( _a - 180, WEDGE_ANG_TOL ) )  _a = 180; \
	else if( _a >= 360 - WEDGE_ANG_TOL )  _a = 360; }

	ANG_SMASH(a);
	ANG_SMASH(b);
	ANG_SMASH(c);
	ANG_SMASH(d);

	/* Ensure A < B */
	if( a > b )  {
		t = a;
		a = b;
		b = t;
	}
	/* Ensure that C < D */
	if( c > d )  {
		t = c;
		c = d;
		d = t;
	}

	if( NEAR_ZERO( a-b, WEDGE_ANG_TOL ) )  a_eq_b = 1;
	if( NEAR_ZERO( a-c, WEDGE_ANG_TOL ) )  a_eq_c = 1;
	if( NEAR_ZERO( a-d, WEDGE_ANG_TOL ) )  a_eq_d = 1;
	if( NEAR_ZERO( b-c, WEDGE_ANG_TOL ) )  b_eq_c = 1;
	if( NEAR_ZERO( b-d, WEDGE_ANG_TOL ) )  b_eq_d = 1;
	if( NEAR_ZERO( c-d, WEDGE_ANG_TOL ) )  c_eq_d = 1;

	/*
	 *  Test for TOUCHing wedges must come before INside test,
	 *  so that zero-angle wedges that touch a non-zero angle wedge,
	 *  will be properly recognized.  e.g. AB=(0,0) CD=(0,180).
	 */
	if( b_eq_c )  {
		/* Wedges touch along B-C junction */
		if( a_eq_d )
			ret = WEDGE2_IDENTICAL;	/* two zero-angle wedges */
		else
			ret = WEDGE2_TOUCH_AT_BC;
		goto out;
	}

	if( a_eq_d )  {
		/* We know c <= d, d==a, a <= b */
		if( b_eq_c )  {
			ret = WEDGE2_IDENTICAL;
			goto out;
		}
		if( a_eq_b )  {
			ret = WEDGE2_AB_IN_CD;
		} else if( c_eq_d )  {
			ret = WEDGE2_CD_IN_AB;
		} else {
			ret = WEDGE2_TOUCH_AT_DA;
		}
		goto out;
	}

	if( a_eq_c )  {
		if( b_eq_d )  {
			ret = WEDGE2_IDENTICAL;
			goto out;
		}
		/* We already know that A <= B, from sort above */
		if( b < d )  ret = WEDGE2_AB_IN_CD;
		else  ret = WEDGE2_CD_IN_AB;
		goto out;
	}

	if( b_eq_d )  {
		/* a != c, because of previous IF statement */
		if( a < c )  ret = WEDGE2_CD_IN_AB;
		else  ret = WEDGE2_AB_IN_CD;
		goto out;
	}

	/* See if c < a,b < d */
	if( c <= a && a <= d )  a_in_cd = 1;
	if( c <= b && b <= d )  b_in_cd = 1;
	/* See if a < c,d < b */
	if( a < c && c < b )  c_in_ab = 1;
	if( a < d && d < b )  d_in_ab = 1;

	if( a_in_cd && b_in_cd )  {
		if( c_in_ab || d_in_ab )  {
			ret = WEDGE2_OVERLAP;	/* ERROR */
			goto out;
		}
		ret = WEDGE2_AB_IN_CD;
		goto out;
	}
	if( c_in_ab && d_in_ab )  {
		if( a_in_cd || b_in_cd )  {
			ret = WEDGE2_OVERLAP;	/* ERROR */
			goto out;
		}
		ret = WEDGE2_CD_IN_AB;
		goto out;
	}
	if( a_in_cd + b_in_cd + c_in_ab + d_in_ab <= 0 )  {
		ret = WEDGE2_NO_OVERLAP;
		goto out;
	}
	ret = WEDGE2_OVERLAP;			/* ERROR */
out:
	if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
		rt_log(" a_in_cd=%d, b_in_cd=%d, c_in_ab=%d, d_in_ab=%d\n",
			a_in_cd, b_in_cd, c_in_ab, d_in_ab );
		rt_log("nmg_compare_2_wedges(%g,%g, %g,%g) = %d %s\n",
			a, b, c, d, ret, WEDGE2_TO_STRING(ret) );
	}
	if(ret <= WEDGE2_OVERLAP )  {
		rt_log("nmg_compare_2_wedges(%g,%g, %g,%g) ERROR!\n", a, b, c, d);
		rt_bomb("nmg_compare_2_wedges() ERROR\n");
	}
	return ret;
}

/*
 *			N M G _ F I N D _ V U _ I N _ W E D G E
 *
 *  Find the VU which is inside (or on) the given wedge,
 *  fitting as tightly to the given wedge as possible,
 *  and with the lowest value of lo_ang possible.
 *  XXX how to do tie-breaking for the two coincident ones where
 *  XXX two loops come together.
 *
 *  lo_ang < hi_ang on RIGHT side of intersection line
 *  lo_ang > hi_ang on LEFT side of intersection line
 *
 *  There are three wedges involved here:
 *	the original one, from lo to hi,
 *	the current best "candidate" so far,
 *	and "this", the current one being considered.
 */
static int
nmg_find_vu_in_wedge( vs, start, end, lo_ang, hi_ang, wclass, skip_array )
struct nmg_vu_stuff	*vs;
int	start;		/* vu index of coincident range */
int	end;
double	lo_ang;
double	hi_ang;
int	wclass;
int	skip_array[];
{
	register int	i;
	double	cand_lo;
	double	cand_hi;
	int	candidate;

	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("nmg_find_vu_in_wedge(start=%d,end=%d, lo=%g, hi=%g) START\n",
			start, end, lo_ang, hi_ang);

	candidate = -1;
	cand_lo = lo_ang;
	cand_hi = hi_ang;

	/* Consider all the candidates */
	for( i=start; i < end; i++ )  {
		int	this_wrt_orig;
		int	this_wrt_cand;

		NMG_CK_VERTEXUSE( vs[i].vu );
		if( skip_array[i] )  {
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("Skipping index %d\n", i);
			continue;
		}

		/* Ignore wedges crossing, or on other side of line */
		if( vs[i].wedge_class != wclass && vs[i].wedge_class != WEDGE_ON )  {
			if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
				rt_log("Seeking wedge_class=%s, [%d] has wedge_class %s\n",
					WEDGECLASS2STR(wclass), i, WEDGECLASS2STR(vs[i].wedge_class) );
			}
			continue;
		}

		if( vs[i].wedge_class == WEDGE_ON )  {
			/* Ensure that comparisons will work */
			if( wclass == WEDGE_RIGHT )  {
				vs[i].lo_ang = 0;
				vs[i].hi_ang = 180;
			} else {
				vs[i].lo_ang = 360;
				vs[i].hi_ang = 180;
			}
		}

		this_wrt_orig = nmg_compare_2_wedges(
			vs[i].lo_ang, vs[i].hi_ang,
			lo_ang, hi_ang );
		switch( this_wrt_orig )  {
		case WEDGE2_AB_IN_CD:
			break;
		case WEDGE2_IDENTICAL:
			candidate = i;
			goto out;
		default:
			continue;	/* not inside wedge */
		}

		if( candidate < 0 ) {
			/* This wedge AB is inside original wedge.
			 * If candidate is -1, use AB as candidate.
			 */
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("Initial candidate %d selected\n", i);
			candidate = i;
			cand_lo = vs[i].lo_ang;
			cand_hi = vs[i].hi_ang;
			continue;
		}

		this_wrt_cand = nmg_compare_2_wedges(
			vs[i].lo_ang, vs[i].hi_ang,
			cand_lo, cand_hi );
		switch( this_wrt_cand )  {
		case WEDGE2_CD_IN_AB:
			/* This wedge AB contains candidate wedge CD, therefore
			 * this wedge is closer to original wedge */
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("This candidate %d is closer\n", i);
			candidate = i;
			cand_lo = vs[i].lo_ang;
			cand_hi = vs[i].hi_ang;
			break;
		case WEDGE2_NO_OVERLAP:
			/* No overlap, but both are inside.  Take lower angle */
			if( vs[i].lo_ang < cand_lo )  {
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("Taking lower angle %d\n", i);
				candidate = i;
				cand_lo = vs[i].lo_ang;
				cand_hi = vs[i].hi_ang;
			}
			break;
		default:
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("Continuing with search\n");
			continue;
		}
	}
out:
	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("nmg_find_vu_in_wedge(start=%d,end=%d, lo=%g, hi=%g) END candidate=%d\n",
			start, end, lo_ang, hi_ang,
			candidate);
	return candidate;	/* is -1 if none found */
}

/*
 *			N M G _ I S _ W E D G E _ B E F O R E _ C R O S S
 *
 *  Determine if the 'wedge' vu, which is either a LEFT or RIGHT wedge,
 *  should be processed before or after the 'cross' vu, which is a
 *  CROSS wedge.
 *
 *  Returns -
 *	1	if wedge should be processed before cross
 *	0	if cross should be processed before wedge
 */
static int
nmg_is_wedge_before_cross( wedge, cross )
CONST struct nmg_vu_stuff	*wedge;
CONST struct nmg_vu_stuff	*cross;
{
	int	class2;
	int	ret = -1;

	if( cross->wedge_class != WEDGE_CROSS || wedge->wedge_class == WEDGE_CROSS )
		rt_bomb("nmg_is_wedge_before_cross() bad input\n");

	/* LEFT/RIGHT Wedge is (AB), CROSS wedge is (CD) */
	class2 = nmg_compare_2_wedges( wedge->lo_ang, wedge->hi_ang,
		cross->lo_ang, cross->hi_ang );

	switch(class2)  {
	default:
		rt_log("nmg_is_wedge_before_cross() class2=%s\n",
			WEDGE2_TO_STRING(class2) );
		rt_bomb("nmg_is_wedge_before_cross(): bad wedge comparison\n");
	case WEDGE2_NO_OVERLAP:
		/* wedge is not inside cross, cross is not inside wedge. */
		/* Do wedge first */
		ret = 1;
		break;
	case WEDGE2_TOUCH_AT_BC:
		/* Should only happen when wedge is WEDGE_RIGHT */
		if( wedge->wedge_class != WEDGE_RIGHT )
			rt_bomb("WEDGE not RIGHT with WEDGE2_TOUCH_AT_BC?\n");
		ret = 1;
		break;
	case WEDGE2_TOUCH_AT_DA:
		/* Should only happen when wedge is WEDGE_LEFT */
		if( wedge->wedge_class != WEDGE_LEFT )
			rt_bomb("WEDGE not LEFT with WEDGE2_TOUCH_AT_DA?\n");
		ret = 1;
		break;
	case WEDGE2_AB_IN_CD:
		/* 'wedge' is inside the 'cross' wedge, do it second. */
		ret = 0;
		break;
	}
	if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
		rt_log("nmg_is_wedge_before_cross() class2=%s, ret=%d\n",
			WEDGE2_TO_STRING(class2), ret );
	}
	return ret;
}

/*
 *			N M G _ F A C E _ V U _ C O M P A R E
 *
 *  Support routine for nmg_face_coincident_vu_sort(), via qsort().
 *
 *  It is important to note that an edge on the LEFT side of the ray
 *  will have a "lo" angle which is numerically LARGER than the "hi" angle.
 *  However, all are measured in the usual units:
 *  0 = ON_REV, 90 = RIGHT, 180 = ON_FORW, 270 = LEFT.
 *
 *  Returns -
 *	-1	when A < B
 *	 0	when A == B
 *	+1	when A > B
 */
#define	A_LT_B		{ret = -1; goto out;}
#define AB_EQUAL	{ret = 0; goto out;}
#define A_GT_B		{ret = 1; goto out;}
static int
nmg_face_vu_compare( aa, bb )
#if __STDC__
 const void	*aa;
 const void	*bb;
#else
 CONST genptr_t	aa;
 CONST genptr_t	bb;
#endif
{
	register CONST struct nmg_vu_stuff *a = (CONST struct nmg_vu_stuff *)aa;
	register CONST struct nmg_vu_stuff *b = (CONST struct nmg_vu_stuff *)bb;
	register double	diff;
	int	lo_equal = 0;
	int	hi_equal = 0;
	register int	ret = 0;

	lo_equal = NEAR_ZERO( a->lo_ang - b->lo_ang, WEDGE_ANG_TOL );
	hi_equal = NEAR_ZERO( a->hi_ang - b->hi_ang, WEDGE_ANG_TOL );
	/* If both have the same assessment & angles match, => tie */
	if( a->wedge_class == b->wedge_class && lo_equal && hi_equal ) {
		/* Break the tie */
tie_break:
		if( a->loop_index == b->loop_index )  {
			/* Within a loop, sort by vu sequence number */
			if( a->seq < b->seq )  A_LT_B;
			if( a->seq == b->seq )  AB_EQUAL;
			A_GT_B;
		}
		/* Select smallest inbound angle */
		diff = a->in_vu_angle - b->in_vu_angle;
		if( NEAR_ZERO( diff, WEDGE_ANG_TOL ) )  {
			/* Gak, this really means trouble! */
			rt_log("nmg_face_vu_compare(): two loops (single vertex) have same in_vu_angle%g?\n",
				a->in_vu_angle);
			rt_bomb("nmg_face_vu_compare():  can't decide\n");
			AB_EQUAL;
		}
		if( diff < 0 )  A_LT_B;
		A_GT_B;
	}
	switch( a->wedge_class )  {
	case WEDGE_ON:
		switch( b->wedge_class )  {
		case WEDGE_ON:
			goto tie_break;
		default:
			rt_bomb("nmg_face_vu_compare() WEDGE_ON 1?\n");
		}
		break;
	case WEDGE_LEFT:
		switch( b->wedge_class )  {
		case WEDGE_LEFT:
			if( lo_equal )  {
				/* hi_equal case handled above */
				if( a->hi_ang < b->hi_ang ) A_LT_B;
				A_GT_B;
			}
			if( a->lo_ang > b->lo_ang )  A_LT_B;
			A_GT_B;
		case WEDGE_CROSS:
			/* A is LEFT, B is CROSS */
			if( nmg_is_wedge_before_cross( a, b ) )  A_LT_B;
			A_GT_B;
		case WEDGE_RIGHT:
			diff = 360 - a->lo_ang;/* CW version of left angle */
			if( b->lo_ang <= diff )  A_GT_B;
			A_LT_B;
		case WEDGE_ON:
			rt_bomb("nmg_face_vu_compare() WEDGE_ON 2?\n");
		}
	case WEDGE_CROSS:
		switch( b->wedge_class )  {
		case WEDGE_LEFT:
			/* A (AB) is CROSS, (CD) is LEFT */
			if( nmg_is_wedge_before_cross( b, a ) )  A_GT_B;
			A_LT_B;
		case WEDGE_CROSS:
			if( lo_equal )  {
				if( a->hi_ang > b->hi_ang )  A_LT_B;
				A_GT_B;
			}
			if( a->lo_ang < b->lo_ang )  A_LT_B;
			A_GT_B;
		case WEDGE_RIGHT:
			/* A is CROSS, B is RIGHT */
			if( nmg_is_wedge_before_cross( b, a ) )  A_GT_B;
			A_LT_B;
		case WEDGE_ON:
			rt_bomb("nmg_face_vu_compare() WEDGE_ON 3?\n");
		}
	case WEDGE_RIGHT:
		switch( b->wedge_class )  {
		case WEDGE_LEFT:
			diff = 360 - b->lo_ang;/* CW version of left angle */
			if( a->lo_ang <= diff )  A_LT_B;
			A_GT_B;
		case WEDGE_CROSS:
			/* A is RIGHT, B is CROSS */
			if( nmg_is_wedge_before_cross( a, b ) )  A_LT_B;
			A_GT_B;
		case WEDGE_RIGHT:
			if( lo_equal )  {
				if( a->hi_ang < b->hi_ang )  A_GT_B;
				A_LT_B;
			}
			if( a->lo_ang < b->lo_ang )  A_LT_B;
			A_GT_B;
		case WEDGE_ON:
			rt_bomb("nmg_face_vu_compare() WEDGE_ON 4?\n");
		}
	}
out:
	if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
		rt_log("nmg_face_vu_compare(vu=x%x, vu=x%x) %s %s, %s\n",
			a->vu, b->vu,
			WEDGECLASS2STR(a->wedge_class),
			WEDGECLASS2STR(b->wedge_class),
			ret==(-1) ? "A<B" : (ret==0 ? "A==B" : "A>B") );
	}
	return ret;
}

/*
 *			N M G _ F A C E _ V U _ D O T
 *
 *  For the purpose of computing the dot product of the edges around
 *  this vertexuse and the ray direction vector, the edge vectors should
 *  both be pointing inwards to the vertex,
 *  rather than in the edge direction, so that it is possible to sort
 *  the vertexuse's into sequence by "angle" along the ray direction,
 *  starting with the vertexuse that the ray first encounters.
 */
static void
nmg_face_vu_dot( vsp, lu, rs, ass )
struct nmg_vu_stuff		*vsp;
struct loopuse			*lu;
CONST struct nmg_ray_state	*rs;
int				ass;
{
	struct edgeuse	*this_eu;
	struct edgeuse	*othereu;
	vect_t		vec;
	fastf_t		dot;
	struct vertexuse	*vu;
	int		this;

	vu = vsp->vu;
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_LOOPUSE(lu);
	this_eu = nmg_find_eu_with_vu_in_lu( lu, vu );

	/* First, consider the edge inbound into this vertex */
	this = NMG_V_ASSESSMENT_PREV(ass);
	if( this == NMG_E_ASSESSMENT_ON_REV )  {
		vsp->min_vu_dot = -1;		/* straight back */
	} else if( this == NMG_E_ASSESSMENT_ON_FORW )  {
		vsp->min_vu_dot = 1;		/* straight forw */
	} else {
		othereu = RT_LIST_PLAST_CIRC( edgeuse, this_eu );
		if( vu->v_p != othereu->vu_p->v_p )  {
			/* Vector from othereu to this_eu */
			VSUB2( vec, vu->v_p->vg_p->coord,
				othereu->vu_p->v_p->vg_p->coord );
			VUNITIZE(vec);
			vsp->min_vu_dot = VDOT( vec, rs->dir );
		} else {
			vsp->min_vu_dot = 99;		/* larger than +1 */
		}
	}

	/* Second, consider the edge outbound from this vertex (forw) */
	this = NMG_V_ASSESSMENT_NEXT(ass);
	if( this == NMG_E_ASSESSMENT_ON_REV )  {
		dot = -1;		/* straight back */
		if( dot < vsp->min_vu_dot )  vsp->min_vu_dot = dot;
	} else if( this == NMG_E_ASSESSMENT_ON_FORW )  {
		dot = 1;		/* straight forw */
		if( dot < vsp->min_vu_dot )  vsp->min_vu_dot = dot;
	} else {
		othereu = RT_LIST_PNEXT_CIRC( edgeuse, this_eu );
		if( vu->v_p != othereu->vu_p->v_p )  {
			/* Vector from othereu to this_eu */
			VSUB2( vec, vu->v_p->vg_p->coord,
				othereu->vu_p->v_p->vg_p->coord );
			VUNITIZE(vec);
			dot = VDOT( vec, rs->dir );
			if( dot < vsp->min_vu_dot )  {
				vsp->min_vu_dot = dot;
			}
		}
	}
}

/*
 *			N M G _ S P E C I A L _ W E D G E _ P R O C E S S I N G
 *
 *  If one loop gets cut, then unwind the whole call stack, and reassess
 *  where things stand.  (The caller needs to re-call in that case).
 *
 *  The goal here is to get rid of nested wedges.
 *  To do that, join loops wherever possible, cut them sparingly.
 *
 *  Returns -
 *	0	Nothing needed changing, OK to proceed with vertex sorting.
 *	1	Loops were cut or joined, need to reclassify everything
 *		at this vertexuse.
 */
static int
nmg_special_wedge_processing( vs, start, end, lo_ang, hi_ang, wclass, exclude, tol )
struct nmg_vu_stuff	*vs;
int	start;		/* vu index of coincident range */
int	end;
double	lo_ang;
double	hi_ang;
int	wclass;
int	*exclude;
CONST struct rt_tol	*tol;
{
	register int	i;
	int		outer_wedge;
	int		inner_wedge;
	int		class2;
	struct loopuse	*outer_lu;
	struct loopuse	*inner_lu;
	int		not_these[128];

	RT_CK_TOL(tol);

	if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
		char			buf[128];
		FILE			*fp;
		struct model		*m;
		long			*b;
		struct rt_vlblock	*vbp;
		static int		num = 0;

		rt_log("nmg_special_wedge_processing(start=%d,end=%d, lo=%g, hi=%g, wclass=%s)\n",
			start, end, lo_ang, hi_ang,
			WEDGECLASS2STR(wclass) );
		VPRINT("\tvertex", vs[start].vu->v_p->vg_p->coord);

		/* Plot all the loops that touch here. */
		m = nmg_find_model((long *)vs[start].vu);
		b = (long *)rt_calloc( m->maxindex, sizeof(long), "nmg_special_wedge_processing flag[]" );
		vbp = rt_vlblock_init();
		for( i=start; i < end; i++ )  {
			struct loopuse	*lu;
			lu = nmg_find_lu_of_vu(vs[i].vu);
			rt_log("\tvu[%d]=x%x, lu=x%x\n", i, vs[i].vu, lu);
			nmg_vlblock_lu(vbp, lu, b, 255, 0, 0, 0 );
		}
		for( i=start; i < end; i++ )  {
			struct loopuse	*lu;
			lu = nmg_find_lu_of_vu(vs[i].vu);
			nmg_pr_lu_briefly(lu,0);
		}
		sprintf(buf, "wedge%d.pl", num++);
		fp = fopen(buf, "w");
		rt_plot_vlblock( fp, vbp );
		fclose(fp);
		rt_log("wrote %s\n", buf);
		rt_free( (char *)b, "nmg_special_wedge_processing flag[]" );
		rt_vlblock_free(vbp);
	}

	if( end-start >= 128 )  rt_bomb("nmg_special_wedge_processing: array overflow\n");
	if( !exclude )  {
		bzero( (char *)not_these, sizeof(not_these) );
		exclude = not_these;
	}

again:
	/* May be many "outer" wedges to iterate over this side of line */
	outer_wedge = nmg_find_vu_in_wedge( vs, start, end,
		lo_ang, hi_ang, wclass, exclude );
	if( outer_wedge <= -1 )  return 0;	/* No wedges to process */

	exclude[outer_wedge] = 1;	/* Don't return this wedge again */

	/* There is at least one wedge on this side of the line */
	outer_lu = nmg_find_lu_of_vu( vs[outer_wedge].vu );
	NMG_CK_LOOPUSE(outer_lu);

again_inner:
	inner_wedge = nmg_find_vu_in_wedge( vs, start, end,
		vs[outer_wedge].lo_ang, vs[outer_wedge].hi_ang,
		wclass, exclude );
	if( inner_wedge <= -1 )  {
		/*
		 *  See if there is another outer wedge that starts where
		 *  outer_wedge left off.
		 *  exclude[outer_wedge] is already set.
		 */
		goto again;
	}
	if( inner_wedge == outer_wedge )  rt_bomb("nmg_special_wedge_processing() identical vu selections?\n");

	class2 = nmg_compare_2_wedges( vs[outer_wedge].lo_ang, vs[outer_wedge].hi_ang,
		vs[inner_wedge].lo_ang, vs[inner_wedge].hi_ang );
	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("+++nmg_special_wedge_processing() outer=%d, inner=%d, class2=%s\n", outer_wedge, inner_wedge, WEDGE2_TO_STRING(class2) );

	inner_lu = nmg_find_lu_of_vu( vs[inner_wedge].vu );
	NMG_CK_LOOPUSE(inner_lu);

	if( outer_lu == inner_lu )  {
		struct loopuse	*new_lu;
		if( class2 == WEDGE2_IDENTICAL &&
		    NEAR_ZERO( vs[inner_wedge].hi_ang - vs[inner_wedge].lo_ang, WEDGE_ANG_TOL )
		    )  {
rt_g.NMG_debug |= DEBUG_VU_SORT|DEBUG_FCUT;
			if(rt_g.NMG_debug&DEBUG_VU_SORT)
				rt_log("nmg_special_wedge_processing:  inner and outer wedges from same loop, WEDGE2_IDENTICAL & 0deg spread, already in final form.\n");
			exclude[inner_wedge] = 1;	/* Don't return this wedge again */
			/* Don't need to recurse only because this is a crack */
			goto again_inner;
		}
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("nmg_special_wedge_processing:  inner and outer wedges from same loop, cutting loop\n");
		new_lu = nmg_cut_loop( vs[outer_wedge].vu, vs[inner_wedge].vu );
		NMG_CK_LOOPUSE(new_lu);
		NMG_CK_LOOPUSE(inner_lu);
		nmg_loop_g( inner_lu->l_p, tol );
		nmg_loop_g( new_lu->l_p, tol );
		nmg_lu_reorient(inner_lu, tol);
		nmg_lu_reorient(new_lu, tol);
		return 1;		/* cutjoin was done */
	}

	/* XXX Or they could be WEDGE2_IDENTICAL */
	/* XXX If WEDGE2_IDENTICAL, could we join and then simplify? */
	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("wedge at vu[%d] is inside wedge at vu[%d]\n", inner_wedge, outer_wedge);

	if( outer_lu->orientation == inner_lu->orientation )  {
		/*
		 *  Two loops meet at this vu.  Joining them will impose
		 *  a natural edgeuse ordering onto the vu's.
		 */
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("joining loops\n");
		vs[inner_wedge].vu = nmg_join_2loops( vs[outer_wedge].vu,
			vs[inner_wedge].vu );
		return 1;		/* cutjoin was done */
	}

	/* Recurse on inner wedge */
	if( nmg_special_wedge_processing( vs, start, end,
	    vs[inner_wedge].lo_ang, vs[inner_wedge].hi_ang, wclass, exclude, tol ) )
		return 1;	/* An inner wedge was cut */

	/*
	 *  Inner and outer are different loopuses,
	 *  have different orientations,
	 *  and have nothing complex inside the wedge of the inner loop.
	 *  Join inner and outer loops here, to impose a proper vu ordering.
	 */
	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("Inner wedge is simple, join inner and outer loops.\n");

	vs[inner_wedge].vu = nmg_join_2loops( vs[outer_wedge].vu,
		vs[inner_wedge].vu );
	return 1;		/* cutjoin was done */
}

/*
 *			N M G _ F A C E _ C O I N C I D E N T _ V U _ S O R T
 *
 *  Given co-incident vertexuses (same distance along the ray),
 *  sort them into the "proper" order for driving the state machine.
 */
int
nmg_face_coincident_vu_sort( rs, start, end )
struct nmg_ray_state	*rs;
int			start;		/* first index */
int			end;		/* last index + 1 */
{
	int		num;
	struct nmg_vu_stuff	*vs;
	struct nmg_loop_stuff *ls;
	int		nloop;
	unsigned	nvu;
	int		i;
	struct loopuse	*lu;
	int		ass;
	int		l;
	int		retries = 0;

	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("nmg_face_coincident_vu_sort(, %d, %d) START\n", start, end);

	num = end - start;
	vs = (struct nmg_vu_stuff *)rt_malloc( sizeof(struct nmg_vu_stuff)*num,
		"nmg_vu_stuff" );
	ls = (struct nmg_loop_stuff *)rt_malloc( sizeof(struct nmg_loop_stuff)*num,
		"nmg_loop_stuff" );

top:
	if( retries > 20 )  rt_g.NMG_debug |= DEBUG_VU_SORT;
	if( retries++ > 24 )  rt_bomb("nmg_face_coincident_vu_sort() infinite loop\n");
	/* Assess each vu, create list of loopuses, find max angles */
	nloop = 0;
	nvu = 0;
	for( i = end-1; i >= start; i-- )  {
		lu = nmg_find_lu_of_vu( rs->vu[i] );
		NMG_CK_LOOPUSE(lu);
		ass = nmg_assess_vu( rs, i );
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
		   rt_log("vu[%d]=x%x v=x%x assessment=%s\n",
			i, rs->vu[i], rs->vu[i]->v_p, nmg_v_assessment_names[ass] );
		/*  Ignore lone vertices, unless that is all that there is,
		 *  in which case, let just one through.  (return 'start+1');
		 */
		if( *(rs->vu[i]->up.magic_p) == NMG_LOOPUSE_MAGIC )  {
			if( nvu > 0 || i > start )  {
				/* Drop this loop of a single vertex in sanitize() */
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
				/* "continue" keeps vu from being added to vs[] */
				continue;
			}
		}

		vs[nvu].vu = rs->vu[i];
		vs[nvu].seq = -1;		/* Not assigned yet */

		/* x_dir is -dir, y_dir is -left */
		vs[nvu].in_vu_angle = nmg_vu_angle_measure( rs->vu[i],
			rs->ang_x_dir, rs->ang_y_dir, ass, 1 ) * rt_radtodeg;
		vs[nvu].out_vu_angle = nmg_vu_angle_measure( rs->vu[i],
			rs->ang_x_dir, rs->ang_y_dir, ass, 0 ) * rt_radtodeg;

		/* Special case for LEFT & ON combos */
		if( ass == NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_LEFT) )
			vs[nvu].in_vu_angle = 360;
		else if( ass == NMG_V_COMB(NMG_E_ASSESSMENT_LEFT, NMG_E_ASSESSMENT_ON_REV) )
			vs[nvu].out_vu_angle = 360;

		vs[nvu].wedge_class = nmg_wedge_class( ass, vs[nvu].in_vu_angle, vs[nvu].out_vu_angle );
		if(rt_g.NMG_debug&DEBUG_VU_SORT) rt_log("nmg_wedge_class = %d %s\n", vs[nvu].wedge_class, WEDGECLASS2STR(vs[nvu].wedge_class));
		/* Sort the angles (Don't forget to sort for CROSS case too) */
		if( (vs[nvu].wedge_class == WEDGE_LEFT && vs[nvu].in_vu_angle > vs[nvu].out_vu_angle) ||
		    (vs[nvu].wedge_class != WEDGE_LEFT && vs[nvu].in_vu_angle < vs[nvu].out_vu_angle) )  {
			vs[nvu].lo_ang = vs[nvu].in_vu_angle;
			vs[nvu].hi_ang = vs[nvu].out_vu_angle;
		} else {
			vs[nvu].lo_ang = vs[nvu].out_vu_angle;
			vs[nvu].hi_ang = vs[nvu].in_vu_angle;
		}

		/* Check entering and departing edgeuse angle w.r.t. ray */
		/* This is already done once in nmg_assess_vu();  reuse? */
		/* Computes vs[nvu].min_vu_dot */
		nmg_face_vu_dot( &vs[nvu], lu, rs, ass );

		/* Search for loopuse table entry */
		for( l = 0; l < nloop; l++ )  {
			if( ls[l].lu == lu )  goto got_loop;
		}
		/* didn't find loopuse in table, add to table */
		l = nloop++;
		ls[l].lu = lu;
		ls[l].n_vu_in_loop = 0;
		ls[l].min_dot = 99;		/* > +1 */
got_loop:
		ls[l].n_vu_in_loop++;
		vs[nvu].loop_index = l;
		vs[nvu].lsp = &ls[l];
		if( vs[nvu].min_vu_dot < ls[l].min_dot )  {
			ls[l].min_dot = vs[nvu].min_vu_dot;
			ls[l].min_vu = vs[nvu].vu;
		}
		nvu++;
	}

	/*
	 *  For each loop which has more than one vertexuse present on the
	 *  ray, start at the vu which has the smallest angle off the ray,
	 *  and walk the edges of the loop, marking off the vu sequence for
	 *  those vu's on the ray (those vu's found in vs[].vu).
	 */
	for( l=0; l < nloop; l++ )  {
		register struct edgeuse	*eu;
		struct edgeuse	*first_eu;
		int		seq = 0;

		if( ls[l].n_vu_in_loop <= 1 )  continue;

		first_eu = nmg_find_eu_with_vu_in_lu( ls[l].lu, ls[l].min_vu );
		eu = first_eu;
		do {
			register struct vertexuse *vu = eu->vu_p;
			NMG_CK_VERTEXUSE(vu);
			for( i=0; i < nvu; i++ )  {
				if( vs[i].vu == vu )  {
					vs[i].seq = seq++;
					break;
				}
			}
			eu = RT_LIST_PNEXT_CIRC(edgeuse,eu);
		} while( eu != first_eu );
	}

	/* For loops with >1 crossings here, determine proper VU ordering on that loop */
	/* XXX */

	/* Here is where the special wedge-breaking code goes */
	if( nmg_special_wedge_processing( vs, 0, nvu, 0.0, 180.0, WEDGE_RIGHT, 0, rs->tol ) )  {
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("*** nmg_face_coincident_vu_sort(, %d, %d) restarting after 0--180 wedge\n", start, end);
		goto top;
	}
	/* XXX reclass on/on edges from WEDGE_RIGHT to WEDGE_LEFT here? */
	if( nmg_special_wedge_processing( vs, 0, nvu, 360.0, 180.0, WEDGE_LEFT, 0, rs->tol ) ) {
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("*** nmg_face_coincident_vu_sort(, %d, %d) restarting after 180-360 wedge\n", start, end);
		goto top;
	}

	if(rt_g.NMG_debug&DEBUG_VU_SORT)
	{
		rt_log("Loop table (before sort):\n");
		for( l=0; l < nloop; l++ )  {
			rt_log("  index=%d, lu=x%x, min_dot=%g, #vu=%d\n",
				l, ls[l].lu, ls[l].min_dot,
				ls[l].n_vu_in_loop );
		}
	}

	/* Sort the vertexuse table into appropriate order */
	qsort( (genptr_t)vs, (unsigned)nvu, (unsigned)sizeof(*vs),
		nmg_face_vu_compare );

	if(rt_g.NMG_debug&DEBUG_VU_SORT)
	{
		rt_log("Vertexuse table (after sort):\n");
		for( i=0; i < nvu; i++ )  {
			rt_log("  x%x, l=%d, in/o=(%g, %g), lo/hi=(%g,%g), %s, sq=%d\n",
				vs[i].vu, vs[i].loop_index,
				vs[i].in_vu_angle, vs[i].out_vu_angle,
				vs[i].lo_ang, vs[i].hi_ang,
				WEDGECLASS2STR(vs[i].wedge_class),
				vs[i].seq );
		}
	}

	/* Copy new vu's back to main array */
#if 0
	/* XXX I'm not sure if this is right or not */
	if( rs->state == NMG_STATE_IN )  {
		/*
		 *  If the state is IN, then need to traverse the vertexuse's
		 *  in the opposite order.
		 *  All the sorting is done from the point of view of
		 *  being in OUT state.
		 */
		if(rt_g.NMG_debug&DEBUG_VU_SORT)
			rt_log("Reversed processing order, state=IN.\n");
		for( i=0; i < nvu; i++ )  {
			rs->vu[start+i] = vs[nvu-1-i].vu;
		}
	} else
#endif
	{
		for( i=0; i < nvu; i++ )  {
			rs->vu[start+i] = vs[i].vu;
		}
	}
	if(rt_g.NMG_debug&DEBUG_VU_SORT)  {
		for( i=0; i < nvu; i++ )  {
			rt_log(" vu[%d]=x%x, v=x%x\n",
				start+i, rs->vu[start+i], rs->vu[start+i]->v_p );
		}
	}

	rt_free( (char *)vs, "nmg_vu_stuff");
	rt_free( (char *)ls, "nmg_loop_stuff");

	if(rt_g.NMG_debug&DEBUG_VU_SORT)
		rt_log("nmg_face_coincident_vu_sort(, %d, %d) END, ret=%d\n", start, end, start+nvu);

	return start+nvu;
}

/*
 *			N M G _ S A N I T I Z E _ F U
 *
 *  Eliminate any OT_BOOLPLACE self-loops that remain behind in this face.
 */
void
nmg_sanitize_fu(fu)
struct faceuse	*fu;
{
	struct loopuse	*lu;
	struct loopuse	*lunext;

	NMG_CK_FACEUSE(fu);

	lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
	while( RT_LIST_NOT_HEAD(lu, &fu->lu_hd) )  {
		NMG_CK_LOOPUSE(lu);
		lunext = RT_LIST_PNEXT(loopuse,lu);
		if( lu->orientation == OT_BOOLPLACE )  {
			/* XXX What to do with return code? */
			if( nmg_klu(lu) )  rt_bomb("nmg_sanitize_fu() nmg_klu() emptied face?\n");
		}
		lu = lunext;
	}
}

/*
 *			N M G _ F A C E _ R S _ I N I T
 *
 *  Set up nmg_ray_state structure.
 *  "left" is a vector that lies in the plane of the face
 *  which contains the loops being operated on.
 *  It points in the direction "left" of the ray, such that the first
 *  OT_SAME loop in the first OT_SAME fact that it crosses will cross
 *  the ray in a left-to-right manner consistent with the CCW loop rule.
 *  There are some special conditions placed on the "dir" argument to
 *  make this happen;  see the comments in nmg_inter.c for details, or
 *  Mike's notes "The 'Left' Vector Choice" dated 27-Aug-93, page 1.
 */
void
nmg_face_rs_init( rs, b, fu1, fu2, pt, dir, tol )
struct nmg_ray_state	*rs;
struct nmg_ptbl	*b;		/* table of vertexuses in fu1 on intercept line */
struct faceuse	*fu1;		/* face being worked */
struct faceuse	*fu2;		/* for plane equation */
point_t		pt;
vect_t		dir;
CONST struct rt_tol	*tol;
{
	plane_t	n1;

	RT_CK_TOL(tol);
	NMG_CK_PTBL(b);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	bzero( (char *)rs, sizeof(*rs) );
	rs->tol = tol;
	rs->vu = (struct vertexuse **)b->buffer;
	rs->nvu = b->end;
	rs->eg_p = (struct edge_g *)NULL;
	rs->sA = fu1->s_p;
	rs->sB = fu2->s_p;
	rs->fu1 = fu1;
	rs->fu2 = fu2;
	VMOVE( rs->pt, pt );
	VMOVE( rs->dir, dir );
	NMG_GET_FU_PLANE( n1, fu1 );
	VCROSS( rs->left, n1, dir );
	VUNITIZE( rs->left );
	switch( fu1->orientation )  {
	case OT_SAME:
		break;
	case OT_OPPOSITE:
		VREVERSE(rs->left, rs->left);
		break;
	default:
		rt_bomb("nmg_face_rs_init: bad orientation\n");
	}
	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		struct loopuse *lu;
		struct edgeuse *eu;
		struct vertexuse *vu;
		int i;

		rt_log("\tfu->orientation=%s\n", nmg_orientation(fu1->orientation) );
		HPRINT("\tfg N", n1);
		VPRINT("\t  pt", pt);
		VPRINT("\t dir", dir);
		VPRINT("\tleft", rs->left);
		rt_log( "\tvertexuses in fu that are on lintersect line:\n" );
		for( i=0 ; i<NMG_TBL_END( b ) ; i++ )
		{
			vu = (struct vertexuse *)NMG_TBL_GET( b , i );
			nmg_pr_vu_briefly( vu , "\t  " );
		}
		rt_log( "\tLoopuse in fu (x%x):\n" , fu1 );
		for( RT_LIST_FOR( lu , loopuse , &fu1->lu_hd ) )
		{
			rt_log( "\tLOOPUSE x%x:\n" , lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
				nmg_pr_vu_briefly( vu , "\tVertex Loop: " );
			}
			else
			{
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					struct edgeuse *eu_next;
					vect_t eu_dir;
					fastf_t eu_len;
					double inv_len;

					nmg_pr_eu_briefly( eu , "\t\t" );
					eu_next = RT_LIST_PNEXT_CIRC( edgeuse , eu );
					VSUB2( eu_dir , eu_next->vu_p->v_p->vg_p->coord , eu->vu_p->v_p->vg_p->coord );
					eu_len = MAGNITUDE( eu_dir );
					if( eu_len < VDIVIDE_TOL )
						inv_len = 0.0;
					else
						inv_len = 1.0/eu_len;
					for( i=0 ; i<3 ; i++ )
						eu_dir[i] = eu_dir[i] * inv_len;
					rt_log( "\t\t\teu_dir = ( %g , %g , %g ), length = %g\n", V3ARGS( eu_dir ) , eu_len );
				}
			}
		}
	}
	rs->state = NMG_STATE_OUT;

	/* For measuring angle CCW around plane from -dir */
	VREVERSE( rs->ang_x_dir, dir );
	VREVERSE( rs->ang_y_dir, rs->left );
}

/*
 *			N M G _ F A C E _ N E X T _ V U _ I N T E R V A L
 *
 *  Handle the extent of coincident vertexuses at this distance.
 *  ptbl_vsort() will have forced all the distances to be
 *  exactly equal if they are within tolerance of each other.
 *
 *  Two cases:  lone vertexuse, and range of vertexuses.
 *
 *  Return value is where next interval starts.
 */
HIDDEN int
nmg_face_next_vu_interval( rs, cur, mag, other_rs_state )
struct nmg_ray_state	*rs;
int		cur;
fastf_t		*mag;
int		other_rs_state;
{
	int	j;
	int	k;
	int	m;
	struct vertex	*v;

	RT_CK_TOL(rs->tol);

	if( cur == rs->nvu-1 || mag[cur+1] != mag[cur] )  {
		/* Single vertexuse at this dist */
		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log("nmg_face_next_vu_interval() fu=x%x, single vertexuse at index %d\n", rs->fu1, cur);
		nmg_face_state_transition( rs, cur, 0, other_rs_state );
#if PLOT_BOTH_FACES
		nmg_2face_plot( rs->fu1, rs->fu2 );
#else
		nmg_face_plot( rs->fu1 );
#endif
		return cur+1;
	}

	/* Find range of vertexuses at this distance */
	v = rs->vu[cur]->v_p;
	for( j = cur+1; j < rs->nvu; j++ )  {
		/* If distance along the ray changes, start a new interval */
		if( mag[j] != mag[cur] )  break;
#if 0
		/* If vertex changes, it starts a new interval */
		/* XXX Note that they will be sorted by pointer addres. */
		/* XXX This might cause inconsistencies later */
		if( rs->vu[j]->v_p != v )  break;
#endif
	}

	/* vu Interval runs from [cur] to [j-1] inclusive */
	if(rt_g.NMG_debug&DEBUG_FCUT)
		rt_log("nmg_face_next_vu_interval() fu=x%x vu's on list interval [%d] to [%d] equal\n", rs->fu1, cur, j-1 );

	/* Ensure that all vu's point to same vertex */
	for( k = cur+1; k < j; k++ )  {
		if( rs->vu[k]->v_p == v )  continue;
		/* Trouble.  Print out the interval and die */
		rt_log("At k=%d, vertex changed from v=x%x!\n", k, v);
		rt_log("pt_equality=%d\n", rt_pt3_pt3_equal(v->vg_p->coord,
			rs->vu[k]->v_p->vg_p->coord, rs->tol ) );
		for( k = cur; k < j; k++ )  {
			rt_log("  %d vu=%8x v=%8x mag=%g\n", k,
				rs->vu[k], rs->vu[k]->v_p, mag[k] );
			NMG_CK_VERTEX_G(rs->vu[k]->v_p->vg_p);
			VPRINT("\tpt", rs->vu[k]->v_p->vg_p->coord);
		}
		rt_bomb("nmg_face_combine: vu block with differing vertices\n");
	}
	/* All vu's point to the same vertex, sort them */
	m = nmg_face_coincident_vu_sort( rs, cur, j );

	/* Process vu list, up to cutoff index 'm', which can be less than j */
	for( k = cur; k < m; k++ )  {
		nmg_face_state_transition( rs, k, 1, other_rs_state );
#if PLOT_BOTH_FACES
		nmg_2face_plot( rs->fu1, rs->fu2 );
#else
		nmg_face_plot( rs->fu1 );
#endif
	}
	rs->vu[j-1] = rs->vu[m-1]; /* for next iteration's lookback */
	if(rt_g.NMG_debug&DEBUG_FCUT)
		rt_log("nmg_face_next_vu_interval() vu[%d] set to x%x\n", j-1, rs->vu[j-1] );
	return j;
}

/*
 *			N M G _ F A C E _ C O M B I N E
 *
 *	collapse loops,vertices within face fu1 (relative to fu2)
 *
 */
HIDDEN void
nmg_face_combineX(rs1, mag1, rs2, mag2)
struct nmg_ray_state	*rs1;
fastf_t			*mag1;
struct nmg_ray_state	*rs2;
fastf_t			*mag2;
{
	register int	cur1, cur2;
	register int	nxt1, nxt2;

	RT_CK_TOL(rs1->tol);
	RT_CK_TOL(rs2->tol);

	if(rt_g.NMG_debug&DEBUG_FCUT)
		rt_log("nmg_face_combine()\n");

#if PLOT_BOTH_FACES
	nmg_2face_plot( rs1->fu1, rs1->fu2 );
#else
	nmg_face_plot( rs1->fu1 );
	nmg_face_plot( rs1->fu2 );
#endif

	/*  Handle next block of coincident vertexuses.
	 *  Sometimes only one list has a block in it.
	 */
	cur1 = cur2 = 0;
	for( ; cur1 < rs1->nvu && cur2 < rs2->nvu; cur1=nxt1, cur2=nxt2 )  {
		int	old_rs1_state = rs1->state;

		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log("\nnmg_face_combineX() vu block, index1=%d, index2=%d\n", cur1, cur2);

		if( mag1[cur1] < mag2[cur2] )  {
			if(rt_g.NMG_debug&DEBUG_FCUT)
				rt_log("\nnmg_face_combineX() doing index1 block (at end)\n");
			nxt1 = nmg_face_next_vu_interval( rs1, cur1, mag1, rs2->state );
			nxt2 = cur2;
		} else if( mag1[cur1] > mag2[cur2] )  {
			if(rt_g.NMG_debug&DEBUG_FCUT)
				rt_log("\nnmg_face_combineX() doing index2 block (at end)\n");
			nxt1 = cur1;
			nxt2 = nmg_face_next_vu_interval( rs2, cur2, mag2, old_rs1_state );
		} else {
			struct vertexuse	*vu1;
			struct vertexuse	*vu2;
			vu1 = rs1->vu[cur1];
			vu2 = rs2->vu[cur2];
			NMG_CK_VERTEXUSE(vu1);
			NMG_CK_VERTEXUSE(vu2);
			if( vu1->v_p != vu2->v_p )  {
				rt_log("cur1=%d, cur2=%d, v1=x%x, v2=x%x\n",
					cur1, cur2, vu1->v_p, vu2->v_p);
				rt_bomb("nmg_face_combineX: vertex lists scrambled");
			}
			if(rt_g.NMG_debug&DEBUG_FCUT)
				rt_log("\nnmg_face_combineX() doing index1 block\n");
			nxt1 = nmg_face_next_vu_interval( rs1, cur1, mag1, rs2->state );
			if(rt_g.NMG_debug&DEBUG_FCUT)
				rt_log("\nnmg_face_combineX() doing index2 block\n");
			nxt2 = nmg_face_next_vu_interval( rs2, cur2, mag2, old_rs1_state );
		}
	}

	/*
	 *  Here, one list is exhausted, but the other may not be.
	 *  Press on until both are.
	 */
	for( ; cur1 < rs1->nvu; cur1 = nxt1 )  {
		nxt1 = nmg_face_next_vu_interval( rs1, cur1, mag1, rs2->state );
	}
	for( ; cur2 < rs2->nvu; cur2 = nxt2 )  {
		nxt2 = nmg_face_next_vu_interval( rs2, cur2, mag2, rs1->state );
	}

	if( rs1->state != NMG_STATE_OUT || rs2->state != NMG_STATE_OUT )  {
		rt_log("ERROR nmg_face_combine() ended in state '%s'/'%s'?\n",
			nmg_state_names[rs1->state],
			nmg_state_names[rs2->state] );
		rt_log("cur1 = %d of %d, cur2 = %d of %d\n",
			cur1, rs1->nvu, cur2, rs2->nvu );

		if( rt_g.debug || rt_g.NMG_debug )  {
			/* Drop a plot file */
			rt_g.NMG_debug |= DEBUG_VU_SORT|DEBUG_FCUT|DEBUG_PLOTEM;
			nmg_pl_comb_fu( 0, 1, rs1->fu1 );
			nmg_pl_comb_fu( 0, 2, rs1->fu2 );
		}

#if 0
		rt_log("nmg_face_combine() bad ending state, pushing on\n");
#else
		/* This is the production setting */
		rt_bomb("nmg_face_combine() bad ending state\n");
#endif
	}
}

/*
 *			N M G _ F A C E _ C U T J O I N
 *
 *  The main face cut handler.
 *  Called from nmg_inter.c by nmg_isect_2faces().
 *
 *  A wrapper for nmg_face_combine, for now.
 *
 *  The two vertexuse lists may be of different lengths, because
 *  one may have multiple uses of a vertex, while the other has only
 *  a single use of that same vertex.
 */
void
nmg_face_cutjoin(b1, b2, fu1, fu2, pt, dir, tol)
struct nmg_ptbl	*b1;		/* table of vertexuses in fu1 on intercept line */
struct nmg_ptbl	*b2;		/* table of vertexuses in fu2 on intercept line */
struct faceuse	*fu1;		/* face being worked */
struct faceuse	*fu2;		/* for plane equation */
point_t		pt;
vect_t		dir;
CONST struct rt_tol	*tol;
{
	fastf_t		*mag1;
	fastf_t		*mag2;
	fastf_t		dist_tol = 0.005;	/* XXX */
	struct vertexuse **vu1, **vu2;
	int		i;
	struct nmg_ray_state	rs1;
	struct nmg_ray_state	rs2;

	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("\nnmg_face_cutjoin(fu1=x%x, fu2=x%x) START\n", fu1, fu2);
	}

	RT_CK_TOL(tol);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	/* Perhaps this should only happen when debugging is on? */
	if( b1->end <= 0 || b2->end <= 0 )  {
		rt_log("nmg_face_cutjoin(fu1=x%x, fu2=x%x): WARNING empty list %d %d\n",
			fu1, fu2, b1->end, b2->end );
		return;
	}

top:
	mag1 = (fastf_t *)rt_calloc(b1->end+1, sizeof(fastf_t),
		"vector magnitudes along ray, for sort");
	mag2 = (fastf_t *)rt_calloc(b2->end+1, sizeof(fastf_t),
		"vector magnitudes along ray, for sort");

	/*
	 *  Sort hit points by increasing distance, vertex ptr, vu ptr,
	 *  and eliminate any duplicate vu's.
	 */
	ptbl_vsort(b1, fu1, fu2, pt, dir, mag1, dist_tol);
	ptbl_vsort(b2, fu2, fu1, pt, dir, mag2, dist_tol);

	vu1 = (struct vertexuse **)b1->buffer;
	vu2 = (struct vertexuse **)b2->buffer;

	/* Print list of intersections */
	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("Ray vu intersection list:\n");
		for( i=0; i < b1->end; i++ )  {
			rt_log(" %d %e ", i, mag1[i] );
			nmg_pr_vu_briefly( vu1[i], (char *)0 );
		}
		for( i=0; i < b2->end; i++ )  {
			rt_log(" %d %e ", i, mag2[i] );
			nmg_pr_vu_briefly( vu2[i], (char *)0 );
		}
	}

#if 0
	/* Check to see if lists are different */
	{
		i = b1->end-1;
		if( b2->end-1 < i )  i = b2->end-1;
		for( ; i >= 0; i-- )  {
			NMG_CK_VERTEXUSE(vu1[i]);
			NMG_CK_VERTEXUSE(vu2[i]);
			if( vu1[i]->v_p == vu2[i]->v_p ) continue;
			rt_log("Index %d mis-match, x%x != x%x\n",
				i, vu1[i]->v_p, vu2[i]->v_p );
		}
	}
#endif

#if 0
	/* this block of code checks if the two lists of intersection vertexuses
	 * contain vertexuses from the appropriate faceuse */
	{
		int found1=(-1),found2=(-1); /* -1 => not set, 0 => no vertexuses from faceuse found */
		int tmp_found;
		struct faceuse *fu;
		struct vertexuse *vu;

		/* list b1 should contain vertexuses from faceuse fu1 */
		for( i=0 ; i<NMG_TBL_END( b1 ) ; i++ )
		{
			tmp_found = 0;
			vu = (struct vertexuse *)NMG_TBL_GET( b1 , i );
			fu = nmg_find_fu_of_vu(vu);
			if( fu == fu1 )
				tmp_found = 1;
			if( found1 == (-1) )
				found1 = tmp_found;
			else if( tmp_found != found1 ) /* some found, some not found!!!!! */
			{
				rt_log ("nmg_face_cutjoin: Intersection list is screwy for face 1\n" );
				break;
			}
		}

		/* and list b2 should contain vertexuses from faceuse fu2 */
		for( i=0 ; i<NMG_TBL_END( b2 ) ; i++ )
		{
			tmp_found = 0;
			vu = (struct vertexuse *)NMG_TBL_GET( b2 , i );
			fu = nmg_find_fu_of_vu(vu);
			if( fu == fu2 )
				tmp_found = 1;
			if( found2 == (-1) )
				found2 = tmp_found;
			else if( tmp_found != found2 ) /* some found, some not found!!!!! */
			{
				rt_log ("nmg_face_cutjoin: Intersection list is screwy for face 2\n" );
				break;
			}
		}
		if( !found1 )
			rt_log( "nmg_face_cutjoin: intersection list for face 1 doesn't contain vertexuses from face 1!!!\n" );
		if( !found2 )
			rt_log( "nmg_face_cutjoin: intersection list for face 2 doesn't contain vertexuses from face 2!!!\n" );
	}
#endif
	/* Check to make sure that intersector didn't miss anything */
	if( nmg_ck_vu_ptbl( b1, fu1 ) || nmg_ck_vu_ptbl( b2, fu2 ) )  goto top;

	nmg_face_rs_init( &rs1, b1, fu1, fu2, pt, dir, tol );
	nmg_face_rs_init( &rs2, b2, fu2, fu1, pt, dir, tol );

	nmg_face_combineX( &rs1, mag1, &rs2, mag2 );

	rt_free((char *)mag1, "vector magnitudes");
	rt_free((char *)mag2, "vector magnitudes");

	/* Can't do simplifications here,
	 * because the caller's linked lists & pointers might get disrupted.
	 */

	/* Merging uses of common edges is OK, though, and quite necessary. */
	if( i = nmg_mesh_two_faces( fu1, fu2, tol ) )  {
		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log("nmg_face_cutjoin() meshed %d edges\n", i);
	}
	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("nmg_face_cutjoin(fu1=x%x, fu2=x%x) END\n", fu1, fu2);
	}
}

/*
 *			N M G _ E D G E _ G E O M _ I S E C T _ L I N E
 *
 *  Force the edge geometry structure for a given edge to be that of
 *  the intersection line between the two faces.
 *
 *  XXX What about orientation?  Which way should direction vector point?
 *  XXX What about edgeuse orientation flags?
 */
void
nmg_edge_geom_isect_line( e, rs )
struct edge		*e;
struct nmg_ray_state	*rs;
{
	register struct edge_g	*eg;

	NMG_CK_EDGE(e);
	if( !e->eg_p )  {
		/* No edge geometry so far */
		if( !rs->eg_p )  {
			nmg_edge_g( e );
			eg = e->eg_p;
			NMG_CK_EDGE_G(eg);
			VMOVE( eg->e_pt, rs->pt );
			VMOVE( eg->e_dir, rs->dir );
			rs->eg_p = eg;
		} else {
			nmg_use_edge_g( e, rs->eg_p );
		}
		return;
	}
	/* Edge has edge geometry */
	if( e->eg_p == rs->eg_p )  return;
	if( !rs->eg_p )  {
		/* Smash edge geom with isect line geom, and remember it */
		eg = e->eg_p;
		NMG_CK_EDGE_G(eg);
		VMOVE( eg->e_pt, rs->pt );
		VMOVE( eg->e_dir, rs->dir );
		rs->eg_p = eg;
		return;
	}
	/*
	 * Edge has an edge geometry struct, different from that of isect line.
	 * Force all uses of this edge geom to take on isect line's geometry.
	 * Everywhere e->eg_p is seen, replace with rs->eg_p.
	 */
	nmg_move_eg( e->eg_p, rs->eg_p, rs->sA );
	nmg_move_eg( e->eg_p, rs->eg_p, rs->sB );
}

/*
 *  State machine transition tables
 *  Indexed by MNG_V_ASSESSMENT values.
 */

struct state_transitions {
	int	assessment;
	int	new_state;
	int	action;
};

static CONST struct state_transitions nmg_state_is_out[17] = {
	{ NMG_LEFT_LEFT,	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ NMG_LEFT_RIGHT,	NMG_STATE_IN,		NMG_ACTION_VFY_EXT },
	{ NMG_LEFT_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_VFY_EXT },
	{ NMG_LEFT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_RIGHT,	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_RIGHT,	NMG_STATE_ON_R,		NMG_ACTION_VFY_EXT },
	{ NMG_ON_REV_ON_FORW,	NMG_STATE_ON_N,		NMG_ACTION_NONE },
	{ NMG_ON_REV_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LONE,		NMG_STATE_OUT,		NMG_ACTION_NONE }
};

static CONST struct state_transitions nmg_state_is_on_L[17] = {
	{ NMG_LEFT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_RIGHT,	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_LEFT,	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_RIGHT,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_ON_REV_LEFT,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
	{ NMG_ON_REV_RIGHT,	NMG_STATE_ON_B,		NMG_ACTION_VFY_EXT },
	{ NMG_ON_REV_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_VFY_MULTI },
	{ NMG_ON_REV_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LONE,		NMG_STATE_ON_L,		NMG_ACTION_LONE_V_ESPLIT }
};

static CONST struct state_transitions nmg_state_is_on_R[17] = {
	{ NMG_LEFT_LEFT,	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_FORW,	NMG_STATE_ON_B,		NMG_ACTION_NONE },
	{ NMG_LEFT_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_RIGHT_LEFT,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
	{ NMG_RIGHT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_FORW,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
	{ NMG_RIGHT_ON_REV,	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_REV,	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ NMG_LONE,		NMG_STATE_ON_R,		NMG_ACTION_LONE_V_ESPLIT }
};

static CONST struct state_transitions nmg_state_is_on_B[17] = {
	{ NMG_LEFT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_REV,	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
	{ NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_REV,	NMG_STATE_ON_L,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_LEFT,	NMG_STATE_ON_R,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_RIGHT,	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
	{ NMG_ON_FORW_ON_FORW,	NMG_STATE_ON_B,		NMG_ACTION_VFY_MULTI },
	{ NMG_ON_FORW_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_REV,	NMG_STATE_ON_B,		NMG_ACTION_VFY_MULTI },
	{ NMG_LONE,		NMG_STATE_ON_B,		NMG_ACTION_LONE_V_ESPLIT }
};

static CONST struct state_transitions nmg_state_is_on_N[17] = {
	{ NMG_LEFT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_RIGHT,	NMG_STATE_OUT,		NMG_ACTION_VFY_MULTI },
	{ NMG_LEFT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_REV,	NMG_STATE_ON_L,		NMG_ACTION_VFY_MULTI },
	{ NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_RIGHT,	NMG_STATE_ON_R,		NMG_ACTION_VFY_MULTI },
	{ NMG_ON_FORW_ON_FORW,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
	{ NMG_ON_FORW_ON_REV,	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_REV,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
	{ NMG_LONE,		NMG_STATE_ON_N,		NMG_ACTION_LONE_V_ESPLIT }
};

static CONST struct state_transitions nmg_state_is_in[17] = {
	{ NMG_LEFT_LEFT,	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
	{ NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_LEFT_ON_FORW,	NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
	{ NMG_LEFT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_RIGHT_LEFT,	NMG_STATE_OUT,		NMG_ACTION_CUTJOIN },
	{ NMG_RIGHT_RIGHT,	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
	{ NMG_RIGHT_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_CUTJOIN },
	{ NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_FORW_ON_FORW,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_ON_FORW_ON_REV,	NMG_STATE_OUT,		NMG_ACTION_NONE },
	{ NMG_ON_REV_LEFT,	NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
	{ NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
	{ NMG_ON_REV_ON_FORW,	NMG_STATE_ON_B,		NMG_ACTION_CUTJOIN },
	{ NMG_ON_REV_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
	{ NMG_LONE,		NMG_STATE_IN,		NMG_ACTION_LONE_V_JAUNT }
};

/*
 *			N M G _ I N S E R T _ V U _ I F _ O N _ E D G E
 *
 *	This code checks if the vertex from a loop of a single vertex lies on
 *	an edge containing vu2. If so, the edge is split, vu1 is inserted,
 *	the loop containing vu1 is killed, and the newly created edgeuse is
 *	returned in "new_eu".
 *
 *		returns:
 *			0 - did nothing
 *			1 - inserted vu1 and created a new edge
 */
int
nmg_insert_vu_if_on_edge( vu1 , vu2 , new_eu , tol )
struct vertexuse *vu1;	/* vertexuse from a loop of a single vertex */
struct vertexuse *vu2;	/* vertexuse from another loop */
struct edgeuse *new_eu;	/* use of new edge that may be created (implicit return ) */
struct rt_tol *tol;	/* tolerance for collinearity check */
{
	struct edgeuse *eu_from;	/* edgeuse that starts at end of vu2's eu */
	struct edgeuse *eu_to;		/* edgeuse that terminates at vu2 */
	vect_t eu_vect;			/* edge vector starting or terminating at vu2 */
	vect_t vect_to_loop;		/* vector from start of eu_vect to vu1 */
	fastf_t eu_len_sq;		/* square of length of edge containing vu2 */
	fastf_t dist_to_loop_sq;	/* square of distance between vu1 and vu2 */

	NMG_CK_VERTEXUSE( vu1 );
	if( *vu1->up.magic_p != NMG_LOOPUSE_MAGIC )
		rt_bomb( "nmg_insert_vu_if_on_edge: vu1 is not from a loop of a single vertex" );

	NMG_CK_VERTEXUSE( vu2 );
	if( *vu2->up.magic_p != NMG_EDGEUSE_MAGIC )
		rt_bomb( "nmg_insert_vu_if_on_edge: vu2 is not from an edgeuse" );

	RT_CK_TOL( tol );

	if(rt_g.NMG_debug&DEBUG_FCUT)
		rt_log( "nmg_insert_vu_if_on_edge: vu1=x%x, vu2=x%x\n" , vu1 , vu2 );

	eu_from = RT_LIST_PNEXT_CIRC( edgeuse , vu2->up.eu_p );
	eu_to = RT_LIST_PLAST_CIRC( edgeuse , vu2->up.eu_p );
	if( rt_3pts_collinear( vu1->v_p->vg_p->coord , vu2->v_p->vg_p->coord , eu_from->vu_p->v_p->vg_p->coord , tol ))
	{
		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log( "\t points are collinear with vu2's eu ( %g , %g , %g ) -> ( %g , %g , %g )\n",
				V3ARGS( vu2->v_p->vg_p->coord ),
				V3ARGS( eu_from->vu_p->v_p->vg_p->coord ) );
		VSUB2( eu_vect , eu_from->vu_p->v_p->vg_p->coord , vu2->v_p->vg_p->coord );
		VSUB2( vect_to_loop , vu1->v_p->vg_p->coord , vu2->v_p->vg_p->coord );
		if( VDOT( eu_vect , vect_to_loop ) > 0.0 )
		{
			eu_len_sq = MAGSQ( eu_vect );
			dist_to_loop_sq = MAGSQ( vect_to_loop );
			if( dist_to_loop_sq < eu_len_sq )
			{
				struct edgeuse *new_eu;

				if(rt_g.NMG_debug&DEBUG_FCUT)
					rt_log( "\tvu1 is on vu2's eu, creating new edge (MAGSQ=%g, tol->dist_sq=%g)\n" , dist_to_loop_sq , tol->dist_sq );
				new_eu = nmg_esplit( vu1->v_p , vu2->up.eu_p );
				nmg_klu( vu1->up.lu_p );
				return( 1 );
			}
		}
	}
	if( rt_3pts_collinear( vu1->v_p->vg_p->coord , vu2->v_p->vg_p->coord , eu_to->vu_p->v_p->vg_p->coord , tol ))
	{
		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log( "\t points are collinear with eu that ends at vu2 ( %g , %g , %g ) -> ( %g , %g , %g )\n",
				V3ARGS( eu_to->vu_p->v_p->vg_p->coord ),
				V3ARGS( vu2->v_p->vg_p->coord ) );
		VSUB2( eu_vect , vu2->v_p->vg_p->coord , eu_to->vu_p->v_p->vg_p->coord );
		VSUB2( vect_to_loop , vu1->v_p->vg_p->coord , eu_to->vu_p->v_p->vg_p->coord );
		if( VDOT( eu_vect , vect_to_loop ) > 0.0 )
		{
			eu_len_sq = MAGSQ( eu_vect );
			dist_to_loop_sq = MAGSQ( vect_to_loop );
			if( dist_to_loop_sq < eu_len_sq )
			{
				struct edgeuse *new_eu;

				if(rt_g.NMG_debug&DEBUG_FCUT)
					rt_log( "\tvu1 is on eu that ends at vu2, creating new edge (MAGSQ=%g, tol->dist_sq=%g)\n" , dist_to_loop_sq , tol->dist_sq );
				new_eu = nmg_esplit( vu1->v_p , eu_to );
				nmg_klu( vu1->up.lu_p );
				return( 1 );
			}
		}
	}
	return( 0 );
}

/*
 *			N M G _ F A C E _ S T A T E _ T R A N S I T I O N
 *
 *  Given current (old) state, assess the current vertexuse, and
 *  pull the appropriate action and new state from the tables.
 *  Then perform the indicated action.
 *
 *  The real work happens in the nmg_assess_vu() and in the tables.
 *
 *  Explicit Returns -
 *	Nothing useful yet.
 *
 *  Implicit Returns -
 *	Modifications to the NMG shell being operated on.
 *	Updated state etc. in nmg_ray_state structure.
 */
int
nmg_face_state_transition( rs, pos, multi, other_rs_state )
struct nmg_ray_state	*rs;
int			pos;
int			multi;
int			other_rs_state;
{
	struct vertexuse	*vu;
	int			assessment;
	int			old_state;
	int			new_state;
	CONST struct state_transitions	*stp;
	struct vertexuse	*prev_vu;
	struct edgeuse		*eu;
	struct loopuse		*lu;
	struct faceuse		*fu;
	struct loopuse		*prev_lu;
	struct edgeuse	*first_new_eu;
	struct edgeuse	*second_new_eu;
	int			e_assessment;
	int			action;
	int			e_pos;

	vu = rs->vu[pos];
	NMG_CK_VERTEXUSE(vu);
	RT_CK_TOL(rs->tol);

	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("nmg_face_state_transition(vu x%x, pos=%d) START\n",
			vu, pos);
		rt_log("Plotting this loopuse, before action:\n");
		nmg_pr_lu_briefly(nmg_find_lu_of_vu(vu), (char *)0);
		nmg_face_lu_plot(nmg_find_lu_of_vu(vu), rs->vu[0], rs->vu[rs->nvu-1] );
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		nmg_vfu( &rs->fu1->s_p->fu_hd, rs->fu1->s_p );
		nmg_vfu( &rs->fu2->s_p->fu_hd, rs->fu2->s_p );
nmg_fu_touchingloops(rs->fu1);
nmg_fu_touchingloops(rs->fu2);
	}

	assessment = nmg_assess_vu( rs, pos );
	old_state = rs->state;
	switch( old_state )  {
	default:
	case NMG_STATE_ERROR:
		rt_bomb("nmg_face_state_transition: was in ERROR state\n");
	case NMG_STATE_OUT:
		stp = &nmg_state_is_out[assessment];
		break;
	case NMG_STATE_ON_L:
		stp = &nmg_state_is_on_L[assessment];
		break;
	case NMG_STATE_ON_R:
		stp = &nmg_state_is_on_R[assessment];
		break;
	case NMG_STATE_ON_B:
		stp = &nmg_state_is_on_B[assessment];
		break;
	case NMG_STATE_ON_N:
		stp = &nmg_state_is_on_N[assessment];
		break;
	case NMG_STATE_IN:
		stp = &nmg_state_is_in[assessment];
		break;
	}

	if( assessment != stp->assessment )  {
		rt_log("assessment=%d, stp->assessment=%d, error\n", assessment, stp->assessment);
		rt_bomb("nmg_face_state_transition() bad table\n");
	}
	action = stp->action;
	new_state = stp->new_state;
	rs->last_action = action;
#if 1
	/*
	 *  Major optimization here.
	 *  If the state machine for the other face is still in OUT state,
	 *  then take no actions in this face,
	 *  because any cutting or joining done here will have no effect
	 *  on the final result of the boolean, it's just extra work.
	 *  This can reduce the amount of unnecessary topology by 75% or more.
	 */
/* XXX This is a bit too agressive.  r5 */
	if( other_rs_state == NMG_STATE_OUT && action != NMG_ACTION_ERROR &&
	    action != NMG_ACTION_NONE )  {
		action = NMG_ACTION_NONE_OPTIM;
	}
#endif

	if(rt_g.NMG_debug&DEBUG_FCUT)  {
		rt_log("nmg_face_state_transition(vu x%x, pos=%d)\n\told=%s, assessed=%s, new=%s, action=%s\n",
			vu, pos,
			nmg_state_names[old_state], nmg_v_assessment_names[assessment],
			nmg_state_names[new_state], action_names[action] );
	}

	/*
	 *  Force edge geometry that lies on the intersection line
	 *  to use the edge_g structure of the intersection line (ray).
	 */
	if( NMG_V_ASSESSMENT_PREV(assessment) == NMG_E_ASSESSMENT_ON_REV )  {
		eu = nmg_find_eu_of_vu(vu);
		eu = RT_LIST_PLAST_CIRC( edgeuse, eu );
		NMG_CK_EDGEUSE(eu);
		if( rs->eg_p && eu->e_p->eg_p != rs->eg_p )  {
rt_log("force prev eu to ray\n");
			nmg_edge_geom_isect_line( eu->e_p, rs );
		}
	}
	if( NMG_V_ASSESSMENT_NEXT(assessment) == NMG_E_ASSESSMENT_ON_FORW )  {
		eu = nmg_find_eu_of_vu(vu);
		NMG_CK_EDGEUSE(eu);
		if( rs->eg_p && eu->e_p->eg_p != rs->eg_p )  {
rt_log("force next eu to ray\n");
			nmg_edge_geom_isect_line( eu->e_p, rs );
		}
	}

	switch( action )  {
	default:
	case NMG_ACTION_ERROR:
	bomb:
	    {
		struct rt_vls	str;

		rt_log("nmg_face_state_transition: got action=ERROR\n");
	    	rt_vls_init(&str);
		rt_vls_printf(&str,"nmg_face_state_transition(vu x%x, pos=%d)\n\told=%s, assessed=%s, new=%s, action=%s\n",
			vu, pos,
			nmg_state_names[old_state], nmg_v_assessment_names[assessment],
			nmg_state_names[new_state], action_names[action] );
	     if( rt_g.debug || rt_g.NMG_debug )  {
		/* First, print this faceuse */
		lu = nmg_find_lu_of_vu( vu );
	     	NMG_CK_LOOPUSE(lu);
		/* Drop a plot file */
		rt_g.NMG_debug |= DEBUG_FCUT|DEBUG_PLOTEM;
		nmg_pl_comb_fu( 0, 1, lu->up.fu_p );
		/* Print the faceuse for later analysis */
		rt_log("Loop with the offending vertex\n");
		nmg_pr_lu_briefly(lu, (char *)0);
		rt_log("The whole face\n");
		nmg_pr_fu(lu->up.fu_p, (char *)0);
		nmg_face_lu_plot(lu, rs->vu[0], rs->vu[rs->nvu-1] );
		{
			FILE	*fp = fopen("error.pl", "w");
			nmg_pl_m(fp, nmg_find_model((long *)lu));
			fclose(fp);
			rt_log("wrote error.pl\n");
		}
		/* Store this face in a .g file for examination! */
		nmg_stash_model_to_file( "error.g", nmg_find_model((long*)lu), "nmg_fcut.c error dump" );
	     }
		/* Explode */
	    	rt_bomb(rt_vls_addr(&str));
	    }
	case NMG_ACTION_NONE:
	case NMG_ACTION_NONE_OPTIM:
		if( *(vu->up.magic_p) == NMG_LOOPUSE_MAGIC )  {
			lu = vu->up.lu_p;
			/* Drop this loop of a single vertex in sanitize() */
			if( lu->orientation == OT_UNSPEC );
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
		}
		break;
	case NMG_ACTION_VFY_EXT:
		/* Verify loop containing this vertex has external orientation */
		lu = nmg_find_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		switch( lu->orientation )  {
		case OT_SAME:
			break;
		default:
			rt_log("nmg_face_state_transition: VFY_EXT got orientation=%s\n",
				nmg_orientation(lu->orientation) );
			break;
		}
		break;
	case NMG_ACTION_VFY_MULTI:
		/*  Ensure that there are multiple vertexuse's at this
		 *  vertex along the ray.
		 *  If not, the table entry is illegal.
		 */
		if( multi )  break;
		rt_log("nmg_face_state_transition: VFY_MULTI had only 1 vertex\n");
		goto bomb;
	case NMG_ACTION_LONE_V_ESPLIT:
		/*
		 *  Split edge to include vertex from this lone vert loop.
		 *  This only happens in an "ON" state, so split the edge that
		 *  starts (or ends) with the previously seen vertex.
		 *  Note that the forward going edge may point the wrong way,
		 *  i.e., not lie on the ray at all.
		 *  Also note that the previous member(s) of vu[] may be
		 *  lone vert loops that were not processed due to optimization,
		 *  so it may be necessary to look back a ways to find
		 *  the vertexuse which started this ON edge.
		 */
		lu = nmg_find_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		for( e_pos = pos-1; e_pos >= 0; e_pos-- )  {
			prev_vu = rs->vu[e_pos];
			NMG_CK_VERTEXUSE(prev_vu);
			prev_lu = nmg_find_lu_of_vu( prev_vu );
			/* lu is lone vert loop; l_p is distinct from prev_lu->l_p */
			if( *prev_vu->up.magic_p == NMG_EDGEUSE_MAGIC )
				break;
			/* Not an edgeuse, prob. a loopuse, continue backwards */
#if 0
			rt_log("prev_vu->up is %s\n", rt_identify_magic(*prev_vu->up.magic_p) );
			nmg_pr_vu(prev_vu, "prev ");
			nmg_pr_vu(rs->vu[e_pos], "cur  ");
#endif
		}
		if( e_pos < 0 ) rt_bomb("nmg_face_state_transition: LONE_V_ESPLIT can't find start of edge!\n");
		eu = prev_vu->up.eu_p;
		NMG_CK_EDGEUSE(eu);
		e_assessment = nmg_assess_eu( eu, 1, rs, e_pos );	/* forw */
		if( e_assessment == NMG_E_ASSESSMENT_ON_FORW )  {
			/* "eu" (forw) is the right edge to split */
		} else {
			e_assessment = nmg_assess_eu( eu, 0, rs, e_pos ); /*rev*/
			if( e_assessment == NMG_E_ASSESSMENT_ON_REV )  {
				/* (reverse) "eu" is the right one */
				eu = RT_LIST_PLAST_CIRC( edgeuse, eu );
			} else {
				/* What went wrong? */
				rt_log("LONE_V_ESPLIT:  rev e_assessment = %s\n", nmg_e_assessment_names[e_assessment]);
				rt_bomb("nmg_face_state_transition: LONE_V_ESPLIT could not find ON edge to split\n");
			}
		}
		/* Break edge, update vu table with new value */
		if( vu->v_p == eu->vu_p->v_p )  {
			/* Edge already starts at same vertex */
			rs->vu[pos] = eu->vu_p;
		} else if( vu->v_p == eu->eumate_p->vu_p->v_p )  {
			/* Edge already ends at same vertex */
			rs->vu[pos] = RT_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p;
		} else {
			/* Break edge */
			rs->vu[pos] = nmg_ebreak( vu->v_p, eu )->vu_p;
		}
		/* Kill lone vertex loop (and vertexuse) */
		nmg_klu(lu);
		if(rt_g.NMG_debug&DEBUG_FCUT)  {
			rt_log("After LONE_V_ESPLIT, the final loop:\n");
			lu = nmg_find_lu_of_vu(rs->vu[pos]);
			NMG_CK_LOOPUSE(lu);
			nmg_pr_lu(lu, "   ");
			nmg_face_lu_plot(lu, rs->vu[0], rs->vu[rs->nvu-1] );
		}
		break;
	case NMG_ACTION_LONE_V_JAUNT:
		/*
		 * Take current loop on a jaunt from current (prev_vu) edge
		 * up to the vertex (vu) of this lone vertex loop,
		 * and back again.
		 * This only happens in "IN" state.
		 */
		prev_vu = rs->vu[pos-1];
		NMG_CK_VERTEXUSE(prev_vu);

		if( *prev_vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
			/* Both prev and current are lone vertex loops */
			rs->vu[pos] = nmg_join_2singvu_loops( prev_vu, vu );
		    	/* Set orientation */
			lu = nmg_find_lu_of_vu(rs->vu[pos]);
			NMG_CK_LOOPUSE(lu);
			/* If state is IN, this is a "crack" loop */
			nmg_set_lu_orientation( lu, old_state==NMG_STATE_IN );
		} else {
			rs->vu[pos] = nmg_join_singvu_loop( prev_vu, vu );
		}

		/*  We know edge geom is null, make it be the isect line */
		first_new_eu = RT_LIST_PLAST_CIRC(edgeuse, rs->vu[pos]->up.eu_p);
		nmg_edge_geom_isect_line( first_new_eu->e_p, rs );

		/* Fusing to this new edge will happen in nmg_face_cutjoin() */

		/* Recompute loop geometry.  Bounding box may have expanded */
		nmg_loop_g(nmg_find_lu_of_vu(rs->vu[pos])->l_p, rs->tol);

		if(rt_g.NMG_debug&DEBUG_FCUT)  {
			rt_log("After LONE_V_JAUNT, the final loop:\n");
			nmg_pr_lu_briefly(nmg_find_lu_of_vu(rs->vu[pos]), (char *)0);
			nmg_face_lu_plot(nmg_find_lu_of_vu(rs->vu[pos]), rs->vu[0], rs->vu[rs->nvu-1] );
		}
		break;
	case NMG_ACTION_CUTJOIN:
		/*
		 *  Cut loop into two, or join two into one.
		 *  The operation happens between the previous vu
		 *  and the current one.
		 *  If the two vu's are part of the same loop,
		 *  then cut the loop into two, otherwise
		 *  join the two loops into one.
		 */
		lu = nmg_find_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		prev_vu = rs->vu[pos-1];
		NMG_CK_VERTEXUSE(prev_vu);
		prev_lu = nmg_find_lu_of_vu( prev_vu );
		NMG_CK_LOOPUSE(prev_lu);

		if( lu->l_p == prev_lu->l_p )  {
			int is_crack;
			/* Same loop, cut into two */
			is_crack = nmg_loop_is_a_crack(lu);
			if(rt_g.NMG_debug&DEBUG_FCUT)
				rt_log("nmg_cut_loop(prev_vu=x%x, vu=x%x) is_crack=%d\n", prev_vu, vu, is_crack);
			prev_lu = nmg_cut_loop( prev_vu, vu );
			nmg_loop_g( lu->l_p, rs->tol );
			nmg_loop_g( prev_lu->l_p, rs->tol );

			nmg_lu_reorient( lu, rs->tol );
			nmg_lu_reorient( prev_lu, rs->tol );

			if(rt_g.NMG_debug&DEBUG_FCUT)  {
				rt_log("After CUT, the final loop:\n");
				nmg_pr_lu_briefly(nmg_find_lu_of_vu(rs->vu[pos]), (char *)0);
				nmg_face_lu_plot(nmg_find_lu_of_vu(rs->vu[pos]), rs->vu[0], rs->vu[rs->nvu-1] );
			}
			break;
		}
		/*
		 *  prev_vu and vu are in different loops,
		 *  join the two loops into one loop.
		 *  No edgeuses are deleted at this stage,
		 *  so some "snakes" may appear in the process.
		 */
		if(rt_g.NMG_debug&DEBUG_FCUT)
			rt_log("nmg_join_2loops(prev_vu=x%x, vu=x%x)\n",
			prev_vu, vu);

		if( *prev_vu->up.magic_p == NMG_LOOPUSE_MAGIC ||
		    *vu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
		    	/* One (or both) is a loop of a single vertex */
		    	/* This is the special boolean vertex marker */
		    	struct edgeuse *new_eu;

			if( *prev_vu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *vu->up.magic_p != NMG_LOOPUSE_MAGIC )  {
			    	if(rt_g.NMG_debug&DEBUG_FCUT)
			    		rt_log( "\tprev_vu is a vertex loop\n" );
			    	/* if prev_vu is geometrically on an edge that goes through vu,
			    	 * then split that edge at prev_vu */
/* XXX If there is an ON condition, this is bad. */
#if 0
			    	if( nmg_insert_vu_if_on_edge( prev_vu , vu , new_eu , rs->tol ) )
			    		rt_bomb("insert_vu_if_on_edge 1\n");
			    	if( nmg_insert_vu_if_on_edge( prev_vu , vu , new_eu , rs->tol ) )
			    		rs->vu[pos-1] = new_eu->vu_p;
			    	else
#endif
				    	rs->vu[pos-1] = nmg_join_singvu_loop( vu, prev_vu );
			} else if( *vu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *prev_vu->up.magic_p != NMG_LOOPUSE_MAGIC )  {
			    	if(rt_g.NMG_debug&DEBUG_FCUT)
			    		rt_log( "\tvu is a vertex loop\n" );
			    	/* if vu is geometrically on an edge that goes through prev_vu,
			    	 * then split that edge at vu */
/* XXX If there is an ON condition, this is bad. */
#if 0
			    	if( nmg_insert_vu_if_on_edge( vu , prev_vu , new_eu , rs->tol ) )
			    		rt_bomb("insert_vu_if_on_edge 2\n");
			    	if( nmg_insert_vu_if_on_edge( vu , prev_vu , new_eu , rs->tol ) )
			    		rs->vu[pos] = new_eu->vu_p;
			    	else
#endif
				    	rs->vu[pos] = nmg_join_singvu_loop( prev_vu, vu );
			} else {
				/* Both are loops of single vertex */
			    	if(rt_g.NMG_debug&DEBUG_FCUT)
			    		rt_log( "\tprev_vu and vu are vertex loops\n" );
				vu = rs->vu[pos] = nmg_join_2singvu_loops( prev_vu, vu );
			    	/* Set orientation */
				lu = nmg_find_lu_of_vu(vu);
				NMG_CK_LOOPUSE(lu);
				nmg_set_lu_orientation( lu, old_state==NMG_STATE_IN );
			}
		} else {
			rs->vu[pos] = nmg_join_2loops( prev_vu, vu );
		}

		/* XXX If an edge has been built between prev_vu and vu,
		 * force it's geometry to lie on the ray.
		 */
		vu = rs->vu[pos];
		lu = nmg_find_lu_of_vu(vu);
		NMG_CK_LOOPUSE(lu);
		prev_vu = rs->vu[pos-1];
		eu = prev_vu->up.eu_p;
		NMG_CK_EDGEUSE(eu);
		first_new_eu = RT_LIST_PLAST_CIRC(edgeuse, rs->vu[pos]->up.eu_p);
		NMG_CK_EDGEUSE(first_new_eu);
		if( eu == first_new_eu )  {
			/*  We know edge geom is null, make it be the isect line */
			nmg_edge_geom_isect_line( first_new_eu->e_p, rs );
		}

		/* Recompute loop geometry.  Bounding box may have expanded */
		nmg_loop_g(lu->l_p, rs->tol);

		if(rt_g.NMG_debug&DEBUG_FCUT)  {
			rt_log("After JOIN, the final loop:\n");
			nmg_pr_lu_briefly(lu, (char *)0);
			nmg_face_lu_plot( lu, rs->vu[0], rs->vu[rs->nvu-1] );
		}
		break;
	}

	rs->state = new_state;

	if( rt_g.NMG_debug & DEBUG_VERIFY )  {
		/* Verify both faces are still OK */
		nmg_vfu( &rs->fu1->s_p->fu_hd, rs->fu1->s_p );
		nmg_vfu( &rs->fu2->s_p->fu_hd, rs->fu2->s_p );
nmg_fu_touchingloops(rs->fu1);
nmg_fu_touchingloops(rs->fu2);
	}
}
                                                                                                                                      