/* 
 * Program to multiply two rectangualar matrizes A(n,m) * B(m,n), where
 * (n < m) and (n mod 16 = 0) and (m mod n = 0). (Otherwise fill with 0s 
 * to fit the shape.)
 *
 * written by Harald Prokop (prokop@mit.edu) Fall 97.
 */

static const char *ident __attribute__((__unused__))
     = "$HeadURL: https://bradley.csail.mit.edu/svn/repos/cilk/5.4.3/examples/rectmul.cilk $ $LastChangedBy: bradley $ $Rev: 517 $ $Date: 2005-10-12 12:58:40 -0400 (Wed, 12 Oct 2005) $";

/*
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "getoptions.h"
#include "pp_time.h"
#include "cblas.h"

#ifndef ALGO
#define ALGO 0
#endif

#ifndef BLOCK_EDGE
#define BLOCK_EDGE 64
#endif
#define BLOCK_SIZE (BLOCK_EDGE*BLOCK_EDGE)
typedef double block[BLOCK_SIZE];
typedef block *pblock;

double min;
double max;
int count;
double sum;

#include "../common/basic_ops.c"


/* Checks if each A[i,j] of a martix A of size nb x nb blocks has value v 
 */

int 
check_block (block *R, double v)
{
  int i;
  
  for (i = 0; i < BLOCK_SIZE; i++) 
    if (((double *) R)[i] != v) 
      return 1;
  
  return 0;
}

int 
check_matrix(block *R, long x, long y, long o, double v)
{
  int a,b;

  if ((x*y) == 1) 
    return check_block(R,v);
  else {
    if (x>y) {
      a = cilk_spawn check_matrix(R,x/2,y,o,v);
      b = cilk_spawn check_matrix(R+(x/2)*o,(x+1)/2,y,o,v);
      cilk_sync;
    } 
    else {
      a = cilk_spawn check_matrix(R,x,y/2,o,v);
      b = cilk_spawn check_matrix(R+(y/2),x,(y+1)/2,o,v);
      cilk_sync;
    }
  }  
  return (a+b);
}

long 
add_block(block *T, block *R)
{
  long i;
  
  for (i = 0; i < BLOCK_SIZE; i += 4) {
    ((double *) R)[i] += ((double *) T)[i];
    ((double *) R)[i + 1] += ((double *) T)[i + 1];
    ((double *) R)[i + 2] += ((double *) T)[i + 2];
    ((double *) R)[i + 3] += ((double *) T)[i + 3];
  }
  
  return BLOCK_SIZE;
}
 
/* Add matrix T into matrix R, where T and R are bl blocks in size 
 *
 */
 
long 
add_matrix(block *T, long ot, block *R, long or_, long x, long y)
{
  long flops0, flops1;
  
  if ((x+y)==2) {
    return add_block(T,R);
  } 
  else {
    if (x>y) {
      flops0 = cilk_spawn add_matrix(T,ot,R,or_,x/2,y);
      flops1 = cilk_spawn add_matrix(T+(x/2)*ot,ot,R+(x/2)*or_,or_,(x+1)/2,y);
    } 
    else {
      flops0 = cilk_spawn add_matrix(T,ot,R,or_,x,y/2);
      flops1 = cilk_spawn add_matrix(T+(y/2),ot,R+(y/2),or_,x,(y+1)/2);
    }
    
    cilk_sync;
    return flops0 + flops1;
  }
}


void 
init_block(block* R, double v)
{
  int i;

  for (i = 0; i < BLOCK_SIZE; i++)
    ((double *) R)[i] = v;
}

void 
init_matrix(block *R, long x, long y, long o, double v)
{
  if ((x+y)==2) 
    init_block(R,v);
  else  {
    if (x>y) {
      cilk_spawn init_matrix(R,x/2,y,o,v);
      cilk_spawn init_matrix(R+(x/2)*o,(x+1)/2,y,o,v);
    } 
    else {
      cilk_spawn init_matrix(R,x,y/2,o,v);
      cilk_spawn init_matrix(R+(y/2),x,(y+1)/2,o,v);
    }
    cilk_sync;
  }
}

