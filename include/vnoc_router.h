#ifndef _VNOC_ROUTER_H_
#define _VNOC_ROUTER_H_

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

#include "vnoc_topology.h"
#include "vnoc_predictor.h"


extern "C" {
#include "router.h"
#include "SIM_router.h"
#include "SIM_reg.h"
#include "SIM_crossbar.h"
#include "SIM_arbiter.h"
#include "SIM_misc.h"
#include "SIM_link.h"
#include "SIM_clock.h"
#include "SIM_array.h"
#include "SIM_router_power.h"
}


using namespace std;

class EVENT;
class ROUTER;
class VNOC;


////////////////////////////////////////////////////////////////////////////////
//
// POWER_MODULE
//
////////////////////////////////////////////////////////////////////////////////

class POWER_MODULE {
    ROUTER *_router; // its owner;
    long _flit_size;

    //SIM_power_router_info_t _router_info; // orion1
    SIM_router_info_t _router_info; // orion2
    //router_info_t _router_info; // orion3

    //SIM_power_router_t _router_power; // orion1
    SIM_router_power_t _router_power; // orion2
    //router_power_t _router_power; // orion3

    SIM_router_area_t _router_area; // orion2
    //router_area_t _router_area; // orion3

    //SIM_power_arbiter_t _arbiter_vc_power; // orion1
    SIM_arbiter_t _arbiter_vc_power; // orion2

    //SIM_power_bus_t _link_power;
    SIM_bus_t _link_power;

    vector<DATA> _buffer_write;
    vector<DATA> _buffer_read;
    vector<DATA> _crossbar_read;
    vector<DATA> _crossbar_write;
    vector<DATA> _link_traversal;
    vector<long> _crossbar_input;
    vector<vector<DATA_ATOMIC_UNIT> > _arbiter_vc_req;
    vector<vector<unsigned long> > _arbiter_vc_grant;
    // freq. boost/throttle;
    // sketch variables to store scaled (up or down - depending
    // on if its freq. boost or throttle) energy; if working at
    // the default Vdd, then scaling factor is 1;
    // _scaled_energy will store total energy spent so far,
    // until current time; it scaled according to how Vdd has been 
    // scaled; for example normally we could have a run till 100 n
    // for default value of Vdd=1.2V for which energy is say e = 100 J;
    // if say at time 70 ns we would throttle freq. (which would also 
    // require to lower Vdd) then the energy would have to be
    // e = e1 + e2 = 70 + 30*energy_scaling_factor (less energy);
    // this "e" will be stored by _scaled_energy, which is
    // the accumulated energy with the appropriate scalings for
    // different periods of time according to how Vdd changed;
    // note that because freq is lower not all packets would have been
    // processed (as in the default case) and to finish them we
    // would have to continue simulation longer which will increase 
    // curr_time used to compute avg. power consumption, which will
    // also get smaller (due to this increase in curr_time but also
    // due to the lower Vdd);
    // Note: freq will be boosted or throttled by changing the clock
    // when events are created during simulation;
    double _scaled_energy; 
    // _prev_unscaled_energy stores the value of total accumulated energy
    // (as computed, unscaled by Orion) till the last time when a recording
    // took place; needed to compute "delta" of energy that needs to be 
    // scaled and accumulated;
    double _prev_unscaled_energy;
    // components;
    double _scaled_energy_buffer; 
    double _prev_unscaled_energy_buffer;
    double _scaled_energy_crossbar; 
    double _prev_unscaled_energy_crossbar;
    double _scaled_energy_arbiter; 
    double _prev_unscaled_energy_arbiter;
    double _scaled_energy_link; 
    double _prev_unscaled_energy_link;
    double _scaled_energy_clock; 
    double _prev_unscaled_energy_clock;

    double _current_vdd;
    double _current_period; // Tclock = 1/current_freq;
    double _energy_scaling_factor;

 public:
    POWER_MODULE(long physical_ports_count, long vc_count,
                 long flit_size, double link_length);
    ~POWER_MODULE() {}

    void power_buffer_read(long in_port, DATA & read_d);
    void power_buffer_write(long in_port, DATA & write_d);
    void power_crossbar_trav(long in_port, long out_port, DATA & trav_d);
    void power_vc_arbit(long pc, long vc, DATA_ATOMIC_UNIT req, unsigned long gra);
    void power_link_traversal(long in_port, DATA & read_d);
    void power_clock_record();
    double power_buffer_report();
    double power_link_report();
    double power_crossbar_report();
    double power_arbiter_report();
    double power_clock_report();
    
