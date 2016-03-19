#include "vnoc_utils.h"
#include "vnoc_topology.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <limits.h>


using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// TOPOLOGY
//
////////////////////////////////////////////////////////////////////////////////

TOPOLOGY::TOPOLOGY( int argc, char *argv[]) : _rng()
{
    // (1) reset NOC topology to defaults;
    // _ary_size is basically the "network size" in one dimension; for example,
    // for a 8x8 regular mesh, _ary_size = 4; I personally do not like this
    // terminology; it's not intuitive; but this is what people use;
    // this is the terminology used in W. Dally's book;
    _ary_size = 8; // I refer to it as "network size" in one dimension;
    _cube_size = 2; // stays always 2 for now;
    _link_length = 2000.0; // in micrometers; 2000*1e-6 = 2 mm;
    _pipeline_stages_per_link = 0; // link is not pipelined;
    _vc_sharing_mode = SHARED;

    // traffic related;
    _traffic_type = TRACEFILE_TRAFFIC; // UNIFORM_TRAFFIC;
    // default 10.0% more/additional probability to send packets to hotspot nodes;
    _hotspot_percentage = 10.0; 
    _injection_rate = 0.015; // default a small value;
    
    // predictor related;
    // EXPONENTIAL_AVERAGING, HISTORY, RECURSIVE_LEAST_SQUARE, ARMA
    _predictor_type = HISTORY;
    _control_period = 100; // cycles;
    _history_window = 200; // cycles; like in Li Shang paper;
    _history_weight = 3; // like in Li Shang paper;
    _do_dvfs = true;
    _dvfs_mode = ASYNC; // SYNC;
    _use_freq_boost = false;
    _use_link_pred = true;

    _routing_algo = XY;
    _input_buffer_size = 16;
    _output_buffer_size = 16;
    _vc_number = 4;
    _packet_size = 6; // TBBBBH 
    _flit_size = 1; // just one phit of 64 bits to keep things simple;
    _link_bandwidth = 64;
    _simulation_cycles_count = 10000;
    _warmup_cycles_count = 1000;
    _use_gui = false; // by default we do not show any graphics;
    _gui_step_by_step = false;
    _rng_seed = 1; // time(NULL);
    _verbose = false;
    

    // (2) parse in user defined topology;
    parse_command_arguments( argc, argv); // reeds in also _rng_seed;
    populate_hotspot_sketch_arrays(); // done only for hotspot traffic;

    print_topology();
        
    // now set the actual seed of the internal random gen;
    _rng.set_seed( _rng_seed);
}