long 
multiply_matrix(block *A, long oa, block *B, long ob, long x, long y, long z, block *R, long or_, int add)
{
  if ((x+y+z) == 3) {
    if (add)
      return mult_add_block(A, B, R);
    else
      return multiply_block(A, B, R);
  } 
  else {
    long flops0, flops1;
    
    if ((x>=y) && (x>=z)) {  
      flops0 = cilk_spawn multiply_matrix(A,oa,B,ob,x/2,y,z,R,or_,add);
      flops1 = cilk_spawn multiply_matrix(A+(x/2)*oa,oa,B,ob,(x+1)/2,y,z,R+(x/2)*or_,or_,add);
    } 
    else {
      if ((y>x) && (y>z)){
            flops0 = cilk_spawn multiply_matrix(A+(y/2),oa,B+(y/2)*ob,ob,x,(y+1)/2,z,R,or_,add);

            if (/*SYNCHED*/true) {
	      cilk_sync;
	      flops1 = cilk_spawn multiply_matrix(A,oa,B,ob,x,y/2,z,R,or_,1);
            } 
	    else {
	      block *tmp = (block *)alloca(x*z*sizeof(block));

	      flops0 = cilk_spawn multiply_matrix(A,oa,B,ob,x,y/2,z,tmp,z,0);
	      cilk_sync;
	      
	      flops1 = cilk_spawn add_matrix(tmp,z,R,or_,x,z);
            }
      }
      else {
	flops0 = cilk_spawn multiply_matrix(A,oa,B,ob,x,y,z/2,R,or_,add);
	flops1 = cilk_spawn multiply_matrix(A,oa,B+(z/2),ob,x,y,(z+1)/2,(R+(z/2)),or_,add);
      } 
    }
    
    cilk_sync;
    return flops0 + flops1;
  }
}

int 
run(long x, long y, long z, int check)
{
  block *A, *B, *R;
  long flops;
  double f;
  pp_time_t tm;
  memset( &tm, 0, sizeof(tm) );
  
  A = (block *)malloc(x*y * sizeof(block));
  B = (block *)malloc(y*z * sizeof(block));
  R = (block *)malloc(x*z * sizeof(block));
  
  cilk_spawn init_matrix(A, x, y, y, 1.0);
  cilk_spawn init_matrix(B, y, z, z, 1.0); 
  cilk_spawn init_matrix(R, x, z, z, 0.0);
  cilk_sync;
  
  /* Timing. "Start" timers */
  cilk_sync;			
  pp_time_start( &tm );
  
  flops = cilk_spawn multiply_matrix(A,y,B,z,x,y,z,R,z,0);
  cilk_sync;
  
  /* Timing. "Stop" timers */
  pp_time_end( &tm );
  
  f = (double) flops / pp_time_read(&tm);
  
  if (check) {
    check = cilk_spawn check_matrix(R, x, z, z, y*BLOCK_EDGE);
    cilk_sync;
  }

  if (min>f) min=f;
  if (max<f) max=f;
  sum+=f;
  count++;
  
  if (check)
    printf("WRONG RESULT!\n");
  else {	
    printf("\nCilk Example: rectmul\n");
    printf("Options: x = %ld\n", BLOCK_EDGE*x);
    printf("         y = %ld\n", BLOCK_EDGE*y);
    printf("         z = %ld\n\n", BLOCK_EDGE*z);
    printf("Mflops        = %4f \n", (double) flops / pp_time_read(&tm));
    printf("Running time = %g %s\n", pp_time_read(&tm), pp_time_unit() );
  }
  
  free(A);
  free(B);
  free(R);
  
  return 0;
}


int usage(void) {
  fprintf(stderr, "\nUsage: rectmul [<cilk-options>] [<options>]\n\n");
  return 1;
}

char *specifiers[] = { "-x", "-y", "-z", "-c", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, INTARG, INTARG, BOOLARG, BENCHMARK, BOOLARG, 0 };

int 
cilk_main(int argc, char *argv[])
{
  int x, y, z, benchmark, help, t, check;

  /* standard benchmark options */
  x = 512;
  y = 512;
  z = 512;
  check = 0;

  get_options(argc, argv, specifiers, opt_types, &x, &y, &z, &check, &benchmark, &help);

  if (help) return usage();

  if (benchmark) {
    switch (benchmark) {
    case 1:      /* short benchmark options -- a little work*/
      x = 256;
      y = 256;
      z = 256;
     break;
    case 2:      /* standard benchmark options*/
      x = 512;
      y = 512;
      z = 512;
      break;
    case 3:      /* long benchmark options -- a lot of work*/
      x = 2048;
      y = 2048;
      z = 2048;
      break;
    }
  }

  x = x/BLOCK_EDGE;
  y = y/BLOCK_EDGE;
  z = z/BLOCK_EDGE;

  if (x<1) x=1;
  if (y<1) y=1;
  if (z<1) z=1;
  
  t = cilk_spawn run(x,y,z,check);
  cilk_sync;

  return t; 
}
