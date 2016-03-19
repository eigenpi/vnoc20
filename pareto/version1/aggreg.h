/**********************************************************
 * Filename:    aggreg.h
 *
 * Description: This file contains declarations for
 *              class Trace
 *              class Generator
 *
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *********************************************************/

#if !defined(_AGGREG_H_INCLUDED_)
#define _AGGREG_H_INCLUDED_

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <ostream>
#include <iostream>
#include <iomanip>
#include "_types.h"
#include "source.h"

using namespace std;


/* dafault values */
const int16s BYTE_SIZE  = 8;
const int16s PREAMBLE   = 8;
const int16s MIN_PACKET = 1; // 64;
const int16s MAX_PACKET = 16; // 1518;

const DOUBLE PCT_SHAPE  = 1.7;
const DOUBLE GAP_SHAPE  = 1.2;

const DOUBLE MIN_ALPHA  = 1.0;
const DOUBLE MAX_ALPHA  = 2.0;

typedef DOUBLE timestamp_t;

////////////////////////////////////////////////////////////////////////////////////

class Trace
{
public:
    timestamp_t TimeStamp;
    pct_size_t  PacketSize;

    Trace()                                 { TimeStamp = 0.0; PacketSize = 0; }
    Trace(timestamp_t ts, pct_size_t ps )   { TimeStamp = ts; PacketSize = ps; }
    ~Trace()                                {}
};

