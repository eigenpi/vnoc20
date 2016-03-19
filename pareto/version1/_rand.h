/**********************************************************
 * Filename:    _rand.h
 *
 * Description: This file contains routines for generating 
 *              random values
 * 
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *********************************************************/

#ifndef _RAND_H_V001_INCLUDED_
#define _RAND_H_V001_INCLUDED_

#include <stdlib.h>
#include <time.h>
#include <math.h>

typedef double rnd_t;

const int16u SMALL_RANGE  =  15;  /* range of random values returned by rand()  [0 ... 2^15] */
const int16u LARGE_RANGE  =  24;  /* extended range of random values [0 ... 2^24] */

const int16u EXTENTION    = LARGE_RANGE - SMALL_RANGE;
const int16u EXTEN_MASK   = (0x0001 << (EXTENTION + 1)) - 1;

#define EXT_RANGE( X, Y )  ((((int32u)X) << EXTENTION ) | ( Y & EXTEN_MASK ))

#if defined( RND_LARGE_RANGE )

    int32u _lrand(void)        { return EXT_RANGE( rand(), rand() ); }
    #define RAND               _lrand
    const int32u RND_MAXIMUM = EXT_RANGE( RAND_MAX, RAND_MAX );
    
#else

    #define RAND               rand
    const int32u RND_MAXIMUM = RAND_MAX;

#endif


const rnd_t SMALL_VAL = 0.5 / RND_MAXIMUM;

inline void   _seed(void)                    { srand( (unsigned)time( NULL ) );          }
inline rnd_t _uniform01(void)                { return ((rnd_t)RAND()) / RND_MAXIMUM;     }
inline rnd_t _uniform_(rnd_t low, rnd_t hi)  { return (hi - low) * _uniform01() + low;   }
inline rnd_t _uniform_non0(void)             { return _uniform_( SMALL_VAL, 1.0 );       }
inline rnd_t _exponent_(void)                { return -log( _uniform_non0() );           }
inline rnd_t _pareto_(rnd_t shape)           { return pow( _uniform_non0(), -1.0/shape); }

#endif /* _RAND_H_V001_INCLUDED_ */
