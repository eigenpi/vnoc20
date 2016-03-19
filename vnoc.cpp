#include <math.h>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <limits.h>

#include "vnoc.h"
#include "vnoc_event.h"


using namespace std;


////////////////////////////////////////////////////////////////////////////////
//
// PREDICTOR_MODULE
//
////////////////////////////////////////////////////////////////////////////////


void PREDICTOR_MODULE::maintain_for_prediction_SYNC(ROUTER *my_router) 
{
    // called each time a given router is "simulated" simulate_one_router();

    // does only maintainance (as opposed to ASYNC version, which does
    // both maintainance and prediction);
    // prediction is done once every  "_history_window" base cycles
    // through events of type EVENT::SYNC_PREDICT_DVFS_SET; that 
    // way, each router performs prediction immediately after its
    // time is greater or equal than "_history_window" base cycles;

    // it's basically the first part of maintain_or_perform_prediction_ASYNC();

    // (1) history based prediction;
    if ( _predictor_type == HISTORY) {

        // (a) maintain;
        for (long i = 1; i < 5; i++) {
             _BU_predicted_out_i[i] +=
                my_router->compute_BU_for_downstream_driven_by_out_i( i);
        }

        _BU_predicted_all_inputs +=
            my_router->compute_BU_overall_input_buffers_this_r();

        _counter_router_own_cycles ++;

        my_router->incr_cycle_counter_4_prediction(); // not used for SYNC;
    }

    // (2) exponential averaging; not used currently;
    else if ( _predictor_type == EXPONENTIAL_AVERAGING) {
    }        
}

void PREDICTOR_MODULE::perform_prediction_SYNC(ROUTER *my_router) 
{
    // (1) history based prediction;
    if ( _predictor_type == HISTORY) {

        // (b) perform; because we are at end of history window;
        for ( long i = 1; i < 5; i++) {
            // ---> last cycle of this history window; 
            // finish eq.3 in Li Shang paper;
            _BU_predicted_out_i[i] = _BU_predicted_out_i[i] / 
                _counter_router_own_cycles;
            // then, do actual prediction: eq.5 in Li Shang paper;
            _BU_predicted_out_i[i] = 
                ( (_history_weight * _BU_predicted_out_i[i]) + 
                  _BU_predicted_out_i_last[i] ) / ( _history_weight + 1 );
            //printf("  p:%.1f a:%.1f", _BU_predicted_out_i[i], _BU_predicted_out_i_last[i]);
            //printf(" %.1f", _BU_predicted_out_i[i]);
            // record current prediction in "last" for next time
            // that is, for the end of a new history window;
            _BU_predicted_out_i_last[i] = _BU_predicted_out_i[i];
            // ---> finish eq.2 in Li Shang paper;
            _LU_predicted_out_i[i] = _counter_flits_sent_during_hw[i] / 
                _counter_router_own_cycles;
            // then, do actual prediction: eq.5 in Li Shang paper;
            _LU_predicted_out_i[i] = 
                ( (_history_weight * _LU_predicted_out_i[i]) + 
                  _LU_predicted_out_i_last[i] ) / ( _history_weight + 1 );
            // record current prediction in "last" for next time
            // that is, for the end of a new history window;
            _LU_predicted_out_i_last[i] = _LU_predicted_out_i[i];
            // reset LU counter of flits transmitted to have it ready
            // for the next period's calculations;
            _counter_flits_sent_during_hw[i] = 0;
        }

        // last cycle calculation for overall BU of this router;
        _BU_predicted_all_inputs = _BU_predicted_all_inputs /
            _counter_router_own_cycles; // should equal _history_window for ASYNC mode;
        // do also actual prediction;
        _BU_predicted_all_inputs =
            ( (_history_weight * _BU_predicted_all_inputs) + 
              _BU_predicted_all_inputs_last ) / ( _history_weight + 1 );
        // record it as "last" for next time;
        _BU_predicted_all_inputs_last = _BU_predicted_all_inputs;
        //printf(" %.2f  ", _BU_predicted_all_inputs);
            


        // also, reset the counter that keeps track where
        // we are inside this history window;
        my_router->reset_cycle_counter_4_prediction();
        // also, reset LU counter of router own cycles for the next period;
        // prepare it for the calculation of LU during the next hw;
        _counter_router_own_cycles = 0;


        // () then, update DVFS settings;
        // Note: inside this call, the function that does the actual
        // dvfs settings change (if indeed they change compared to 
        // prev. history window) will also call the function
        // scale_and_accumulate_energy(), to keep track of scaled
        // energy so far;


        // Note: this was initially in set_frequencies_and_vdd()
        // and meant to be called only if dvfs was done; but I had the issues 
        // of no DVFS setting to be done and scaled energy would 
        // remain 0; I brought it here so that in that case scaled energy
        // would be the same as the not scaled energy;
        // (2) because dvfs settings may be changed here we also call 
        // scale_and_accumulate_energy(), to keep track of scaled
        // energy so far; we accumulate the energy
        // with the appropriate scaling because the power module accumulate
        // the energy, unscaled as computed by Orion;
        my_router->power_module().scale_and_accumulate_energy();


        if ( my_router->vnoc()->topology()->use_freq_boost()) {

            update_DVFS_settings_throttle_and_boost( my_router);
            // also, reset all BU related variable to prepare them
            // for calculations during the next history window
            // period;
            reset_all_BU_related_variables();

        } else {

            update_DVFS_settings_throttle_only( my_router);
            // also, reset all BU related variable to prepare them
            // for calculations during the next history window
            // period;
            reset_all_BU_related_variables();

        }

    }

    // (2) exponential averaging; not used currently;
    else if ( _predictor_type == EXPONENTIAL_AVERAGING) {
    }
}


