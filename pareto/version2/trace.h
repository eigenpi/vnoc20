/**********************************************************
 * Filename:    source.h
 *
 * Description: This file contains declarations for
 *              class Trace
 *
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *********************************************************/

#if !defined(_TRACE_H_INCLUDED_)
#define _TRACE_H_INCLUDED_

#include "_types.h"
#include "_rand_MT.h"

typedef int16s   pct_size_t;
typedef DOUBLE   bytestamp_t;

/* dafault values */
const int16s BYTE_SIZE  = 8;
const int16s PREAMBLE   = 8;

const int16s MIN_PCT   = 64;
const int16s MAX_PCT   = 1518;

const int8u  PRIOR_L0  = 0;  /* lowest priority */
const int8u  PRIOR_L1  = 1;  
const int8u  PRIOR_L2  = 2;  
const int8u  PRIOR_L3  = 3;  /* highest priority */

////////////////////////////////////////////////////////////////////////////////////

class Trace
{
public:
    int16s      SourceID : 10;
    int16s      QueueID  :  6;
    bytestamp_t ByteStamp;
    pct_size_t  PacketSize;

    Trace( int16s sid = 0, int16s qid = PRIOR_L0, bytestamp_t bs = 0.0, pct_size_t ps = 0 )   
    { 
        SourceID   = sid;
        QueueID    = qid;
        ByteStamp  = bs;
        PacketSize = ps; 
    }
    ~Trace()  {}
    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    Append( bytestamp_t prev_end )
    // DESCRIPTION: Updates bytestamp of the current packet to account for the 
    //              waiting time in buffer.  This happens when several sources 
    //              have overlaping (in time) packets
    // NOTES:       free_stamp - bytestamp when channel becomes free  
    ////////////////////////////////////////////////////////////////////////////////
    inline void Append( bytestamp_t free_stamp )
    { 
        ByteStamp = MAX( ByteStamp, free_stamp + PacketSize );
    }
};

////////////////////////////////////////////////////////////////////////////////
// FUNCTION:    ostream& operator<< ( ostream& os, const Trace& trc )
// DESCRIPTION: Insert operator for the class Trace
// NOTES:
////////////////////////////////////////////////////////////////////////////////
inline ostream& operator<< ( ostream& os, const Trace& trc )
{
    return ( os << trc.SourceID  << "   " 
                << trc.QueueID   << "   "
                << trc.ByteStamp << "   " 
                << trc.PacketSize );
}


#endif /* _TRACE_H_INCLUDED_ */