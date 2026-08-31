#include "gps.h"
#include "main.h"
#include "int.h"
#include "stm32f1xx_hal_uart.h"
#include "usbd_cdc_if.h"
#include "usart.h"
#include "eeprom.h"
#include "frequency.h"
#include "cdcio.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MAX_GPS_LINE    512

static char gps_line[MAX_GPS_LINE];
PackedTime  gps_time                          = { .raw = GPS_EMPTY_DATE_TIME };
PackedDate  gps_date                          = { .raw = GPS_EMPTY_DATE_TIME };
char        gps_latitude_str[16]              = { '\0' };
char        gps_longitude_str[16]             = { '\0' };
char        gps_msl_altitude_str[10]          = { '\0' };
char        gps_geoid_separation_str[10]      = { '\0' };
int32_t     gps_latitude_deg_x10M             = PPB_EMPTY_DEG_X10M_COORD;
int32_t     gps_longitude_deg_x10M            = PPB_EMPTY_DEG_X10M_COORD;
char        gps_locator[GPS_LOCATOR_SIZE + 1] = { '\0' };
char        gps_hdop_str[8]                   = { '\0' };
uint8_t     num_sats                          = 0;
uint32_t    gga_frames                        = 0;

static size_t gps_line_len    = 0;
int8_t        gps_time_offset = 0; // -14/+14
static int8_t gps_day_offset  = 0; // -1/+1

// Store last frame receive time
uint32_t last_frame_receive_time = 0;

uint32_t          gps_invalid_frames      = 0;
volatile uint32_t gps_fifo_overflow_gps   = 0;
volatile uint32_t gps_fifo_overflow_comm  = 0;

static uint32_t gps_last_pgdos_generated_sec = 0;

// Indicates whether this is the first GPS UART RX after an MCU or UART reset.
// When 'true', errors are cleared because the GPS module sends data before
// UART initialization, which causes frame or noise errors.
static volatile bool gps_first_gps_uart_rx = false;

#define FIFO_BUFFER_SIZE 1024

#if FIFO_BUFFER_SIZE != APP_TX_DATA_SIZE
#error "USB CDC TX buffer size must match FIFO_BUFFER_SIZE. Change CDC TX buffer size in CubeMX."
#endif

typedef struct {
    uint8_t buffer[FIFO_BUFFER_SIZE];
    size_t  read;
    size_t  write;
} fifo_buffer_t;

typedef enum { FIFO_WRITE, FIFO_READ } fifo_operation;

static volatile fifo_buffer_t fifo_buffer_gps  = { 0 };
static volatile fifo_buffer_t fifo_buffer_comm = { 0 };

static void gps_cdc_rx_callback(const uint8_t* buf, uint32_t len);

static size_t fifo_next(volatile const fifo_buffer_t* fifo, fifo_operation op)
{
    if (op == FIFO_WRITE) {
        return (fifo->write + 1) % FIFO_BUFFER_SIZE;
    } else {
        return (fifo->read + 1) % FIFO_BUFFER_SIZE;
    }
}

static bool fifo_write(volatile fifo_buffer_t* fifo, const uint8_t c)
{
    size_t next = fifo_next(fifo, FIFO_WRITE);
    if (next == fifo->read) {
        return false;
    }
    fifo->buffer[fifo->write] = c;
    fifo->write               = next;
    return true;
}

static bool fifo_read(volatile fifo_buffer_t* fifo, uint8_t* c)
{
    if (fifo->read == fifo->write) {
        return false;
    }
    *c         = fifo->buffer[fifo->read];
    fifo->read = fifo_next(fifo, FIFO_READ);

    return true;
}

const char* gps_model_type_to_string(uint8_t model)
{
    switch (model)
    {
        case GPS_MODEL_ATGM336H:
            return "ATGM336H";

        case GPS_MODEL_NEO6M:
            return "NEO-6M";

        case GPS_MODEL_NEOM9N:
            return "NEO-M9N";

        default:
        case GPS_MODEL_UNKNOWN:
            return "Generic";
    }
}

#define GPS_RX_BUFFER_SIZE  20
static volatile uint8_t gps_it_buf[GPS_RX_BUFFER_SIZE];