    // utils;
    void set_current_vdd(double vdd) { _current_vdd = vdd; }
    void set_current_period(double tick) { _current_period = tick; }
    double current_period() const { return _current_period; }
    void set_energy_scaling_factor(double esf) { _energy_scaling_factor = esf; }
    double scaled_energy() const { return _scaled_energy; }
    double scaled_energy_buffer() const { return _scaled_energy_buffer; }
    double scaled_energy_crossbar() const { return _scaled_energy_crossbar; }
    double scaled_energy_arbiter() const { return _scaled_energy_arbiter; }
    double scaled_energy_link() const { return _scaled_energy_link; }
    double scaled_energy_clock() const { return _scaled_energy_clock; }

    // freq. boost/throttle;
    void scale_and_accumulate_energy(); 
};

////////////////////////////////////////////////////////////////////////////////
//
// FLIT
//
////////////////////////////////////////////////////////////////////////////////

class FLIT {
    public:
        enum FLIT_TYPE { HEADER, BODY, TAIL };
    private:
        long _id;
        FLIT_TYPE _type;
        double _start_time;
        double _finish_time;
        ADDRESS _src_addr;
        ADDRESS _des_addr;
        DATA _data; // vector<unsigned long long>;

    public:
        FLIT() : _id(0), _type(HEADER), _src_addr(0), _des_addr(0), 
            _start_time(0), _finish_time(0), _data() {}
        FLIT( long id, FLIT_TYPE type, ADDRESS &src, ADDRESS &des,
            double start_time, const DATA &data) : 
            _id(0), _type(type), 
            _src_addr(src), _des_addr(des), 
            _start_time(start_time), _finish_time(0), 
            _data(data) {}
        FLIT( const FLIT &f) : _id(f.id()), _type(f.type()),
            _src_addr(f.src_addr()), _des_addr(f.des_addr()),
            _start_time(f.start_time()), _finish_time(f.finish_time()), 
            _data(f.data()) {}
        ~FLIT() {}

        int id() const { return _id; }
        void set_id(int id) { _id = id; }
        FLIT_TYPE type() const { return _type; }
        double start_time() const { return _start_time; }
        double finish_time() const { return _finish_time; }
        ADDRESS src_addr() const { return _src_addr; }
        ADDRESS des_addr() const { return _des_addr; }
        DATA &data() { return _data; }
        const DATA &data() const { return _data; }
};

////////////////////////////////////////////////////////////////////////////////
//
// ROUTER_INPUT
//
////////////////////////////////////////////////////////////////////////////////

class ROUTER_INPUT {
    private:
        ROUTER *_router; // its owner;
        // input buffers: <physical port index<vc index<buffer>>>
        vector<vector<vector<FLIT> > > _input_buff;
        // state of each input vc;
        vector<vector<VC_STATE> > _vc_state; // IDLE, ROUTING, VC_AB, SW_AB, SW_TR, HOME
        // candidate routing vcs; this stores the routing matrix/mapping
        // between inputs and outputs; recall that: typedef pair<long,long> VC_PAIR;
        vector<vector<vector<VC_PAIR> > > _routing; // pair<long, long>
        // the selected routing vc;
        vector<vector<VC_PAIR> > _selected_routing;
        // this is a flag record that the buffer of injection is full;
        // used to control the packet injection and decrease mem usage;
        bool _injection_buff_full;

    public:
        ROUTER_INPUT(long physical_ports_count, long vc_count);
        ROUTER_INPUT();
        ~ROUTER_INPUT() {}