void PREDICTOR_MODULE::maintain_or_perform_prediction_ASYNC(ROUTER *my_router) 
{
    // called each time a given router is "simulated" simulate_one_router();

    // because a given router counts its own cycles and only when
    // a number "_history_window" router cycles are gone, prediction is made 
    // for the given router, predictions happen for different routers 
    // at different times; not all routers are updated in terms
    // of dvfs at the same synchronous time, say "_history_window" 
    // of base cycles;
    // each router is processed to perform prediction exactly after 
    // "_history_window" router cycles; because the router may be
    // already throttled, the prediction and hence changes in its dvfs
    // settings may be at a different (asynchronous) time than
    // othe rrouter that may be runing at the base freq.

    // (1) history based prediction;
    if ( _predictor_type == HISTORY) {

        // (a) maintain;
        if (my_router->cycle_counter_4_prediction() < _history_window) { 

            for (long i = 1; i < 5; i++) {
                // ---> compute input buffer occupancy of input buffer
                // of downstream router and accumulate it; actual
                // average (eq.3 in Li Shang paper) will be done in the
                // last cycle of this history window, when the actual 
                // prediction (eq.5 in Li Shang paper) will also be made;
                _BU_predicted_out_i[i] +=
                    my_router->compute_BU_for_downstream_driven_by_out_i( i);
                //printf(" %.1f", _BU_predicted_out_i[i]);
            }


            // accumulate also the overall buffer utilization of all
            // input buffers of this router (used by freq throttle portion);
            _BU_predicted_all_inputs +=
                my_router->compute_BU_overall_input_buffers_this_r();

            // ---> do link utilization bookeeping;
            // here we only count the clock cycles of this router
            // that pass during the "_history_window"; if dvfs settings
            // are done for each router (ASYNC) individually, after each "_history_window"
            // of this router clock cycles, then the counter incremented here
            // will be the same as "_history_window" at the end of the
            // "_history_window" period;
            // if however, all routers have dvfs settings updated (SYNC) synchronously
            // after "_history_window" of base cycles, then this counter 
            // incremented here may be different than the actual "_history_window"
            // as a number; this depends on whether this router has been throttled
            // or boosted at the end of the last control period;
            // the counter incremented here is the N variable in the
            // denuminator of eq. 2 in Li Shang paper; this counter is
            // reset at the end of the control period when actaul predictions 
            // are made;
            _counter_router_own_cycles ++;
            // Note: the numerator of eq. 2 in Li Shang paper is another 
            // counter that gets incremented each time a flit is sent
            // over the link; 
            // this is done inside send_flit_via_physical_link()
            // by calling record_flit_transmission_for_LU_calculation();

            // also, increment the counter that keeps track where
            // we are inside this history window;
            my_router->incr_cycle_counter_4_prediction();

        } 

        // (b) perform; because we are at end of history window;
        else { 

            for (long i = 1; i < 5; i++) {
                // ---> last cycle of this history window; 
                // finish eq.3 in Li Shang paper;
                _BU_predicted_out_i[i] = _BU_predicted_out_i[i] / 
                    _counter_router_own_cycles; // should equal _history_window for ASYNC mode;
                // printf(" %.1f", _BU_predicted_out_i[i]);
                // then, do actual prediction: eq.5 in Li Shang paper;
                _BU_predicted_out_i[i] = 
                    ( (_history_weight * _BU_predicted_out_i[i]) + 
                      _BU_predicted_out_i_last[i] ) / ( _history_weight + 1 );
                //printf("  p:%.1f a:%.1f", _BU_predicted_out_i[i], _BU_predicted_out_i_last[i]);
                //printf(" %.1f", _BU_predicted_out_i[i]);
                // record current prediction in "last" for next time
                // that is, for the end of a new history window;
                _BU_predicted_out_i_last[i] = _BU_predicted_out_i[i];
                //printf(" %.2f", _BU_predicted_out_i[i]);

                // ---> finish eq.2 in Li Shang paper;
                _LU_predicted_out_i[i] = double(_counter_flits_sent_during_hw[i]) / 
                    double(_counter_router_own_cycles);
                //printf(" %.1f", _LU_predicted_out_i[i]);
                //printf(" %d %d ",_counter_flits_sent_during_hw[i],_counter_router_own_cycles);
                // then, do actual prediction: eq.5 in Li Shang paper;
                _LU_predicted_out_i[i] = 
                    ( (_history_weight * _LU_predicted_out_i[i]) + 
                      _LU_predicted_out_i_last[i] ) / ( _history_weight + 1 );
                //printf(" %.1f", _LU_predicted_out_i[i]);
                // record current prediction in "last" for next time
                // that is, for the end of a new history window;
                _LU_predicted_out_i_last[i] = _LU_predicted_out_i[i];
                // reset LU counter of flits transmitted to have it ready
                // for the next period's calculations;
                _counter_flits_sent_during_hw[i] = 0;
            }

            // last cycle calculation for overall BU of this router;
            _BU_predicted_all_inputs = _BU_predicted_all_inputs /
                _counter_router_own_cycles; // should equal _history_window for ASYNC mode;
            // do also actual prediction;
            _BU_predicted_all_inputs =
                ( (_history_weight * _BU_predicted_all_inputs) + 
                      _BU_predicted_all_inputs_last ) / ( _history_weight + 1 );
            // record it as "last" for next time;
            _BU_predicted_all_inputs_last = _BU_predicted_all_inputs;
            //printf(" %.2f  ", _BU_predicted_all_inputs);


            // record the prediction error;
            // Note: _BU_predicted_all_inputs just computed is basically what 
            // happened during just ended history window; this is actual BU
            // which I want to compare with the previous value, that we used
            // as a prediction of what would happend duing this just ended 
            // window;
            //_predictions_count ++;
            //double this_prediction_error =
            //    fabs(_BU_predicted_all_inputs - _BU_predicted_all_inputs_last);
            //_BU_all_prediction_err_as_percentage +=
            //    this_prediction_error / double(my_router->overall_size_input_buffs());
            //printf("  p:%.1f a:%.1f", _BU_predicted_all_inputs_last, _BU_predicted_all_inputs);
            //printf("  %.1f %.1f", this_prediction_error, double(my_router->overall_size_input_buffs()));
            


            // also, reset the counter that keeps track where
            // we are inside this history window;
            my_router->reset_cycle_counter_4_prediction();
            // also, reset LU counter of router own cycles for the next period;
            // prepare it for the calculation of LU during the next hw;
            _counter_router_own_cycles = 0;


            // () then, update DVFS settings;
            // Note: inside this call, the function that does the actual
            // dvfs settings change (if indeed they change compared to 
            // prev. fistory window) will also call the function
            // scale_and_accumulate_energy(), to keep track of scaled
            // energy so far;


            // Note: this was initially in set_frequencies_and_vdd()
            // and meant to be called only if dvfs was done; but I had the issues 
            // of no DVFS setting to be done and scaled energy would 
            // remain 0; I brought it here so that in that case scaled energy
            // would be the same as the not scaled energy;
            // (2) because dvfs settings may be changed here we also call 
            // scale_and_accumulate_energy(), to keep track of scaled
            // energy so far; we accumulate the energy
            // with the appropriate scaling because the power module accumulate
            // the energy, unscaled as computed by Orion;
            my_router->power_module().scale_and_accumulate_energy();


            if ( my_router->vnoc()->topology()->use_freq_boost()) {

                update_DVFS_settings_throttle_and_boost( my_router);
                // also, reset all BU related variable to prepare them
                // for calculations during the next history window
                // period;
                reset_all_BU_related_variables();

            } else {

                update_DVFS_settings_throttle_only( my_router);
                // also, reset all BU related variable to prepare them
                // for calculations during the next history window
                // period;
                reset_all_BU_related_variables();

            }


        }

    }

    // (2) exponential averaging; not used currently;
    else if ( _predictor_type == EXPONENTIAL_AVERAGING) {
        //_predicted_BU = (long)( _alpha * _actual_BU + (1 - _alpha) * _last_predicted_BU);
    }          
}



void PREDICTOR_MODULE::update_DVFS_settings_throttle_only( ROUTER *my_router) 
{
    // here we implement the logic of the frequency tuning for router
    // and links; that is frequency throttle from Asit Mishra (see
    // tables 1,2 in his paper, but w/o using boost frequency) and link
    // control from Li Shang paper; frequency throttle is done using predicted
    // values for BU; link control is done the same way but only with
    // three f,V levels to keep hardware cost low;

    // (1) use link prediction; this is similar to Li Shang paper;
    if ( my_router->vnoc()->topology()->use_link_pred() == true) {
        // this follows the logic of algo 1 from Li Shang paper;
        long num_of_shift_freq_up = 0;
        long num_of_shift_freq_down = 0;
        long num_of_shift_freq_down_bu_low = 0;
        long num_of_shift_freq_down_bu_high = 0;
        long num_of_keep_freq_same = 0;
        double TL_low = 0.3, TL_high = 0.4;
        double TH_low = 0.6, TH_high = 0.7;
        double T_low = 0.0, T_high = 0.0;
        for (long i = 1; i < 5; i++) {
            if ( _BU_predicted_out_i[i] < 0.5) {
                T_low = TL_low;
                T_high = TL_high;
            } else {
                T_low = TH_low;
                T_high = TH_high;
            }
            if ( _LU_predicted_out_i[i] < T_low) {
                num_of_shift_freq_down ++;
                if (_BU_predicted_out_i[i] < 0.5) {
                    num_of_shift_freq_down_bu_low ++;
                } else {
                    num_of_shift_freq_down_bu_high ++;
                }
            } else if ( _LU_predicted_out_i[i] > T_high) {
                num_of_shift_freq_up ++;
            } else {
                num_of_keep_freq_same ++;
            }
        }
        // at this time we know how many links should have freq changed
        // up, down, or kept as is;
       
        if ( num_of_shift_freq_up > 0) {
            if (my_router->dvfs_level() == DVFS_THROTTLE_2) {
                my_router->set_frequencies_and_vdd(DVFS_THROTTLE_1);
             } else if (my_router->dvfs_level() == DVFS_THROTTLE_1) {
                my_router->set_frequencies_and_vdd(DVFS_BASE);
            } else {
                // do nothing; freq is already the highest;
            }
            
        } 
        else if ( num_of_shift_freq_down > 0) {
            if (my_router->dvfs_level() == DVFS_BASE) {
                my_router->set_frequencies_and_vdd(DVFS_THROTTLE_1);
            } else if (my_router->dvfs_level() == DVFS_THROTTLE_1) {
                my_router->set_frequencies_and_vdd(DVFS_THROTTLE_2);
            } else {
                // do nothing; freq is already the lowest;
            }
        } 
        else {
            // do nothing; freq is kept unchanged;
        }

    }
    

    // (2) do not use link prediction; this is similar to Asit Mishra paper;
    else {
        long num_of_congestion_low_signals = 0;
        long num_of_congestion_high_signals = 0;
        for (long i = 1; i < 5; i++) {
            if ( _BU_predicted_out_i[i] > 0.65) {
                // the prediction done locally here at the output of this
                // router about the BU of the downstream input-port router
                // represents basically the "congestion_high" signal discussed
                // in Asit Mishra paper;
                num_of_congestion_high_signals ++;
            } else if ( _BU_predicted_out_i[i] < 0.35) {
                num_of_congestion_low_signals ++;
            }
        }
        // see Asit Mishra paper, tables 1,2;
        //printf(" %.1f", _BU_predicted_all_inputs);
        if ( _BU_predicted_all_inputs >= 0.15) { // 0.6
            // irrespective of what the signals from neighbors say
            // we set the frequency to the highest freq, f_H, because
            // this router itself is on the verge of congestion
            my_router->set_frequencies_and_vdd( DVFS_BASE);
        } else if ( _BU_predicted_all_inputs < 0.1 &&  // 0.6..0.4
                    _BU_predicted_all_inputs >= 0.05) {
            if ( num_of_congestion_high_signals > 0) { 
                // set frequency to the middle freq, f_M
                my_router->set_frequencies_and_vdd( DVFS_THROTTLE_1);
            }
            else {
                my_router->set_frequencies_and_vdd( DVFS_BASE);
            }
        } else {
            if ( num_of_congestion_high_signals > 0) { 
                // set frequency to the low freq, f_L
                my_router->set_frequencies_and_vdd( DVFS_THROTTLE_2);
            }
            else {
                my_router->set_frequencies_and_vdd( DVFS_BASE);
            }
        }
    }
    
}


