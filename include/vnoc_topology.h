#ifndef _VNOC_TOPOLOGY_H_
#define _VNOC_TOPOLOGY_H_

#include "vnoc_utils.h"
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <string>


using namespace std;

// DVFS related;
enum DVFS_LEVEL { DVFS_BOOST, DVFS_BASE, DVFS_THROTTLE_1, DVFS_THROTTLE_2 };

// predictors related;
enum PREDICTOR_TYPE { EXPONENTIAL_AVERAGING, HISTORY, RECURSIVE_LEAST_SQUARE, ARMA };
enum DVFS_MODE { ASYNC, SYNC };

enum ROUTING_ALGORITHM { XY = 0, TXY = 1 };
// synthetic traffic type
enum TRAFFIC_TYPE { UNIFORM_TRAFFIC, HOTSPOT_TRAFFIC, TRANSPOSE1_TRAFFIC,
    TRANSPOSE2_TRAFFIC, TRACEFILE_TRAFFIC, IPCORE_TRAFFIC, 
    SELFSIMILAR_TRAFFIC };
// if shared mode, then the buffer of a vc can be used by
// multiple flits of different packets;
enum VIRTUAL_CHANNEL_SHARING { SHARED, NOT_SHARED };
// idle means empty and not owned by anybody;
enum VC_STATE { IDLE, ROUTING, VC_AB, SW_AB, SW_TR, HOME };

typedef vector<long> ADDRESS;
// VC_PAIR will be used to store relations between input and output
// vc's; these pair/relations reflect routing matrices/mappings;
typedef pair<long, long> VC_PAIR;
typedef vector<unsigned long long> DATA; // data "carried" by each flit;
typedef unsigned long long DATA_ATOMIC_UNIT; // 64 bits for now; we could take it as param;


const VC_PAIR VC_NULL = VC_PAIR(-1,-1); 

#define REPORT_STATS_PERIOD 2000

// the "input" buffers from the local PE have a size/capacity of 100;
#define BUFF_BOUND 512

#define S_ELPS 0.00000001
#define ATOM_WIDTH 64
#define ZERO 0
#define MAX_64_ 0xffffffffffffffffLL
#define CORR_EFF 0.8

// power estimations related stuff;
#define POWER_NOM 1e9
// the three voltages we work with (in V);
// adopted from A. Coskun paper TCAD'2009 for a tech node of 65nm;
#define VDD_BOOST        1.3
#define VDD_BASE         1.2 
#define VDD_THROTTLE_1   1.1 
#define VDD_THROTTLE_2   1.0 
// the three frequencies we work with (in GHz);
#define FREQ_BOOST       2.5
#define FREQ_BASE        2.0 
#define FREQ_THROTTLE_1  1.8 
#define FREQ_THROTTLE_2  1.6 
// energy scaling factors;
#define SCALING_BOOST      1.1736    
#define SCALING_BASE       1.0    
#define SCALING_THROTTLE_1 0.8403
#define SCALING_THROTTLE_2 0.6944 

// delay it takes a flit to traverse the physical link from upstream to 
// downstream router;
#define WIRE_DELAY_BASE 1.0 // 0.9; 
// delay of a router's pipeline stage;
#define PIPE_DELAY_BASE 1.0 // 1.0; 
// delay it takes the credit control signal to go back to upstream router to 
// let it know that there is room in this downstream router;
#define CREDIT_DELAY_BASE 1.0 // 1.0; 
// if DVFS is done and the above delays are for the BASE CASE (that is
// 1.0 is the normalized 2.0GHz), then the boost and two throttle cases 
// (2.5GHz, 1.8 GHz and 1.6GHz) would have these "normalized" delays;
// the boost case:
#define WIRE_DELAY_BOOST        0.8
#define PIPE_DELAY_BOOST        0.8 
#define CREDIT_DELAY_BOOST      0.8
// 1.8GHz case:
#define WIRE_DELAY_THROTTLE_1   1.111 
#define PIPE_DELAY_THROTTLE_1   1.111 
#define CREDIT_DELAY_THROTTLE_1 1.111 
// 1.6GHz case:
#define WIRE_DELAY_THROTTLE_2   1.25
#define PIPE_DELAY_THROTTLE_2   1.25 
#define CREDIT_DELAY_THROTTLE_2 1.25 


////////////////////////////////////////////////////////////////////////////////
//
// TOPOLOGY
//
// this class basically stores info about the topology of the network
// and its elements; additional parameters are also stored inside
// the host vnoc object;
// these parameters can be set by the user via the command line arguments,
// which are parsed by one of the functions of this class;
//
////////////////////////////////////////////////////////////////////////////////

class TOPOLOGY 
{
    private:

