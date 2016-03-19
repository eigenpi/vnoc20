#include <math.h>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <limits.h>

#include "vnoc.h"
#include "vnoc_utils.h"


using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// utility functions;
//
////////////////////////////////////////////////////////////////////////////////

void *my_malloc(size_t size) 
{
    void *ret;
    if ((ret = malloc (size)) == NULL) {
        fprintf(stderr,"Error:  Unable to malloc memory.  Aborting.\n");
        abort ();
        exit (1);
    }
    return (ret);
}

void *my_realloc(void *ptr, size_t size) 
{
    void *ret;
    if ((ret = realloc (ptr,size)) == NULL) {
        fprintf(stderr,"Error:  Unable to realloc memory.  Aborting.\n");
        exit (1);
    }
    return (ret);
}

////////////////////////////////////////////////////////////////////////////////
//
// RANDOM
//
////////////////////////////////////////////////////////////////////////////////

RANDOM_NUMBER_GENERATOR::RANDOM_NUMBER_GENERATOR(long seed) :_seed(seed)
{
    srandom( _seed);
}

RANDOM_NUMBER_GENERATOR::RANDOM_NUMBER_GENERATOR() : _seed(1)
{
    srandom( _seed);
}

double RANDOM_NUMBER_GENERATOR::sflat01()
{
    double val = random() * 1.0 / RAND_MAX;
    return val;
}

void RANDOM_NUMBER_GENERATOR::set_seed(long seed) 
{
    _seed = seed;
    srandom( seed);
}

double RANDOM_NUMBER_GENERATOR::flat_d(double low, double high) 
{
    assert( low < high);
    return (high - low) * sflat01() + low;
}

long RANDOM_NUMBER_GENERATOR::flat_l(long low, long high)
{
    assert( low < high);
    long val = long((high - low) * sflat01() + low);
    assert( val >= low && val < high);
    return val;
}

unsigned long RANDOM_NUMBER_GENERATOR::flat_ul(unsigned long low, 
    unsigned long high) 
{
    assert( low < high);
    unsigned long val = (unsigned long)((high-low) * sflat01() + low);
    assert( val >= low && val < high);
    return val;
}

unsigned long long RANDOM_NUMBER_GENERATOR::flat_ull(unsigned long long low,
    unsigned long long high) 
{
    assert( low < high);
    unsigned long long val = (unsigned long long)((high-low) * sflat01() + low);
    assert( val >= low && val < high);
    return val;
}

double RANDOM_NUMBER_GENERATOR::gauss01()
{
    // mean = 0, variance = 1;
    double modifier;
    double compile_b;
    double in_a, in_b;
    double out_a;
    static double out_b;
    static int recalc = 1;

    if (recalc) {
        // Range from (0:1], not [0:1). Had to change this to prevent log(0).
        in_a = 1.0 - sflat01();
        in_b = sflat01();

        modifier = sqrt( -2.0 * log(in_a));
        compile_b = 2.0 * PI * in_b;

        out_a = modifier * cos(compile_b);
        out_b = modifier * sin(compile_b);

        recalc = 0;
        return out_b;
    }

    recalc = 1;
    return out_b;
}

double RANDOM_NUMBER_GENERATOR::gauss_mean_d(double mean, double variance) 
{
    double ret = gauss01() * variance + mean;
    return ret;
}

long RANDOM_NUMBER_GENERATOR::gauss_mean_l(long mean, double variance) 
{
    return ((long)(gauss01() * variance + mean));
}

unsigned long RANDOM_NUMBER_GENERATOR::gauss_mean_ul(unsigned long mean, 
    double variance) 
{
    return((unsigned long)(gauss01() * variance + mean));
}

unsigned long long RANDOM_NUMBER_GENERATOR::gauss_mean_ull(unsigned long long mean, 
    double variance) 
{
    return ((unsigned long long)(gauss01() * variance + mean));
}