static void gps_start_gps_rx()
{
    if (HAL_UART_Receive_DMA(&huart3, (uint8_t*)gps_it_buf, GPS_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

static void gps_start_comm_rx()
{
    CDC_SetRxHandler_FS(gps_cdc_rx_callback);
}

// ATGM336H set baudrate commands
static const char * const atgm336h_baudcommands[] = {
    "$PCAS01,1*1D\r\n",     // 9600bps
    "$PCAS01,2*1E\r\n",     // 19200bps
    "$PCAS01,3*1F\r\n",     // 38400bps
    "$PCAS01,4*18\r\n",     // 57600bps
    "$PCAS01,5*19\r\n"      // 115200bps
};

static void gps_sendcommand(const char* cmd, size_t len)
{
    while (huart3.gState != HAL_UART_STATE_READY);
    HAL_UART_Transmit_DMA(&huart3, (const uint8_t*)cmd, len);
    // Wait for transfer completed
    while (huart3.gState != HAL_UART_STATE_READY);
}

static int gps_change_module_baudrate(uint32_t baudrate)
{
    const char* command = NULL;
    switch(ee_storage.gps_model)
    {
        case GPS_MODEL_ATGM336H:
            switch (baudrate) {
                case 9600:
                    command = atgm336h_baudcommands[0];
                    break;
                case 19200:
                    command = atgm336h_baudcommands[1];
                    break;
                case 38400:
                    command = atgm336h_baudcommands[2];
                    break;
                case 57600:
                    command = atgm336h_baudcommands[3];
                    break;
                case 115200:
                    command = atgm336h_baudcommands[4];
                    break;
                default:
                    return -1;  // error
            }
            break;
        case GPS_MODEL_NEO6M:
            // TODO
        case GPS_MODEL_NEOM9N:
            // TODO
        case GPS_MODEL_UNKNOWN:
            break;
    }

    size_t len;
    if (command != NULL) {
        len = strlen(command);
        gps_sendcommand(command, len);
        // Wait for the GPS module to process the baud rate change command
        // and allow the physical RX/TX lines to stabilize (critical for STM32 clones)
        HAL_Delay(30);
    }

    return 0;
}

static void gps_reconfigure_uart(UART_HandleTypeDef *huart, uint32_t baudrate)
{
    // Wait for HAL to finish transmission
    while (huart->gState != HAL_UART_STATE_READY);

    // Wait for the hardware transmitter to physically send the last bit
    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) == RESET);

    // De-initialize UART
    HAL_UART_DeInit(huart);

    // Configure new baud rate
    huart->Init.BaudRate     = baudrate;
    huart->Init.WordLength   = UART_WORDLENGTH_8B;
    huart->Init.StopBits     = UART_STOPBITS_1;
    huart->Init.Parity       = UART_PARITY_NONE;
    huart->Init.Mode         = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(huart) != HAL_OK) {
        Error_Handler();
    }
}

static void gps_reconfigure_gps_uart(uint32_t baudrate)
{
    gps_reconfigure_uart(&huart3, baudrate);
    gps_first_gps_uart_rx = true;
    gps_start_gps_rx();
}

void gps_setbaudrate(uint32_t baudrate)
{
    gps_change_module_baudrate(baudrate);
    gps_reconfigure_gps_uart(baudrate);
}

void gps_reset_uart()
{
    gps_reconfigure_gps_uart(ee_storage.gps_baudrate);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart == &huart3) {
        for (size_t i = 0; i < GPS_RX_BUFFER_SIZE; i++) {
            if (!fifo_write(&fifo_buffer_gps, gps_it_buf[i])) {
                ++gps_fifo_overflow_gps;
            }
        }
        gps_first_gps_uart_rx = false;
        gps_start_gps_rx();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if (huart == &huart3 && gps_first_gps_uart_rx) {
        // This is the first GPS UART RX after an MCU or UART reset.
        // Error likely occurred because the GPS module sent data before UART initialization.
        // This is an expected situation, so we only need to restart the RX.
        HAL_UART_DMAStop(huart);
        gps_start_gps_rx();
    }
}

static void gps_cdc_rx_callback(const uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (!fifo_write(&fifo_buffer_comm, buf[i])) {
            ++gps_fifo_overflow_comm;
        }
    }
}

void gps_start_it()
{
    gps_first_gps_uart_rx = true;
    gps_start_gps_rx();
    gps_start_comm_rx();
}