void PREDICTOR_MODULE::update_DVFS_settings_throttle_and_boost( ROUTER *my_router) 
{
    // here we implement the logic of the frequency tuning for router
    // and links; that is frequency throttle from Asit Mishra (see
    // tables 1,2 in his paper, but w/o using boost freqymcy) and link
    // control from Li Shang paper; frequency throttle is done using predicted
    // values for BU; link control is done the same way but only with
    // three f,V levels to keep hardware cost low;

    // (1) use link prediction; this is similar to Li Shang paper;
    if ( my_router->vnoc()->topology()->use_link_pred() == true) {
        // this follows the logic of algo 1 from Li Shang paper;
        long num_of_shift_freq_up = 0;
        long num_of_shift_freq_down = 0;
        long num_of_shift_freq_down_bu_low = 0;
        long num_of_shift_freq_down_bu_high = 0;
        long num_of_keep_freq_same = 0;
        double TL_low = 0.3, TL_high = 0.4;
        double TH_low = 0.6, TH_high = 0.7;
        double T_low = 0.0, T_high = 0.0;
        for (long i = 1; i < 5; i++) {
            if ( _BU_predicted_out_i[i] < 0.5) {
                T_low = TL_low;
                T_high = TL_high;
            } else {
                T_low = TH_low;
                T_high = TH_high;
            }
            if ( _LU_predicted_out_i[i] < T_low) {
                num_of_shift_freq_down ++;
                if (_BU_predicted_out_i[i] < 0.5) {
                    num_of_shift_freq_down_bu_low ++;
                } else {
                    num_of_shift_freq_down_bu_high ++;
                }
            } else if ( _LU_predicted_out_i[i] > T_high) {
                num_of_shift_freq_up ++;
            } else {
                num_of_keep_freq_same ++;
            }
        }
        // at this time we know how many links should have freq changed
        // up, down, or kept as is;
       
        // 1
        if ( num_of_shift_freq_up > 0) {
            if (my_router->dvfs_level() == DVFS_THROTTLE_2) {
                my_router->set_frequencies_and_vdd(DVFS_THROTTLE_1);
            } else if (my_router->dvfs_level() == DVFS_THROTTLE_1) {
                my_router->set_frequencies_and_vdd(DVFS_BASE);
            } else if (my_router->dvfs_level() == DVFS_BASE) {
                my_router->set_frequencies_and_vdd(DVFS_BOOST);
            } else {
                // do nothing; freq is already the highest;
            }
        } 
        else if ( num_of_shift_freq_down > 0) {
            if (my_router->dvfs_level() == DVFS_BOOST) {
                my_router->set_frequencies_and_vdd(DVFS_BASE);
            } else if (my_router->dvfs_level() == DVFS_BASE) {
                my_router->set_frequencies_and_vdd(DVFS_THROTTLE_1);
            } else if (my_router->dvfs_level() == DVFS_THROTTLE_1) {
                my_router->set_frequencies_and_vdd(DVFS_THROTTLE_2);
            } else {
                // do nothing; freq is already the lowest;
            }
        } 
        else {
            // do nothing; freq is kept unchanged;
        }
    }
    

    // (2) do not use link prediction; this is similar to Asit Mishra paper;
    else {
        long num_of_congestion_low_signals = 0;
        long num_of_congestion_high_signals = 0;
        for (long i = 1; i < 5; i++) {
            if ( _BU_predicted_out_i[i] > 0.65) {
                // the prediction done locally here at the output of this
                // router about the BU of the downstream input-port router
                // represents basically the "congestion_high" signal discussed
                // in Asit Mishra paper;
                num_of_congestion_high_signals ++;
            } else if ( _BU_predicted_out_i[i] < 0.35) {
                num_of_congestion_low_signals ++;
            }
        }
        // see Asit Mishra paper, tables 1,2;
        //printf(" %.1f", _BU_predicted_all_inputs);
        if ( _BU_predicted_all_inputs >= 0.15) { // 0.6
            // irrespective of what the signals from neighbors say
            // we set the frequency to the highest freq, f_H, because
            // this router itself is on the verge of congestion
            my_router->set_frequencies_and_vdd( DVFS_BOOST);
        } else if ( _BU_predicted_all_inputs < 0.1 &&  // 0.6..0.4
                    _BU_predicted_all_inputs >= 0.05) {
            if ( num_of_congestion_high_signals > 0) { 
                // set frequency to the middle freq, f_M
                my_router->set_frequencies_and_vdd( DVFS_THROTTLE_1);
            }
            else {
                my_router->set_frequencies_and_vdd( DVFS_BASE);
            }
        } else {
            if ( num_of_congestion_high_signals > 0) { 
                // set frequency to the low freq, f_L
                my_router->set_frequencies_and_vdd( DVFS_THROTTLE_2);
            }
            else {
                my_router->set_frequencies_and_vdd( DVFS_BASE);
            }
        }
    }
    
}

////////////////////////////////////////////////////////////////////////////////
//
// POWER_MODULE
//
////////////////////////////////////////////////////////////////////////////////

POWER_MODULE::POWER_MODULE( long physical_ports_count, long vc_count,
                            long flit_size, double link_length) :
    _flit_size( flit_size),
    _router_info(),
    _router_power(),
    _router_area(),
    _arbiter_vc_power(),
    _link_power(),
    _buffer_write(),
    _buffer_read(),
    _crossbar_read(),
    _crossbar_write(),
    _link_traversal(),
    _crossbar_input(),
    _arbiter_vc_req(),
    _arbiter_vc_grant()
{

    // () buffer init

    // orion3:
    //router_initialize(
    //    &_router_info, // router_info_t *info
    //    &_router_power, // router_power_t *router_power
    //    NULL); // router_area_t *router_area
    //router_initialize(
    //    &_router_info, // router_info_t *info
    //    NULL, // router_power_t *router_power
    //    &_router_area); // router_area_t *router_area
    // orion2:
    SIM_router_init( // see SIM_router.c
        &_router_info, // SIM_router_info_t *info
        &_router_power, // SIM_router_power_t *SIM_router_power
        NULL); // SIM_router_area_t *SIM_router_area
    // the above calls SIM_router_power_init():
    //SIM_router_power_init( // see SIM_router_power.c
    //    &_router_info, // SIM_router_info_t *info
    //    &_router_power); // SIM_router_power_t *SIM_router_power
    //SIM_router_init( // not interested in area for now;
    //    &_router_info, // SIM_router_info_t *info
    //    NULL, // SIM_router_power_t *SIM_router_power
    //    &_router_area); // SIM_router_area_t *SIM_router_area
    // orion1:
    //FUNC(SIM_router_power_init, &_router_info, &_router_power);

    _buffer_write.resize( physical_ports_count);
    _buffer_read.resize( physical_ports_count);
    _crossbar_read.resize( physical_ports_count);
    _crossbar_write.resize( physical_ports_count);
    _link_traversal.resize( physical_ports_count);
    _crossbar_input.resize( physical_ports_count,0);
    for (long i = 0; i < physical_ports_count; i ++) {
        _buffer_write[i].resize( _flit_size, 0);
        _buffer_read[i].resize( _flit_size, 0);
        _crossbar_read[i].resize( _flit_size, 0);
        _crossbar_write[i].resize( _flit_size, 0);
        _link_traversal[i].resize( _flit_size, 0);
    }


    // () arbiter init
    SIM_arbiter_init(
        &_arbiter_vc_power, // SIM_arbiter_t *arb
        1, // int arbiter_model 
        1, // int ff_model
        physical_ports_count * vc_count, // u_int req_width
        0, // double length
        NULL); // SIM_array_info_t *info
    _arbiter_vc_req.resize( physical_ports_count);
    _arbiter_vc_grant.resize( physical_ports_count);
    for (long i = 0; i < physical_ports_count; i ++) {
        _arbiter_vc_req[i].resize( vc_count, 1);
        _arbiter_vc_grant[i].resize( vc_count, 1);
    }


    // () crossbar init
    SIM_crossbar_init(
        &( _router_power.crossbar), //SIM_crossbar_t *crsbar
        MATRIX_CROSSBAR, // int model; MATRIX_CROSSBAR, MULTREE_CROSSBAR
        physical_ports_count, // u_int n_in
        physical_ports_count, // u_int n_out
        1, // u_int in_seg
        1, // u_int out_seg
        ATOM_WIDTH, // u_int data_width; it's 64 fixed - do not change 'coz of orion3;
        1, // u_int degree
        TRISTATE_GATE, // int connect_type; TRANS_GATE, TRISTATE_GATE
        NP_GATE, // int trans_type
        1, // double in_len
        1, // double out_len
        NULL); // double *req_len


    // () bus/link init
    SIM_bus_init(
        &_link_power, // SIM_bus_t *bus
        GENERIC_BUS, // int model; RESULT_BUS, GENERIC_BUS
        IDENT_ENC, // int encoding
        ATOM_WIDTH, // u_int width; 64;
        0, // u_int grp_width; grp_width only matters for BUSINV_ENC
        1, // u_int n_snd; # of senders
        1, // u_int n_rcv; # of receivers
        link_length, // double length
        0); // double time; rise and fall time, 0 means using default transistor sizes

    // () DVFS related;
    _scaled_energy = 0.0;
    _prev_unscaled_energy = 0.0;
    _scaled_energy_buffer = 0.0;
    _prev_unscaled_energy_buffer = 0.0;
    _scaled_energy_crossbar = 0.0;
    _prev_unscaled_energy_crossbar = 0.0;
    _scaled_energy_arbiter = 0.0;
    _prev_unscaled_energy_arbiter = 0.0;
    _scaled_energy_link = 0.0;
    _prev_unscaled_energy_link = 0.0;
    _scaled_energy_clock = 0.0;
    _prev_unscaled_energy_clock = 0.0;

    _current_vdd = VDD_BASE; 
    _current_period = PIPE_DELAY_BASE;
    _energy_scaling_factor = SCALING_BASE; 
}