        // UNIFORM_TRAFFIC, HOTSPOT_TRAFFIC, TRANSPOSE1_TRAFFIC, TRANSPOSE2_TRAFFIC 
        TRAFFIC_TYPE _traffic_type;
        double _injection_rate; // packet generation rate;
        string _trace_file;
        vector<long> _hotspots; // indices of hotspot nodes?
        vector<long> _non_hotspots; // indices of non-hotspot nodes?
        // packets are sent to hotspot nodes with this much additional 
        // percent probability
        double _hotspot_percentage;

        ROUTING_ALGORITHM _routing_algo;
        long _input_buffer_size; // buffer size for each virtual channel;
        long _output_buffer_size; // output buffer size;
        long _vc_number; // virtual channels count per input port;
        // size size in terms of number of flits; should be min of 2 to have
        // a Head and  a Tail flit; default is 4 to have 2 Body flits too;
        long _packet_size; 
        long _flit_size; // flit size as multiple of "64 bits";
        // the bandwidth - in bits - of physical link; default is 64 bits, which
        // is the default of minimum flit dimension (ie _flit_size = 1);
        long _link_bandwidth;
        double _simulation_cycles_count;
        double _warmup_cycles_count;
        bool _use_gui;
        // this option should be used with "use_gui"; if set true by user, 
        // then each user will have to hit "Proceed" button to advance the
        // simulation after every printing interval; default is false;
        bool _gui_step_by_step;
        long _rng_seed; // seed for random number generator; else is set to 1;
        // if _verbose is true then detailed framework run will be printed
        // by calling "print_*" functions; default is true;
        bool _verbose;

        // for example, if you want to simulate a mesh of 4x4 you should set
        // _ary_size = 4 and _cube_size = 2; see Dally's book for this
        // terminology...
        long _ary_size; // k-ary; i.e., "network size" in one dimension;
        long _cube_size; // n-cube;
        double _link_length; // physical link length in um;
        long _pipeline_stages_per_link; // number of pipeline registers for each link?
        // virtual channel sharing? depends on the type of routing;
        // by default it is SHARED;
        VIRTUAL_CHANNEL_SHARING _vc_sharing_mode;

        RANDOM_NUMBER_GENERATOR _rng;

        // prediction related;
        PREDICTOR_TYPE _predictor_type;
        //long _predict_distance;
        long _control_period;
        long _history_window;
        long _history_weight; 
        bool _do_dvfs;
        bool _use_freq_boost;
        bool _use_link_pred; // link in Li Shang paper;
        // _dvfs_mode can be asynchronous (each router counts its own
        // number of clock cycles, say 100 of them, and then it performs
        // prediction) or synchronous with all other routers, when
        // we count say 100 of "base" cycles and then perform prediction
        // for all routers;
        DVFS_MODE _dvfs_mode;
        

    public:
        TOPOLOGY( int argc, char *argv[]);
        ~TOPOLOGY() {}
    
        bool parse_command_arguments( int argc, char *argv[]);
        bool check_topology();
        void print_topology();
        double injection_rate() const { return _injection_rate; }

        // traffic related;
        TRAFFIC_TYPE traffic_type() const { return _traffic_type; }
        vector<long> hotspots() const { return _hotspots; }
        vector<long> non_hotspots() const { return _non_hotspots; }
        double hotspot_percentage() const { return _hotspot_percentage; }
        void populate_hotspot_sketch_arrays();

        // prediction related;
        PREDICTOR_TYPE predictor_type() const { return _predictor_type; }
        //long predict_distance() const { return _predict_distance; }
        long control_period() const { return _control_period; }
        long history_window() const { return _history_window; }
        long history_weight() const { return _history_weight; }
        bool do_dvfs() const { return _do_dvfs; }
        bool use_freq_boost() const { return _use_freq_boost; }
        bool use_link_pred() const { return _use_link_pred; }
        DVFS_MODE dvfs_mode() const { return _dvfs_mode; }

        long ary_size() const { return _ary_size; }
        long cube_size() const { return _cube_size; }
        long virtual_channel_number() const { return _vc_number; }
        long input_buffer_size() const { return _input_buffer_size; }
        long output_buffer_size() const { return _output_buffer_size; }
        long packet_size() const { return _packet_size; }
        long flit_size() const { return _flit_size; }
        long link_bandwidth() const { return _link_bandwidth; }
        double link_length() const { return _link_length; }
        ROUTING_ALGORITHM routing_algo() const { return _routing_algo; }
        double simulation_cycles_count() const { return _simulation_cycles_count; }
        double warmup_cycles_count() const { return _warmup_cycles_count; }
        VIRTUAL_CHANNEL_SHARING vc_sharing_mode() const { return _vc_sharing_mode; }
        string trace_file() { return _trace_file; }
        void set_rng_seed(long seed) { _rng_seed = seed; }
        long rng_seed() const { return _rng_seed; }
        RANDOM_NUMBER_GENERATOR &rng() { return _rng; }
        bool use_gui() { return _use_gui; }
        bool gui_step_by_step() { return _gui_step_by_step; }
        bool verbose() const { return _verbose; }
};

#endif