bool TOPOLOGY::parse_command_arguments( int argc, char *argv[])
{
    bool result = true;

    // trace file is mandatory;
    if (argc < 2) {
        printf("\nUsage: vnoc [Options...]\n\n");
        printf(" Option\t\tDescription. (Default hardcoded value)\n");
        printf(" [trace_file:]\tName of trace file (for real application traffic case)\n");
        printf("              \tRead README.txt for description of trace files format\n");
        printf(" [traffic:]\tType of traffic. Must be UNIFORM, HOTSPOT, TRANSPOSE1,\n");
        printf("           \tTRANSPOSE2, SELFSIMILAR, TRACEFILE. (UNIFORM) \n");
        printf(" [hotspots: int int ...]  List of id's of hotspot nodes in the network \n");
        printf("                          (One node in the center of the network) \n");
        printf(" [hotspot_percentage:]    Packets are sent to hotspot nodes with this \n");
        printf("                          much additional percent probability. (10.0)\n");
        printf(" [injection_rate:]        Injection rate. Used with synthetic traffic. (0.015)\n");
        printf(" [ary_size:]\tBasically nx and ny for square meshes. (9) \n");
        printf(" [packet_size:]\tPacket size, for synthetic traffic case. (5) \n");
        printf(" [flit_size:]\tFlit size, for synthetic traffic case. (1) \n");
        printf(" [inp_buf:]\tRouter input-port buffers size in # flits. (5) \n");
        printf(" [out_buf:]\tRouter output-port buffers size in # flits. (5) \n");
        printf(" [routing:]\tRouting algorithm. Must be XY or TXY - Torus XY. (XY) \n");
        printf(" [vc_n:]\tNumber of virtual channels. (2) \n");
        printf(" [link_bw:]\tLink bandwidth in bits. (64) \n");
        printf(" [cycles:]\tSimulation cycles count. (10000) \n");
        printf(" [warmup:]\tWarmup cycles count. (1000) \n");
        printf(" [seed:]\tSeed for internal RNG. (1) \n");
        printf(" [use_gui]\tIf present, GUI will be used\n");
        printf(" [gui_sbs]\tIf present, GUI will be used step-by-step. Use with use_gui.\n");
        printf(" [verbose]\tIf present, debugging info will be printed. \n");
        //printf(" [predict_dist:]\tPrediction distance in cycles (5) \n");
        //printf(" [ctrl_period:]\tDVFS control period in cycles. (15) \n");
        printf(" [hist_window:]\tWindow of history based predictor in cycles. (1000) \n");
        printf(" [do_dvfs:]\tPerform DVFS or not. Must be 0 if False or 1 if True. (1) \n");
        printf(" [dvfs_mode:]\tMust be SYNC or ASYNC - (ASYNC) \n");
        printf(" [use_boost:]\tPerform frequency boost. Must be 0 if False or 1 if True. (0) \n");
        printf(" [use_link_pred:]\tUse also link prediction. Must be 0 if False or 1 if True. (1) \n");

        exit(1);
    }

    int i = 1; // start with 1; 0 is theoretically the name of the executable;
 
    while ( i < argc) {

        if (strcmp (argv[i],"tracefile:") == 0) {
            if (argc <= i+1) {
                printf ("Error:  tracefile option requires a string parameter.\n");
                exit (1);
            } 
            _trace_file = argv[i+1];
            i += 2;
            continue;
        }

        if ( !strcmp(argv[i], "ary_size:")) {
            _ary_size = atoi(argv[i+1]);
            if (_ary_size < 2 || _ary_size > 128) { 
                printf("Error:  ary_size value must be between [2 128].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if (strcmp (argv[i],"routing:") == 0) {
            if (argc <= i+1) {
                printf ("Error:  routing option requires a string parameter.\n");
                exit (1);
            } 
            if (strcmp(argv[i+1], "XY") == 0) {
                _routing_algo = XY;
            } 
            else if (strcmp(argv[i+1], "TXY") == 0) {
                _routing_algo = TXY;
            } else {
                printf("Error:  routing algorithm must be XY or TXY.\n");
                exit (1);
            }
            i += 2;
            continue;
        }

        if (strcmp (argv[i],"traffic:") == 0) {
            if (argc <= i+1) {
                printf ("Error:  traffic option requires a string parameter.\n");
                exit (1);
            } 
            if (strcmp(argv[i+1], "UNIFORM") == 0) {
                _traffic_type = UNIFORM_TRAFFIC;
            } else if (strcmp(argv[i+1], "HOTSPOT") == 0) {
                _traffic_type = HOTSPOT_TRAFFIC;
            } else if (strcmp(argv[i+1], "TRANSPOSE1") == 0) {
                _traffic_type = TRANSPOSE1_TRAFFIC;
            } else if (strcmp(argv[i+1], "TRANSPOSE2") == 0) {
                _traffic_type = TRANSPOSE2_TRAFFIC;
            } else if (strcmp(argv[i+1], "SELFSIMILAR") == 0) {
                _traffic_type = SELFSIMILAR_TRAFFIC;
            } else if (strcmp(argv[i+1], "TRACEFILE") == 0) {
                _traffic_type = TRACEFILE_TRAFFIC;
            } else {
                printf("Error:  traffic must be UNIFORM, HOTSPOT, TRANSPOSE1, TRANSPOSE2, SELFSIMILAR or TRACEFILE.\n");
                exit (1);
            }
            i += 2;
            continue;
        }

        if (strcmp(argv[i], "hotspots:") == 0) {
            long a_hotspot = 0;
            while (++i < argc) { 
                if (strstr(argv[i], ":")) {
                    --i;
                    break; 
                }
                if (!sscanf(argv[i], "%d", &a_hotspot)) {
                    printf("Error:  While reading hotspots id's.\n");
                    exit(1);
                }
                _hotspots.push_back(a_hotspot);
            }
            continue;
        }
        if (strcmp(argv[i], "hotspot_percentage:") == 0) {
            _hotspot_percentage = atof(argv[i+1]);
            if (_hotspot_percentage < 5.0 || _hotspot_percentage > 95.0) { 
                printf("Error:  _hotspot_percentage value must be between [5.0 95.0].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if ( !strcmp(argv[i], "injection_rate:")) {
            _injection_rate = atof(argv[i+1]);
            if (_injection_rate < 0.0001 || _injection_rate > 1.0) { 
                printf("Error:  injection_rate value must be between [0.0001 1].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if ( !strcmp(argv[i], "inp_buf:")) {
            _input_buffer_size = atoi(argv[i+1]); 
            if (_input_buffer_size < 1 || _input_buffer_size > 1024) { 
                printf("Error:  inp_buf value must be between [1 1024].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "out_buf:")) {
            _output_buffer_size = atoi(argv[i+1]);
            if (_output_buffer_size < 1 || _output_buffer_size > 1024) { 
                printf("Error:  out_buf value must be between [1 1024].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if ( !strcmp(argv[i], "vc_n:")) {
            _vc_number = atoi(argv[i+1]); 
            if (_vc_number < 1 || _vc_number > 128) { 
                printf("Error:  vc_n value must be between [1 128].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if ( !strcmp(argv[i], "packet_size:")) {
            _packet_size = atoi(argv[i+1]);
            if (_packet_size < 2 || _packet_size > 32) { 
                printf("Error:  packet_size value must be between [2 32].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "flit_size:")) {
            _flit_size = atoi(argv[i+1]);
            if (_flit_size < 1 || _flit_size > 128) { 
                printf("Error:  flit_size value must be between [1 128].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if ( !strcmp(argv[i], "link_bw:")) {
            _link_bandwidth = atoi(argv[i+1]);
            if (_link_bandwidth < 1 || _link_bandwidth > 128) { 
                printf("Error:  link_bw value must be between [1 128].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if ( !strcmp(argv[i], "cycles:")) {
            _simulation_cycles_count = atoi(argv[i+1]);
            if (_simulation_cycles_count <= 0 || _simulation_cycles_count > 500000) { 
                printf("Error:  cycles value must be between [1 500000].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "warmup:")) {
            _warmup_cycles_count = atoi(argv[i+1]);
            if (_warmup_cycles_count <= 0 || _warmup_cycles_count > 500000) { 
                printf("Error: warmup value must be between [1 500000].\n");
                exit(1);
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "seed:")) {
            _rng_seed = atoi(argv[i+1]);
            if (_rng_seed < 1 || _rng_seed > LONG_MAX) { 
                printf("Error:  rng_seed value must be between [1 %ld].\n", LONG_MAX);
                exit(1); 
            }
            i += 2; 
            continue;
        }

        if (strcmp(argv[i],"use_gui") == 0) {
            _use_gui = true;
            i += 1; // do not take any parameter value;
            continue;
        }
        if (strcmp(argv[i],"gui_sbs") == 0) {
            if ( !_use_gui) {
                printf("Error:  gui_sbs can be used only if use_gui is used first as well.\n");
                exit(1);                
            }
            _gui_step_by_step = true;
            i += 1; // do not take any parameter value;
            continue;
        }

        if (strcmp(argv[i],"verbose") == 0) {
            _verbose = true; // default is false;
            i += 1; // do not take any parameter value;
            continue;
        }
        //if ( !strcmp(argv[i], "predict_dist:")) {
        //    _predict_distance = atoi(argv[i+1]);
        //    if (_predict_distance < 1 || _predict_distance > 500) { 
        //        printf("Error:  predict_dist value must be between [1 500].\n");
        //        exit(1); 
        //    }
        //    i += 2; 
        //    continue;
        //}
        if ( !strcmp(argv[i], "ctrl_period:")) {
            _control_period = atoi(argv[i+1]);
            if (_control_period < 1 || _control_period > 500) { 
                printf("Error:  ctrl_period value must be between [1 500].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "hist_window:")) {
            _history_window = atoi(argv[i+1]);
            if (_history_window < 1 || _history_window > 10000) { 
                printf("Error:  hist_window value must be between [30 10000].\n");
                exit(1); 
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "do_dvfs:")) {
            long do_dvfs_temp = atoi(argv[i+1]);
            if (do_dvfs_temp < 0 || do_dvfs_temp > 1) { 
                printf("Error:  do_dvfs value must be 0 (false) or 1 (true).\n");
                exit(1); 
            }
            if ( do_dvfs_temp == 0) { 
                _do_dvfs = false; 
            } else {
                _do_dvfs = true;
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "use_boost:")) {
            long use_freq_boost_temp = atoi(argv[i+1]);
            if (use_freq_boost_temp < 0 || use_freq_boost_temp > 1) { 
                printf("Error:  use_boost value must be 0 (false) or 1 (true).\n");
                exit(1); 
            }
            if ( use_freq_boost_temp == 0) { 
                _use_freq_boost = false; 
            } else {
                _use_freq_boost = true;
            }
            i += 2; 
            continue;
        }
        if ( !strcmp(argv[i], "use_link_pred:")) {
            long use_use_link_pred_temp = atoi(argv[i+1]);
            if (use_use_link_pred_temp < 0 || use_use_link_pred_temp > 1) { 
                printf("Error:  use_link_pred value must be 0 (false) or 1 (true).\n");
                exit(1); 
            }
            if ( use_use_link_pred_temp == 0) { 
                _use_link_pred = false; 
            } else {
                _use_link_pred = true;
            }
            i += 2; 
            continue;
        }
        if (strcmp (argv[i],"dvfs_mode:") == 0) {
            if (argc <= i+1) {
                printf ("Error:  dvfs mode requires a string parameter.\n");
                exit (1);
            } 
            if (strcmp(argv[i+1], "SYNC") == 0) {
                 _dvfs_mode = SYNC;
            } 
            else if (strcmp(argv[i+1], "ASYNC") == 0) {
                _dvfs_mode = ASYNC;
            } else {
                printf("Error:  dvfs mode must be SYNC or ASYNC.\n");
                exit (1);
            }
            i += 2;
            continue;
        }

        printf("Error:  Parameter #%d '%s' not recognized.\n", i, argv[i]);
        exit(1);
    }
    
    return result;
}

bool TOPOLOGY::check_topology()
{
    bool result = true;

    // very simple sanity checks;
    if ( _ary_size < 2 && _ary_size > 1024) {
        result = false;
    }
    if ( _cube_size < 2 && _cube_size > 4) {
        result = false;
    }
    if ( _input_buffer_size < 0 && _input_buffer_size > 1024) {
        result = false;
    }
    if ( _output_buffer_size < 0 && _output_buffer_size > 1024) {
        result = false;
    }
    if ( _vc_number < 1 && _vc_number > 128) {
        result = false;
    }
    if ( _flit_size < 1 && _flit_size > 128) {
        result = false;
    }
    if ( _link_bandwidth < 1 && _link_bandwidth > 128) {
        result = false;
    }
    if ( _link_length < 0.01 && _link_length > 3000.) {
        result = false;
    }
    if ( _pipeline_stages_per_link < 0 && _pipeline_stages_per_link > 32) {
        result = false;
    }

    if (!result) {
        printf("Error:  NOC topology check failed.\n\n");
        exit(1);
    }
    return result;
}

void TOPOLOGY::print_topology()
{
    printf("ary_size:                 %dx%d \n", _ary_size, _ary_size);
    printf("cube_size:                %d \n", _cube_size);
    printf("packet_size:              %d \n", _packet_size);
    printf("flit_size:                %d \n", _flit_size);
    printf("input_buffer_size:        %d \n", _input_buffer_size );
    printf("output_buffer_size:       %d \n", _output_buffer_size);
    printf("virtual_channel_number:   %d \n", _vc_number);
    printf("link_bandwidth:           %d \n", _link_bandwidth);
    printf("link_length [um]:         %g \n", _link_length);
    //printf("pipeline_stages_per_link: %d \n", _pipeline_stages_per_link);
    printf("seed:                     %ld \n", _rng_seed);
    if ( _routing_algo == XY) {
        printf("routing_algo:             %s \n", "XY");
    } else if ( _routing_algo == TXY) {
        printf("routing_algo:             %s \n", "TXY (Torus XY)");
    }
    if ( _traffic_type == UNIFORM_TRAFFIC) {
        printf("traffic type:             %s \n", "UNIFORM");
    } else if ( _traffic_type == HOTSPOT_TRAFFIC) {
        printf("traffic type:             %s \n", "HOTSPOT");
    } else if ( _traffic_type == TRANSPOSE1_TRAFFIC) {
        printf("traffic type:             %s \n", "TRANSPOSE1");
    } else if ( _traffic_type == TRANSPOSE2_TRAFFIC) {
        printf("traffic type:             %s \n", "TRANSPOSE2");
    }  else if ( _traffic_type == SELFSIMILAR_TRAFFIC) {
        printf("traffic type:             %s \n", "SELFSIMILAR");
    } else if ( _traffic_type == TRACEFILE_TRAFFIC) {
        printf("traffic type:             %s \n", "TRACEFILE");
    } else if ( _traffic_type == IPCORE_TRAFFIC) {
        printf("traffic type:             %s \n", "IPCORE");
    }
    if ( _traffic_type == TRACEFILE_TRAFFIC) {
        printf("trace_file:               %s \n", _trace_file.c_str());
    }
    if ( _traffic_type == HOTSPOT_TRAFFIC) {
        printf("Hotspot id's:             ");
        for (vector<long>::iterator my_iter = _hotspots.begin();
            my_iter < _hotspots.end(); my_iter++ ) {
            printf("%d ", (*my_iter));
        }
        printf(" \n");
        printf("hotspot_percentage:       %.2f \n", _hotspot_percentage);
    }
    if ( _traffic_type != TRACEFILE_TRAFFIC &&
        _traffic_type != IPCORE_TRAFFIC) {
        printf("injection_rate:           %.4f \n", _injection_rate);
    }
    printf("simulation_cycles_count:  %.2f \n", _simulation_cycles_count);
    printf("warmup_cycles_count:      %.2f \n", _warmup_cycles_count);
    //printf("predict_dist:             %d \n", _predict_distance);
    //printf("ctrl_period:              %d \n", _control_period);
    printf("hist_window:              %d \n", _history_window);
    if ( _do_dvfs == false) {
        printf("do_dvfs:                  %s \n", "False");
    } else {
        printf("do_dvfs:                  %s \n", "True");
    }
    if ( _dvfs_mode == SYNC) {
        printf("dvfs_mode:                %s \n", "SYNC");
    } else if ( _dvfs_mode == ASYNC) {
        printf("dvfs_mode:                %s \n", "ASYNC");
    }
    if ( _use_freq_boost == false) {
        printf("use_boost:                %s \n", "False");
    } else {
        printf("use_boost:                %s \n", "True");
    }
    if ( _use_link_pred == false) {
        printf("use_link_pred:            %s \n", "False");
    } else {
        printf("use_link_pred:            %s \n", "True");
    }
    printf("\n");
}

void TOPOLOGY::populate_hotspot_sketch_arrays()
{
    if ( _traffic_type == HOTSPOT_TRAFFIC) {

        // (1) if user did not provide a list of hotspots, create one single hotspot
        // at the center of the mesh;
        if ( _hotspots.empty()) { 
            // assumption: I work with square meshes for now;
            long default_hotspot = _ary_size/2 * _ary_size + _ary_size/2;
            _hotspots.push_back( default_hotspot);
        }

        // (2) initialize the non_hotspots sketch array too;
        _non_hotspots.clear();
        long num_of_nodes = _ary_size * _ary_size; // num of routers;
        for ( long id=0; id < num_of_nodes; id++) {
            if ( find(_hotspots.begin(), _hotspots.end(), id) == _hotspots.end()) {
                _non_hotspots.push_back( id);
            }
        }
    }
}