void POWER_MODULE::power_buffer_write(long in_port, DATA &write_d)
{
    // orion2:
    for (long i = 0; i < _flit_size; i ++) {
        DATA_ATOMIC_UNIT old_d = _buffer_write[in_port][i];
        DATA_ATOMIC_UNIT new_d = write_d[i];
        // eqivalent of: FUNC(SIM_buf_power_data_write,...) from orion1;
        SIM_buf_power_data_write( // see SIM_router.c
            &( _router_info.in_buf_info), // SIM_array_info_t *info 
            &( _router_power.in_buf), // SIM_array_t *arr
            (u_char *) (&old_d), // u_char *data_line
            (u_char *) (&old_d), // u_char *old_data
            (u_char *) (&new_d)); // u_char *new_data
        _buffer_write[in_port][i] = write_d[i];
    }
    /*---
    // debugging purposes only:
    for (long i = 0; i < _flit_size; i ++) {
        DATA_ATOMIC_UNIT old_d = _buffer_write[in_port][i];
        DATA_ATOMIC_UNIT new_d = write_d[i];
        SIM_array_data_write( // see SIM_array_m.c
            &( _router_info.in_buf_info), // SIM_array_info_t *info
            &( _router_power.in_buf), //  SIM_array_t *arr
            NULL, // SIM_array_set_state_t *set
            8, // u_int n_item; PARM(flit_width) / 8; works only for flit_width=64;
            (u_char *) (&old_d), // u_char *data_line
            (u_char *) (&old_d), // u_char *old_data
            (u_char *) (&new_d)); // u_char *new_data
        //FUNC(SIM_reg_power_data_write, // see SIM_reg.c
        //      &( _router_info.in_buf_info), // &( SIM_array_info_t in_buf_info)
        //      &( _router_power.in_buf), // &( SIM_array_t in_buf)
        //      (u_int) in_port, 
        //      (LIB_Type_max_uint) old_d,
        //      (LIB_Type_max_uint) new_d);
        _buffer_write[in_port][i] = write_d[i];
    }
    ---*/
    // orion1:
    //for (long i = 0; i < _flit_size; i ++) {
    //    DATA_ATOMIC_UNIT old_d = _buffer_write[in_port][i];
    //    DATA_ATOMIC_UNIT new_d = write_d[i];
    //    FUNC(SIM_buf_power_data_write, 
    //          &( _router_info.in_buf_info), 
    //          &( _router_power.in_buf),
    //          (char *) (&old_d), 
    //          (char *) (&old_d),
    //          (char *) (&new_d));
    //    _buffer_write[in_port][i] = write_d[i];
    //}
}

void POWER_MODULE::power_buffer_read(long in_port, DATA &read_d)
{
    // orion2:
    for (long i = 0; i < _flit_size; i++) {
        // eqivalent of: FUNC(SIM_buf_power_data_read,...) from orion1;
        SIM_buf_power_data_read( // see SIM_router.c
            &( _router_info.in_buf_info), // SIM_array_info_t *info
            &( _router_power.in_buf), // SIM_array_t *arr
            read_d[i]); // LIB_Type_max_uint data
        _buffer_read[in_port][i] = read_d[i];
    }
    /*---
    // debugging purposes only:
    for (long i = 0; i < _flit_size; i++) {
        SIM_array_data_read( // see SIM_array_m.c
            &( _router_info.in_buf_info), // SIM_array_info_t *info 
            &( _router_power.in_buf), // SIM_array_t *arr 
            read_d[i]); // LIB_Type_max_uint data
        //FUNC(SIM_reg_power_data_read, // see SIM_reg.c
        //      &( _router_info.in_buf_info), // SIM_array_info_t in_buf_info;
        //      &( _router_power.in_buf), // SIM_array_t in_buf;
        //      (P_DATA_T) read_d[i]);
        _buffer_read[in_port][i] = read_d[i];
    }
    ---*/
    // orion1: 
    //for (long i = 0; i < _flit_size; i++) {
    //    FUNC(SIM_buf_power_data_read,
    //          &( _router_info.in_buf_info),
    //          &( _router_power.in_buf), 
    //          read_d[i]);
    //    _buffer_read[in_port][i] = read_d[i];
    //}
}

void POWER_MODULE::power_vc_arbit(long pc, long vc,
    DATA_ATOMIC_UNIT req, unsigned long gra)
{
    // orion2:
    SIM_arbiter_record(
        &_arbiter_vc_power, // SIM_arbiter_t *arb
        (LIB_Type_max_uint) req, // LIB_Type_max_uint new_req
        (LIB_Type_max_uint) _arbiter_vc_req[pc][vc], // LIB_Type_max_uint old_req
        (u_int) gra, // u_int new_grant
        (u_int) _arbiter_vc_grant[pc][vc]); // u_int old_grant
    _arbiter_vc_req[pc][vc] = req;
    _arbiter_vc_grant[pc][vc] = gra;
    // orion1:
    //SIM_arbiter_record( &_arbiter_vc_power, req, 
    //                    _arbiter_vc_req[pc][vc], gra, 
    //                    _arbiter_vc_grant[pc][vc]);
    //_arbiter_vc_req[pc][vc] = req;
    //_arbiter_vc_grant[pc][vc] = gra;
}

void POWER_MODULE::power_crossbar_trav(long in_port, long out_port, DATA &trav_d)
{
    // orion2:
    for (long i = 0; i < _flit_size; i++) {
        SIM_crossbar_record(
            &( _router_power.crossbar), // SIM_crossbar_t *xb
            1, // int io 
            (LIB_Type_max_uint) trav_d[i], // LIB_Type_max_uint new_data
            (LIB_Type_max_uint) _crossbar_read[in_port][i], // LIB_Type_max_uint old_data
            1, // u_int new_port
            1); // u_int old_port
        SIM_crossbar_record(
            &( _router_power.crossbar), 
            0, 
            (LIB_Type_max_uint) trav_d[i],
            (LIB_Type_max_uint) _crossbar_write[out_port][i],
            (u_int) _crossbar_input[out_port], 
            (u_int) in_port);
        _crossbar_read[in_port][i] = trav_d[i];
        _crossbar_write[out_port][i] = trav_d[i];
        _crossbar_input[out_port] = in_port;
    }
    // orion1:
    //for (long i = 0; i < _flit_size; i++) {
    //    SIM_crossbar_record(
    //       &( _router_power.crossbar), 1, trav_d[i],
    //        _crossbar_read[in_port][i], 1, 1);
    //    SIM_crossbar_record(
    //        &( _router_power.crossbar), 0, trav_d[i],
    //        _crossbar_write[out_port][i],
    //        _crossbar_input[out_port], in_port);
    //    _crossbar_read[in_port][i] = trav_d[i];
    //    _crossbar_write[out_port][i] = trav_d[i];
    //    _crossbar_input[out_port] = in_port;
    //}
}

void POWER_MODULE::power_link_traversal(long in_port, DATA &read_d)
{
    // orion2:
    for (long i = 0; i < _flit_size; i++) {
        DATA_ATOMIC_UNIT old_d = _link_traversal[in_port][i];
        DATA_ATOMIC_UNIT new_d = read_d[i];
        SIM_bus_record(
            &_link_power, // SIM_bus_t *bus
            (LIB_Type_max_uint) old_d, // LIB_Type_max_uint old_state
            (LIB_Type_max_uint) new_d); // LIB_Type_max_uint new_state
        _link_traversal[in_port][i] = read_d[i];
    }
    // orion1:
    //for (long i = 0; i < _flit_size; i++) {
    //    DATA_ATOMIC_UNIT old_d = _link_traversal[in_port][i];
    //    DATA_ATOMIC_UNIT new_d = read_d[i];
    //    SIM_bus_record( &_link_power, old_d, new_d);
    //    _link_traversal[in_port][i] = read_d[i];
    //}
}

void POWER_MODULE::power_clock_record()
{
    // record a simulation cycle of the NoC;
    SIM_simulation_cycles_record( &_router_info); // SIM_router_info_t *info
}


