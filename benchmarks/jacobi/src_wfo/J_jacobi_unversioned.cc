/*
 * Copyright (C) 2011 Hans Vandierendonck (hvandierendonck@acm.org)
 * Copyright (C) 2011 George Tzenakis (tzenakis@ics.forth.gr)
 * Copyright (C) 2011 Dimitrios S. Nikolopoulos (dsn@ics.forth.gr)
 * 
 * This file is part of Swan.
 * 
 * Swan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Swan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Swan.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
* Copyright (c) 2008, BSC (Barcelon Supercomputing Center)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY BSC ''AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <sys/time.h>
#include <time.h>
#include "wf_interface.h"
#include "pp_time.h"

// #define NB 512
#define NB 128
#define B 32
#define FALSE (0)
#define TRUE (1)

using obj::unversioned;
using obj::indep;
using obj::outdep;
using obj::inoutdep;

typedef float (*vector_t);
typedef unversioned<float[B]> h_vector_t;
typedef indep<float[B]> vin;
typedef outdep<float[B]> vout;

typedef float (*block_t)[B];
typedef unversioned<float[B][B]> h_block_t;
typedef indep<float[B][B]> bin;
typedef inoutdep<float[B][B]> binout;

h_block_t A[NB][NB];

void alloc_and_genmat ()
{
   int init_val, i, j, ii, jj;
   block_t p;

   init_val = 1325;

   for (ii = 0; ii < NB; ii++) 
     for (jj = 0; jj < NB; jj++)
     {
	 // A[ii][jj] = (float *)malloc(B*B*sizeof(float));
       	 // if (A[ii][jj]==NULL) { printf("Out of memory\n"); exit(1); }
	 p=(block_t)A[ii][jj];
        for (i = 0; i < B; i++) 
           for (j = 0; j < B; j++) {
              init_val = (3125 * init_val) % 65536;
      	      p[i][j] = (float)((init_val - 32768.0) / 16384.0);
           }
     }
}



long usecs (void)
{
  struct timeval t;

  gettimeofday(&t,NULL);
  return t.tv_sec*1000000+t.tv_usec;
}

//#pragma css task output(v)
void clear(vout v)
{
    int i;
                                                                               
  for (i=0; i<B; i++)
      (*v)[i] = (float)0.0;
}

//#pragma css task input(A[B][B]) output(v[B])
void getlastrow(bin A, vout v)
{
   int j;
   for (j=0; j<B; j++) (*v)[j]=(*A)[B-1][j];
}

//#pragma css task input(A[32][32]) output(v[32])
void getlastcol(bin A, vout v)
{
   int i;
   for (i=0; i<B; i++) (*v)[i]=(*A)[i][B-1];
}

//#pragma css task input(A[32][32]) output(v[32])
void getfirstrow(bin A, vout v)
{
   int j;
   for (j=0; j<B; j++) (*v)[j]=(*A)[0][j];
}

//#pragma css task input(A[32][32]) output(v[32])
void getfirstcol(bin A, vout v)
{
   int i;
   for (i=0; i<B; i++) (*v)[i]=(*A)[i][0];
}

//#pragma css task input (lefthalo[32], tophalo[32], righthalo[32], bottomhalo[32]) inout(A[32][32]) highpriority
void jacobi(vin lefthalo, vin tophalo,
            vin righthalo, vin bottomhalo,
            binout A )
{
   int i,j;
   float left,top, right, bottom;

   block_t Ablock = *A;

   for (i=0;(i<B); i++)
     for (j=0;j<B; j++)
     {
       left=(j==0? (*lefthalo)[j]:Ablock[i][j-1]);
       top=(i==0? (*tophalo)[i]:Ablock[(i-1)][j]);
       right=(j==B-1? (*righthalo)[i]:Ablock[i][j+1]);
       bottom=(i==B-1? (*bottomhalo)[i]:Ablock[(i+1)][j]);

       Ablock[i][j] = 0.2*(Ablock[i][j] + left + top + right + bottom);
     }

}

void compute(int niters)
{
   int iters;
   int ii, jj;
   h_vector_t lefthalo, tophalo, righthalo, bottomhalo;

   for (iters=0; iters<niters; iters++)
      for (ii=0; ii<NB; ii++) 
         for (jj=0; jj<NB; jj++) {
            if (ii>0) {
		spawn( getlastrow, (bin)A[ii-1][jj], (vout)tophalo);
		}
            else {
		spawn( clear, (vout)tophalo);
		}
            if (jj>0) {
		spawn( getlastcol, (bin)A[ii][jj-1], (vout)lefthalo);
		}
            else {
		spawn( clear, (vout)lefthalo);
		}
            if (ii<NB-1) {
		spawn( getfirstrow, (bin)A[ii+1][jj], (vout)bottomhalo);
		}
            else {
		spawn( clear, (vout)bottomhalo);
		}
            if (jj<NB-1) {
		spawn( getfirstcol, (bin)A[ii][jj+1], (vout)righthalo);
		}
            else {
		spawn( clear, (vout)lefthalo);
		}

	    spawn( jacobi,(vin)lefthalo, (vin)tophalo,
		   (vin)righthalo, (vin)bottomhalo,
		   (binout)A[ii][jj]);
         }

   ssync();
}

int main(int argc, char* argv[])
{
    int niters;
    pp_time_t tm;
    memset( &tm, 0, sizeof(tm) );

    if( argc > 1 ) {
       niters = atoi( argv[1] );
    } else
       niters = 1;

    alloc_and_genmat();

    pp_time_start( &tm );
    run(compute,niters);
    pp_time_end( &tm );

    printf("Running time  = %g %s\n", pp_time_read(&tm), pp_time_unit() );
}