////////////////////////////////////////////////////////////////////////////////
// FUNCTION:    ostream& operator<< ( ostream& os, const Trace& trc )
// DESCRIPTION: Insert operator for the class Trace
// NOTES:
////////////////////////////////////////////////////////////////////////////////
ostream& operator<< ( ostream& os, const Trace& trc )
{
    return ( os << trc.TimeStamp << "    " << trc.PacketSize );
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

class Generator
{
private:
    SourceLink* pFirst;         /* pointer to the source of the next packet */
    timestamp_t ByteTime;       /* Byte transmission time */
    bytestamp_t TotalBytes;     /* Total bytes generated   */
    int32s      TotalPackets;   /* Total packets generated */ 
    bytestamp_t Elapsed;        /* Elapsed time (expressed in ByteTime units)
                                  since the beginning of the trace */

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void InsertInOrder( SourceLink* pSrc )
    // DESCRIPTION: Inserts Source of packet into linked list of Sources 
    //              ordered by packet arrival times
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void InsertInOrder( SourceLink* pSrc )
    {
        SourceLink *pPrv = NULL, *pNxt  = pFirst;
        
        for( ; pNxt && pSrc->GetArrival() > pNxt->GetArrival(); pNxt = pNxt->GetNext() )
            pPrv = pNxt;

        pSrc->Insert(pPrv, pNxt);
        if( !pPrv ) pFirst = pSrc;
    }

    ////////////////////////////////////////////////////////////////////////////////

public:
    int16u      Preamble;   /* minimum interpacket gap */
    int16u      MinPacket;  /* minimum packet size */
    int16u      MaxPacket;  /* maximum packet size */

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    Generator( DOUBLE line_rate_Mbps, DOUBLE load = 0.0, int16s sources = 0 )
    // DESCRIPTION: Constructor
    // NOTES:       line_rate_Mbps  - rate of generated trace
    //              load            - desired line load (bandwidth utilization) 
    //                                of the generated trace
    //              sources         - number of sources to aggregate
    ////////////////////////////////////////////////////////////////////////////////
    Generator( DOUBLE line_rate_Mbps, DOUBLE load = 0.0, int16s sources = 0 )                   
    { 
        _seed();

        pFirst    = NULL;  
        Preamble  = PREAMBLE;
        MinPacket = MIN_PACKET;
        MaxPacket = MAX_PACKET;
        ByteTime  = BYTE_SIZE / ( line_rate_Mbps * 1.0E6);  /* byte transmission time (sec) */

        AddSources( load, sources, PCT_SHAPE, GAP_SHAPE );
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    ~Generator()
    // DESCRIPTION: Destructor
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    ~Generator()        { RemoveSources(); }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void Reset( void )
    // DESCRIPTION: resets all statistic variables and elapsed time
    // NOTES:       Reset should be called to generate multiple traces with same 
    //              set of sources 
    ////////////////////////////////////////////////////////////////////////////////
    void Reset( void )
    {
        TotalBytes   = 0.0;
        Elapsed      = 0.0;
        TotalPackets = 0;
        for( SourceLink* p = pFirst; p; p = p->GetNext() )
            p->Reset();
    }

    ////////////////////////////////////////////////////////////////////////////////

    int32s      GetPackets(void)  { return TotalPackets; }
    bytestamp_t GetBytes(void)    { return TotalBytes;   }
    bytestamp_t GetTime(void)     { return Elapsed;      }
    DOUBLE      GetLoad(void)     { return TotalBytes / Elapsed; }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void AddSource( pct_sz, preamble, min_gap, pshape, gshape )
    // DESCRIPTION: Adds new source to the Generator.
    // NOTES:       OFF periods are Pareto-distributed with alpha = gshape 
    //              and mimimum gap size of (coef * packet_size).
    //              ON periods consist of bursts of packets with 
    //              Pareto-distributed number of packets per burst (minimum = 1) 
    //              and alpha = pshape.
    ////////////////////////////////////////////////////////////////////////////////
    void AddSource(pct_size_t pct_sz, pct_size_t preamble, int32u min_gap, DOUBLE pshape, DOUBLE gshape )
    {
        InsertInOrder( new SourceLink( pct_sz, preamble, min_gap, pshape, gshape ) );
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    AddSources( DOUBLE load, int16s sources, DOUBLE pshape, DOUBLE gshape )
    // DESCRIPTION: 
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void AddSources( DOUBLE load, int16s sources, DOUBLE on_shape, DOUBLE off_shape )
    {
        pct_size_t  packet_size;
        DOUBLE on_coef, off_coef, coef;

        if( ( load > 0.0 && load <= 1.0 )  &&
            ( on_shape  > MIN_ALPHA && on_shape  < MAX_ALPHA ) &&
            ( off_shape > MIN_ALPHA && off_shape < MAX_ALPHA ) )
        {
            on_coef  = on_shape  / ( on_shape - 1.0 );
            off_coef = off_shape / ( off_shape - 1.0 );
            coef     = (sources / load - 1.0) * ( on_coef / off_coef ); 
            
            /* coef - coefficient used to calculate minimum inter-burst interval
             *        such that aggregated traffic from all sources would produce 
             *        the desired link load.  
             *   (1)  LOAD = SOURCES * ( MEAN_ON / MEAN_ON + MEAN_OFF );
             *   (2)  MEAN_ON   = MIN_ON  * on_shape  / (on_shape - 1)  = MIN_ON  * on_coef;
             *   (3)  MEAN_OFF  = MIN_OFF * off_shape / (off_shape - 1) = MIN_OFF * off_coef;
             *   (4)  MIN_OFF   = MIN_ON * ( SOURCES / LOAD - 1) * ( on_coef / off_coef );
             *        MIN_OFF   = MIN_ON * COEF
             *   (5)  COEF      = ( SOURCES / LOAD - 1) * ( on_coef / off_coef )
             *
             *        Due to infinite variance of Pareto distribution with 
             *        [1 < alpha < 2], the aggregated load of generated trace
             *        may fluctuate considerably.  Multiple iterations may be 
             *        needed to choose the one with load closest to the specified. 
             */
            while( sources-- > 0 )
            {
                /* Every source has a constant packet size from uniform
                 * distribution [MinPacket ... MaxPacket]
                 */
                packet_size = round<int16u>(_uniform_(MinPacket, MaxPacket));
                AddSource(packet_size, Preamble, (int32u)(coef * packet_size), on_shape, off_shape);
            }
        }
        Reset();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    RemoveSource( void )
    // DESCRIPTION: Removes first source
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void RemoveSource( void )
    {
        if( pFirst )
        {
            SourceLink* pSrc = pFirst;
            pFirst = pFirst->GetNext();
            delete pSrc;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    RemoveSources( void )
    // DESCRIPTION: Removes all sources
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void RemoveSources( void )  { while( pFirst ) RemoveSource(); }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    Trace GenerateTrace( void )
    // DESCRIPTION: Return the next packet from the aggregated traffic
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    Trace GenerateTrace( void )
    {
        SourceLink* pNextSrc = pFirst;
        /* The source at the head of the linked list has a packet with 
         * earliest arrival time that has not been sent yet. 
         */
        pFirst  = pFirst->GetNext();    /* remove head of the linked list */

        Trace Trc(0.0, pNextSrc->GetPctSize());

        /* Elapsed + PacketSize + Preamble  -- earliest time the packet can be sent 
         * pFirst->GetArrival() -- packet arrival time
         */
        Elapsed = MAX( pNextSrc->GetArrival(), Elapsed + Trc.PacketSize + Preamble );
        Trc.TimeStamp = Elapsed * ByteTime; /* get timestamp from the bytestamp */
        TotalBytes   += Trc.PacketSize;     
        TotalPackets++;

        pNextSrc->ExtractPacket();          /* receive new packet */
        InsertInOrder( pNextSrc );          /* place the source in the list 
                                             * in order of packet arrival */
        return Trc;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void OutputTraces( int32s packets )
    // DESCRIPTION: Outputs 'packets' packets to 'cout' stream, then outputs statistics
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void OutputTraces( int32s packets )
    {
        Reset();

        cout.setf(ios::fixed);
        cout.precision(8);
        
        while( packets-- > 0 )
            cout << GenerateTrace() << endl;

        ShowStatistic();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void ShowStatistic(void)
    // DESCRIPTION: Outputs statistic to the 'clog' stream
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void ShowStatistic(void)
    {
        clog << "\n Total packets: " << TotalPackets 
             << "\n Total bytes:   " << TotalBytes 
             << "\n Total seconds: " << Elapsed * ByteTime 
             << "\n Link load:     " << TotalBytes / Elapsed << endl;
    }

    ////////////////////////////////////////////////////////////////////////////////
};

#endif // _AGGREG_H_INCLUDED_
