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

#include <iostream.h>
#include <iomanip.h>
#include "_types.h"
#include "source.h"

//#define NEXT_SOURCE( P )       P = (Source*)P->GetNext()

////////////////////////////////////////////////////////////////////////////////////

class Generator 
{
private:
    DList       SRC;            /* Doubly-linked list of sources */
    Trace       NextPacket;     /* Future packet */
    bytestamp_t TotalBytes;     /* Total bytes generated   */
    int32s      TotalPackets;   /* Total packets generated */ 
    bytestamp_t Elapsed;        /* Elapsed time (expressed in ByteTime units)
                                   since the beginning of the trace */

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    void InsertInOrder( Source* pSrc )
    // DESCRIPTION: Inserts Source of packet into linked list of Sources 
    //              ordered by packet arrival times
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    Source* InsertInOrder( Source* pSrc )
    {
        bytestamp_t arrival = pSrc->GetArrival();
        DLinkable*  pPrv    = NULL;
        DLinkable*  pNxt    = SRC.GetHead();
       
        for( ; pNxt && arrival > ((Source*)pNxt)->GetArrival(); pNxt = pNxt->GetNext())
            pPrv = pNxt;

        SRC.Insert( pPrv, pSrc, pNxt );                     /* insert new source   */
        NextPacket = ((Source*)SRC.GetHead())->GetTrace();  /* get the next packet */  
        NextPacket.Append( Elapsed + Preamble );            /* prevent overlap with previous packet */
        return pSrc;
    }

    ////////////////////////////////////////////////////////////////////////////////

public:
    int16u      Preamble;   /* minimum interpacket gap */

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    Generator()
    // DESCRIPTION: Constructor
    ////////////////////////////////////////////////////////////////////////////////
    Generator(void)                   
    { 
        Preamble  = PREAMBLE;
        Reset();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    ~Generator()
    // DESCRIPTION: Destructor
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    virtual ~Generator()        {}

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

        DList lst = SRC;
        SRC.Clear();

        Source* ptr;
        while( ( ptr = (Source*)lst.RemoveHead() ) != NULL )
        {
            ptr->Reset();
            InsertInOrder( ptr );
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    inline int32s      GetPackets(void)  { return TotalPackets; }
    inline bytestamp_t GetBytes(void)    { return TotalBytes;   }
    inline bytestamp_t GetTime(void)     { return Elapsed;      }
    inline DOUBLE      GetLoad(void)     { return TotalBytes / Elapsed; }
    inline int32s      GetSources(void)  { return SRC.GetCount(); }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    inline void AddSource( Source* )
    // DESCRIPTION: Adds a new source of to the Generator.
    ////////////////////////////////////////////////////////////////////////////////
    inline void AddSource( Source* pSrc )    { if(pSrc) InsertInOrder( pSrc ); }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    RemoveSource( Source* )  
    // DESCRIPTION: Removes source
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    inline void     RemoveSource( Source* pSrc )    { SRC.Remove(pSrc); }
    inline Source*  RemoveSource( void )            { return (Source*)SRC.RemoveHead(); }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    SetLoad( DOUBLE load )  
    // DESCRIPTION: Sets total node load (all sources are to have equal load)
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    void SetLoad( DOUBLE load )
    {
        if( SRC.GetCount() > 0 ) 
        {
            load /= SRC.GetCount();
            for( DLinkable* ptr = SRC.GetHead(); ptr; ptr = ptr->GetNext())
                ((Source*)ptr)->SetLoad( load );
        }
    }
        
    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    Trace PeekNextPacket(void)
    // DESCRIPTION: Peek at next packet before it arrives
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    inline Trace PeekNextPacket(void)   { return NextPacket; }

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION:    Trace GetNextPacket(void)
    // DESCRIPTION: Return the next packet from the aggregated traffic
    // NOTES:
    ////////////////////////////////////////////////////////////////////////////////
    Trace GetNextPacket(void)
    {
        Trace trc = NextPacket;
        if( SRC.GetHead() )
        {
            Elapsed       = NextPacket.ByteStamp;
            TotalBytes   += NextPacket.PacketSize;     
            TotalPackets++;

            Source* pSrc = (Source*)SRC.RemoveHead(); /* Extract packet from the first source */
            pSrc->ExtractPacket();                    /* receive new packet */
            InsertInOrder( pSrc );                    /* place the source in the linked list */
        }
        return trc;
    }

    ////////////////////////////////////////////////////////////////////////////////
};

#endif // _AGGREG_H_INCLUDED_