int RANDOM_NUMBER_GENERATOR::poisson( double this_mean)
{
    // http://www.pamvotis.org/vassis/RandGen.htm
    float R;
    float sum = 0.0;
    float z;
    int i = -1;
    while ( sum <= this_mean) {
        R = (float)rand()/(float)(RAND_MAX+1);
        z = -log(R);
        sum += z;
        i++;
    }
    return i;
}

int RANDOM_NUMBER_GENERATOR::pareto( double alpha)
{
    // http://www.pamvotis.org/vassis/RandGen.htm
    // do not use this one; does not have the beta parameter?
    float R;
    R = (float)rand()/(float)(RAND_MAX+1);
    return (float)1/(float)(pow(R,(float)1/alpha));
}

////////////////////////////////////////////////////////////////////////////////
//
// TRAFFIC_INJECTOR
//
////////////////////////////////////////////////////////////////////////////////

// Note: this is the way routers are indexed for a 4x4 NoC:
//  3      7    11     15
//  2      6    10     14
//  1      5     9     13
//  0      4     8     12
// and this is their addressing:
// (0,3) (1,3) (2,3) (3,3) 
// (0,2) (1,2) (2,2) (3,2) 
// (0,1) (1,1) (2,1) (3,1) 
// (0,0) (1,0) (2,0) (3,0)
//
// Note: I should change to work with only (x,y) addresses and counted
// first along x direction;


TRAFFIC_INJECTOR::TRAFFIC_INJECTOR( long id, long x, long y,
    TRAFFIC_TYPE traffic_type, double injection_rate, long seed,
    VNOC *vnoc, ROUTER *router) 
    // Generator is to aggregate traffic from 128 sources and to generate
    // the load of "injection_rate" on a link with rate that
    // I "cooked" to 8.0E-6 such that inside the vnoc_pareto.h to have
    // a ByteTime of 1, which is my simulation cycle;
    //_gen_pareto_level2(8.0E-6, injection_rate, 128)
{
    // selfsimilar;
    _gen_pareto_level2.initialize(1.0E-2, injection_rate, 128, seed);
    _prev_injection_time = 0.0;
    _prev_num_injected_packets = 0;
    _selected_src_for_selfsimilar = false;
    _task_start_time = 0.0;
    _task_stop_time = 0.0;

    _id = id;
    _x = x; 
    _y = y;
    _traffic_type = traffic_type;
    _injection_rate = injection_rate;
    _vnoc = vnoc;
    _router = router;
}

long TRAFFIC_INJECTOR::get_dest_uniform(void)
{
    long dest_id = _id;
    while (dest_id == _id) 
        dest_id = (long) (drand48() * _vnoc->get_num_of_traffic_sinks());
    return dest_id;
}

long TRAFFIC_INJECTOR::get_dest_transpose1(void)
{
    // return index of transposed 1 router;
    long dest_x=0, dest_y=0;
    if ( _vnoc->nx() != _vnoc->ny()) {
        printf("Error:  Transpose1 traffic can be used only with a square topology.");
        exit(-1);
    }
    dest_x = _vnoc->nx() - 1 - _y;
    dest_y = _vnoc->ny() - 1 - _x;
    // id = x * ny + y because of the way routers are indexed in the 2D mesh;
    return (dest_x * _vnoc->ny() + dest_y);
}

long TRAFFIC_INJECTOR::get_dest_transpose2(void)
{
    // return index of transposed 2 router;
    long dest_x=0, dest_y=0;
    if ( _vnoc->nx() != _vnoc->ny()) {
        printf("Error:  Transpose2 traffic can be used only with a square topology.");
        exit (-1);
    }
    dest_x = _y;
    dest_y = _x;
    // id = x * ny + y because of the way routers are indexed in the 2D mesh;
    return (dest_x * _vnoc->ny() + dest_y);
}

