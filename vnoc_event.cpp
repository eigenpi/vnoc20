#include "vnoc_event.h"
#include "vnoc_gui.h"
#include <assert.h>


using namespace std;


////////////////////////////////////////////////////////////////////////////////
//
// EVENT_QUEUE
//
////////////////////////////////////////////////////////////////////////////////

EVENT_QUEUE::EVENT_QUEUE(double start_time, TOPOLOGY *topology) :
    _event_count(0), 
    _events()
{
    _current_sim_time = start_time;
    _topology = topology;
}

void EVENT_QUEUE::insert_initial_events()
{
    // call only after vnoc object has been set;
    for ( long i = 0; i < _vnoc->routers_count(); i++) {
        add_event( EVENT(EVENT::ROUTER_SINGLE, 0.0, i));
    }
    if ( _vnoc->topology()->dvfs_mode() == SYNC) {
        add_event( EVENT(EVENT::SYNC_PREDICT_DVFS_SET,
            double(_vnoc->topology()->history_window()) + 0.001));
    }
    
}

bool EVENT_QUEUE::run_simulation() 
{
    double report_at_time = 0;
    char msg[BUFFER_SIZE];
    _queue_events_simulated = 0; // reset counter;

    if ( _topology->use_gui()) {
        sprintf( msg, "INITIAL - Time: %.2f  All packets injected: %ld  Packets arrived after warmup: %ld ",
                 0.0, 0, 0);
        _vnoc->gui()->update_screen( PRIORITY_MAJOR, msg, ROUTERS);
    }


    // () pick up events from queue and process as long as it's not empty or
    // forced to stop; simulation cycles count is an integer as number of
    // clock cycles in BASE DVFS settings - case in which the clock period
    // is 1; throttle 1 and 2 periods are a bit longer than 1 but that
    // does not matter; events are ordered in queue based on their timings;
    // first time an event is found in queue with "timing" later than
    // simulation cycles count, simulation is stopped; note that for example,
    // if all routers are set to operate at freq. throttle 2, total number of
    // such clock periods simulated will be less than the simulation cycles 
    // count, which is always measured in BASE clock cycles;
    double simulation_cycles_count = _topology->simulation_cycles_count();
    while ( _events.size() > 0 && _current_sim_time <= simulation_cycles_count) {


        // () check if warmup cycles are finished; only after this we should
        // start computing latency statistics;
        if ( _vnoc->warmup_done() == false && 
            _current_sim_time >= _topology->warmup_cycles_count()) {
            _vnoc->set_warmup_done(); // set it true; mark that warmup is done;
        }



        // () simulation: retrieve/consume events and process them; create
        // and add more events to queue;
        EVENT this_event = *get_event();
        remove_top_event();
        _queue_events_simulated ++;
        assert( _current_sim_time <= this_event.start_time() + S_ELPS);
        // Note: when traffic is synthetic (uniform, transpose, hotspot)
        // current_sim_time will be integer; when using tracefiles, injection
        // times are real values; that are in between "clock cycles" that
        // have edges as the times when events of type ROUTER are extracted
        // from queue and simulated;
        _current_sim_time = this_event.start_time();


        if ( _current_sim_time > report_at_time) {
            // early stop if we see that the avg. latency is too large
            // it means that we are well beyond the saturation point and 
            // continuing to run would be waste of time; avg. latency
            // would end up being very high anyways;
            if ( _vnoc->update_and_check_for_early_termination() == true) {
                printf("\nError:  Simulation terminated due to avg. latency too large.");
                break;
            }            

            if ( _vnoc->verbose()) {
                printf("-------------------------------------------------------------");
                printf("\nCurrent time: %.2f  Simulated queue-events: %ld",
                       _current_sim_time, _queue_events_simulated);
                printf("\nAll packets injected: %ld  Packets arrived after warmup: %ld\n",
                       _vnoc->total_packets_injected_count(), _vnoc->packets_arrived_count_after_wu());
            }
            printf("%.2f%% \n", (100 * _current_sim_time / simulation_cycles_count));

            _vnoc->update_and_print_simulation_results( _vnoc->verbose());

            report_at_time += REPORT_STATS_PERIOD;

            if ( _topology->use_gui()) {
                double left_time = _topology->simulation_cycles_count() - _current_sim_time;
                sprintf( msg, "Current time: %.2f  Simulated queue-events: %ld  All packets injected: %ld  Packets arrived after warmup: %ld ",
                    _current_sim_time, _queue_events_simulated,
                    _vnoc->total_packets_injected_count(), _vnoc->packets_arrived_count_after_wu());
                _vnoc->gui()->update_screen( ( _topology->gui_step_by_step() ? 
                    PRIORITY_MAJOR : PRIORITY_MINOR), msg, ROUTERS);
            }
        }
        

        // () this switch case is the main engine of the simulation;
        

        switch ( this_event.type()) {

            case EVENT::PE :
                // next function will call simulate_one_traffic_injector()
                // for each injector in the network; if injecting from tracefile,
                // then packets are read from tracefiles directly;
                _vnoc->receive_EVENT_PE( this_event);
                break;

            case EVENT::ROUTER_SINGLE :
                assert(this_event.from_router_id() >= 0 && 
                    this_event.from_router_id() < _vnoc->routers_count());
                //printf(" %d", this_event.id());                
                _vnoc->receive_EVENT_ROUTER_SINGLE( this_event);
                break;

            case EVENT::SYNC_PREDICT_DVFS_SET :
                // done only when dvfs mode is SYNC; done once every other
                // "_history_window" base cycles;
                _vnoc->receive_EVENT_SYNC_PREDICT_DVFS_SET( this_event);
                break;

            case EVENT::LINK :
                _vnoc->receive_EVENT_LINK( this_event);
                break;

            case EVENT::CREDIT :
                _vnoc->receive_EVENT_CREDIT( this_event);
                break;

            default:
                printf("DUMMY\n");
                assert(0);
                break;

        }

    }
    


    // () gui to be or not to be;
    if ( _topology->use_gui()) {
        sprintf( msg, "FINAL - Time: %.2f  All packets injected: %ld  Packets arrived after warmup: %ld ",
            _current_sim_time, _vnoc->total_packets_injected_count(), _vnoc->packets_arrived_count_after_wu());
        _vnoc->gui()->update_screen( PRIORITY_MAJOR, msg, ROUTERS);
    }
}