// Parses NMEA coordinate string (DDMM.MMMMMMM) into int32_t scaled by 10^7.
// Returns scaled coordinate value: (Degrees * 10^7), or 0 on syntax/bounds failure.
static int32_t gps_parse_coordinate(const char* nmea_coord)
{
    if (nmea_coord == NULL) {
        return PPB_EMPTY_DEG_X10M_COORD;
    }

    // Find the decimal point separating whole minutes and fractional minutes
    const char* dot = strchr(nmea_coord, '.');
    if (dot == NULL) {
        return PPB_EMPTY_DEG_X10M_COORD;
    }

    // Minutes (MM) always occupy exactly two numeric characters immediately preceding the decimal point
    const char* min_start = dot - 2;
    if (min_start < nmea_coord) {
        return PPB_EMPTY_DEG_X10M_COORD;
    }

    // Parse integer degrees (all characters in the string before the minutes part)
    int32_t deg = 0;
    for (const char* p = nmea_coord; p < min_start; ++p) {
        if (*p < '0' || *p > '9') {
            return PPB_EMPTY_DEG_X10M_COORD;
        }

        deg = deg * 10 + (*p - '0');
    }

    // Verify degrees do not exceed physical maximums to prevent signed 32-bit integer overflow
    // Max latitude is 90 degrees; max longitude is 180 degrees
    if (deg > 180) {
        return PPB_EMPTY_DEG_X10M_COORD;
    }

    // Validate that both minutes (MM) characters are valid numeric digits
    if (min_start[0] < '0' || min_start[0] > '9' || min_start[1] < '0' || min_start[1] > '9') {
        return PPB_EMPTY_DEG_X10M_COORD;
    }

    // Parse integer minutes (MM) and scale to 10^7
    int32_t min_int = (min_start[0] - '0') * 10 + (min_start[1] - '0');
    if (min_int >= 60) {
        return PPB_EMPTY_DEG_X10M_COORD;
    }

    int32_t total_minutes_scaled = min_int * 10000000;

    // Parse fractional minutes up to 7 decimal places
    int32_t     scale = 1000000;
    const char* p     = dot + 1;
    while (*p >= '0' && *p <= '9' && scale > 0) {
        total_minutes_scaled += (*p - '0') * scale;
        scale /= 10;
        p++;
    }

    // Round up if the 8th fractional digit is >= 5
    if (*p >= '5' && *p <= '9' && scale == 0) {
        ++total_minutes_scaled;
    }

    int32_t deg_frac_scaled = (total_minutes_scaled + 30) / 60;
    return (deg * 10000000) + deg_frac_scaled;
}

static void gps_compute_locator(int32_t lat_x10M, int32_t lon_x10M)
{
    if (lat_x10M == PPB_EMPTY_DEG_X10M_COORD || lon_x10M == PPB_EMPTY_DEG_X10M_COORD) {
        gps_locator[0] = '\0';
        return;
    }

    // Clamp coordinates to grid boundaries
    if (lon_x10M > 1799999999)
        lon_x10M = 1799999999;
    if (lon_x10M < -1800000000)
        lon_x10M = -1800000000;
    if (lat_x10M > 899999999)
        lat_x10M = 899999999;
    if (lat_x10M < -900000000)
        lat_x10M = -900000000;

    // Shift coordinates to strictly positive domain
    uint32_t lon_rem = (uint32_t)lon_x10M + 1800000000U;
    uint32_t lat_rem = (uint32_t)lat_x10M + 900000000U;

    int pos = 0;

    // Process pairs
    // Pair 1: Field (20 degrees lon, 10 degrees lat) -> Letters A-R
    if (pos + 1 < GPS_LOCATOR_SIZE) {
        gps_locator[pos++] = (char)('A' + (lon_rem / 200000000U));
        gps_locator[pos++] = (char)('A' + (lat_rem / 100000000U));
        lon_rem %= 200000000U;
        lat_rem %= 100000000U;
    }

    // Pair 2: Square (2 degrees lon, 1 degree lat) -> Digits 0-9
    if (pos + 1 < GPS_LOCATOR_SIZE) {
        gps_locator[pos++] = (char)('0' + (lon_rem / 20000000U));
        gps_locator[pos++] = (char)('0' + (lat_rem / 10000000U));
        lon_rem %= 20000000U;
        lat_rem %= 10000000U;
    }

    // Pair 3: Subsquare (5 min lon, 2.5 min lat) -> Letters a-x
    if (pos + 1 < GPS_LOCATOR_SIZE) {
        lon_rem *= 12U;
        lat_rem *= 24U;
        gps_locator[pos++] = (char)('a' + (lon_rem / 10000000U));
        gps_locator[pos++] = (char)('a' + (lat_rem / 10000000U));
        lon_rem %= 10000000U;
        lat_rem %= 10000000U;
    }

    // Pair 4: Extended Square (0.5 min lon, 0.25 min lat) -> Digits 0-9
    if (pos + 1 < GPS_LOCATOR_SIZE) {
        lon_rem *= 10U;
        lat_rem *= 10U;
        gps_locator[pos++] = (char)('0' + (lon_rem / 10000000U));
        gps_locator[pos++] = (char)('0' + (lat_rem / 10000000U));
        lon_rem %= 10000000U;
        lat_rem %= 10000000U;
    }

    // Pair 5: Sub-extended Square (Grid 24x24) -> Letters a-x
    if (pos + 1 < GPS_LOCATOR_SIZE) {
        lon_rem *= 24U;
        lat_rem *= 24U;
        gps_locator[pos++] = (char)('a' + (lon_rem / 10000000U));
        gps_locator[pos++] = (char)('a' + (lat_rem / 10000000U));
    }

    // Null-terminate the string
    gps_locator[pos] = '\0';
}

