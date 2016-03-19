/**********************************************************
 * Filename:    _link.h
 *
 * Description: This file contains declaration for class Linkable
 * 
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *********************************************************/

#ifndef _LINK_H_V001_INCLUDED_
#define _LINK_H_V001_INCLUDED_

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
class Linkable 
{
  private:
    Linkable* pNext;

  public:
    //T Item;     // item being linked          

    Linkable( Linkable* next = NULL )   { pNext = next; }
    virtual ~Linkable()                 {}

    inline Linkable* GetNext(void)                        { return pNext; }
    inline void      Insert(Linkable* prv, Linkable* nxt) { if(prv) prv->pNext = this; pNext = nxt; }
    inline void      Remove(Linkable* prv)                { if(prv) prv->pNext = pNext; }
};
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
class DLinkable 
{
  private:
    DLinkable* pPrev;
    DLinkable* pNext;

  public:
    //T Item;     // item being linked      

    DLinkable( DLinkable* prev = NULL, DLinkable* next = NULL )   { pPrev = prev; pNext = next; }
    virtual ~DLinkable()                 {}

    inline DLinkable* GetNext(void)                    { return pNext; }
    inline DLinkable* GetPrev(void)                    { return pPrev; }
    inline void       InsertAfter( DLinkable* ptr )    { Insert( ptr, ptr? ptr->pNext : NULL ); }
    inline void       InsertBefore( DLinkable* ptr )   { Insert( ptr? ptr->pNext : NULL, ptr ); }

    inline void       Insert(DLinkable* prv, DLinkable* nxt) 
    { 
        if(( pPrev = prv )) pPrev->pNext = this; 
        if(( pNext = nxt )) pNext->pPrev = this; 
    }

    inline void       Remove(void)                     
    { 
        if( pPrev ) pPrev->pNext = pNext; 
        if( pNext ) pNext->pPrev = pPrev; 
    }
};
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

class SList
{
private:
    Linkable* pHead;
    Linkable* pTail;
    int32s    Count;

public:
    SList()                                         { Clear(); }
    virtual ~SList()                                {}

    inline void         Clear(void)                 { pHead = pTail = NULL;  Count = 0; }
    inline int32s       GetCount(void)              { return Count; }
    inline Linkable*    GetHead(void)               { return pHead; }
    inline Linkable*    GetTail(void)               { return pTail; }

    inline void         Append( Linkable* ptr )    { Insert( pTail, ptr, NULL ); }
    inline void         Insert( Linkable* prv, Linkable* ptr, Linkable* nxt )    
    { 
        if( ptr ) 
        { 
            if( prv == NULL || nxt == pHead ) pHead = ptr;
            if( nxt == NULL || prv == pTail ) pTail = ptr;
            ptr->Insert(prv, nxt); 
            Count++; 
        }
    }
    
    inline Linkable*    RemoveHead(void)            { return Remove( NULL, pHead ); }
    inline Linkable*    Remove( Linkable* prv, Linkable* ptr )  
    { 
        if( ptr ) 
        { 
            if( ptr == pHead ) pHead = pHead->GetNext();
            if( ptr == pTail ) pTail = prv;
            ptr->Remove( prv ); 
            Count--; 
        } 
        return ptr;
    }
    void Iterator( void (*pf)(Linkable*) )
    {
        for( Linkable* ptr = pHead; ptr; ptr=ptr->GetNext() ) pf( ptr );
    }
};
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

class DList
{
private:
    DLinkable* pHead;
    DLinkable* pTail;
    int32s     Count;

public:
    DList()                                          { Clear(); }
    virtual ~DList()                                 {}

    inline void         Clear(void)                  { pHead = pTail = NULL;  Count = 0; }
    inline int32s       GetCount(void)               { return Count; }
    inline DLinkable*   GetHead(void)                { return pHead; }
    inline DLinkable*   GetTail(void)                { return pTail; }

    inline void         Append( DLinkable* ptr )     { Insert( pTail, ptr, NULL ); }
    inline void         InsertHead( DLinkable* ptr ) { Insert( NULL, ptr, pHead ); }
    
    inline void         Insert( DLinkable* prv, DLinkable* ptr, DLinkable* nxt )    
    { 
        if( ptr ) 
        { 
            if( prv == NULL || nxt == pHead ) pHead = ptr;
            if( nxt == NULL || prv == pTail ) pTail = ptr;
            ptr->Insert(prv, nxt); 
            Count++; 
        }
    }
    
    inline DLinkable*   RemoveHead(void)             { return Remove( pHead ); }
    inline DLinkable*   RemoveTail(void)             { return Remove( pTail ); }
    inline DLinkable*   Remove( DLinkable* ptr )  
    { 
        if( ptr ) 
        { 
            if( ptr == pHead ) pHead = pHead->GetNext();
            if( ptr == pTail ) pTail = pTail->GetPrev();
            ptr->Remove(); 
            Count--; 
        } 
        return ptr;
    }
    
    //void Iterator( void (*pf)(DLinkable*) )
    template < class T > void Iterator( void (T::*pf)(DLinkable*) )
    {
        for( DLinkable* ptr = pHead; ptr; ptr=ptr->GetNext() ) pf( ptr );
    }
};
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

class CircDList
{
private:
    DLinkable* pFirst;
    int32s     Count;

public:
    CircDList()                                     { Clear(); }
    virtual ~CircDList()                            {}

    inline void         Clear(void)                 { pFirst = NULL;  Count = 0; }
    inline int32s       GetCount(void)              { return Count;  }
    inline DLinkable*   GetFirst(void)              { return pFirst; }

    inline void         Append( DLinkable* ptr )    { Insert( pFirst? pFirst->GetPrev(): NULL, ptr, pFirst ); }
    inline void         Insert( DLinkable* prv, DLinkable* ptr, DLinkable* nxt )    
    { 
        if( ptr ) 
        { 
            if( !prv || !nxt )  (pFirst = ptr)->Insert(ptr, ptr);
            else                ptr->Insert(prv, nxt); 
            Count++;
        }
    }
    
    inline DLinkable*   RemoveFirst(void)           { return Remove( pFirst ); }
    inline DLinkable*   Remove( DLinkable* ptr )  
    { 
        if( ptr ) 
        { 
            if( ptr == pFirst ) pFirst = pFirst->GetNext();
            if( ptr == pFirst ) pFirst = NULL;
            ptr->Remove(); 
            Count--; 
        } 
        return ptr;
    }
    //void Iterator( void (*pf)(DLinkable*) )
    template < class T > void Iterator( void (T::*pf)(DLinkable*) )
    {
        DLinkable* ptr = pFirst;
        do { pf( ptr ); } while( (ptr = ptr->GetNext()) != pFirst );
    }
};

#endif /* _LINK_H_V001_INCLUDED_ */