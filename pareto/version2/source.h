/**********************************************************
 * Filename:    source.h
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
#include "_rand_MT.h"
#include "_link.h"
#include "trace.h"

const int16u MIN_BURST  = 1;
const DOUBLE MIN_ALPHA  = 1.0;
const DOUBLE MAX_ALPHA  = 2.0;

template < class T > inline void SetInRange( T& x, T y, T z ) { if( x <= y || x >= z ) x = (y + z) / 2; }

///////////////////////////////////////////////////////////////////////////////
class Source : public DLinkable
{
private:
    int16u       ID;            /* source ID */
    int16u       Priority;      /* priority of the packets (determines the queue)*/
    bytestamp_t  Elapsed;       /* elapsed time (in byte transmission times) */

protected:
    pct_size_t   PctSize;       /* packet size (bytes)     */
    pct_size_t   Preamble;      /* minimum inter-packet gap value (in bytes) */
    pct_size_t   PctSpace;      /* packet size plus minimum inter-packet gap */
    int32u       BurstSize;     /* number of packets remaining in current burst */

    virtual inline int32u      GetBurstSize(void) = 0;
    virtual inline bytestamp_t GetGapSize(void)   = 0;

    inline bytestamp_t SetGap( bytestamp_t gap ) { return gap < Preamble? Preamble : gap; }

public:

    Source( int16u id, int16u prior, pct_size_t pct_sz, pct_size_t preamble ) : DLinkable()
    { 
        ID        = id;
        Priority  = prior;
        PctSize   = pct_sz;
        Preamble  = preamble;
        PctSpace  = PctSize + Preamble;
    }

    virtual ~Source()       {}

    ///////////////////////////////////////////////////////////////////////////////

    inline void Reset(void)
    {
        // quick start: simulate start at random time during ON- or OFF- period
        bytestamp_t burst_size = GetBurstSize() * PctSpace;
        bytestamp_t period     = burst_size + GetGapSize();
        bytestamp_t start_time = _uniform_real_( 0, period );

        if( start_time > burst_size )   // time started during OFF period
        {
            BurstSize = GetBurstSize(); 
            Elapsed   = period - start_time;
        }
        else                            // time started during ON period 
        {
            BurstSize = (int32s)( ( burst_size - start_time )/PctSpace + 1 );
            Elapsed   = 0.0;
        }
    }
    ///////////////////////////////////////////////////////////////////////////////
    virtual inline void SetLoad( DOUBLE load ) = 0;

    inline bytestamp_t GetID(void)               { return ID; }
    inline bytestamp_t GetPriority(void)         { return Priority; }
    inline bytestamp_t GetArrival(void)          { return Elapsed;  }
    inline pct_size_t  GetPctSize(void)          { return PctSize;  } 
    inline Trace       GetTrace(void)            { return Trace( ID, Priority, Elapsed, PctSize ); }
    inline void        GetTrace( Trace& trc )    { trc.SourceID   = ID;
                                                   trc.QueueID    = Priority;
                                                   trc.ByteStamp  = Elapsed;
                                                   trc.PacketSize = PctSize;
                                                 }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void ExtractPacket(void)
    // DESCRIPTION: Generates new packet
    // NOTES:       If BurstSize > 0, the new packet assumed to immediately follow
    //              the previous packet (with minimum inter-packet gap = Preamble.)
    //              If BurstSize == 0, new burst size is generated from specified 
    //              distribution. Elapsed time is also incremented by a 
    //              random value to account for inter-burst gap.
    ////////////////////////////////////////////////////////////////////////////////
    inline void ExtractPacket(Trace& trc)
    {
        GetTrace( trc );
        ExtractPacket();
    }
    ////////////////////////////////////////////////////////////////////////////////
    inline void ExtractPacket(void)
    {
        if( BurstSize == 0 )
        {
            BurstSize = GetBurstSize();
            Elapsed  += GetGapSize();
        }
        BurstSize--;
        Elapsed += PctSpace;
    }
    ///////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////////
class SourcePareto : public Source
{
private:
    bytestamp_t  MinGap;        /* minimum inter-burst gap value (in bytes)  */
    DOUBLE       ONShape;       /* shape parameter for burst distribution    */
    DOUBLE       OFFShape;      /* shape parameter for inter-burst gap distribution */

    virtual inline int32u      GetBurstSize(void) { return round<int32u>(_pareto_(ONShape) * MIN_BURST); }
    virtual inline bytestamp_t GetGapSize(void)   { return _pareto_(OFFShape) * MinGap; }

public:

