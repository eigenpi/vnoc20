#ifndef _VNOC_H_
#define _VNOC_H_

#include <assert.h>
#include <stdio.h>
#include <vector>
#include <utility>
#include <map>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include "vnoc_topology.h"
#include "vnoc_router.h"
#include "vnoc_pareto.h"


using namespace std;
using boost::mt19937;
using boost::poisson_distribution;
using boost::variate_generator;

class EVENT;
class EVENT_QUEUE;
class GUI_GRAPHICS;


////////////////////////////////////////////////////////////////////////////////
//
// TRAFFIC_INJECTOR
//
////////////////////////////////////////////////////////////////////////////////

class TRAFFIC_INJECTOR {
    private:
        long _id;
        long _x, _y;
        TRAFFIC_TYPE _traffic_type;
        double _injection_rate; // generation rate;
        // make copy here for fast retrieval of different params;
        VNOC *_vnoc;
        // router to which it will be hooked; it will inject traffic into 
        // the input-port buffer corresponding to local PE (port id 0);
        ROUTER *_router; 
        // variables used for selfsimilar traffic management/implementation;
        // initially we select randomly 1/4 of routers as sources of traffic;
        // TODO: change these during the simulation to maintain all the time
        // a fixed number of sources?
        bool _selected_src_for_selfsimilar;
        double _task_start_time;
        double _task_stop_time;
        // selfsimilar related;
        Generator _gen_pareto_level2;
        double _prev_injection_time;
        int _prev_num_injected_packets;
        
    public:
        TRAFFIC_INJECTOR( long id, long x, long y, TRAFFIC_TYPE traffic_type,
            double injection_rate, long seed, VNOC *vnoc, ROUTER *router);
        ~TRAFFIC_INJECTOR() {} 

        long id() const { return _id; }
        long x() const { return _x; }
        long y() const { return _y; }
        TRAFFIC_TYPE traffic_type() const { return _traffic_type; }
        double injection_rate() const { return _injection_rate; }
        void set_selected_src_for_selfsimilar() { _selected_src_for_selfsimilar = true; }
        bool selected_src_for_selfsimilar() const { 
            return _selected_src_for_selfsimilar; 
        }
        void set_task_start_time( double task_start_time) {  _task_start_time = task_start_time; } 
        void set_task_stop_time( double task_stop_time) {  _task_stop_time = task_stop_time; }

        // utils;
        long get_dest_uniform(void);
        long get_dest_transpose1(void);
        long get_dest_transpose2(void);
        long get_dest_hotspot(void);

        long inject_selsimilar_traffic_or_update_task_duration(void);
        Trace generate_new_trace() {
            // this gives us a Trace object with:
            // Trc.TimeStamp : time at which injection is to be done
            // Trc.PacketSize : of that many packets to inject
            return _gen_pareto_level2.GenerateTrace();
        }

        void simulate_one_traffic_injector();   
};

////////////////////////////////////////////////////////////////////////////////
//
// VNOC - versatile network-on-chip simulator;
//
// this is the "host" of the whole animal; it contains the main objects
// of topology info, the routers, the gui, and the queue of evenets that 
// are processed during the simulation;
//
////////////////////////////////////////////////////////////////////////////////

class VNOC {
    private:

        // because we work only with 2D meshes for now, nx and ny basically
        // store _ary_size, which is stored in topology object; 
        long _nx, _ny; // for 2D meshes only;
        long _routers_count;
        // if _verbose is true then detailed debug info will be printed;
        bool _verbose;
        // set true once the simulator finished running the warmup cycles;
        bool _warmup_done;
        // _latency is the final result of running the vNOC simulator; introduced
        // in order to be able to read it from other classes, when vNOC is used 
        // multiple times;
        double _latency;
        double _power;
        long _total_packets_injected_count; // totall, all of them;
        // total number of flits succesfully carried to their destination; 
        // but after the warmup period;
        long _packets_arrived_count_after_wu;
        double _packets_per_cycle; // average;

        // traffic related;
        TRAFFIC_TYPE _traffic_type;
        // used for selfsimilar case, where we need to generate duration tasks;
        mt19937 _gen_4_poisson_level1;
        poisson_distribution<int> _poisson_level1;
        variate_generator< mt19937, poisson_distribution<int> > _rvt_level1;
        //Generator _gen_pareto_level2;