    public:
        // _input_buff;
        vector<vector<vector<FLIT> > > &input_buff() { return _input_buff; }
        const vector<vector<vector<FLIT> > > &input_buff() const 
            { return _input_buff; }
        vector<FLIT> &input_buff(long i, long j) { return _input_buff[i][j]; } 
        const vector<FLIT> &input_buff(long i, long j) const 
            { return _input_buff[i][j]; }
        //_vc_state;
        vector<vector<VC_STATE> > &vc_state() { return _vc_state; }
        const vector<vector<VC_STATE> > &vc_state() const { return _vc_state; }
        VC_STATE vc_state(long i, long j) { return _vc_state[i][j]; }
        const VC_STATE vc_state(long i, long j) const { return _vc_state[i][j]; }
        void vc_state_update(long i, long j, VC_STATE state)
            { _vc_state[i][j] = state;}
        // _routing;
        vector<vector<vector<VC_PAIR> > > &routing() { return _routing; }
        const vector<vector<vector<VC_PAIR> > > &routing() const 
            { return _routing; }
        vector<VC_PAIR> &routing(long i, long j) { return _routing[i][j];}
        const vector<VC_PAIR> &routing(long i, long j) const 
            { return _routing[i][j]; }
        void add_routing(long i, long j, VC_PAIR t) { _routing[i][j].push_back(t); }
        void clear_routing(long i, long j) { _routing[i][j].clear(); }
        // _selected_routing;
        vector<vector<VC_PAIR> > &selected_routing() { return _selected_routing; }
        const vector<vector<VC_PAIR> > & selected_routing() const 
            { return _selected_routing; }
        VC_PAIR &selected_routing(long i, long j) { return _selected_routing[i][j]; }
        const VC_PAIR &selected_routing(long i, long j) const 
            { return _selected_routing[i][j]; }
        void assign_selected_routing(long i, long j, VC_PAIR c) 
            { _selected_routing[i][j] = c; }
        void clear_selected_routing(long i, long j) 
            { _selected_routing[i][j] = VC_NULL; }

        // utils;
        void add_flit(long i, long j, const FLIT &flit) 
            { _input_buff[i][j].push_back(flit); }
        void remove_flit(long i, long j)
            { _input_buff[i][j].erase(_input_buff[i][j].begin()); }
        FLIT &get_flit(long i, long j) {
            assert( _input_buff[i][j].size() > 0);
            return ( _input_buff[i][j][0]); 
        }
        FLIT &get_flit(long i, long j, long k) {
            assert( _input_buff[i][j].size() > k);
            return ( _input_buff[i][j][k]); 
        }

        void set_injection_buff_full() { _injection_buff_full = true; }
        void clear_injection_buff_full() { _injection_buff_full = false; }
        bool injection_buff_full() const {return _injection_buff_full; }
};

////////////////////////////////////////////////////////////////////////////////
//
// ROUTER_OUTPUT
//
////////////////////////////////////////////////////////////////////////////////

class ROUTER_OUTPUT {
    public:
        enum VC_USAGE { USED, FREE };
    private:
        ROUTER *_router; // its owner;
        // used for input of next router;
        //long _buffer_size_next_r;
        vector<vector<long> > _counter_next_r;
        vector<vector<VC_STATE> > _flit_state; // IDLE, ROUTING, VC_AB, SW_AB, SW_TR, HOME;
        // assigned to the input
        vector<vector<VC_PAIR> > _assigned_to; // pair<long, long>
        vector<vector<VC_USAGE> > _vc_usage; // USED FREE
        vector<vector<FLIT> > _out_buffer; // actual local output buffers;
        vector<vector<VC_PAIR> > _out_addr; // output address;
        vector<long> _local_counter; // one for each output port;

    public:
        ROUTER_OUTPUT(long i, long j, long c, long d);
        ROUTER_OUTPUT();
        ~ROUTER_OUTPUT() {}

    public:
        //long buffer_size_next_r() const { return _buffer_size_next_r; }
        vector<vector<long> > &counter_next_r() { return _counter_next_r; }
        long counter_next_r(long i, long j) const { return _counter_next_r[i][j]; }
        void counter_next_r_inc(long i, long j) { _counter_next_r[i][j]++; }
        void counter_next_r_dec(long i, long j) { _counter_next_r[i][j]--; }

        VC_STATE flit_state(long i) const { return _flit_state[i][0]; }

        const VC_PAIR &assigned_to(long i, long j) const 
            { return _assigned_to[i][j]; }
        void acquire(long i, long j, VC_PAIR pair) 
            { _vc_usage[i][j] = USED; _assigned_to[i][j] = pair; }
        void release(long i, long j) 
            { _vc_usage[i][j] = FREE; _assigned_to[i][j] = VC_NULL; }
        vector<vector<VC_USAGE> > &vc_usage() { return _vc_usage; }
        VC_USAGE vc_usage(long i, long j) { return _vc_usage[i][j]; }
        VC_USAGE vc_usage(long i, long j) const { return _vc_usage[i][j]; }
        
        void add_flit(long i, const FLIT &flit)
            { _out_buffer[i].push_back(flit); _local_counter[i]--; }
        void remove_flit(long i);
        FLIT &get_flit(long i) {
            assert( _out_buffer[i].size() > 0);
            return _out_buffer[i][0];
        }
        vector<FLIT> &out_buffer(long i) { return _out_buffer[i]; }
        const vector<FLIT> &out_buffer(long i) const 
            { return _out_buffer[i]; }