long TRAFFIC_INJECTOR::get_dest_hotspot(void)
{
    assert( _vnoc->topology()->hotspots().size() > 0);
    bool i_am_hotspot = false;
    vector<long>::iterator iter = 
        find( _vnoc->topology()->hotspots().begin(), _vnoc->topology()->hotspots().end(), _id);
    if ( iter != _vnoc->topology()->hotspots().end()) { // this node is one of the hotspots
        i_am_hotspot = true;
    }

    // now calculate the probability of sending the packet to hotspot nodes 
    // versus non hotspot nodes
    int n_of_hotspots_dest = _vnoc->topology()->hotspots().size();
    if ( i_am_hotspot) {
        n_of_hotspots_dest --;
    }
    int n_of_normal_dest = _vnoc->nx() * _vnoc->ny() - n_of_hotspots_dest - 1;

    double normal_prob = 
        (1.0 - n_of_hotspots_dest * _vnoc->topology()->hotspot_percentage()/100.0) / 
        (n_of_normal_dest + n_of_hotspots_dest);

    if ( normal_prob < 0) {
        printf("Error:  Something is wrong with hotspot parameters.");
        exit(-1);
    }

    // calculate the probability of choosing a non hotspot node
    double sum_normal_prob = normal_prob * n_of_normal_dest; 

    long dest_id = _id;
    long index = -1;
    if ( drand48() < sum_normal_prob) { 
        // set the destination to one of the normal nodes
        while ( dest_id == _id) {
            index = (unsigned int) (drand48() * _vnoc->topology()->non_hotspots().size());
            dest_id = _vnoc->topology()->non_hotspots()[index];
        }
    }
    else {
        assert(n_of_hotspots_dest);
        // set the destination to one of the hotspots
        while ( dest_id == _id) {
            index = (unsigned int) (drand48() * _vnoc->topology()->hotspots().size());
            dest_id = _vnoc->topology()->hotspots()[index];
        }
    }

    return dest_id;
}

void TRAFFIC_INJECTOR::simulate_one_traffic_injector()
{
    // called by VNOC::receive_EVENT_PE();
    
    // this is the case of synthetic traffic;
    // UNIFORM_TRAFFIC, HOTSPOT_TRAFFIC, TRANSPOSE1_TRAFFIC, 
    // TRANSPOSE2_TRAFFIC, SELFSIMILAR_TRAFFIC
    // injection_rate is packet generation rate;
    bool injected_a_packet_here = false;


    // (1) selfsimilar traffic is handled differently;
    if ( _traffic_type == SELFSIMILAR_TRAFFIC) {
        long dest_id = _id;
        // dest_id will only tell us here to which destination packets
        // were sent; not really used here; all book-keeping is done 
        // inside this call;
        dest_id = inject_selsimilar_traffic_or_update_task_duration();
    }
    

    // (2) uniform, transpose 1/2, hotspot traffic; 
    else {
        if ( drand48() < _injection_rate) { // mimic Poisson arrival
            long dest_id = _id;

            if (_traffic_type == UNIFORM_TRAFFIC) {
                dest_id = get_dest_uniform();
            } else if (_traffic_type == TRANSPOSE1_TRAFFIC) {
                dest_id = get_dest_transpose1();
            } else if (_traffic_type == TRANSPOSE2_TRAFFIC) {
                dest_id = get_dest_transpose2();
            } else if (_traffic_type == HOTSPOT_TRAFFIC) {
                dest_id = get_dest_hotspot();
            } else {
                printf("Error:  Traffic type unknown during simulation of injector traffic.");
                exit(-1);
            }

            if ( dest_id != _id) {
                // here we basically "generate a packet" and get it placed into the 
                // input-port buffer corresponding to local PE of this router;

                // inject or not a packet into the local PE queue;
                // it will not be injected if PE input buffer is full;
                injected_a_packet_here =
                    _router->receive_packet_from_local_traffic_injector( dest_id);

                // vnoc level counter of all injected packets;
                if ( injected_a_packet_here == true) { 
                    _vnoc->incr_total_packets_injected_count();
                }
            } 
            //else {
            //    printf("\n Warning:  PE sends packet to itself.");
            //}
        }
    } 
    
}