double POWER_MODULE::power_buffer_report()
{
    double buffer_energy = 0.0;
    // orion2:
    // this function is used inside orion_router.c also for the case of orion2
    // simulations; calls SIM_array_stat_energy() to get energy of in_buf,
    // which is described in SIM_array_m.c; it returns the energy of the whole
    // router;
    // Note: do NOT use this function; it returns always the same values
    // for all routers; it's the way orion3 "cooked" its power estimation;
    // does not take into account all activity during NoC simulation;
    /*---
    // debugging purposes only:
    buffer_energy = SIM_router_stat_energy( // see SIM_router_power.c
                           &( _router_info),
                           &( _router_power), 
                           0, // print_depth
                           NULL, // name
                           AVG_ENERGY, // max_flag
                           0.5, // load
                           0, // plot_flag
                           2e9); // Freq.
    // energy of one array only; this is called a few times by the above one;
    return SIM_array_stat_energy( // see SIM_array_m.c
        &( _router_info.in_buf_info), // SIM_array_info_t *info
        &( _router_power.in_buf), // SIM_array_t *arr
        _router_info.n_in, // double n_read
        _router_info.n_in, // double n_write
        0, // int print_depth
        NULL, // char *path
        2e9); // int max_avg; PARM(Freq)
    ---*/
    // power of array only;
    buffer_energy = SIM_array_power_report( // see SIM_array_l.c
        &( _router_info.in_buf_info), // &( SIM_array_info_t in_buf_info)
        &( _router_power.in_buf)); // &( SIM_array_t in_buf)
    //printf("\n buffer_energy: %g", buffer_energy); 
    // orion1:
    //return SIM_array_power_report(
    //    &( _router_info.in_buf_info),
    //    &( _router_power.in_buf));
    return buffer_energy;
}

double POWER_MODULE::power_arbiter_report()
{
    return SIM_arbiter_report( &_arbiter_vc_power);
}

double POWER_MODULE::power_crossbar_report()
{
    return SIM_crossbar_report( &( _router_power.crossbar));
}

double POWER_MODULE::power_link_report()
{
    // Note: this repots actually energy in J;
    // Note: the way orion2 computes energy is: n_switch * e_switch
    // where e_switch = EnergyFactor * (TCap), where EnergyFactor = Vdd^2
    // and TCap = CC2metal * length [F/um * um]; 
    return SIM_bus_report( &_link_power);
}

double POWER_MODULE::power_clock_report()
{
    double clock_energy = 0.0;
    // Note: this repots actually energy in J; this is a function that calls
    // SIM_total_clockEnergy();
    clock_energy = SIM_total_during_n_cycles_clockEnergy(
        &_router_info, // SIM_router_info_t *info
        &_router_power); // SIM_router_power_t *SIM_router_power
    //printf("\n clock_energy: %g", clock_energy); 
    return clock_energy;
}

void POWER_MODULE::scale_and_accumulate_energy()
{
    // used for DVFS; read detailed comments in declaration of this class;

    // retrieve Orion power and compute "delta" energy consumed since
    // last call of this routine;
    double curr_unscaled_energy =
        power_buffer_report() + power_crossbar_report() +
        power_arbiter_report() + power_link_report() +
        power_clock_report();       
    double delta_unscaled_energy =
        curr_unscaled_energy - _prev_unscaled_energy;
    _prev_unscaled_energy = curr_unscaled_energy; // prepare for next time;

    // accumulate and store locally the total scaled energy consumed so far;
    _scaled_energy += (delta_unscaled_energy * _energy_scaling_factor);

    // compute also individual components;
    _scaled_energy_buffer += 
        ((power_buffer_report() - _prev_unscaled_energy_buffer) * _energy_scaling_factor);  
    _scaled_energy_crossbar += 
        ((power_crossbar_report() - _prev_unscaled_energy_crossbar) * _energy_scaling_factor);  
    _scaled_energy_arbiter += 
        ((power_arbiter_report() - _prev_unscaled_energy_arbiter) * _energy_scaling_factor);
    _scaled_energy_link += 
        ((power_link_report() - _prev_unscaled_energy_link) * _energy_scaling_factor);  
    _scaled_energy_clock += 
        ((power_clock_report() - _prev_unscaled_energy_clock) * _energy_scaling_factor); 

    _prev_unscaled_energy_buffer = power_buffer_report();
    _prev_unscaled_energy_crossbar = power_crossbar_report();
    _prev_unscaled_energy_arbiter = power_arbiter_report();
    _prev_unscaled_energy_link = power_link_report();
    _prev_unscaled_energy_clock = power_clock_report();
}


////////////////////////////////////////////////////////////////////////////////
//
// VNOC
//
////////////////////////////////////////////////////////////////////////////////

VNOC::VNOC( TOPOLOGY *topology, EVENT_QUEUE *event_queue, bool verbose) :
    _gen_4_poisson_level1(),
    _poisson_level1( 600), // mean of 600 cycles; TODO: make it a parameter;
    _rvt_level1(_gen_4_poisson_level1, _poisson_level1),
    // Generator is to aggregate traffic from 128 sources and to generate
    // the load of "injection_rate" on a link with rate that
    // I "cooked" to 8.0E-6 such that inside the vnoc_pareto.h to have
    // a ByteTime of 1, which is my simulation cycle;
    //_gen_pareto_level2(8.0E-6, topology->injection_rate(), 128),
    _routers(),
    _traffic_injectors(),
    _total_packets_injected_count(0),
    _packets_arrived_count_after_wu(0),
    _input_file_st(),
    _verbose(verbose)
{
    _topology = topology; // get topology first, 'coz lots of things depend on it;
    _event_queue = event_queue;
    _traffic_type = _topology->traffic_type(); // make a local copy;

    _latency = 0.0;
    _power = 0.0;
    _packets_per_cycle = 0.0;
    _warmup_done = false; // will be set true after warmup cycles;

    // set seed of generator for poisson distr;
    _gen_4_poisson_level1.seed( _topology->rng_seed()); // time(NULL)

    long ary_size = _topology->ary_size(); // "network size" in one dimension;
    long cube_size = _topology->cube_size();
    long vc_number = _topology->virtual_channel_number();
    long buffer_size = _topology->input_buffer_size();
    long output_buffer_size = _topology->output_buffer_size();
    long flit_size = _topology->flit_size();
    double injection_rate = _topology->injection_rate(); // packet generation rate;

    // + 1 means, one for injection;
    long phy_ports_t = cube_size * 2 + 1;
    _routers_count = ary_size;
    for ( long i = 0; i < cube_size - 1; i++) {
        _routers_count = _routers_count * ary_size;
    }
    // set also _nx and _ny;
    _nx = _ny = ary_size; // working with 2D meshes only;


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


    // () create all routers;
    // create also all traffic injectors and hook them up at the same time
    // to their individual router;
    // Note: we work only with 2D regular mesh networks for
    // the time being; so, address has only 2 ints;
    ADDRESS add_t;
    add_t.resize( cube_size, 0);
    for ( long i = 0; i < _routers_count; i++) {
        _routers.push_back( // add this router;
            ROUTER(phy_ports_t, vc_number, buffer_size,
                output_buffer_size, add_t, ary_size, flit_size, // add_t is (0,0) for i=0;
                this, i, _topology->link_length(),
                _topology->predictor_type()) );


        // compute the address of the router will be inserted in next iteration;
        add_t[cube_size - 1]++;
        for ( long j = cube_size - 1; j > 0; j--) {
            if ( add_t[j] == ary_size) {
                add_t[j] = 0;
                add_t[j-1]++;
            }
        }
    }


    // () create the traffic injectors and hook'em up to routers;
    for ( long i = 0; i < _routers_count; i++) {
        // id = x * ny + y because of the way routers are indexed in the 2D mesh;
        _traffic_injectors.push_back(
            TRAFFIC_INJECTOR( i,
                (i / ary_size), // x; assume just 2D mesh;
                (i % ary_size), // y; assume just 2D mesh;
                _traffic_type, 
                injection_rate,
                _topology->rng_seed(),
                this,
                &_routers[i]));
    }
    


    // () traffic related initializations;
    if ( _traffic_type == TRACEFILE_TRAFFIC) {
        // open the main trace file; also adds an event of type PE;
        // to the simulation-queue, which will kick of the simulation 
        // and insertion of additional events PE later on;

        _input_file_st.open( _topology->trace_file().c_str());
        if ( !_input_file_st) {
            printf("\nError: Cannot open 'main' source file: %s\n",_topology->trace_file().c_str());
            exit(1);
        }
        double event_time_t;
        _input_file_st >> event_time_t;

        // one time deal of creating an PE event; when this will be 
        // processed more PE events will be added until no more packets are
        // in trace files that need be injected;
        // when this type of events will be taken out from the simulation-queue,
        // there will be reading from "local" trace file for the router whose 
        // role is source, whose address is read from lines in the "main" trace file
        // (at the beginning of VNOC::receive_EVENT_PE());
        // in this case the first packet injected may be a bit later than 0.0,
        // we read it from the tracefile;
        _event_queue->add_event( EVENT(EVENT::PE, event_time_t));
    }
    else if ( _traffic_type == IPCORE_TRAFFIC) {

    } 
    else { // uniform, transpose 1/2, hotspot, and selfsimilar;
        // add an event also of type PE (but this time it's taken from
        // traffic injectors hooked to each router); which will kick of the simulation 
        // and insertion of additional events PE later on; we start all
        // simulations at time 0.0;
        _event_queue->add_event( EVENT(EVENT::PE, 0.0));

        // selfsimilar traffic requires additional initializations;
        // select randomly about 1/4 of all routers to operate as sources;
        if ( _traffic_type == SELFSIMILAR_TRAFFIC) {
            select_randomly_sources_for_selfsimilar_traffic();
        }
    }


    // () DVFS related initializations;
    // can be one of: DVFS_BOOST, DVFS_BASE, DVFS_THROTTLE_1, DVFS_THROTTLE_2
    set_frequencies_and_vdd( DVFS_BASE); 
}


