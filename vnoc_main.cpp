////////////////////////////////////////////////////////////////////////////////
//
// VNOC: versatile network on chip simulator. Version 2.0
// cristinel.ababei@marquette.edu
//
////////////////////////////////////////////////////////////////////////////////

#include "vnoc_topology.h"
#include "vnoc_event.h"
#include "vnoc_gui.h"
#include "vnoc.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>


using namespace std;


////////////////////////////////////////////////////////////////////////////////
//
// launching point;
//
////////////////////////////////////////////////////////////////////////////////

int main( int argc, char *argv[])
{
    // welcome;
    char welcome[] =
        "\n--------------------------------------------------\n"
        "VNOC: Versatile NOC Simulator, Version 2.0 \n"
        "Cristinel Ababei, Compiled "__DATE__" \n"
        "--------------------------------------------------\n";
    printf("%s", welcome);



    // (1) runtime;
    // cpu time: method 1;
    clock_t start_clock, end_clock;
    clock_t diff_clock;
    start_clock = clock();
    assert(start_clock != (clock_t)(-1));
    // cpu time: method 2;
    //struct tms t1, t2;
    //times(&t1);
    // wall clock;
    timeval start_wall, end_wall; // sec and microsec;
    gettimeofday( &start_wall, 0);



    // (2) create topology object; some minimal sanity checks;
    TOPOLOGY topology( argc, argv);
    // create queue object, which is the primary engine of running the 
    // event-driven simulation;
    EVENT_QUEUE event_queue( 0.0, &topology); // start time = 0.0;
    // create network's host; also open the main trace file where in 
    // addition an event type PE is added to the simulation-queue,
    // which will kick of the simulation and insertion of additional
    // events PE later on;
    VNOC vnoc( &topology, &event_queue, topology.verbose()); 
    event_queue.set_vnoc( &vnoc);

    // insert an event type ROUTER into simulation-queue, which will 
    // help kick of the simulation and insertion of additional 
    // events ROUTER later on;
    event_queue.insert_initial_events();


    GUI_GRAPHICS gui( &topology, &vnoc);
    vnoc.set_gui( &gui); // innitially gui is empty;

    if ( topology.use_gui()) { // if user asked to use the gui;
        // mark flag that we are gonna use the gui; set is_gui_usable
        // and wait_for_user_input_automode; then build;
        gui.set_graphics_state( true, 1);
        gui.build();
        gui.init_draw_coords( 100.0, 100.0);
    } else { // gui is not usable;
        gui.set_graphics_state( false, 1);
    }
    


    // (3) run VNOC simulator;
    vnoc.run_simulation();


    // entertain user;
    printf("\n\nRESULTS SUMMARY:\n================\n");
    vnoc.update_and_print_simulation_results();
    vnoc.compute_and_print_prediction_stats();


    // (4) runtime: cpu and wall times;

    // cpu time: method 1;
    end_clock = clock();
    assert(end_clock != (clock_t)(-1));
    diff_clock = end_clock - start_clock;
    printf("\n\n vnoc total cputime  = %.3f sec\n",
        (double)diff_clock/CLOCKS_PER_SEC); // (processor time, clock())
    // cpu time: method 2;
    //times(&t2);
    //printf("vnoc total cputime = %.3f sec\n",
    //    (double)(t2.tms_utime-t1.tms_utime)/HZ); // (processor time, tms)
    // wall clock;
    gettimeofday( &end_wall, 0);
    double diff_sec_usec = end_wall.tv_sec - start_wall.tv_sec + 
        double(end_wall.tv_usec - start_wall.tv_usec) / 1000000.0;
    printf (" vnoc total walltime = %.3f sec\n\n", diff_sec_usec); // (wall clock time)



    // (5) clean-up;
    if ( gui.is_gui_usable()) {
        gui.close_graphics(); // close down X Display
    }
}
