/**********************************************************
 * Filename:    _rand_MT.h
 *
 * Description: This file contains routines for generating 
 *              random values
 * 
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *********************************************************/

#ifndef _RAND_MT_H_V001_INCLUDED_
#define _RAND_MT_H_V001_INCLUDED_

#include <math.h>

#include "_types.h"
#include "MersenneTwister.h"

//extern MTRand RND;

typedef DOUBLE  rnd_real_t;
typedef int32u  rnd_int_t;

const rnd_real_t  SMALL_VAL = 1.0 / 0xFFFFFFFFUL;

static MTRand RND;

inline void       _seed(void)                            { RND.seed(); }
inline rnd_real_t _uniform_real_0_1(void)   /* [0,1] */  { return RND.rand(); }  
inline rnd_real_t _uniform_real_0_X1(void)  /* [0,1) */  { return RND.randExc(1.0); }  
inline rnd_real_t _uniform_real_X0_1(void)  /* (0,1] */  { return 1.0 - _uniform_real_0_X1(); }  

inline rnd_real_t _uniform_real_(rnd_real_t low, rnd_real_t hi) { return RND.rand( hi - low ) + low;    }
inline rnd_int_t  _uniform_int_ (rnd_int_t low,  rnd_int_t hi)  { return RND.randInt( hi - low ) + low; }

inline rnd_real_t _exponent_(void)                       { return -log( _uniform_real_X0_1() );            }
inline rnd_real_t _pareto_(rnd_real_t shape)             { return  pow( _uniform_real_X0_1(), -1.0/shape); }

#endif /* _RAND_MT_H_V001_INCLUDED_ */
