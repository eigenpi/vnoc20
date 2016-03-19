#ifndef _VNOC_PREDICTOR_H_
#define _VNOC_PREDICTOR_H_

#include "vnoc.h"


using namespace std;


class ROUTER;

////////////////////////////////////////////////////////////////////////////////
//
// PREDICTOR_MODULE
//
////////////////////////////////////////////////////////////////////////////////


class PREDICTOR_MODULE {
    private:
        PREDICTOR_TYPE _predictor_type;
        // period at which DVFS can be done; default 50 like in Li Shang paper;
        long _control_period; // multiple of _predict_distance the best;

        // () variables used for EXPONENTIAL_AVERAGING prediction;
        double _alpha;
        // () variables used for HISTORY;
        long _history_window;
        long _history_weight; 

        // BU predicted at output port i=1..4, which are proxy for the actual
        // BU predictions of the input buffers in downsteam routers; these
        // are used in algorithm 1 of Li Shang paper; 
        double _BU_predicted_out_i[5];
        double _BU_predicted_out_i_last[5];
        // link utilization predicted for the link driven by the aforementioned
        // output port; also used in algorithm 1 of Li Shang paper; 
        double _LU_predicted_out_i[5]; 
        double _LU_predicted_out_i_last[5]; 
        // denuminator of eq. 2 in Li Shang paper; this counter is
        // reset at the end of the control period when actaul predictions 
        // are made;
        long _counter_router_own_cycles;
        // numerator of eq. 2 in Li Shang paper;
        long _counter_flits_sent_during_hw[5];

        // ovearll BU utilization for this router; looks at all input ports
        // of this buffer itself; used in the freq. throttle section 
        // of Asit Mishra paper;
        double _BU_predicted_all_inputs;
        double _BU_predicted_all_inputs_last;


        // statistics;
        long _predictions_count;
        double _BU_all_prediction_err_as_percentage; // e.g., 0.55 + 0.16 + 0.9

    public:
        PREDICTOR_MODULE( PREDICTOR_TYPE predictor_type,
            long control_period, long history_window) {
            _predictor_type = predictor_type;
            _control_period = control_period; // 50 cycles by default;

            _alpha = 0.5;

            _history_window = history_window;
            _history_weight = 3; 

            _BU_predicted_all_inputs = 0.0;
            _BU_predicted_all_inputs_last = 0.0;
            for (long i = 1; i < 5; i++) {
                _BU_predicted_out_i[i] = 0.0;
                _BU_predicted_out_i_last[i] = 0.0;
                _LU_predicted_out_i[i] = 0.0; // LU related;
                _LU_predicted_out_i_last[i] = 0.0; // LU related;
                _counter_flits_sent_during_hw[i] = 0; // LU related;
            }
            _counter_router_own_cycles = 0; // used for BU, LU;
            
            _predictions_count = 1;
            _BU_all_prediction_err_as_percentage = 0.0;
        }
        ~PREDICTOR_MODULE() {} 

        PREDICTOR_TYPE predictor_type() const { return _predictor_type; }
        long control_period() const { return _control_period; }
        long history_window() const { return _history_window; }
        long history_weight() const { return _history_weight; }

        void maintain_or_perform_prediction_ASYNC(ROUTER *my_router);
        void maintain_for_prediction_SYNC(ROUTER *my_router);
        void perform_prediction_SYNC(ROUTER *my_router);

        void update_DVFS_settings_throttle_only(ROUTER *my_router);
        void update_DVFS_settings_throttle_and_boost( ROUTER *my_router);
        
        void record_flit_transmission_for_LU_calculation(long i) {
            // i is 1..4 as the index of any of the four output ports;
            _counter_flits_sent_during_hw[i] ++;
        }
        void reset_all_BU_related_variables() {
            // do nor rest the "last" BU variables because they need
            // to store the previous predictions; however, reset
            // the current BU variables because they are used
            // to keep track of calculations during the next history
            // window;
            _BU_predicted_all_inputs = 0.0;
            for (long i = 1; i < 5; i++) {
                _BU_predicted_out_i[i] = 0.0;
            }
        }        
        
        double BU_all_prediction_err_as_percentage() const { 
            return _BU_all_prediction_err_as_percentage; 
        }
        long predictions_count() const { return _predictions_count; }
};

#endif

