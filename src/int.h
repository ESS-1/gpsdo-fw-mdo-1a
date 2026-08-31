#ifndef _INT_060B0C260189_H_
#define _INT_060B0C260189_H_

#include <stdbool.h>
#include <stdint.h>

extern volatile bool     allow_adjustment;
extern volatile uint32_t frequency;
extern volatile uint32_t num_samples;
extern volatile uint32_t device_uptime;
extern volatile uint32_t last_pps_out;
extern volatile bool     pps_out_up;
extern volatile int32_t  ppb_frequency;
extern volatile int32_t  ppb_frequency_error;
extern volatile int32_t  ppb_correction;
extern volatile int32_t  ppb_millis;
extern volatile int32_t  pps_error;
extern volatile int32_t  pps_millis;
extern volatile uint32_t pps_sync_count;
extern volatile bool     sync_pps_out;
extern volatile bool     is_ppb_current;
extern volatile bool     gps_lock_status;
extern volatile bool     suppress_adjustment;

// For correction algorithms
// OCXO models
typedef enum {
	OCXO_MODEL_ISOTEMP,
	OCXO_MODEL_OX256B,
	OCXO_MODEL_UNKNOWN,

    OCXO_MODEL_MAX = OCXO_MODEL_UNKNOWN
} ocxo_model_type;

// Correction algorithms
typedef enum {
	CORRECTION_ALGO_DANKAR,
	CORRECTION_ALGO_FREDZO,
	CORRECTION_ALGO_ERIC_H,
	CORRECTION_ALGO_ERIC_H_PLUS,

    CORRECTION_ALGO_MAX = CORRECTION_ALGO_ERIC_H_PLUS
} correction_algo_type;

const char* ocxo_model_type_to_string(uint8_t model);
const char* correction_algo_type_to_string(uint8_t model);

void set_brightness(uint8_t brightness);

uint32_t get_default_correction_factor(correction_algo_type algo);
void increment_correction_factor_value(uint32_t* value, correction_algo_type algo, int32_t increment);
uint32_t get_default_warmup_time(ocxo_model_type model);

#endif
