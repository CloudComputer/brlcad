/*
 *			V I E W
 *
 * Ray Tracing program, lighting model
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	March 27, 1984
 *
 * Notes -
 *	The normals on all surfaces point OUT of the solid.
 *	The incomming light rays point IN.  Thus the sign change.
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "debug.h"

extern int ikfd;		/* defined in iklib.o */
extern int ikhires;		/* defined in iklib.o */

extern int outfd;		/* defined in rt.c */

#define MAX_LINE	1024	/* Max pixels/line */
static long scanline[MAX_LINE];	/* 1 scanline pixel buffer */
static long *pixelp;		/* pointer to first empty pixel */
static int scanbytes;		/* # of bytes of scanline to be written */

#define BACKGROUND	0x00404040		/* Grey */
vect_t l0color = {  28,  28, 255 };		/* R, G, B */
vect_t l1color = { 255,  28,  28 };
vect_t l2color = { 255, 255, 255 };		/* White */
extern vect_t l0vec;
extern vect_t l1vec;
extern vect_t l2vec;

viewit( PartHeadp, rayp, xscreen, yscreen )
struct partition *PartHeadp;
struct ray *rayp;
int xscreen, yscreen;
{
	static long inten;
	static double diffuse2, cosI2;
	static double diffuse1, cosI1;
	static double diffuse0, cosI0;
	static vect_t work0, work1;
	static int r,g,b;
	register struct partition *pp;
	register struct hit *hitp;

	pp = PartHeadp->pt_forw;
	if( pp == PartHeadp )  {
		printf("viewit:  null partition list\n");
		return;
	}
	hitp = pp->pt_inhit;

	/*
	 * Diffuse reflectance from each light source
	 */

	/* For light from eye, use l0vec = rayp->r_dir, -VDOT */
	diffuse0 = 0;
	if( (cosI0 = VDOT(hitp->hit_normal, l0vec)) >= 0.0 )
		diffuse0 = cosI0 * 0.5;		/* % from this src */
	diffuse1 = 0;
	if( (cosI1 = VDOT(hitp->hit_normal, l1vec)) >= 0.0 )
		diffuse1 = cosI1 * 0.5;		/* % from this src */
	diffuse2 = 0;
	if( (cosI2 = VDOT(hitp->hit_normal, l2vec)) >= 0.0 )
		diffuse2 = cosI2 * 0.2;		/* % from this src */

	if(debug&DEBUG_HITS)  {
		hit_print( " In", hitp );
		printf("cosI0=%f, diffuse0=%f   ", cosI0, diffuse0 );
	}

#ifdef notyet
	/* Specular reflectance from first light source */
	/* reflection = (2 * cos(i) * NormalVec) - IncidentVec */
	/* cos(s) = -VDOT(reflection, r_dir) = cosI0 */
	f = 2 * cosI1;
	VSCALE( work, hitp->hit_normal, f );
	VSUB2( reflection, work, l1vec );
	if( not_shadowed && cosI0 > cosAcceptAngle )
		/* Do specular return */;
#endif notyet

	VSCALE( work0, l0color, diffuse0 );
	VSCALE( work1, l1color, diffuse1 );
	VADD2( work0, work0, work1 );
	VSCALE( work1, l2color, diffuse2 );
	VADD2( work0, work0, work1 );
	if(debug&DEBUG_HITS)  VPRINT("RGB", work0);

	r = work0[0];
	g = work0[1];
	b = work0[2];
	if( r > 255 ) r = 255;
	if( g > 255 ) g = 255;
	if( b > 255 ) b = 255;
	if( r<0 || g<0 || b<0 )  {
		VPRINT("@@ Negative RGB @@", work0);
		inten = 0x0080FF80;
	} else {
		inten = (b << 16) |		/* B */
			(g <<  8) |		/* G */
			(r);			/* R */
	}

	if( ikfd > 0 )
		ikwpixel( xscreen, yscreen, inten);
	if( outfd > 0 )
		*pixelp++ = inten;
}

wbackground( x, y )
int x, y;
{
	if( ikfd > 0 )
		ikwpixel( x, y, BACKGROUND );
	if( outfd > 0 )
		*pixelp++ = BACKGROUND;
}

/*
 *  			H I T _ P R I N T
 */
hit_print( str, hitp )
char *str;
register struct hit *hitp;
{
	printf("** %s HIT, dist=%f\n", str, hitp->hit_dist );
	VPRINT("** Point ", hitp->hit_point );
	VPRINT("** Normal", hitp->hit_normal );
}

/*
 *  			D E V _ S E T U P
 *  
 *  Prepare the Ikonas display for operation with
 *  npts x npts of useful pixels.
 */
dev_setup(npts)
int npts;
{
	if( npts > MAX_LINE )  {
		printf("view:  %d pixels/line is too many\n", npts);
		exit(12);
	}
	if( outfd > 0 )  {
		/* Output is destined for a pixel file */
		pixelp = &scanline[0];
		if( npts > 512 )
			scanbytes = MAX_LINE * sizeof(long);
		else
			scanbytes = 512 * sizeof(long);
	}  else  {
		/* Output directly to Ikonas */
		if( npts > 512 )
			ikhires = 1;

		ikopen();
		load_map(1);		/* Standard map: linear */
		ikclear();
		if( npts <= 64 )  {
			ikzoom( 7, 7 );		/* 1 pixel gives 8 */
			ikwindow( (0)*4, 4063+29 );
		} else if( npts <= 128 )  {
			ikzoom( 3, 3 );		/* 1 pixel gives 4 */
			ikwindow( (0)*4, 4063+25 );
		} else if ( npts <= 256 )  {
			ikzoom( 1, 1 );		/* 1 pixel gives 2 */
			ikwindow( (0)*4, 4063+17 );
		}
	}
}

/*
 *  			D E V _ E O L
 *  
 *  This routine is called by main when the end of a scanline is
 *  reached.
 */
dev_eol( y )
int y;
{
	if( outfd > 0 )  {
		write( outfd, (char *)scanline, scanbytes );
		bzero( (char *)scanline, scanbytes );
		pixelp = &scanline[0];
	}
}