static bool change_time(int time_source, uint8_t *time_dest, int correction, int max_value)
{
    bool overlap = false;
    int value = time_source + correction;
    if(value > max_value)
    {
        value = 0;
        overlap = true;
    }

    *time_dest = (uint8_t)value;
    return overlap;
}

static void gps_safe_copy_string(char* dst, const char* src, size_t dst_char_count)
{
    if (src != NULL) {
        strlcpy(dst, src, dst_char_count);
    } else {
        dst[0]   = '\0';
    }
}

// Maybe use X-CUBE-GNSS here?
static void gps_parse(char* line)
{
    if (strstr(line, "GGA") == line+3) 
    {
        char* pch = strsep(&line, ",");

        pch = strsep(&line, ","); // Time

        if (pch != NULL && strlen(pch) >= 6)
        {
            // GPSDO screen is updated once every second, when receiving the PPS signal
            // BUT, the GGA frame is received a fraction of second AFTER the PPS pulse
            // To achieve accurate time display, we will add one second to the received time
            // to compensate this delay

            int hour = (10 * (pch[0] - '0')) + (pch[1] - '0');
            int min  = (10 * (pch[2] - '0')) + (pch[3] - '0');
            int sec  = (10 * (pch[4] - '0')) + (pch[5] - '0');

            // Let's start with seconds value, to propagate overlap to minutes and hours if needed
            bool overlap = change_time(sec, &(gps_time.seconds), 1, 59);
            if(overlap)
            {   // Need to propagate overlap to minutes
                overlap = change_time(min, &(gps_time.minutes), 1, 59);
            }
            else
            {
                gps_time.minutes = (uint8_t)min;
            }

            if (gps_time_offset == 0 && !overlap) 
            {   // Leave hour unchanged
                gps_time.hours = (uint8_t)hour;
            } 
            else 
            {   // Need to fix hour
                int relative_hour = (hour + (int)gps_time_offset);
                if(overlap)
                {   // Propagate second / minute overlap
                    relative_hour+=1;
                }
                if(relative_hour >= 24)
                {
                    hour = relative_hour - 24;
                    gps_day_offset = 1;
                }
                else if(relative_hour < 0)
                {
                    hour = relative_hour + 24;
                    gps_day_offset = -1;
                }
                else
                {
                    hour = relative_hour;
                    gps_day_offset = 0;
                }

                gps_time.hours = (uint8_t)hour;
            }

            pch = strsep(&line, ","); // Latitude
            gps_safe_copy_string(gps_latitude_str, pch, ARRAY_SIZE(gps_latitude_str));
            gps_latitude_deg_x10M = gps_parse_coordinate(pch);

            pch = strsep(&line, ","); // N/S
            if (pch != NULL && gps_latitude_deg_x10M != PPB_EMPTY_DEG_X10M_COORD && pch[0] == 'S') {
                gps_latitude_deg_x10M *= -1;
            }

            pch = strsep(&line, ","); // Longitude
            gps_safe_copy_string(gps_longitude_str, pch, ARRAY_SIZE(gps_longitude_str));
            gps_longitude_deg_x10M = gps_parse_coordinate(pch);

            pch = strsep(&line, ","); // E/W
            if (pch != NULL && gps_longitude_deg_x10M != PPB_EMPTY_DEG_X10M_COORD && pch[0] == 'W') {
                gps_longitude_deg_x10M *= -1;
            }

            gps_compute_locator(gps_latitude_deg_x10M, gps_longitude_deg_x10M);

            strsep(&line, ","); // Fix

            pch = strsep(&line, ","); // Num sats used
            num_sats = pch != NULL ? atoi(pch) : 0;

            pch = strsep(&line, ","); // HDOP
            gps_safe_copy_string(gps_hdop_str, pch, ARRAY_SIZE(gps_hdop_str));

            pch = strsep(&line, ","); // MSL Elevation
            gps_safe_copy_string(gps_msl_altitude_str, pch, ARRAY_SIZE(gps_msl_altitude_str));

            strsep(&line, ","); // Unit

            pch = strsep(&line, ","); // Geoid Separation
            gps_safe_copy_string(gps_geoid_separation_str, pch, ARRAY_SIZE(gps_geoid_separation_str));

            // strsep(&line, ","); // Unit

            gga_frames++;
        }
    } 
    else if (strstr(line, "RMC") == line+3) 
    {
        char* pch = strsep(&line, ",");

        pch = strsep(&line, ","); // Time
        pch = strsep(&line, ","); // Alert
        pch = strsep(&line, ","); // Latitude
        pch = strsep(&line, ","); // N/S
        pch = strsep(&line, ","); // Longitude
        pch = strsep(&line, ","); // E/W
        pch = strsep(&line, ","); // Speed
        pch = strsep(&line, ","); // Orientation
        pch = strsep(&line, ","); // Date

        if(pch!=NULL && strlen(pch)>=6)
        {   // Ignore empty dates
            char d0    = pch[0] - '0';
            char d1    = pch[1] - '0';
            char m0    = pch[2] - '0';
            char m1    = pch[3] - '0';
            char y0    = pch[4] - '0';
            char y1    = pch[5] - '0';
            int  day   = d0 * 10 + d1;
            int  month = m0 * 10 + m1;
            int  year  = 2000 + y0 * 10 + y1;

            if (gps_time_offset != 0) {
                day += gps_day_offset;
                // Quick and dirty poor man's Gregorian calendar handling
                bool is_leap_year = ((year % 4) == 0);
                if((day > (is_leap_year ? 29 : 28)) && month == 2)
                {   // Case of February
                    day = 1;
                    month = 3;
                }
                else if(day > 31 && month == 12)
                {   // Need to change year
                    day = 1;
                    month = 1;
                    year += 1;
                }
                else if( (day > 30 && (month == 4 || month == 6 || month == 9 || month == 11)) ||
                         (day > 31))
                {   // Case of other months with 30 or 31 days
                    day = 1;
                    month++;
                }
                else if(day < 1)
                {
                    if(month == 1)
                    {   // Need to change year
                        day = 31;
                        month = 12;
                        year--;
                    }
                    else if(month == 3)
                    {   // Case of february
                        day = is_leap_year ? 29 : 28;
                        month--;
                    }
                    else if(month == 2 || month == 4 || month == 6 ||month == 8 || month == 9 || month == 11)
                    {   // Months after a 31 day month
                        day = 31;
                        month--;
                    }
                    else
                    {   // Months after a 30 day month
                        day = 30;
                        month--;
                    }
                }
            }

            gps_date.day   = day;
            gps_date.month = month;
            gps_date.year  = year;
        }
    }
}