        // the name of the trace file; see README.txt for a description of trace
        // files format;
        ifstream _input_file_st;
        vector<TRAFFIC_INJECTOR> _traffic_injectors;

    public:
        // _routers was made public to be accessed by the gui;
        vector<ROUTER> _routers;
        TOPOLOGY *_topology;
        EVENT_QUEUE *_event_queue;
        GUI_GRAPHICS *_gui;
        
    public:
        VNOC( TOPOLOGY *topology, EVENT_QUEUE *event_queue, bool verbose=true);
        ~VNOC() {
            // if we used traffic from trace files, then close up all opened files;
            if ( _traffic_type == TRACEFILE_TRAFFIC) {
                _input_file_st.close();
                for ( int i = 0; i < _routers.size(); i++) {
                    _routers[i].close_injection_file();
                }
            }
        }

        // basics; some of them replicate info stored in topology object;
        long nx() const { return _nx; }
        long ny() const { return _ny; }

        void set_warmup_done() { _warmup_done = true; }
        bool warmup_done() { return _warmup_done; }
        TOPOLOGY *topology() const { return _topology; }
        EVENT_QUEUE *event_queue() { return _event_queue; }
        GUI_GRAPHICS *gui() { return _gui; };
        void set_gui(GUI_GRAPHICS *gui) { _gui = gui; };

        vector<ROUTER> &routers() { return _routers; }
        const vector<ROUTER> &routers() const { return _routers; }
        ROUTER &router(const ADDRESS &a);
        const ROUTER &router(const ADDRESS &a) const;
        ROUTER *router( long id) {
            //assert( id >= 0 && id < _routers_count);
            return ( &_routers[ id]);
        }

        // traffic related;
        TRAFFIC_TYPE traffic_type() const { return _traffic_type; }
        long get_num_of_traffic_sinks(void) const { return _routers_count; }
        void incr_total_packets_injected_count() { _total_packets_injected_count ++; }
        void incr_packets_arrived_count_after_wu() { _packets_arrived_count_after_wu ++; }
        long total_packets_injected_count(void) const { return _total_packets_injected_count; }
        long packets_arrived_count_after_wu(void) const { return _packets_arrived_count_after_wu; }
        // selfsimilar;
        void select_randomly_sources_for_selfsimilar_traffic();
        int generate_new_task_start_time_from_poisson() {
            // (1) version 1: using the boost poisson_distribution;
            return _rvt_level1();
            // (2) version 2: using my own implementation; results seem
            // o be similar with either of them; use version 2 so that
            // boost is not needed;
            //return _topology->rng().poisson( 600);
        }
        //Trace generate_new_trace() {
            // this gives us a Trace object with:
            // Trc.TimeStamp : time at which injection is to be done
            // Trc.PacketSize : of that many packets to inject
            // return _gen_pareto_level2.GenerateTrace();
        //}
        long get_a_router_id_randomly() {
            return _topology->rng().flat_l(0, _routers_count - 1);
        }
        
        bool check_address(const ADDRESS &a) const;
        long routers_count() { return _routers_count; }
        long total_packets_injected_count() { return _total_packets_injected_count; }
        bool verbose() const { return _verbose; }
        void set_latency(double val) { _latency = val; }
        double latency() const { return _latency; }
        void set_power(double val) { _power = val; }
        double power() const { return _power; }
        void set_packets_per_cycle(double val) { _packets_per_cycle = val; }
        double packets_per_cycle() const { return _packets_per_cycle; }

        // receive_ functions are used inside the main while
        // loop of the simulation-queue;
        bool receive_EVENT_PE( EVENT this_event);
        bool receive_EVENT_ROUTER( EVENT this_event);
        bool receive_EVENT_LINK( EVENT this_event);
        bool receive_EVENT_CREDIT( EVENT this_event);
        
        void receive_EVENT_ROUTER_SINGLE( EVENT this_event);
        void receive_EVENT_SYNC_PREDICT_DVFS_SET( EVENT this_event);
        

        bool run_simulation();
        void check_simulation();
        void print_network_routers();
        void update_and_print_simulation_results( bool verbose = true);
        bool update_and_check_for_early_termination();
        
        // DVFS related;
        void set_frequencies_and_vdd( DVFS_LEVEL to_level);
        void compute_and_print_prediction_stats();
};


#endif
