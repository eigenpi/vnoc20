#include "vnoc.h"
#include "vnoc_event.h"

#include <math.h>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <limits.h>


using namespace std;

// I should encapsulate this one inside _vnoc;
unsigned long VC_MASK[10] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

////////////////////////////////////////////////////////////////////////////////
//
// ROUTER_INPUT
//
////////////////////////////////////////////////////////////////////////////////

ROUTER_INPUT::ROUTER_INPUT():
    _input_buff(),
    _vc_state(),
    _routing(),
    _selected_routing(),
    _injection_buff_full(false)
{
}

ROUTER_INPUT::ROUTER_INPUT(long physical_ports_count, long vc_count):
    _input_buff(),
    _vc_state(),
    _routing(),
    _selected_routing(),
    _injection_buff_full(false)
{
    long i = 0;
    _input_buff.resize( physical_ports_count);
    for ( i = 0; i < physical_ports_count; i++) {
        _input_buff[i].resize( vc_count);
    }
    _vc_state.resize( physical_ports_count);
    for ( i = 0; i < physical_ports_count; i++) {
        _vc_state[i].resize( vc_count, IDLE);
    }
    _routing.resize( physical_ports_count);
    for ( i = 0; i < physical_ports_count; i++) {
        _routing[i].resize( vc_count);
    }
    _selected_routing.resize( physical_ports_count);
    for ( i = 0; i < physical_ports_count; i++) {
        _selected_routing[i].resize(vc_count, VC_NULL);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// ROUTER_OUTPUT
//
////////////////////////////////////////////////////////////////////////////////

ROUTER_OUTPUT::ROUTER_OUTPUT():
    //_buffer_size_next_r(),
    _counter_next_r(),
    _flit_state(),
    _assigned_to(),
    _vc_usage(), // USED, FREE;
    _out_buffer(),
    _out_addr(),
    _local_counter()
{
}

ROUTER_OUTPUT::ROUTER_OUTPUT(long physical_ports_count, long vc_count,
                             long input_buffer_size, long output_buffer_size):
    //_buffer_size_next_r( input_buffer_size),
    _counter_next_r(),
    _flit_state(),
    _assigned_to(),
    _vc_usage(),
    _out_buffer(),
    _out_addr(),
    _local_counter()
{
    _counter_next_r.resize( physical_ports_count);
    for ( long i = 0; i < physical_ports_count; i++) {
        // initialize the counter at any output port - which maintains
        // the number of available space of input buffer of down-stream/next
        // router that has vc_count virtual channels - with input_buffer_size;
        _counter_next_r[i].resize( vc_count, input_buffer_size);
    }
    _assigned_to.resize( physical_ports_count);
    for ( long i = 0; i < physical_ports_count; i++) {
        // innitially none of the output ports is assigned 
        // to any input port;
        _assigned_to[i].resize( vc_count, VC_NULL);
    }
    _vc_usage.resize( physical_ports_count);
    for ( long i = 0; i < physical_ports_count; i++) {
        _vc_usage[i].resize( vc_count, ROUTER_OUTPUT::FREE);
    }
    _out_buffer.resize( physical_ports_count);
    _flit_state.resize( physical_ports_count);
    _out_addr.resize( physical_ports_count);
    // each of the output ports has initially output_buffer_size 
    // space available;
    _local_counter.resize( physical_ports_count, output_buffer_size);
}

void ROUTER_OUTPUT::remove_flit( long i)
{ 
    _out_buffer[i].erase( _out_buffer[i].begin());
    if ( _flit_state[i].size() > 0) { // this fixed a bug in the original vnoc;
        _flit_state[i].erase( _flit_state[i].begin() );
    }
    _local_counter[i]++; 
}

////////////////////////////////////////////////////////////////////////////////
//
// ROUTER
//
////////////////////////////////////////////////////////////////////////////////

ROUTER::ROUTER( long physical_ports_count, long vc_number, long buffer_size,
                long out_buffer_size, const ADDRESS &address, long ary_size,
                long flit_size, VNOC *owner_vnoc, long id, double link_length,
                PREDICTOR_TYPE predictor_type):
    _id(id),
    _address(address),
    _input(physical_ports_count, vc_number),
    _output(physical_ports_count, vc_number, buffer_size, out_buffer_size),
    _power_module(physical_ports_count, vc_number, flit_size, link_length),
    _predictor_module( predictor_type, 
        owner_vnoc->topology()->control_period(),
        owner_vnoc->topology()->history_window()),
    _ary_size(ary_size), // "network size" in one dimension;
    _flit_size(flit_size),
    _physical_ports_count(physical_ports_count),
    _vc_number(vc_number),
    _buffer_size(buffer_size),
    _out_buffer_size(out_buffer_size),
    _total_delay(0.0),
    _local_injection_time(0.0), // used only with tracefile traffic;
    _local_injection_file()
{
    _vnoc = owner_vnoc;
    _init_data.resize( flit_size);
    for (long i = 0; i < flit_size; i++) {
        _init_data[i] = _vnoc->topology()->rng().flat_ull(0, ULLONG_MAX);
    }

    // set the routing algo for this router as required by user;
    _routing_algo = _vnoc->topology()->routing_algo();


    // compute _overall_size_input_buffs; do not include local PE buffer;
    _overall_size_input_buffs = (_physical_ports_count - 1) * _vc_number * buffer_size;

    
    // () traffic related initializations;
    if ( _vnoc->traffic_type() == TRACEFILE_TRAFFIC) {
        // get and open the "local" trace file name for this router;
        // these files are named like tests/bench.x.y with x,y being
        // the coordinates extracted from address of this router; the contents
        // of these local files to each router are "conglomerated" into the
        // the "main" trace file too tests/bench;
        init_local_injection_file();
        local_injection_file() >> _local_injection_time;
    }
    else if ( _vnoc->traffic_type() == IPCORE_TRAFFIC) {

    } 
    else { // uniform, transpose 1/2, hotspot;

    }


    // () prediction related initializations;
    _cycle_counter_4_prediction = 0;

    // () DVFS related initializations;
    _dvfs_level = DVFS_BASE; 
    _dvfs_level_prev = DVFS_BASE;
    _wire_delay = WIRE_DELAY_BASE;
    _pipe_delay = PIPE_DELAY_BASE;
    _credit_delay = CREDIT_DELAY_BASE;

    // reset the sketch "timers" that say whether we can send flits over each of
    // the links driven by output ports; only output ports 1..4 drive links;
    _can_send_on_link_after_time.resize( _physical_ports_count);
    for ( long i = 0; i < _physical_ports_count; i++) {
        _can_send_on_link_after_time[i] = 0.0;
    }

    // stats about injected packets at this router;
    _inj_packet_counter = 0;
    _num_injections_failed = 0;
}

void ROUTER::init_local_injection_file()
{
    string name_t = _vnoc->topology()->trace_file();

    for (long i = 0; i < _address.size(); i++) {
        ostringstream n_t;
        n_t << _address[i];
        name_t = name_t +  "." + n_t.str();
    }
    _local_injection_file = new ifstream;
    _local_injection_file->open( name_t.c_str());
    if ( !_local_injection_file) {
        printf("\nError: Cannot open 'local' trace file: %s\n", name_t.c_str());
        exit(1);
    }
}

long ROUTER::receive_packet_from_local_trace_file()
{
    // retrieve packets from individual trace files named for example
    // like: tests/bench.x.y
    // these files have the injection information only for router at address x,y;
    // this function is called by:
    // VNOC::receive_EVENT_PE() and ROUTER::send_flit_to_out_buffer_SW_TR()
    // Note: this is called by send_flit_to_out_buffer_SW_TR() only if the 
    // input buffer PE is not full; once that PE buffer gets some flits out
    // thru the switch and will have room available it would read
    // from trace file more packets (each packet will have its own
    // injection time as it appears in tracefiles even though they may
    // be injected at simulation times that are later than the time 
    // stamps as start-time for thsoe flits from tracefile); 
    // this way makes for no flit actualy to be dropped;
    long num_packets_inj_here = 0;
    double injection_time = _vnoc->event_queue()->current_sim_time();
    long cube_size = _vnoc->topology()->cube_size();
    ADDRESS src_addr;
    ADDRESS des_addr;
    long packet_size;
    // read from the trace files associated with this router address
    // as many lines (i.e., packets injected) until this router's local
    // time is less than the simulation-queue "current time";
    // Note1: when this router was created as an object, its local time
    // was read as the first thing from the first line of the local-to-this 
    // router trace file; here, we continue to read from the local file
    // until local time is less than the simulation-queue "current time";
    // Note2: I admit this is kind of ugly and intricated way of opening
    // in one place and reading from other places from a file; makes the
    // code difficult to understand; 

    while ( ( _input.injection_buff_full() == false) && 
            ( _local_injection_time <= injection_time + S_ELPS)) {

        src_addr.clear();
        des_addr.clear();

        // read source address;
        for ( long i = 0; i < cube_size; i++) {
            long t; local_injection_file() >> t;
            if ( local_injection_file().eof()) { return num_packets_inj_here; }
            src_addr.push_back(t);
        }
        assert( src_addr[0] < _ary_size && src_addr[1] < _ary_size);

        // read destination address;
        for ( long i = 0; i < cube_size; i++) {
            long t; local_injection_file() >> t;
            if ( local_injection_file().eof()) { return num_packets_inj_here; }
            des_addr.push_back(t);
        }
        assert( des_addr[0] < _ary_size && des_addr[1] < _ary_size);

        // read packet size;
        local_injection_file() >> packet_size;

        // inject this packet: src_addr -> des_addr;
        inject_packet( _inj_packet_counter,
            src_addr, 
            des_addr, 
            _local_injection_time, packet_size);
        // tracks packets injected at this router across the entire simulation;
        _inj_packet_counter ++;
        // counts only packets injected during this call of this function
        // returns this value to calling function; will help ckeep track
        // of all packets injected "so far" into network from all routers;
        num_packets_inj_here ++;

        // get the local time as the first info on the next line
        // in the local-to-this router trace file;
        if ( !local_injection_file().eof()) {
            local_injection_file() >> _local_injection_time;
            if ( local_injection_file().eof()) { return num_packets_inj_here; }
        }
    }
    return num_packets_inj_here;
}

bool ROUTER::receive_packet_from_local_traffic_injector(long dest_id)
{
    // retrieve packets injected by the traffic_injector hooked
    // to this router and generated with a certain traffic pattern;
    // this function is called by:
    // TRAFFIC_INJECTOR::simulate_one_traffic_injector();

    bool injected_a_packet_here = false;
    double injection_time = _vnoc->event_queue()->current_sim_time();
    long packet_size = _vnoc->topology()->packet_size();

    ADDRESS des_addr;
    // id = x * ny + y because of the way routers are indexed in the 2D mesh;
    des_addr.push_back( dest_id / _vnoc->ny()); // x of dest;
    des_addr.push_back( dest_id % _vnoc->ny()); // y of dest;
    assert( des_addr[0] < _ary_size && des_addr[1] < _ary_size);
    

    if ( _input.injection_buff_full() == false) {
        // inject this packet into the input-port buffer of this router: 
        // src_addr -> des_addr;
        inject_packet( _inj_packet_counter, // used as flit id;
                       _address, // source address is this router;
                       des_addr, // destination address has been decided inside the injector;
                       injection_time,
                       packet_size);

        // count packets injected at this router across the entire simulation;
        _inj_packet_counter ++;
        injected_a_packet_here = true;
    } else {
        _num_injections_failed ++;
    }
    return injected_a_packet_here;
}

void ROUTER::inject_packet( long flit_id, ADDRESS &sor_addr, ADDRESS &des_addr,
    double time, long packet_size)
{
    VC_PAIR vc_pair;
    for ( long l = 0; l < packet_size; l++) {

        DATA flit_data; // vector<unsigned long long>;
        for ( long i = 0; i < _flit_size; i++) {
            // recall that, by default, I work with flit_size of 1;
            _init_data[i] = static_cast<DATA_ATOMIC_UNIT>( // "make up stuff";
                _init_data[i] * CORR_EFF + _vnoc->topology()->rng().flat_ull(0, ULLONG_MAX));
            flit_data.push_back( _init_data[i]);
        }

        if ( l == 0) {
            vc_pair = pair<long, long>(0, _input.input_buff(0,0).size());
            // if it's the HEADER flit choose the shortest waiting vc queue;
            for ( long i = 0; i < _vc_number; i++) {
                long t = _input.input_buff(0,i).size();
                if ( t < vc_pair.second) {
                    vc_pair = pair<long, long>(i, t);
                }
            }
            // if the input buffer is empty, set it to be ROUTING;
            if ( _input.input_buff(0, vc_pair.first).size() == 0) {
                _input.vc_state_update(0, vc_pair.first, ROUTING);
            }
            // if the input buffer has more than predefined flits, then
            // add the flits and flag it;
            if ( _input.input_buff(0, vc_pair.first).size() > BUFF_BOUND) {
                _input.set_injection_buff_full();
            }
            _input.add_flit( 0, (vc_pair.first),
                             FLIT(flit_id, FLIT::HEADER, sor_addr, des_addr, time, flit_data));
        }
        else if ( l < packet_size - 1) {
            _input.add_flit( 0, (vc_pair.first),
                             FLIT(flit_id, FLIT::BODY, sor_addr, des_addr, time, flit_data));
        }
        else { // l == packet_size - 1
            _input.add_flit( 0, (vc_pair.first),
                             FLIT(flit_id, FLIT::TAIL, sor_addr, des_addr, time, flit_data));
        }
        // power module writing here;
        if ( _vnoc->warmup_done() == true)
            _power_module.power_buffer_write(0, flit_data);
    }
}

void ROUTER::receive_flit_from_upstream(long port_id, long vc_id, FLIT &flit)
{
    // receive flit from upstream (neighboring) router;
    _input.add_flit( port_id, vc_id, flit);

    // power module writing here;
    if ( _vnoc->warmup_done() == true)
        _power_module.power_buffer_write( port_id, flit.data());

    if ( flit.type() == FLIT::HEADER) {
        if ( _input.input_buff(port_id, vc_id).size() == 1) {
            // all right, we just added/stored this flit her in this
            // input-port id, vc index buffer; before this this vc buffer
            // was empty and by adding it size is one now; of course,
            // vc satte should be set to ROUTING;
            _input.vc_state_update( port_id, vc_id, ROUTING);
        }
        // else is the case where the input vc buffer was not empty;
        // it has already some state set; could be ROUTING, VC_AB, SW_AB or SW_TR;
        // stus was according to the situation of the oldest flit 
        // in this buffer;
        // whatever that status was, should be left unchanged so that pushing
        // of flits thru is done in pipelined fashion correctly;
    } else {
        // the case of body or tail flits; only if the vc buffer was
        // empty (means its status should be IDLE already) we change 
        // status to SW_AB directly;  
        if ( _input.vc_state(port_id, vc_id) == IDLE) {
            _input.vc_state_update( port_id, vc_id, SW_AB);
            // sanity checks; it's possible to get a body flit to an empty
            // input-port buffer before actually its header flit arrived here 
            // if the header flit was sent at a link frequency lower f_L
            // while body flit was sent quickly at a frequency f_H say;
            // in that case the event corresponding to body flit would be processed
            // from the simulation queue earlier/before the event corresponding to
            // the header flit;
            assert( _input.selected_routing(port_id, vc_id).first >= 0 );
            assert( _input.selected_routing(port_id, vc_id).second >= 0 );
        }
    }
}

void ROUTER::consume_flit(double time, const FLIT &flit)
{
    // receive (i.e., consume) one flit at the destination router;
    if ( flit.type() == FLIT::TAIL) {

        if ( _vnoc->warmup_done()) {
            _vnoc->incr_packets_arrived_count_after_wu();
            // Note: flit.start_time() is the real time when this flit
            // was injected into the PE input buffer at the source router;
            double delta_delay = time - flit.start_time();
            update_total_delay( delta_delay);
        }
    }
}

void ROUTER::receive_credit(long i, long j)
{
    _output.counter_next_r_inc(i, j);
}

////////////////////////////////////////////////////////////////////////////////
//
// simulation of one router
//
////////////////////////////////////////////////////////////////////////////////

void ROUTER::simulate_one_router()
{
    // simulate all router's pipeline stages;


    // rc, vc_ab, sw_ab, sw_tr, and link_tr
    // do this in reverse order here to "mimic" the advance of 
    // information thru the actual hardware stages; in this way
    // data that from previous pipe stage is not "raced" thru during this
    // call; all data go sequentially thu the hardware;
    // it is this trick of calling these function in reverse order that makes for 
    // the data to go thru router pipe stages in order and taking each a clock cycle;

    // () stage 5: 
    // flits (if any) from output ports of this router are sent out 
    // to their downstream router destinations; flits travel on physical 
    // links here;
    // Note: it inserts link-events into simulation queue; these events will
    // be consumed later, after wire-delay, to "mimic" real delay on wire;
    send_flits_via_physical_link();

    // () stage 4: 
    // get flits (if any) from input buffers of this router, from their
    // vc's, and get them thru the crossbar switch to the output ports buffers
    // where they must be routed;
    // Note: it inserts credit-events into simulation queue; these credit events will
    // be consumed later, after credit-delay, to "mimic" real delay of credit
    // signal flying back on control-wires to the upstream router;
    send_flit_to_out_buffer_SW_TR();


    // () stage 3: 
    // switch arbitration; look at all vc's of all input-port buffers,
    // pick up the ones that are waiting for crossbar traversal arbitration,
    // and give access to one only to a given output port;
    sw_arbitration_stage_SW_AB();

    // () stage 2: 
    // go thru all input ports and thru all their vc indices;
    // if any vc state is VC_AB then process and record in map;
    // then, pick out randomly a winner among all
    // i,j that "wanted" a certain output port id, vc index;
    vc_arbitration_stage_VC_AB();


    // () stage 1: routing computation decision;
    // Note: it inserts credit-events into simulation queue; these credit events will
    // be consumed later, after credit-delay, to "mimic" real delay of credit
    // signal flying back on control-wires to the upstream router;
    routing_decision_stage_RC();



    if ( _vnoc->topology()->do_dvfs() == true) {

        // () make prediction for what's gonna be like in the next 
        // history window; history maintainance is done each cycle
        // but prediction is done periodically only at the end of each 
        // history window;
        // this will also run the dvfs algorithm for frequency throttle
        // of router and links; set the dvfs settings as decided
        // by algorithm;
        if ( _vnoc->topology()->dvfs_mode() == ASYNC) {
            _predictor_module.maintain_or_perform_prediction_ASYNC( this);
        } else {
            _predictor_module.maintain_for_prediction_SYNC( this);
        }


        // for debugging only;
        //if (_vnoc->event_queue()->current_sim_time() > 5000)
        //set_frequencies_and_vdd( DVFS_THROTTLE_2);
        //_predictor_module.update_DVFS_settings( this);
    }

}

////////////////////////////////////////////////////////////////////////////////
//
// send_flits_via_physical_link
//
////////////////////////////////////////////////////////////////////////////////

void ROUTER::send_flit_via_physical_link( long i)
{
    // this is basically link traversal;
    // if the output buffer of this port "i" is not empty,
    // take a flit and ship it out thru the physical link to 
    // the downstream router;
    //      4
    //   --------
    //  |        |
    //  | Router |  
    // 1|   id   | 2: port count/id
    //   \_______|
    //  0   3
    // example: i=3, then to_port_id=4 because flit is sent South to
    // port id 4 of the downstream router below;
    // Note: it inserts link-events into simulation queue; these events will
    // be consumed later, after wire-delay, to "mimic" real delay on wire;
    // 
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

    double current_sim_time = _vnoc->event_queue()->current_sim_time();
    if ( _output.out_buffer(i).size() > 0) {
        long to_router_id = -1;
        long to_port_id = -1;
        if ( i == 1) {
            to_router_id = _id - _ary_size; // West neighbor
            to_port_id = 2;
        } else if ( i == 2) {
            to_router_id = _id + _ary_size; // East neighbor
            to_port_id = 1;
        } else if ( i == 3) {
            assert( (_id % _ary_size) > 0);
            to_router_id = _id - 1; // South neighbor
            to_port_id = 4;            
        } else { // i == 4
            to_router_id = _id + 1; // North neighbor
            assert( (to_router_id % _ary_size) > 0);
            to_port_id = 3;
        } // there should be no other case; assumption: just 2D meshes;
        assert( to_router_id >= 0 && to_router_id < _vnoc->routers_count());
         
        FLIT flit_t( _output.get_flit(i));
        VC_PAIR outadd_t = _output.get_addr(i);

        // power stuff here;
        if ( _vnoc->warmup_done() == true)
            _power_module.power_link_traversal( i, flit_t.data());

        _output.remove_flit(i);
        _output.remove_addr(i);
        
        // add a link event to the simulation queue; this will be processed
        // at the downstream router as a flit arrival that took _wire_delay 
        // to traverse the link;
        _vnoc->event_queue()->add_event( 
            EVENT(EVENT::LINK, (current_sim_time + _wire_delay), 
            to_router_id, to_port_id, outadd_t.second, flit_t) );

        // record that a flit has been sent over this link, for 
        // LU calculation purposes; this basically increments
        // the numerator of eq. 2 in Li Shang paper;
        _predictor_module.record_flit_transmission_for_LU_calculation( i);

        // set when is the earliest another flit could be sent over this link;
        // this is to keep ordering of flits thru link whose frequency may change
        // and else we could get something like a body flit get
        // faster than its header flits if a change in frequency happened;
        _can_send_on_link_after_time[i] = ( current_sim_time + _wire_delay);
    }

    /*---
    // this is replaced with the above, which is simpler and works for 2D meshes;
    // this version here is more generic, and trickier to understand too;
    // currently this is not used;  
    double current_sim_time = _vnoc->event_queue()->current_sim_time();
    if ( _output.out_buffer(i).size() > 0) {
        ADDRESS to_address = _address;
        long to_port_id;
        if ((i % 2) == 0) {
            to_port_id = i - 1;
            to_address[(i - 1) / 2] ++;
            if (to_address[(i-1) / 2] == _ary_size) {
                to_address[(i-1) / 2] = 0;
            }
        } else {
            to_port_id = i + 1;
            to_address[(i - 1) / 2] --;
            if (to_address[(i-1) / 2] == -1) {
                to_address[(i-1) / 2] = _ary_size - 1;
            }
        }
        FLIT flit_t( _output.get_flit(i));
        VC_PAIR outadd_t = _output.get_addr(i);

        // power stuff here;
        if ( _vnoc->warmup_done() == true)
            _power_module.power_link_traversal( i, flit_t.data());

        _output.remove_flit(i);
        _output.remove_addr(i);
        
        // add a link event to the simulation queue; this will be processed
        // at the downstream router as a flit arrival that took _wire_delay 
        // to traverse the link;
        _vnoc->event_queue()->add_event( EVENT(EVENT::LINK,
            (current_sim_time + _wire_delay), to_address, to_port_id,
            outadd_t.second, flit_t));

        // set when is the earliest another flit could be sent over this link;
        // this is to keep ordering of flits thru link whose frequency may change
        // and else we could get something like a body flit get
        // faster than its header flits if a change in frequency happened;
        _can_send_on_link_after_time[i] = ( current_sim_time + _wire_delay);
    }
    ---*/
}

void ROUTER::send_flits_via_physical_link()
{
    // send flits from output ports through links to 
    // downstream routers; flits traverse physical links here;
    // TODO: if user selects zero size for output-port buffers, then
    // this function must be combined with the SW_TR phase;

    double current_sim_time = _vnoc->event_queue()->current_sim_time();
    for ( long i = 1; i < _physical_ports_count; i++) {
    
        // send only if enough time elapssed from last transmission
        // possibly at a different frequency slower or faster;
        if ( current_sim_time >= _can_send_on_link_after_time[i]) {
            send_flit_via_physical_link( i);
            //printf("_");
        } else {
            printf("|");
            //printf("  ct: %.2f %.2f", current_sim_time, _can_send_on_link_after_time[i]);
        }
        
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// send_flit_to_out_buffer: SW_TR
//
////////////////////////////////////////////////////////////////////////////////

void ROUTER::send_flit_to_out_buffer_SW_TR()
{
    // this is basically switch traversal SW_TR phase!
    // send flit to the output buffer after the SA_AB took place;
    // Note: it inserts credit-events into simulation queue; these credit events will
    // be consumed later, after wire-delay, to "mimic" real delay on wire
    // back to the upstream router;

    for ( long i = 0; i < _physical_ports_count; i++) {
        for ( long j = 0; j < _vc_number; j++) {
            if ( _input.vc_state(i, j) == SW_TR) {
                // from input port "i", vc index "j" -> output port "sel_routing.first",
                // vc index "sel_routing.second";
                VC_PAIR sel_routing = _input.selected_routing(i, j);
                // in this upstream router, at the output port with index
                // "sel_routing.first", we maintain for vc with index "sel_routing.second"
                // how many empty slots are available in the downstream router
                // "seen" from this port;
                // when sending a flit now, we know that downstream router will
                // have a slot less of space available; so, we decrement the 
                // counter here;
                _output.counter_next_r_dec(sel_routing.first, sel_routing.second);

                // if this is not a flit from local PE, create and add a credit
                // event to the simulation queue; this will let the upstream
                // router that an empty slot became available here;
                double event_time = _vnoc->event_queue()->current_sim_time();
                if (i > 0) { // meaning != 0;
                    /*---
                    // this is replaced with what's next to it, which is simpler
                    // but works for only 2D meshes; currently this is not used;  
                    ADDRESS cre_add_t = _address;
                    long cre_pc_t = i;
                    if ( (i % 2) == 0) {
                        cre_pc_t = i - 1;
                        cre_add_t[(i-1)/2] ++;
                        if (cre_add_t[(i-1)/2] == _ary_size) {
                            cre_add_t[(i-1)/2] = 0;
                        }
                    } else {
                        cre_pc_t = i + 1;
                        cre_add_t[(i-1)/2] --;
                        if (cre_add_t[(i-1)/2] == -1) {
                            cre_add_t[(i-1)/2] = _ary_size - 1;
                        }
                    }
                    _vnoc->event_queue()->add_event( EVENT(EVENT::CREDIT,
                        event_time + _credit_delay, _address, cre_add_t, cre_pc_t, j));
                    ---*/

                    long to_router_id = -1;
                    long to_port_id = -1;
                    // if the destination is to the right, send credit to
                    // left and vice-versa; if it's up, send it down and vice-versa;
                    if ( i == 1) {
                        to_router_id = _id - _ary_size; // West neighbor
                        to_port_id = 2;
                    } else if ( i == 2) {
                        to_router_id = _id + _ary_size; // East neighbor
                        to_port_id = 1;
                    } else if ( i == 3) {
                        assert( (_id % _ary_size) > 0);
                        to_router_id = _id - 1; // South neighbor
                        to_port_id = 4;            
                    } else { // i == 4
                        to_router_id = _id + 1; // North neighbor
                        assert( (to_router_id % _ary_size) > 0);
                        to_port_id = 3;
                    } // there should be no other case; assumption: just 2D meshes;
                    assert( to_router_id >= 0 && to_router_id < _vnoc->routers_count());

                    _vnoc->event_queue()->add_event( 
                        EVENT(EVENT::CREDIT, (event_time + _credit_delay),
                        to_router_id, to_port_id, j)); // j is VC index;
                }

                // pick up the flit to be sent over the crossbar switch; erase it from
                // input buffer;
                long in_size_t = _input.input_buff(i,j).size();
                assert(in_size_t >= 1);
                FLIT flit_t( _input.get_flit(i,j));
                _input.remove_flit(i, j);

                // power stuff here;
                if ( _vnoc->warmup_done() == true) {
                    _power_module.power_buffer_read( i, flit_t.data());
                    _power_module.power_crossbar_trav( i, sel_routing.first, flit_t.data());
                }
                
                // Note: output buffer is not organized by virtual channels like
                // the input buffers; all flits from various vc indices of various 
                // input ports are stored in the output buffer; 
                _output.add_flit(sel_routing.first, flit_t);

                // if we just shipped over crossbar a flit coming from the local PE
                // and if happened that the PE input buffer was full, then now - because
                // a slot became available - read in more flits from the local trace file;
                if ( i == 0) {
                    if ( _vnoc->traffic_type() == TRACEFILE_TRAFFIC) {
                        if ( _input.injection_buff_full() == true) {
                            if ( _input.input_buff(0,j).size() < BUFF_BOUND) {
                                _input.clear_injection_buff_full();
                                receive_packet_from_local_trace_file();
                            }
                        }
                    }
                }

                // output port "sel_routing.first", vc index "sel_routing.second";
                _output.add_addr(sel_routing.first, sel_routing);
                
                // if the output buffer has just added a tail flit: that means that
                // this output port is available (if it still has empty slots)
                // to accept more flits of other packets from any other input
                // port and vc index; the already existing flits in the output
                // buffer of this port are already waiting in line inside the output
                // buffer pipeline to be shipped thru the physical link;
                if ( flit_t.type() == FLIT::TAIL) {
                    _output.release(sel_routing.first, sel_routing.second);
                }
                if ( in_size_t > 1) { // Note: this was size before moving the flit;
                    if ( flit_t.type() == FLIT::TAIL) {
                        if ( _vnoc->topology()->vc_sharing_mode() == NOT_SHARED) {
                            if (i != 0){
                                if (in_size_t != 1) {
                                    cout<<i<<":"<<in_size_t<<endl;
                                }
                                assert(in_size_t == 1);
                            }
                        }
                        // if input port "i", vc index "j" had more than one flit,
                        // and the one just moved to output port was a TAIL flit;
                        _input.vc_state_update(i, j, ROUTING);
                        // Note: the above basically says that we just moved to output a 
                        // tail flit, but the input port buffer of this vc is not empty,
                        // which means that the next flit in the input buffer of this vc 
                        // must be a head flit of another packet; so, it will have
                        // to go thru the ROUTING + VC_AB phases (as opposed to body
                        // and tail flits that go directly thru SW_AB stage - see the 
                        // else branch of this if);
                    } else {
                        // if input port "i", vc index "j" had more than one flit,
                        // and the one just moved to output port was not a TAIL,
                        // then there are still flits of the same packet to be 
                        // arbitrated;
                        _input.vc_state_update(i, j, SW_AB);
                        // sanity checks;
                        assert( _input.selected_routing(i,j).first >= 0 );
                        assert( _input.selected_routing(i,j).second >= 0 );
                    }
                } else { // in_size_t == 1
                    // input port "i", vc index "j" had only one flit, which
                    // now has just been moved to the output buffer of port 
                    // "sel_routing.first"; hence, input buffer "i", vc "j" becomes 
                    // empty; so, update its status to IDLE;
                    _input.vc_state_update(i, j, IDLE);
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// sw arbitration: SW_AB
//
////////////////////////////////////////////////////////////////////////////////

void ROUTER::sw_arbitration_stage_SW_AB() 
{
    // this is basically switch arbitration SW_AB phase!
    // switch arbitration pipeline stage; processes all input ports
    // at the same time;

    // (1) build map;
    map<long, vector<VC_PAIR> > vc_o_map;
    for ( long i = 0; i < _physical_ports_count; i++) {
        vector<long> vc_i_t;
        for ( long j = 0; j < _vc_number; j++) {
            if ( _input.vc_state(i, j) == SW_AB) {
                VC_PAIR out_t = _input.selected_routing(i, j);
                if (( _output.counter_next_r(out_t.first, out_t.second) > 0) &&
                    ( _output.local_counter(out_t.first) > 0)) {
                    vc_i_t.push_back(j);
                }
            }
        }
        long vc_size_t = vc_i_t.size();
        if ( vc_size_t > 1) {
            long win_t = _vnoc->topology()->rng().flat_l(0, vc_size_t);
            VC_PAIR r_t = _input.selected_routing(i, vc_i_t[win_t]);
            vc_o_map[r_t.first].push_back(VC_PAIR(i, vc_i_t[win_t]));
        } else if ( vc_size_t == 1) {
            VC_PAIR r_t = _input.selected_routing(i, vc_i_t[0]);
            vc_o_map[r_t.first].push_back(VC_PAIR(i, vc_i_t[0]));
        }
    }

    if ( vc_o_map.size() == 0) {
        return;
    }

    // (2) go thru each output port; if the map has recorded that this output
    // port is "desired" by more input ports, then arbitrate it and give it
    // randomly to an input port that competes for it; 
    for ( long i = 0; i < _physical_ports_count; i++) {
        long vc_size_t = vc_o_map[i].size();
        if ( vc_size_t > 0) {
            VC_PAIR vc_win = vc_o_map[i][0];
            if ( vc_size_t > 1) { // if more than 1 pi
                vc_win = vc_o_map[i][ _vnoc->topology()->rng().flat_l(0, vc_size_t) ];
            }
            _input.vc_state_update(vc_win.first, vc_win.second, SW_TR);
            FLIT &flit_t = _input.get_flit(vc_win.first, vc_win.second);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// vc arbitration algorithms: implementation of selection function in routing
//
////////////////////////////////////////////////////////////////////////////////

pair<long, long> ROUTER::vc_selection(long i, long j) 
{
    // the flit here at input port "i", vc index "j" of this upstream
    // router will have to be routed to the downstream routers' input 
    // buffer in some vc index; here we select that vc index randomly 
    // from among the free ones;
    // choose one of the candidate routing vc;

    // (1) first get all free potential vc candidates;
    vector<VC_PAIR > &vc_candidates = _input.routing(i, j);
    long r_size_t = vc_candidates.size();
    assert( r_size_t > 0);
    vector<VC_PAIR > vc_that_are_free;

    for ( long i = 0; i < r_size_t; i++) {
        VC_PAIR v_t = vc_candidates[i];
        if ( _vnoc->topology()->vc_sharing_mode() == SHARED) {
            // if shared mode, then the buffer of a vc can be used by 
            // multiple flits of different packets;
            if ( _output.vc_usage(v_t.first, v_t.second) == ROUTER_OUTPUT::FREE) {
                vc_that_are_free.push_back(vc_candidates[i]);
            }
        } else { // NOT_SHARED
            // if not in shared mode vc selection will go thru only if 
            // the downstream buffer of this vc is really empty; only in that
            // case it is included in the list from which the random pick will
            // be made;
            if ( _output.counter_next_r(v_t.first, v_t.second) == _buffer_size) {
                if (  _output.vc_usage(v_t.first, v_t.second) == ROUTER_OUTPUT::FREE) {
                    vc_that_are_free.push_back(vc_candidates[i]);
                }
            }
        }
    }

    // (2) then, pick out one randomly;
    r_size_t = vc_that_are_free.size();
    long vc_t = 0;
    if ( r_size_t > 0) {
        if ( r_size_t > 1) {
            vc_t = _vnoc->topology()->rng().flat_l(0, r_size_t);
        }
        // return pair of output port id, and vc index;
        return (vc_that_are_free[vc_t]);
    } else {
        return VC_PAIR(-1, -1);
    }
}

void ROUTER::vc_arbitration_stage_VC_AB()
{
    // (1) go thru all input ports and thru all their vc indices;
    // if any vc state is VC_AB then process and record in map;
    map<VC_PAIR, vector<VC_PAIR> > vc_o_i_map;
    DATA_ATOMIC_UNIT vc_request = 0;

    for ( long i = 0; i < _physical_ports_count; i++) {
        for ( long j = 0; j < _vc_number; j++) {
            if ( _input.vc_state(i, j) == VC_AB) {
                // pick out a vc index from among those that are
                // free of the output port where this flit has to go;
                // vc_pair is a pair of (output port id, and vc index), where
                // the vc index is the free vc index of input buffers of
                // downstream router;
                VC_PAIR vc_pair = vc_selection(i,j);
                if ((vc_pair.first >= 0) && (vc_pair.second >= 0)) {
                    // output port id, vc index is "desired" also by
                    // i,j; so record this in map that stores al i,j
                    // that "want" this output port id, vc index;
                    vc_o_i_map[vc_pair].push_back(VC_PAIR(i, j));
                    vc_request = vc_request | VC_MASK[i * _vc_number + j];
                }
            }
        }
    }
    if ( vc_o_i_map.size() == 0) {
        return;
    }

    // (2) now, go and pick out randomly a winner among all
    // input port id, input vc id who wanted/competed-for the
    // ourput-port id=i, out vc index=j;
    for ( long i = 1; i < _physical_ports_count; i++) {
        for ( long j = 0; j < _vc_number; j++) {
            if ( _output.vc_usage(i, j) == ROUTER_OUTPUT::FREE) {
                long cont_temp = vc_o_i_map[VC_PAIR(i,j)].size();
                if ( cont_temp > 0) {
                    VC_PAIR vc_win = vc_o_i_map[VC_PAIR(i,j)][0]; // winner input-port;
                    if ( cont_temp > 1) {
                        vc_win = vc_o_i_map[VC_PAIR(i,j)][ // winner input-port;
                            _vnoc->topology()->rng().flat_l(0, cont_temp)];
                    }
                    // Note: the winner vc_win gets its status changed to SW_AB;
                    // because its next phase will be to traverse the crossbar;
                    // so, it will have to go thru switch arbitration process;
                    _input.vc_state_update(vc_win.first, vc_win.second, SW_AB);
                    // () record the selected routing; this will be used during the
                    // next stage of SA_AB;
                    _input.assign_selected_routing(vc_win.first, 
                                                   vc_win.second, VC_PAIR(i,j));
                    _output.acquire(i, j, vc_win);

                    // power stuff here;
                    if ( _vnoc->warmup_done() == true)
                        _power_module.power_vc_arbit( i, j, vc_request,
                            (vc_win.first) * _vc_number + (vc_win.second));
                }
            }
        }
    }   
}

////////////////////////////////////////////////////////////////////////////////
//
// routing_decision_stage: RC
//
////////////////////////////////////////////////////////////////////////////////

void ROUTER::routing_decision_stage_RC()
{
    // this is basically the routing computation RC phase!
    // it's responsible with either consuming the flits if their destination
    // is this router or "clearing and populating" the "_routing" variable and 
    // setting status to VC_AB for header flits; 
    // for tail flits status become IDLE (buffer is not used);
    // this is done by going thru all input port id's and all vc indices;

    double event_time = _vnoc->event_queue()->current_sim_time();

    // (1) process first the PE injection physical port 0;
    // Note: flits injected by the PE at this router and which are 
    // put in any of the vc channels of port "0", and whose destination
    // is the PE itself, are consumed directly and the vc status is set to HOME;
    // is this a real scenario of PE injecting packets to itself?
    for ( long j = 0; j < _vc_number; j++) {
        // HEADER flit;
        FLIT flit_t;
        if ( _input.vc_state(0,j) == ROUTING) {
            flit_t = _input.get_flit(0,j);
            ADDRESS des_t = flit_t.des_addr();
            ADDRESS sor_t = flit_t.src_addr();
            if ( _address == des_t) {
                // (a) packets injected to itself;
                consume_flit( event_time, flit_t);
                _input.remove_flit(0, j);
                _input.vc_state_update(0, j, HOME);
            } else {
                // (b) packets injected for other destinations than itself;
                _input.clear_routing(0,j);
                _input.clear_selected_routing(0,j);

                // call the actual routing algo; this populates _routing of
                // _input that will be used during vc_arbitration_stage;
                call_current_routing_algorithm( des_t, sor_t, 0, j);

                _input.vc_state_update(0, j, VC_AB);

            }
        // BODY or TAIL type flits;
        // these flits do not undergo the routing computation stage of
        // the router pipeline; they shortcut that;
        } else if ( _input.vc_state(0,j) == HOME) {
            if ( _input.input_buff(0, j).size() > 0) {
                flit_t = _input.get_flit(0, j);
                assert( flit_t.type() != FLIT::HEADER);
                consume_flit( event_time, flit_t);
                _input.remove_flit(0, j);
                if ( flit_t.type() == FLIT::TAIL) {
                    if ( _input.input_buff(0, j).size() > 0) {
                        _input.vc_state_update(0, j, ROUTING);
                    } else {
                        _input.vc_state_update(0, j, IDLE);
                    }
                }
            }
        }
    }

    // (2) then, process the other physical ports;
    for ( long i = 1; i < _physical_ports_count; i++) {
        for ( long j = 0; j < _vc_number; j++) {
            // if flit is consumed here, then send right away back 
            // a CREDIT event/message to let upstream router that an 
            // empty slot just became available;
            FLIT flit_t;
            if ( _input.input_buff(i,j).size() > 0) {
                flit_t = _input.get_flit(i,j);
                ADDRESS des_t = flit_t.des_addr();
                if ( _address == des_t) {
                    /*---
                    // this is replaced with what's next to it, which is simpler
                    // but works for only 2D meshes; currently this is not used;  
                    ADDRESS cre_add_t = _address;
                    long cre_pc_t = i;
                    // if the destination is to the right, send credit to
                    // left and vice-versa; if it's up, send it down and vice-versa;
                    if ((i % 2) == 0) {
                        cre_pc_t = i - 1;
                        cre_add_t[(i-1)/2] ++;
                        if (cre_add_t[(i-1)/2] == _ary_size) {
                            cre_add_t[(i-1)/2] = 0;
                        }
                    } else {
                        cre_pc_t = i + 1;
                        cre_add_t[(i-1)/2] --;
                        if (cre_add_t[(i-1)/2] == -1) {
                            cre_add_t[(i-1)/2] = _ary_size - 1;
                        }
                    }
                    _vnoc->event_queue()->add_event( EVENT(EVENT::CREDIT,
                        event_time + _credit_delay, _address, cre_add_t, cre_pc_t, j));
                    ---*/

                    long to_router_id = -1;
                    long to_port_id = -1;
                    // if the destination is to the right, send credit to
                    // left and vice-versa; if it's up, send it down and vice-versa;
                    if ( i == 1) {
                        to_router_id = _id - _ary_size; // West neighbor
                        to_port_id = 2;
                    } else if ( i == 2) {
                        to_router_id = _id + _ary_size; // East neighbor
                        to_port_id = 1;
                    } else if ( i == 3) {
                        assert( (_id % _ary_size) > 0);
                        to_router_id = _id - 1; // South neighbor
                        to_port_id = 4;            
                    } else { // i == 4
                        to_router_id = _id + 1; // North neighbor
                        assert( (to_router_id % _ary_size) > 0);
                        to_port_id = 3;
                    } // there should be no other case; assumption: just 2D meshes;
                    assert( to_router_id >= 0 && to_router_id < _vnoc->routers_count());

                    _vnoc->event_queue()->add_event( 
                        EVENT(EVENT::CREDIT, (event_time + _credit_delay),
                        to_router_id, to_port_id, j)); // j is VC index;

                }
            }
            //  HEADER flit;
            if ( _input.vc_state(i, j) == ROUTING) {
                flit_t = _input.get_flit(i, j);
                assert(flit_t.type() == FLIT::HEADER);
                ADDRESS des_t = flit_t.des_addr();
                ADDRESS sor_t = flit_t.src_addr();
                if ( _address == des_t) {
                    // (a) flit arrived at input port "i", vc index "j"
                    // has as address this local router;
                    consume_flit( event_time, flit_t);
                    _input.remove_flit(i, j);
                    _input.vc_state_update(i, j, HOME);
                } else {
                    // (b) flits are just passing thru toward other destination;
                    _input.clear_routing(i, j);
                    _input.clear_selected_routing(i, j);

                    // call the actual routing algo; this populates _routing of
                    // _input that will be used during vc_arbitration_stage;
                    call_current_routing_algorithm( des_t, sor_t, i, j);

                    _input.vc_state_update(i, j, VC_AB);
                    
                }
            }
            // BODY or TAIL flits;
            else if ( _input.vc_state(i, j) == HOME) {
                if ( _input.input_buff(i, j).size() > 0) {
                    flit_t = _input.get_flit(i, j);
                    assert( flit_t.type() != FLIT::HEADER);
                    consume_flit( event_time, flit_t);
                    _input.remove_flit(i, j);
                    if ( flit_t.type() == FLIT::TAIL) {
                        if ( _input.input_buff(i, j).size() > 0) {
                            _input.vc_state_update(i, j, ROUTING);
                        } else {
                            _input.vc_state_update(i, j, IDLE);
                        }
                    }
                }
            }
        }
    }
}

void ROUTER::call_current_routing_algorithm(
    const ADDRESS &des_t, const ADDRESS &sor_t,
    long s_ph, long s_vc)
{
    // setup _routing matrix that implements currently used 
    // routing algo (XY or TXY);

    long xoffset = des_t[0] - _address[0];
    long yoffset = des_t[1] - _address[1];

    // (1) XY (XY on mesh)
    int virtual_channels_num = _vc_number;
    if ( _routing_algo == XY) {

        if ( yoffset < 0) {
            for ( long j = 0; j < virtual_channels_num; j++) {
                _input.add_routing(s_ph, s_vc, VC_PAIR(3,j));
            }
        } else if ( yoffset > 0) {
            for ( long j = 0; j < virtual_channels_num; j++) {
                _input.add_routing(s_ph, s_vc, VC_PAIR(4,j));
            }
        } else {
            if ( xoffset < 0) {
                for ( long j = 0; j < virtual_channels_num; j++) {
                    _input.add_routing(s_ph, s_vc, VC_PAIR(1,j));
                }
            } else if ( xoffset > 0) {
                for ( long j = 0; j < virtual_channels_num; j++) {
                    _input.add_routing(s_ph, s_vc, VC_PAIR(2,j));
                }
            }
        }
    }
    // (2) TXY (XY on torus)
    // Note: this to be correted - it seems to use only 2 vc's all the time? 
    else if ( _routing_algo == TXY) {

        bool xdirection = (abs(static_cast<int>(xoffset)) * 2 <= _ary_size) ? true: false; 
        bool ydirection = (abs(static_cast<int>(yoffset)) * 2 <= _ary_size) ? true: false; 

        if ( xdirection) {
            if ( xoffset < 0) {
                _input.add_routing(s_ph, s_vc, VC_PAIR(1, 0));
            } else if ( xoffset > 0) {
                _input.add_routing(s_ph, s_vc, VC_PAIR(2, 1));
            } else {
                if ( ydirection) {
                    if ( yoffset < 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(3, 0));
                    } else if ( yoffset > 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(4, 1));
                    }
                } else {
                    if ( yoffset < 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(4, 0));
                    } else if ( yoffset > 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(3, 1)); 
                    }
                }
            }
        } else {
            if ( xoffset < 0) {
                _input.add_routing(s_ph, s_vc, VC_PAIR(2, 0));
            } else if ( xoffset > 0) {
                _input.add_routing(s_ph, s_vc, VC_PAIR(1, 1));
            } else {
                if ( ydirection) {
                    if ( yoffset < 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(3, 0));
                    } else if ( yoffset > 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(4, 1));
                    }
                } else {
                    if ( yoffset < 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(4, 0));
                    } else if ( yoffset > 0) {
                        _input.add_routing(s_ph, s_vc, VC_PAIR(3, 1)); 
                    }
                }
            }
        }
    }
}

void ROUTER::sanity_check() const
{
    // sanity checks;
    for ( long i = 0; i < _physical_ports_count; i ++) {
        for ( long j = 0; j < _vc_number; j ++) {
            if ( _input.input_buff(i,j).size() > 0) {
                cout << "Input is not empty" << endl;
            }
            if ( _input.vc_state(i,j) != IDLE) {
                cout << "Input state is wrong" << endl;
            } cout << _output.counter_next_r(i,j) << ":";
            if ( _output.counter_next_r(i,j) != _buffer_size) {
                cout << "Output vc counter is wrong" << endl;
            }
            if ( _output.vc_usage(i,j) != ROUTER_OUTPUT::FREE) {
                cout << "Output is not free" << endl;
            }
            if ( _output.assigned_to(i,j) != VC_NULL) {
                cout << "Output is not reset" << endl;
            }
        }
        if ( _output.out_buffer(i).size() > 0) {
            cout << "Output temp buffer is not empty" << endl;
        }
        if ( _output.out_addr(i).size() > 0) {
            cout << "Output temp buffer is not empty" << endl;
        }
        if( _output.local_counter(i) != _out_buffer_size) {
            cout << "Output local counter is not reset" << endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// utils related to predictors
//
////////////////////////////////////////////////////////////////////////////////

double ROUTER::compute_BU_for_downstream_driven_by_out_i( long id)
{
    // return BU of input-port BU of downstream router as a float less than 1;
    // input port of downstream router is driven by the output-port of
    // this router whose index is "id";
    double overall_utilization = 0.0;
    for ( long j = 0; j < _vc_number; j++) {
        // index of output-port is "id", index of virtual-channel of that 
        // output-port is "j";
        // we can simply compute the BU of downstream router input-port
        // by using info about how many empty/available slots has that 
        // downstream-router input-port buffer;
        overall_utilization += ( _buffer_size - _output.counter_next_r(id, j) );
    }
    // return as fraction out of the total buffer space for that input port;
    overall_utilization = overall_utilization / (_vc_number * _buffer_size);
    //printf(" %.1f", overall_utilization);
    return overall_utilization;
}

//double ROUTER::compute_LU_for_link_driven_by_out_i( long id)
//{
//}

double ROUTER::compute_BU_overall_input_buffers_this_r()
{
    // return BU as a float less than 1;
    double overall_utilization = 0.0;
    // Note: I do not include the local PE input buffer; hence,
    // we start with i = 1;
    for ( long i = 1; i < _physical_ports_count; i++) {
        for ( long j = 0; j < _vc_number; j++) {
            if ( _input.input_buff(i,j).size() > 0) {
                overall_utilization += _input.input_buff(i,j).size();
            }
        }
    }
    // return as fraction out of the total buffer space for 
    // the entire router;
    return (overall_utilization / _overall_size_input_buffs);
}


////////////////////////////////////////////////////////////////////////////////
//
// utils related to DVFS
//
////////////////////////////////////////////////////////////////////////////////

void ROUTER::set_frequencies_and_vdd( DVFS_LEVEL to_level)
{
    // Note: I moved this to maintain_or_perform_prediction_ASYNC()
    // (2) because dvfs settings may be changed here we also call 
    // scale_and_accumulate_energy(), to keep track of scaled
    // energy so far; we accumulate the energy
    // with the appropriate scaling because the power module accumulate
    // the energy, unscaled as computed by Orion;
    //_power_module.scale_and_accumulate_energy();


    // (1) if level is the same, no change;
    if ( _dvfs_level == to_level) { 
        // do nothing because the level of this r is already what's wanted;
        return; 
    }


    // (3) actual change of dvfs settings;
    //printf("\n change dvfs %d",_id);
    _dvfs_level_prev = _dvfs_level;
    _dvfs_level = to_level;
    switch ( to_level) {
    case DVFS_BOOST: // see vnoc_topology.h for details on these values;
        _wire_delay = WIRE_DELAY_BOOST;
        _pipe_delay = PIPE_DELAY_BOOST;
        _credit_delay = CREDIT_DELAY_BOOST;
        _power_module.set_current_period( PIPE_DELAY_BOOST);
        _power_module.set_current_vdd( VDD_BOOST);
        _power_module.set_energy_scaling_factor( SCALING_BOOST);
        break;
    case DVFS_BASE:
        _wire_delay = WIRE_DELAY_BASE;
        _pipe_delay = PIPE_DELAY_BASE;
        _credit_delay = CREDIT_DELAY_BASE;
        _power_module.set_current_period( PIPE_DELAY_BASE);
        _power_module.set_current_vdd( VDD_BASE);
        _power_module.set_energy_scaling_factor( SCALING_BASE);
        break;
    case DVFS_THROTTLE_1:
        _wire_delay = WIRE_DELAY_THROTTLE_1;
        _pipe_delay = PIPE_DELAY_THROTTLE_1;
        _credit_delay = CREDIT_DELAY_THROTTLE_1;
        _power_module.set_current_period( PIPE_DELAY_THROTTLE_1);
        _power_module.set_current_vdd( VDD_THROTTLE_1);
        _power_module.set_energy_scaling_factor( SCALING_THROTTLE_1);
        break;
    case DVFS_THROTTLE_2:
        _wire_delay = WIRE_DELAY_THROTTLE_2;
        _pipe_delay = PIPE_DELAY_THROTTLE_2;
        _credit_delay = CREDIT_DELAY_THROTTLE_2;
        _power_module.set_current_period( PIPE_DELAY_THROTTLE_2);
        _power_module.set_current_vdd( VDD_THROTTLE_2);
        _power_module.set_energy_scaling_factor( SCALING_THROTTLE_2);
        break;
    default:
        assert(0); // we shoud never get here;
        break;
    }
}