static bool gps_is_valid(const char* line)
{
    // Check the basic structure
    if (line == NULL || line[0] != '$') {
        return false;
    }

    const char *ptr = line + 1; // Skip the '$' sign
    unsigned char calculated_checksum = 0;

    // XOR all characters until '*' is found
    while (*ptr != '*' && *ptr != '\0') {
        calculated_checksum ^= (unsigned char)*ptr;
        ++ptr;
    }

    // If we reached the end of the string without finding '*'
    if (*ptr == '\0') {
        return false;
    }

    // Read the expected checksum from the string (after '*')
    ++ptr; // Skip the '*' sign

    // Convert hex string to a numeric value
    unsigned long received_checksum = strtoul(ptr, NULL, 16);

    // Compare the calculated checksum with the received one
    return (unsigned char)received_checksum == calculated_checksum;
}

static void gps_process(char* line)
{
    // Validate and parse the frame
    if (gps_is_valid(line)) {
        gps_parse(line);
    } else {
        ++gps_invalid_frames;
    }

    // Get reception time
    last_frame_receive_time = HAL_GetTick();
}

static char gps_hex_digit(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    return digits[value & 0x0Fu];
}

static uint8_t gps_sat_u8(uint32_t value)
{
    return (value > 0xFFu) ? 0xFFu : (uint8_t)value;
}