void VNOC::select_randomly_sources_for_selfsimilar_traffic()
{
    // select randomly about 1/4 of all routers to operate as sources;
    long nr_to_select = _routers_count / 4;
    long nr_of_selected_so_far = 0;
    long router_id = 0;
    double task_start_time = 0.0, task_stop_time = 0.0;
    while ( nr_of_selected_so_far < nr_to_select) {

        router_id = _topology->rng().flat_l(0, _routers_count - 1);

        if ( _traffic_injectors[router_id].selected_src_for_selfsimilar() == false) {
            _traffic_injectors[router_id].set_selected_src_for_selfsimilar();

            // also generate the first task duration; here only, at the 
            // beginning, I start from times generated randomly to get 
            // the starting moments a bit better speard out; that is
            // because the Poisson samples tend to be clustered around the
            // mean value of 600 and the tasks overlap too much all the time;
            // Note: starting with next task duration, task_start_time will
            // begenerated as a Poisson sample;
            task_start_time = double( _topology->rng().flat_l(1, 1200));
            task_stop_time = task_start_time + double( _topology->rng().flat_l(600, 2000));
            _traffic_injectors[router_id].set_task_start_time( task_start_time);
            _traffic_injectors[router_id].set_task_stop_time( task_stop_time);            
            
            //printf("\n (%d) _task_start_time: %f _task_stop_time: %f", router_id,
            //    task_start_time, task_stop_time);

            nr_of_selected_so_far ++;
        }
    }
    printf("\nRouters selected as sources for selfsimilar traffic:\n");
    for ( long i = 0; i < _routers_count; i++) {
        if ( _traffic_injectors[i].selected_src_for_selfsimilar()) {
            printf("%d ", i);
        }
    }  printf(" \n");
}

ROUTER & VNOC::router( const ADDRESS &a)
{
    // Note: this works because .end() positions 
    // the pointer immediately after the last item;
    ADDRESS::const_iterator first = a.begin(); // x;
    ADDRESS::const_iterator last = a.end(); // y;
    long i = (* first); first++;
    for (; first != last; first++) {
        i = i * _topology->ary_size() + (*first); // id = x * ny + y 
    }
    return ( _routers[i]);
}

const ROUTER & VNOC::router( const ADDRESS &a) const
{
    ADDRESS::const_iterator first = a.begin();
    ADDRESS::const_iterator last = a.end();
    long i = (* first); first++;
    for (; first != last; first++) {
        i = i * _topology->ary_size() + (*first);
    }
    return ( _routers[i]);
}

