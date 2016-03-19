#include "aggreg.h"

using namespace std;

int main(void)
{
    //Generator gen_main;
    Generator gen(1e-2, 0.8, 128);      // Generator is to aggregate traffic 
                                        // from 40 sources and to generate 
                                        // the load of 0.3 on a link with 
                                        // rate 100 Mbps (that is average 
                                        // throughput of 30 Mbps)
    //gen_main = gen;//Generator(8e-6, 0.2, 128);
    
    // generate one million packets
    // and output them to STDOUT
    gen.OutputTraces( 10000 ); 


    //Generator gen2(1e-6, 0.2, 128); 
    //gen2.OutputTraces( 100 );
 
    return 0;
}