static bool gps_add_pgdos_frame(uint8_t *buffer, size_t buffer_size, size_t *bytes_written)
{
    if (bytes_written != NULL) {
        *bytes_written = 0u;
    }

    if (buffer == NULL || buffer_size == 0u) {
        return false;
    }

    char device_state = frequency_adjustment_allowed() ? '+' : 'W';
    char gps_state    = gps_lock_status ? '+' : 'N';

    // Format $PGDOS frame
    int payload_len = snprintf(
        (char *)buffer,
        buffer_size,
        "$PGDOS,%c%c,%08"PRIX32",%02X,%08"PRIX32",%08"PRIX32",%04"PRIX16",%02X%02X%02X",
        device_state,
        gps_state,
        device_uptime,
        num_sats,
        (uint32_t)frequency_ppb_x100,
        (uint32_t)frequency_get_inst_ppb_x100(),
        (uint16_t)TIM1->CCR2,
        gps_sat_u8(gps_invalid_frames),
        gps_sat_u8(gps_fifo_overflow_gps),
        gps_sat_u8(gps_fifo_overflow_comm));

    if (payload_len < 0 || (size_t)payload_len >= buffer_size) {
        return false;
    }

    // Calculate checksum
    uint8_t checksum = 0u;
    for (size_t i = 1u; i < (size_t)payload_len; i++) {
        checksum ^= buffer[i];
    }

    size_t pos = (size_t)payload_len;
    if (pos + 5u > buffer_size) {
        return false;
    }

    buffer[pos++] = '*';
    buffer[pos++] = (uint8_t)gps_hex_digit(checksum >> 4);
    buffer[pos++] = (uint8_t)gps_hex_digit(checksum);
    buffer[pos++] = '\r';
    buffer[pos++] = '\n';

    if (bytes_written != NULL) {
        *bytes_written = pos;
    }

    return true;
}

static void gps_run_pgdos(uint8_t* buf, size_t* buf_offset, size_t buf_size)
{
    if (gps_last_pgdos_generated_sec != device_uptime) {
        size_t bytes_written = 0;
        if (gps_add_pgdos_frame(buf + (*buf_offset), buf_size - (*buf_offset), &bytes_written)) {
            (*buf_offset) += bytes_written;
            gps_last_pgdos_generated_sec = device_uptime;
        }
    }
}

#define SEND_BUFFER_SIZE FIFO_BUFFER_SIZE
static uint8_t send_buf[SEND_BUFFER_SIZE];
static uint8_t comm_send_buf[SEND_BUFFER_SIZE];

void gps_run()
{
    size_t send_size = 0;
    uint8_t c;

    while (send_size < SEND_BUFFER_SIZE && fifo_read(&fifo_buffer_gps, &c)) {
        gps_line[gps_line_len++] = c;
        send_buf[send_size++]    = c;
        if (c == '\n' && gps_line_len < MAX_GPS_LINE) {
            gps_line[gps_line_len] = '\0';
            gps_process(gps_line);
            gps_line_len = 0;

            // Try injecting a $PGDOS frame into the GPS data stream
            gps_run_pgdos(send_buf, &send_size, SEND_BUFFER_SIZE);

            continue;
        }
        if (gps_line_len >= MAX_GPS_LINE) {
            gps_line_len = 0;
            return;
        }
    }

    // If no data is received from the GPS module, insert a new $PGDOS frame
    if (send_size == 0 && gps_line_len == 0) {
        gps_run_pgdos(send_buf, &send_size, SEND_BUFFER_SIZE);
    }

    if (send_size) {
        cdcio_transmit(send_buf, (uint16_t)send_size);
    }

    send_size = 0;
    while (send_size < SEND_BUFFER_SIZE && fifo_read(&fifo_buffer_comm, &c)) {
        send_buf[send_size++] = c;
    }

    if (send_size) {
        while (huart3.gState != HAL_UART_STATE_READY)
            ;
        memcpy(comm_send_buf, send_buf, send_size);
        HAL_UART_Transmit_DMA(&huart3, comm_send_buf, send_size);
    }
}