bool VNOC::check_address(const ADDRESS &a) const 
{
    // evaluate the address;
    if ( a.size() != _topology->cube_size()) {
        return false;
    }
    for ( long i = 0; i < a.size(); i++) {
        if ((a[i] >= _topology->ary_size()) || (a[i] < 0)) {
            return false;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// processing of events from the simulation-queue
//
////////////////////////////////////////////////////////////////////////////////

bool VNOC::receive_EVENT_PE( EVENT this_event)
{

    ADDRESS src_addr_t;
    ADDRESS des_addr_t;
    long pack_size_t;
    long ary_size = _topology->ary_size();
    long cube_size = _topology->cube_size();
    long num_packets_inj_here = 0;


    // (1) traffic from trace file;
    // first, read about next injection from the *main* trace file; 
    // trace file has rows in this format:
    // 1.4511e-01 7 7 8 1 5
    // "time" "src addr" "dest addr" "# flits"
    if ( _traffic_type == TRACEFILE_TRAFFIC) {

        for ( long i = 0; i < cube_size; i++) {
            long t; _input_file_st >> t;
            assert( !_input_file_st.eof());
            src_addr_t.push_back(t);
        }
        assert( src_addr_t[0] < ary_size && src_addr_t[1] < ary_size);
        for ( long i = 0; i < cube_size; i++) {
            long t; _input_file_st >> t;
            assert( !_input_file_st.eof());
            des_addr_t.push_back(t);
        }
        assert( des_addr_t[0] < ary_size && des_addr_t[1] < ary_size);
        _input_file_st >> pack_size_t;
        assert( !_input_file_st.eof());

        // here: do a round of reading from local trace file of this router;
        // Q: looking into the case: where here the main trace file says 
        // that I should go and inject from local tracefile of a router
        // but the local input PE buffer of that router is full;
        // if that is the case I should go back here in the main trace
        // file and stay at the beginning of the same line; else the
        // reading from main tracefile continues to next line and 
        // injection of packets at this oruter will only be done
        // if another line in main tracefile will be encountered with 
        // this router as source; but if none will be encountered?
        // A: that is fine, because flits will be read-in into the local
        // PE buffer of this router during SW_TR phase, when if 
        // PE input buffer is less than full, packets will be injected
        // form the local tracefile; so none of the packets from local
        // tracefiles are dropped even though here some lines from main 
        // tracefile are skipped;
        num_packets_inj_here = 
            router(src_addr_t).receive_packet_from_local_trace_file();
        // vnoc level counter of all injected packets;
        _total_packets_injected_count += num_packets_inj_here;

        if ( !_input_file_st.eof()) {
            double event_time_t;
            _input_file_st >> event_time_t;
            if ( !_input_file_st.eof()) {
                // here we keep adding PE events until no more packets are 
                // in trace files that need be injected;
                _event_queue->add_event( EVENT(EVENT::PE, event_time_t));
            }
        }
    }


    // (2) this is case where I actually have implemented in C++
    // the actual IP cores themeselves of some application like
    // MPEG decoder;
    else if ( _traffic_type == IPCORE_TRAFFIC) {

    } 


    // (3) this is the case of synthetic traffic;
    // UNIFORM_TRAFFIC, HOTSPOT_TRAFFIC, TRANSPOSE1_TRAFFIC, 
    // TRANSPOSE2_TRAFFIC, SELFSIMILAR_TRAFFIC
    else {

        // num of injectors is same as num of routers;
        for ( long i = 0; i < _routers_count; i++) {
            // this may or may not inject a packet; if it does
            // then _total_packets_injected_count will be incremented too;
            _traffic_injectors[i].simulate_one_traffic_injector();
        }

        // add new PE event, which will trigger resimulation of all injectors
        // after pipe delay later; this is similar to how routers are simulated;
        // Note: all traffic is injected at the same rate and done with the same
        // period irrespective of what the DVFS settinsg of routers inside the network
        // are; that is because I want the application to stay the same and see
        // how the NoC behaves when some of its routers and links are throttled;
        double delay = PIPE_DELAY_BASE; // should be clock delay;
        _event_queue->add_event( EVENT(EVENT::PE, this_event.start_time() + delay));
    }   
    
}


void VNOC::receive_EVENT_SYNC_PREDICT_DVFS_SET( EVENT this_event)
{
    // perform prediction and possibly change the its dvfs settings
    // for each router; this is the second part (the first part was
    // the maintainance or bookeeping);

    for ( long i = 0; i < _routers_count; i++) {
        _routers[ i].perform_prediction_SYNC();
    }

    // add new SYNC_PREDICT_DVFS_SET event; will do the same
    // in "history_window" base cycles;
    double delay = double(_topology->history_window());
    _event_queue->add_event( EVENT(EVENT::SYNC_PREDICT_DVFS_SET,
        this_event.start_time() + delay));
}

void VNOC::receive_EVENT_ROUTER_SINGLE( EVENT this_event)
{
    // it's the one used currrently!
    // a modified version of VNOC::receive_EVENT_ROUTER() to be able to use
    // during the DVFS implementation; it's processing only one router as
    // opposed to the other one that processes all routers in one shot;


    long router_id = this_event.from_router_id();
    //printf(" %d", router_id);

    DVFS_LEVEL this_dvfs_level_prev = _routers[ router_id].dvfs_level_prev();

    
    _routers[ router_id].simulate_one_router(); // possibly sets new dvfs;

    // record this "simulation cycle" of this router for the purpose
    // of estimating clock energy consumption;
    if ( _warmup_done) {
        _routers[ router_id].power_module().power_clock_record();
    }

    _routers[ router_id].set_dvfs_level_prev(_routers[ router_id].dvfs_level());


    double delay = 0.0;
    if ( this_dvfs_level_prev == DVFS_BOOST) {
        delay = PIPE_DELAY_BOOST;
    }
    if ( this_dvfs_level_prev == DVFS_BASE) {
        delay = PIPE_DELAY_BASE;
    }
    if ( this_dvfs_level_prev == DVFS_THROTTLE_1) {
        delay = PIPE_DELAY_THROTTLE_1;
    }
    if ( this_dvfs_level_prev == DVFS_THROTTLE_2) {
        delay = PIPE_DELAY_THROTTLE_2;
    }

    // add event for this router's simulation next time;
    _event_queue->add_event( EVENT(EVENT::ROUTER_SINGLE,
        this_event.start_time() + delay, router_id));
}


bool VNOC::receive_EVENT_ROUTER( EVENT this_event)
{
    // not used currently!
    // this is the function called when the simulation-queue
    // got extracted and it's the turn of processing all routers
    // for simulation purposes; so that data is pushed thru;
    
    DVFS_LEVEL this_dvfs_level_prev;
    bool there_are_routers_at_boost = false;
    bool there_are_routers_at_base = false;
    bool there_are_routers_at_throttle_1 = false; 
    bool there_are_routers_at_throttle_2 = false;
    for ( long i = 0; i < _routers_count; i++) {
        this_dvfs_level_prev = _routers[i].dvfs_level_prev();

        // this router must be processed only if its DVFS settings
        // are in agreement with the type of ROUTER event; in this way
        // I achieve the trick of simulating in the case of say
        // EVENT::ROUTER_THROTTLE_1 only the routers that have that
        // VDD and freq. of THROTTLE_1 case;
        if ( this_event.type() == EVENT::ROUTER_BOOST && this_dvfs_level_prev != DVFS_BOOST) continue;
        if ( this_event.type() == EVENT::ROUTER && this_dvfs_level_prev != DVFS_BASE) continue;
        if ( this_event.type() == EVENT::ROUTER_THROTTLE_1 && this_dvfs_level_prev != DVFS_THROTTLE_1) continue;
        if ( this_event.type() == EVENT::ROUTER_THROTTLE_2 && this_dvfs_level_prev != DVFS_THROTTLE_2) continue;

        // this will add events of type: CREDIT (when flit is consumed or
        // moved from input buffer to output buffer), LINK (when flit is 
        // taken from output buffer and sent to downstream router via link);
        // Note: processing of router can result in having its DVFS
        // settings changed!
        _routers[i].simulate_one_router(); // including power;

        // record this "simulation cycle" of this router for the purpose
        // of estimating clock energy consumption;
        if ( _warmup_done) {
            _routers[i].power_module().power_clock_record();
        }
        
        // do the trick for dvfs; after a dvfs change one more cycle
        // was simulated to old/prev frequency; this makes sure flits 
        // go in-order on links;
        _routers[i].set_dvfs_level_prev( _routers[i].dvfs_level());
        
        // see if after processing this router has any of the three
        // DVFS settings; if so record it for to know later to insert 
        // the appropriate ROUTER event;
        if (_routers[i].dvfs_level() == DVFS_BOOST) {
            there_are_routers_at_boost = true;
        } 
        if (_routers[i].dvfs_level() == DVFS_BASE) {
            there_are_routers_at_base = true;
        } 
        if (_routers[i].dvfs_level() == DVFS_THROTTLE_1) {
            there_are_routers_at_throttle_1 = true;    
        } 
        if (_routers[i].dvfs_level() == DVFS_THROTTLE_2) {
            there_are_routers_at_throttle_2 = true;    
        }         
    }

    // add new ROUTER event, which will trigger resimulation of all routers
    // after pipe delay later;
    double delay = 0.0;
    if ( there_are_routers_at_boost) {
        delay = PIPE_DELAY_BOOST;
        _event_queue->add_event( EVENT(EVENT::ROUTER_BOOST, this_event.start_time() + delay));
    }
    if ( there_are_routers_at_base) {
        delay = PIPE_DELAY_BASE;
        _event_queue->add_event( EVENT(EVENT::ROUTER, this_event.start_time() + delay));
    }
    // Note: initially I had the next two gated by "if ( _warmup_done) {"
    // assuming that throttling was done only after warmup; removed it;
    if ( there_are_routers_at_throttle_1) {
        delay = PIPE_DELAY_THROTTLE_1;
        _event_queue->add_event( EVENT(EVENT::ROUTER_THROTTLE_1, this_event.start_time() + delay));
    }
    if ( there_are_routers_at_throttle_2) {
        delay = PIPE_DELAY_THROTTLE_2;
        _event_queue->add_event( EVENT(EVENT::ROUTER_THROTTLE_2, this_event.start_time() + delay));
    }
}

bool VNOC::receive_EVENT_LINK( EVENT this_event)
{
    //ADDRESS des_t = this_event.des_addr();
    long router_id = this_event.to_router_id();
    long pc_t = this_event.pc(); // port count or port id;
    long vc_t = this_event.vc(); // vc count or vc index;
    FLIT &flit = this_event.flit();
    //router(des_t).receive_flit_from_upstream(pc_t, vc_t, flit);
    _routers[router_id].receive_flit_from_upstream(pc_t, vc_t, flit);
}

bool VNOC::receive_EVENT_CREDIT( EVENT this_event)
{
    //ADDRESS des_t = this_event.des_addr();
    long router_id = this_event.to_router_id();
    long pc_t = this_event.pc();
    long vc_t = this_event.vc();
    // router(des_t).receive_credit(pc_t, vc_t);
    _routers[router_id].receive_credit(pc_t, vc_t);
}

////////////////////////////////////////////////////////////////////////////////
//
// utils
//
////////////////////////////////////////////////////////////////////////////////

void VNOC::check_simulation() 
{
    // check if the network is back to the inital state?
    vector<ROUTER>::const_iterator first = _routers.begin();
    vector<ROUTER>::const_iterator last = _routers.end();
    for (; first != last; first++) {
        first->sanity_check(); // empty check;
    }
    printf("\nSuccess: check for routers cleaness is ok\n");
}

void VNOC::print_network_routers() 
{
    printf("\n\nRouters occupancies:\n\n");
    for ( long i = 0; i < _routers_count; i++) {
        printf("\nR%d", _routers[i].id());
        _routers[i].print_input_buffers();
    }
}

void VNOC::update_and_print_simulation_results( bool verbose)
{
    // ()
    if ( _warmup_done == false) return;
    if ( verbose == false) return;


    // () entertain user;

    // calculate the total delay it took all packets to travel
    // thru the network from the time they were injected till the
    // time they arrived and were consumed at destinations;
    // these are packets arrived at destinations after warmup;
    double total_delay_at_all_routers = 0;
    for ( long i = 0; i < _routers_count; i++) {
        total_delay_at_all_routers += _routers[i].total_delay();
    }


    // power stuff;
    double total_mem_power = 0.0;
    double total_crossbar_power = 0.0;
    double total_arbiter_power = 0.0;
    double total_link_power = 0.0;
    double total_clock_power = 0.0;
    double total_power = 0.0;

    double total_mem_power_scaled = 0.0;
    double total_crossbar_power_scaled = 0.0;
    double total_arbiter_power_scaled = 0.0;
    double total_link_power_scaled = 0.0;
    double total_clock_power_scaled = 0.0;
    double total_power_scaled = 0.0;

    long total_num_injections_failed = 0;
    
    for ( long i = 0; i < _routers_count; i++) {
        ROUTER *this_router = &_routers[i];

        // all energy consumed by each component;
        total_mem_power += this_router->power_buffer_report();
        total_crossbar_power += this_router->power_crossbar_report();
        total_arbiter_power += this_router->power_arbiter_report();
        total_link_power += this_router->power_link_report();
        total_clock_power += this_router->power_clock_report();

        // get also the scaled energy of each router;
        total_mem_power_scaled += this_router->power_module().scaled_energy_buffer();
        total_crossbar_power_scaled += this_router->power_module().scaled_energy_crossbar();
        total_arbiter_power_scaled += this_router->power_module().scaled_energy_arbiter();
        total_link_power_scaled += this_router->power_module().scaled_energy_link();
        total_clock_power_scaled += this_router->power_module().scaled_energy_clock();
        total_power_scaled += this_router->power_module().scaled_energy(); // total

        // failed injections;
        total_num_injections_failed += this_router->num_injections_failed();
    }

    // total time elapsed since after the warmup was done;
    // note that we recorded events thru the power module only
    // after the warmup was done;
    double delta_time = _event_queue->current_sim_time() - 
        _topology->warmup_cycles_count();
    // compute power averages here from the energy data;
    total_power = 
        total_mem_power + total_crossbar_power + 
        total_arbiter_power + total_link_power + 
        total_clock_power;    
    total_power /= delta_time;
    total_mem_power /= delta_time;
    total_crossbar_power /= delta_time;
    total_arbiter_power /= delta_time;
    total_link_power /= delta_time;
    total_clock_power /= delta_time;

    total_power_scaled /= delta_time;
    total_mem_power_scaled /= delta_time;
    total_crossbar_power_scaled /= delta_time;
    total_arbiter_power_scaled /= delta_time;
    total_link_power_scaled /= delta_time;
    total_clock_power_scaled /= delta_time;

    // store here result; will be retrieved by any application of the 
    // vNOC simulator; average latency per packet;
    _latency = total_delay_at_all_routers / 
        max(_packets_arrived_count_after_wu, long(1));
    // total power (after warmup); we multiply it here with POWER_NOM=1e9
    // to get the right units in W; that is because earlier when energy was
    // divided by delta_time; which, delta_time was measured as multiples
    // of "1" (that corresponds to a clock period of freq=2GHz); that "1"
    // should have been "1/(2GHz)";
    _power = total_power * POWER_NOM;
    // average number of packets injected per cycle after before and 
    // warmup; all of them;
    _packets_per_cycle = 
        ( double(_total_packets_injected_count) / 
         _topology->simulation_cycles_count() ) / 
        _routers_count;

    printf("\n actual injection rate:                 %.4f", _packets_per_cycle);
    printf("\n total num of packets injected:         %d",   _total_packets_injected_count);
    printf("\n total num inj failed (PE buff full):   %d",   total_num_injections_failed);
    printf("\n num of packets delivered after warmup: %d",   _packets_arrived_count_after_wu);
    printf("\n avg latency per packet after warmup:   %.4f [cycles]", _latency);

    if ( _topology->do_dvfs() == true) {
        printf("\n Total power (scaled):                  %.4f [W]", total_power_scaled * POWER_NOM);
        //printf("\n Components (scaled) as percent buf,arb,xbar,link,clk: %.2f %.2f %.2f %.2f %.2f",
        //    100*(total_mem_power_scaled * POWER_NOM) / (total_power_scaled * POWER_NOM),
        //    100*(total_arbiter_power_scaled * POWER_NOM) / (total_power_scaled * POWER_NOM),
        //    100*(total_crossbar_power_scaled * POWER_NOM) / (total_power_scaled * POWER_NOM),
        //    100*(total_link_power_scaled * POWER_NOM) / (total_power_scaled * POWER_NOM),
        //    100*(total_clock_power_scaled * POWER_NOM) / (total_power_scaled * POWER_NOM));
    } else {
        printf("\n Total power (not scaled):              %.4f [W]", total_power * POWER_NOM);
        //printf("\n Components (not scaled) as percent buf,arb,xbar,link,clk: %.2f %.2f %.2f %.2f %.2f",
        //    100*(total_mem_power * POWER_NOM) / (total_power * POWER_NOM),
        //    100*(total_arbiter_power * POWER_NOM) / (total_power * POWER_NOM),
        //    100*(total_crossbar_power * POWER_NOM) / (total_power * POWER_NOM),
        //    100*(total_link_power * POWER_NOM) / (total_power * POWER_NOM),
        //    100*(total_clock_power * POWER_NOM) / (total_power * POWER_NOM));    
    }
    
    //printf("\n queue events processed:                %d",   _event_queue->queue_events_simulated());
    //printf("\n queue size now:                        %d",   _event_queue->event_count());
}

bool VNOC::update_and_check_for_early_termination()
{
    double total_delay_at_all_routers = 0;
    for ( long i = 0; i < _routers_count; i++) {
        total_delay_at_all_routers += _routers[i].total_delay();
    }
    double avg_latency_threshold = 6 * _routers_count;
    double avg_latency_so_far = total_delay_at_all_routers / 
        max(_packets_arrived_count_after_wu, long(1));
    if ( avg_latency_so_far > avg_latency_threshold) {
        return true; // stop simulation;
    }
    return false; // do not terminate;
}

void VNOC::compute_and_print_prediction_stats()
{
    double avg_prediction_err = 0.0;
    for ( long i = 0; i < _routers_count; i++) {
        // as percentage, e.g., 23.15%
        avg_prediction_err += _routers[i].get_avg_prediction_err_so_far();
    }
    avg_prediction_err = avg_prediction_err / _routers_count;
    //printf("\n avg. prediction error:                 %.2f", avg_prediction_err);
}

bool VNOC::run_simulation()
{
    bool result;
    
    result = _event_queue->run_simulation();

    return result;
}

void VNOC::set_frequencies_and_vdd( DVFS_LEVEL to_level)
{
    for ( long i = 0; i < _routers_count; i++) {
        // next function set values for route rand power_module variables;
        _routers[i].set_frequencies_and_vdd( to_level);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// TRAFFIC_INJECTOR
//
////////////////////////////////////////////////////////////////////////////////

// I have this function here and not inside vnoc_utils.cpp in order to avoid
// some forward declaration issues; TODO: fix this;

long TRAFFIC_INJECTOR::inject_selsimilar_traffic_or_update_task_duration(void)
{
    // self similar traffic is generated similarly to how it's described
    // in Li Shang paper; at each source/router - selected randomly - 
    // we initiate task-sessions whose starting moments are generated according to a 
    // Poisson distribution for inter-arrival times;
    // task durations are uniformly distributed within a specified range, with tasks
    // lasting on average from 600-2000 simulation-cycles
    //          _______________________          ______________           
    // r0: _____|  | |||   |   | |||| |__________||||| |||    |___________________
    //          <--------------------->          <------------>
    //          ^   task duration
    //          |
    //          starting moment of task generated with Poisson inter-arrival distr.
    // 
    //                  ________                                 ______________           
    // r13:_____________|||| |||_________________________________|   |   | ||||___
    //
    // routers are selected randomly;
    // inside each task, packet inter-arrivals are self-similar; this can be generated
    // by multiplexing ON/OFF sources that have Pareto-distributed ON and OFF periods;

    long dest_id = _id;


    // () if the router hooked to this injector is not among those selected
    // to operate as source for selfsimilar traffic, then just return the 
    // id of itself, which will mean that no packet will be injected here;
    if ( _selected_src_for_selfsimilar == false) {
        return dest_id;
    }
    

    // () then, if this is a src, we check to see if we are during or inside 
    // "task duration";
    double current_sim_time = _vnoc->event_queue()->current_sim_time();
    if ( current_sim_time >= _task_start_time &&
        current_sim_time <= _task_stop_time) {
        // if we are during a task-duration for this source, then 
        // inject packets as dictated by the level2 pareto selfsimilar
        // generation mechanism;

        // (a) bring the selfsimilar generator to an injection time
        // immediately after the current simulation time; this
        // helps go over intervals of time between task-durations
        // when no packet must be injected whatsoever at this router;
        while ( _prev_injection_time < current_sim_time) {

            Trace this_trace = _gen_pareto_level2.GenerateTrace();
            
            // _prev_injection_time keeps track of the "time"
            // of the generator, that may have a different scale;
            _prev_injection_time = this_trace.TimeStamp;
            _prev_num_injected_packets = this_trace.PacketSize;
        }

        // (b) inject packets;
        // Note: the pareto generator must be set up (during
        // its initialization) such that the "time" of the 
        // generator is incremented with small values; and not some 
        // large amounts that may actually move _prev_injection_time
        // used here too far into the future compared to the 
        // current_sim_time; it's better to have the generator's
        // "time" smaller tahn too large; we can filter out some
        // injections and the traffic still stays selfsimilar;
        // filtering can be done like in the other types of
        // traffic, with the help of _injection_rate;
        if ( _prev_injection_time < _task_stop_time) { // for sanity;

            //printf("\n (%f) _prev_num_injected_packets: %d", 
            //    _prev_injection_time, _prev_num_injected_packets);
            
            if ( drand48() < _injection_rate) { // mimic Poisson arrival

                // first pick randomly a destination different from itself;
                dest_id = _vnoc->get_a_router_id_randomly();
                while ( dest_id == _id) {
                    dest_id = _vnoc->get_a_router_id_randomly();
                }
                bool injected_a_packet_here = false;
                for (long i = 0; i < _prev_num_injected_packets; i++) {
                    // inject or not a packet into the local PE queue;
                    // it will not be injected if PE input buffer is full;
                    injected_a_packet_here =
                        _router->receive_packet_from_local_traffic_injector( dest_id);

                    // vnoc level counter of all injected packets;
                    if ( injected_a_packet_here == true) { 
                        _vnoc->incr_total_packets_injected_count();
                    } else {
                        // if a packet was not injected, because the PE buffer is
                        // full, then just stop attempting to insert any more;
                        break; 
                    }
                }
            }
        }

    }

    else {
        // we are not inside a "task duration"; we are between tasks,
        // but still need to, just one time for each in-between periods,
        // generate the next task by generating a new set of 
        // (_task_start_time, _task_stop_time);
        if ( current_sim_time > _task_stop_time) {
            // this means that we are just out of the previous task-duration;
            // and, we should generate here new start,stop times for the next
            // task-duration;

            // Note: _poisson_distr_level1 is initialized so that it generates
            // samples form Poisson distr. with mean 600 cycles; this initialization is
            // done in the constructor of VNOC;
            _task_start_time = current_sim_time + 
                double( _vnoc->generate_new_task_start_time_from_poisson());

            // the stop time is generated as a random number between 600-2000;
            // TODO: make 600-2000 as parameters;
            _task_stop_time = _task_start_time + 
                double(_vnoc->topology()->rng().flat_l(600, 2000));

            //printf("\n (%d) _task_start_time: %f _task_stop_time: %f", _id,
            //    _task_start_time, _task_stop_time);
        }
    }

    return dest_id;
}