        vector<vector<VC_PAIR> > out_addr() { return _out_addr; }
        const vector<vector<VC_PAIR> > out_addr() const { return _out_addr; }
        vector<VC_PAIR> out_addr(long i) { return _out_addr[i];}
        const vector<VC_PAIR> out_addr(long i) const { return _out_addr[i]; }
        VC_PAIR get_addr(long i) { return _out_addr[i][0];}
        void remove_addr(long i) { 
            assert( _out_addr[i].size() > 0); 
            _out_addr[i].erase( _out_addr[i].begin());
        }
        void add_addr(long i, VC_PAIR b) { _out_addr[i].push_back(b); }

        vector<long> &local_counter() { return _local_counter; }
        long local_counter(long i) { return _local_counter[i]; }
        long local_counter(long i) const { return _local_counter[i]; }
        void local_counter_inc(long i) { _local_counter[i]++; }
        void local_counter_dec(long i) { _local_counter[i]--; }
};

////////////////////////////////////////////////////////////////////////////////
//
// ROUTER
//
////////////////////////////////////////////////////////////////////////////////

class ROUTER {
    private:
        VNOC *_vnoc; // its owner, which has EVENT_QUEUE *_event_queue as well;
        // for 2D meshes, address is just a vector with two elements; x and y;
        ADDRESS _address;
        long _id;
        ROUTER_INPUT _input; // input buffer module;
        ROUTER_OUTPUT _output; // output buffer module;
        POWER_MODULE _power_module; // power module;

        // prediction related;
        PREDICTOR_MODULE _predictor_module;
        long _cycle_counter_4_prediction;
        

        // initial random number
        DATA _init_data;
        long _ary_size;
        long _flit_size; // how many groups of "64 bits";
        long _physical_ports_count;
        long _vc_number;
        long _buffer_size; // of each virtual channel;
        long _out_buffer_size; // of each vc out buffer size;
        // accumulated total "propagation" delay of all packets with destination
        // (i.e., consumed) by this router;
        double _total_delay;
        ROUTING_ALGORITHM _routing_algo; // routing algorithm used;
        // local_injection_time is used only with tracefile traffic, when
        // injection times are read from local files of type tests/bench.x.y
        // and where injection times may be real numbers in between "integer"
        // edges of the clock that is mimic-ed by simulation engine
        // each time when an event of type ROUTER is processed;
        double _local_injection_time; // used for the next packet injection time;
        long _inj_packet_counter;
        // number of packet injection atempts that failed during
        // synthetic traffic; it happens due to PE input buffer being
        // full;
        long _num_injections_failed;
        // one way to run VNOC is by using trace files; one file for each router;
        // an example is found under tests/; these trace file are supposed
        // to come from real applications; the other way is to use synthetic traffic
        // generators; in the former case, _local_injection_file stores the name
        // of the local injection trace file;
        ifstream *_local_injection_file; // input trace file;
        double _link_length;

        // DVFS related variables;
        // DVFS_BOOST, DVFS_BASE, DVFS_THROTTLE_1, DVFS_THROTTLE_2
        DVFS_LEVEL _dvfs_level; 
        DVFS_LEVEL _dvfs_level_prev; // previous "cycle" dvfs level;
        double _wire_delay;
        double _pipe_delay;
        double _credit_delay;
        // a hack to be able to keep ordering of flits being sent thru links
        // when frequency changes and could get something like a body flit get
        // faster than its header flits if a change in frequency happened;
        vector<double> _can_send_on_link_after_time;

        // summation of size of all input buffers of all vc's; stored
        // for faster computations of BU utilizations; 
        long _overall_size_input_buffs;


    public:
        ROUTER( long physical_ports_count, long vc_number, long buffer_size, 
                long out_buffer_size, const ADDRESS &address, long ary_size, 
                long flit_size, VNOC *owner_vnoc, long id, double link_length,
                PREDICTOR_TYPE predictor_type);
        ~ROUTER() {}

