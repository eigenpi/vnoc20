/**********************************************************
 * Filename:    README.TXT
 *
 * Author: Glen Kramer (kramer@cs.ucdavis.edu)
 *         University of California @ Davis
 *
 *********************************************************

This file contains description of a trace generating utility

Distribution list
-----------------

1. README.TXT - this file
2. _types.h
3. _rand.h  
4. source.h
5. aggreg.h


This utility generates self-similar traffic by aggregating multiple 
sources of Pareto-distributed ON- and OFF-periods.

Every source generates packets of only one size.  Pareto distribution 
of burst sizes is achieved by using Pareto distribution for the number 
of packets in a burst (minimum is 1).  Inter-burst gaps are also 
Pareto-distributed.

If packet traces (timestamps and packet sizes) is all what's needed, 
then the main function may look like following:

#include "aggreg.h"

void main(void)
{
    Generator Gen(100.0, 0.3, 40);      // Generator is to aggregate traffic 
                                        // from 40 sources and to generate 
                                        // the load of 0.3 on a link with 
                                        // rate 100 Mbps (that is average 
                                        // throughput of 30 Mbps)

    Gen.OutputTraces( 1000000 );        // generate one million packets 
                                        // and output them to STDOUT
}


Note that due to infinite variance of Pareto distribution with 1 < alpha < 2, 
the aggregated load of generated trace may fluctuate considerably.  Multiple 
iterations may be needed to choose the one with load closest to the specified. 


If the number of packets doesn't matter, but the specific average load should 
be achieved, the main function may look like following:

#include "aggreg.h"

void main(void)
{
    DOUBLE load  = 0.3;                 // desired load
    DOUBLE delta = 0.01;                // acceptable difference between the 
                                        // requested and obtained load

    int32u max_packets = 100000000L;    // generate no more then 100 mln. packets

    Generator Gen(100.0, load, 40);     // Generator is to aggregate traffic 
                                        // from 40 sources and to generate 
                                        // the load of 0.3 on a link with 
                                        // rate 100 Mbps (that is average 
                                        // throughput of 30 Mbps)

    while( Gen.GetPackets() < max_packets && fabs( Gen.GetLoad() - load) > delta )
        cout << Gen.GenerateTrace() << endl; 

}

When class Generator instantiated with non-zero load and number of sources,
every source will have the same values for preamble, burst shape parameter, 
and gap shape parameter. If there is a case when sources should differ in 
any of these parameters, the following code may be used:


    Generator Gen(100.0);               // No load and source specified

    // now add individual sources
    //             packet size | preamble | min. gap | burst shape | gap shape
    Gen.AddSource(      64     ,     8    ,   1000   ,     1.4     ,   1.6  );
    Gen.AddSource(     512     ,    16    ,   4000   ,     1.7     ,   1.3  );
    ........................................................................
    

Preamble specifies the minimum gap between the packets in packet train 
(within a burst) of individual sources.  To specify the preamble value 
for the aggregated traffic use
 
Gen.Preamble = x;

minimum gap specifies the minimum interval between bursts.


Since all the parameters in this case are configurable by user, there is no 
way for the Generator to specify the load.  The resulting load will be 
whatever all the sources aggregate to.

Refer to source code for more information on available methods and 
implementation details.

9-15-2000

*/

