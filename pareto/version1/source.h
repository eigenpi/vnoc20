/**********************************************************
 * Filename:    _types.h
 *
 * Description: This file contains declarations for
 *              class Source
 *              class SourceLink
 *
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *********************************************************/

#if !defined(_SOURCE_H_INCLUDED_)
#define _SOURCE_H_INCLUDED_

//#define RND_LARGE_RANGE

#include "_types.h"
#include "_rand.h"

typedef int16u   pct_size_t;
typedef DOUBLE   bytestamp_t;

#define MIN_BURST   1.0

/* Choose one of the following lines */
/* Use Pareto distributed ON and OFF periods */
inline DOUBLE rnd_val( DOUBLE shape )   { return _pareto_( shape ); }

/* Use exponentially distributed ON and OFF periods */
//inline DOUBLE rnd_val( DOUBLE shape )   { return _exponent_() / (shape - 1.0) + 1.0; }

class Source
{
private:
    bytestamp_t  Elapsed;       /* elapsed time (in byte transmission times) */
    pct_size_t   PctSize;       /* packet size (bytes)     */
    pct_size_t   Preamble;      /* minimum inter-packet gap value (in bytes) */
    int32u       MinGap;        /* minimum inter-burst gap value (in bytes)  */
    int16u       BurstSize;     /* number of packets remaining in current burst */
    DOUBLE       PctShape;      /* shape parameter for burst distribution    */
    DOUBLE       GapShape;      /* shape parameter for inter-burst gap distribution */

public:

    Source(pct_size_t pct_sz, pct_size_t preamble, int32u min_gap, DOUBLE pshape, DOUBLE gshape )
    { 
        PctSize   = pct_sz;
        Preamble  = preamble;
        MinGap    = min_gap;
        PctShape  = pshape;
        GapShape  = gshape;

        Reset();
    }

    virtual ~Source()       {}

    ///////////////////////////////////////////////////////////////////////////////

    inline void Reset(void)
    {
        Elapsed   = 0.0;
        BurstSize = 0;
        ExtractPacket();
    }
    ///////////////////////////////////////////////////////////////////////////////

    inline bytestamp_t GetArrival(void) { return Elapsed; }
    inline pct_size_t  GetPctSize(void) { return PctSize; } 

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void ExtractPacket(void)
    // DESCRIPTION: Generates new packet
    // NOTES:       If BurstSize > 0, the new packet assumed to immediately follow
    //              the previous packet (with minimum inter-packet gap = Preamble.)
    //              If BurstSize == 0, new burst size is generated from Pareto 
    //              distribution. Elapsed time is also incremented by a 
    //              Pareto-distributed value to account for inter-burst gap.
    ////////////////////////////////////////////////////////////////////////////////
    inline void ExtractPacket(void)
    {
        if( BurstSize == 0 )
        {
            BurstSize = round<int16u>( rnd_val( PctShape ) * MIN_BURST );
            Elapsed  += round<int32u>( rnd_val( GapShape ) * MinGap );
        }
        BurstSize--;
        Elapsed += ( PctSize + Preamble );
    }
    ///////////////////////////////////////////////////////////////////////////////
};


////////////////////////////////////////////////////////////////////////////////////

class SourceLink: public Source
{
  private:
    SourceLink* pNext;

  public:
    SourceLink( pct_size_t pct_sz, 
                pct_size_t preamble, 
                int32u min_gap, 
                DOUBLE pshape, 
                DOUBLE gshape ):
                Source( pct_sz, preamble, min_gap, pshape, gshape )  { pNext = NULL; }
    virtual ~SourceLink() {}

    inline SourceLink* GetNext(void)                            { return pNext; }
    inline void        Insert(SourceLink* prv, SourceLink* nxt) { if(prv) prv->pNext = this; pNext = nxt; }
    inline void        Remove(SourceLink* prv)                  { if(prv) prv->pNext = pNext; }
};
////////////////////////////////////////////////////////////////////////////////////

#endif // _SOURCE_H_INCLUDED_