        void set_id(long id) { _id = id; }
        long id() { return _id; }
        void set_owner(VNOC *vnoc) { _vnoc = vnoc; }
        VNOC *vnoc() { return _vnoc; }
        const vector<long> &address() const { return _address; }
        ROUTER_INPUT &input() { return _input; }
        ROUTER_OUTPUT &output() { return _output; }
        POWER_MODULE &power_module() { return _power_module; }
        PREDICTOR_MODULE &predictor_module() { return _predictor_module; }
        long physical_ports_count() { return _physical_ports_count; }
        long vc_number() { return _vc_number; }
        long buffer_size() { return _buffer_size; }
        long inj_packet_counter() { return _inj_packet_counter; }
        long num_injections_failed() { return _num_injections_failed; }

        // calculate the  accumulated total "propagation" delay of all packets;   
        void update_total_delay(double delta) { _total_delay += delta; }
        double total_delay() const { return _total_delay; }

        // traffic related;
        // retrieve packet from trace file associated with this;
        long receive_packet_from_local_trace_file();
        bool receive_packet_from_local_traffic_injector(long dest_id);
        void inject_packet( long flit_id, ADDRESS &sor_addr, ADDRESS &des_addr,
            double time, long packet_size);

        // flit and credit utils;
        void receive_flit_from_upstream(long port_id, long vc_id, FLIT &flit);
        void consume_flit(double t, const FLIT &flit);
        void receive_credit(long i, long j);

        // run simulation of this router;
        void simulate_one_router();
        VC_PAIR vc_selection(long i, long j);
        void routing_decision_stage_RC();
        void vc_arbitration_stage_VC_AB();
        void sw_arbitration_stage_SW_AB();
        void send_flit_to_out_buffer_SW_TR();
        void send_flit_via_physical_link(long i);
        void send_flits_via_physical_link();
        void sanity_check() const;

        ifstream &local_injection_file() { return *_local_injection_file; }
        void init_local_injection_file();
        void close_injection_file() { 
            _local_injection_file->close();
            delete _local_injection_file;
        }
        void call_current_routing_algorithm(
            const ADDRESS &des_t, const ADDRESS &sor_t,
            long s_ph, long s_vc);

        // power estimations related;
        double power_buffer_report() { return _power_module.power_buffer_report(); }
        double power_crossbar_report() { return _power_module.power_crossbar_report(); }
        double power_arbiter_report() { return _power_module.power_arbiter_report(); }
        double power_link_report() { return _power_module.power_link_report(); }
        double power_clock_report() { return _power_module.power_clock_report(); }

        void print_input_buffers() {
            for ( long i = 0; i < _physical_ports_count; i++) {
                printf("\n\t Port %d:",i);
                for ( long j = 0; j < _vc_number; j++) {
                    printf("  VC%d: %d ",j,_input.input_buff(i,j).size());
                }
            }
        }

        // related to predictors
        long cycle_counter_4_prediction() { return _cycle_counter_4_prediction; }
        void incr_cycle_counter_4_prediction() { _cycle_counter_4_prediction++; }
        void reset_cycle_counter_4_prediction() { _cycle_counter_4_prediction = 0; }
        double compute_BU_for_downstream_driven_by_out_i( long id);
        //double compute_LU_for_link_driven_by_out_i( long id);
        double compute_BU_overall_input_buffers_this_r();
        void perform_prediction_SYNC() {
            _predictor_module.perform_prediction_SYNC( this);
        }
      
        // DVFS related;
        DVFS_LEVEL dvfs_level() { return _dvfs_level; }
        DVFS_LEVEL dvfs_level_prev() { return _dvfs_level_prev; }
        void set_wire_delay(double delay) { _wire_delay = delay; }
        void set_pipe_delay(double delay) { _pipe_delay = delay; }
        void set_credit_delay(double delay) { _credit_delay = delay; }
        void set_dvfs_level(DVFS_LEVEL level) { _dvfs_level = level; }
        void set_dvfs_level_prev(DVFS_LEVEL level) { _dvfs_level_prev = level; }
        void set_frequencies_and_vdd( DVFS_LEVEL to_level);
        long overall_size_input_buffs() { return _overall_size_input_buffs; }

        double get_avg_prediction_err_so_far() { 
            return ( 100 *
                _predictor_module.BU_all_prediction_err_as_percentage() /
                _predictor_module.predictions_count()); 
        }
};

////////////////////////////////////////////////////////////////////////////////
//
// LINK
//
////////////////////////////////////////////////////////////////////////////////

class LINK {
  private:
    int _id;
    int _x;
    int _y;
    
  public:
  LINK( int x, int y, int z) : _x(x), _y(y), _id(-1) {}
    ~LINK() {}
    
    int id() const { return _id; }
    void set_id(int id) { _id = id; }
    
};



#endif