    SourcePareto(   int16u id, 
                    int16u prior,
                    pct_size_t pct_sz, 
                    pct_size_t preamble, 
                    DOUBLE load, 
                    DOUBLE on_shape, 
                    DOUBLE off_shape ) : Source( id, prior, pct_sz, preamble )
    { 
        ONShape  = on_shape;
        OFFShape = off_shape;

        SetInRange<DOUBLE>( ONShape,  MIN_ALPHA, MAX_ALPHA );
        SetInRange<DOUBLE>( OFFShape, MIN_ALPHA, MAX_ALPHA );

        SetLoad( load );
        Reset();
    }
    virtual ~SourcePareto()       {}
    ///////////////////////////////////////////////////////////////////////////////
    virtual inline void SetLoad( DOUBLE load )
    {
        SetInRange<DOUBLE>( load, 0.0, 1.0 );
        /* Given  
         *      a - shape parameter of a Pareto distribution (on_shape and off_shape)
         *      b - minimum value from a Perto distribution
         *      q - maximum (cutoff) value
         *
         * For Pareto distribution, mean values are calculated as
         *      mean = (a*b) / (a-1);
         * However, for a series of bounded [b..q] Pareto-distributed values
         *      mean = [(a*b) / (a-1)] * [1 - (b/q)^(a-1)]
         * But, q = b / s^(1/a)  (see function "_pareto_(a)" in _rand.h)
         *      s - smallest non-zero uniformly distributed value 
         * Thus,
         *      mean = [(a*b) / (a-1)] * [1 - s^(1-1/a)]
         *      mean = b * [1 - s^(1-1/a)] / (1-1/a) = b * coef
         *           where coef = [1 - s^(1-1/a)] / (1-1/a)
         */
        DOUBLE on_coef  = (1.0 - pow(SMALL_VAL, 1.0 - 1.0/ONShape)) / (1.0 - 1.0/ONShape);
        DOUBLE off_coef = (1.0 - pow(SMALL_VAL, 1.0 - 1.0/OFFShape)) / (1.0 - 1.0/OFFShape);

        /* (1)  MEAN_BURST   = MIN_BURST * on_coef
         * (2)  MEAN_GAP     = MIN_GAP * off_coef
         *
         *                        MEAN_BURST*PACKET_SIZE
         * (3)  LOAD = ----------------------------------------------
         *             MEAN_BURST*(PACKET_SIZE + PREAMBLE) + MEAN_GAP 
         *
         * (4)  MEAN_GAP = MEAN_BURST * [ PACKET_SIZE * (1 - LOAD) / LOAD - PREAMBLE ]
         *
         *                  on_coef                 PACKET_SIZE
         * (5)  MIN_GAP  = -------- * MIN_BURST * [ ----------- - (PACKET_SIZE + PREAMBLE)]
         *                 off_coef                     LOAD 
         */                           
        MinGap = SetGap((on_coef/off_coef) * MIN_BURST * (PctSize/load - PctSpace));
    }
};

////////////////////////////////////////////////////////////////////////////////////
class SourceExpon : public Source
{
private:
    bytestamp_t  MeanGap;        /* mean inter-burst gap value (in bytes)  */
    DOUBLE       MeanBurst;      /* mean burst size (in packets)  */

    virtual  inline int32u      GetBurstSize(void) { return round<int32u>(_exponent_() * (MeanBurst - MIN_BURST) + MIN_BURST); }
    virtual  inline bytestamp_t GetGapSize(void)   { return _exponent_() * (MeanGap - Preamble ) + Preamble; }

public:

    SourceExpon(    int16u id, 
                    int16u prior, 
                    pct_size_t pct_sz, 
                    pct_size_t preamble, 
                    DOUBLE load, 
                    DOUBLE mean_burst ) : Source(id, prior, pct_sz, preamble )
    { 
        MeanBurst = mean_burst;
        SetLoad( load );
        Reset();
    }
    virtual ~SourceExpon()       {}
    ///////////////////////////////////////////////////////////////////////////////
    virtual inline void SetLoad( DOUBLE load )
    {
        SetInRange<DOUBLE>( load, 0.0, 1.0 );
        MeanGap = SetGap( MeanBurst * ( PctSize / load - PctSpace ));
    }
};

////////////////////////////////////////////////////////////////////////////////////
class SourceCBR : public Source
{
private:
    bytestamp_t Gap;        /* inter-burst gap value (in bytes)  */

    virtual  inline int32u      GetBurstSize(void) { return MIN_BURST;   }
    virtual  inline bytestamp_t GetGapSize(void)   { return Gap;         }

public:

    SourceCBR(      int16u id, 
                    int16u prior,
                    pct_size_t pct_sz, 
                    pct_size_t preamble, 
                    DOUBLE load ) : Source(id, prior, pct_sz, preamble )
    { 
        SetLoad( load );
        Reset();
    }
    
    virtual ~SourceCBR()       {}
    ///////////////////////////////////////////////////////////////////////////////
    virtual inline void SetLoad( DOUBLE load )
    {
        SetInRange<DOUBLE>( load, 0.0, 1.0 );
        Gap = SetGap((1.0 / load - 1.0) * PctSize * MIN_BURST - Preamble);
    }
};

#endif // _SOURCE_H_INCLUDED_
