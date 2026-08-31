#ifndef _GPS_890A365E2D91_H_
#define _GPS_890A365E2D91_H_

#include <stdint.h>
#include <stdbool.h>

#define GPS_DEFAULT_BAUDRATE     9600
#define GPS_LOCATOR_SIZE         10

#define GPS_EMPTY_DATE_TIME      0xFFFFFFFF
#define PPB_EMPTY_DEG_X10M_COORD 0x7FFFFFFF

#pragma pack(push, 1)
typedef union {
    uint32_t raw;
    struct {
        uint8_t seconds;
        uint8_t minutes;
        uint8_t hours;
        uint8_t padding;
    };
} PackedTime;
#pragma pack(pop)

#pragma pack(push, 1)
typedef union {
    uint32_t raw;
    struct {
        uint8_t  day;
        uint8_t  month;
        uint16_t year;
    };
} PackedDate;
#pragma pack(pop)

extern PackedTime gps_time;
extern PackedDate gps_date;
extern char       gps_latitude_str[16];
extern char       gps_longitude_str[16];
extern char       gps_msl_altitude_str[10];
extern char       gps_geoid_separation_str[10];
extern int32_t    gps_latitude_deg_x10M;
extern int32_t    gps_longitude_deg_x10M;
extern char       gps_locator[GPS_LOCATOR_SIZE + 1];
extern char       gps_hdop_str[8];
extern uint8_t    num_sats;
extern uint32_t   gga_frames;

// GPS module models
typedef enum {
    GPS_MODEL_ATGM336H,
    GPS_MODEL_NEO6M,
    GPS_MODEL_NEOM9N,
    GPS_MODEL_UNKNOWN,

    GPS_MODEL_MAX = GPS_MODEL_UNKNOWN
} gps_model_type;

// Min and max values for time offset
#define GPS_MIN_TIME_OFFSET -14
#define GPS_MAX_TIME_OFFSET  14

extern int8_t   gps_time_offset;

// Last tiem a frame was received
extern uint32_t          last_frame_receive_time;
extern uint32_t          gps_invalid_frames;
extern volatile uint32_t gps_fifo_overflow_gps;
extern volatile uint32_t gps_fifo_overflow_comm;

const char* gps_model_type_to_string(uint8_t model);

void gps_setbaudrate(uint32_t baudrate);
void gps_reset_uart();

void gps_start_it();
void gps_run();

#endif
