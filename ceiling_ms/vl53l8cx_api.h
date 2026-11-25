/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vl53l8cx_platform.h"


// ---- The platform specific configuration ----
// from:  jaremekg@JarPcWin11:~/project/pico2/work$ vim ./VL53L8CX_ULD_driver/Platform/platform.h +79
// and:   $ vim VL53L8CX_ULD_driver/CubeIDE_F401RE_Example/Core/Src/platform.c


// typedef struct
// {
//         /* To be filled with customer's platform. At least an I2C address/descriptor
//          * needs to be added */
//         /* Example for most standard platform : I2C address of sensor */
//     uint16_t                    address;

// } VL53L8CX_Platform;

/**
 * The macro below is used to define the number of target per zone sent
 * through I2C.
 * The value must be between 1 and 4.
 */
#define         VL53L8CX_NB_TARGET_PER_ZONE             1

/**
 * The macro below can be used to avoid data conversion into the driver.
 * By default there is a conversion between firmware and user data. Using this macro
 * allows to use the firmware format instead of user format. The firmware format allows
 * an increased precision.
 */
// #define      VL53L8CX_USE_RAW_FORMAT

/**
 * All macro below are used to configure the sensor output. User can
 * define some macros if he wants to disable selected output, in order to reduce
 * I2C access.
 */
// #define VL53L8CX_DISABLE_AMBIENT_PER_SPAD
// #define VL53L8CX_DISABLE_NB_SPADS_ENABLED
// #define VL53L8CX_DISABLE_NB_TARGET_DETECTED
// #define VL53L8CX_DISABLE_SIGNAL_PER_SPAD
// #define VL53L8CX_DISABLE_RANGE_SIGMA_MM
// #define VL53L8CX_DISABLE_DISTANCE_MM
// #define VL53L8CX_DISABLE_REFLECTANCE_PERCENT
// #define VL53L8CX_DISABLE_TARGET_STATUS
// #define VL53L8CX_DISABLE_MOTION_INDICATOR

uint8_t RdByte(VL53L8CX_Platform *p_platform, uint16_t RegisterAdress,
                uint8_t *p_value);
                
uint8_t WrByte(VL53L8CX_Platform *p_platform, uint16_t RegisterAdress,
                uint8_t value);

uint8_t RdMulti(VL53L8CX_Platform *p_platform, uint16_t RegisterAdress,
                uint8_t *p_values, uint32_t size);

uint8_t WrMulti(VL53L8CX_Platform *p_platform, uint16_t RegisterAdress,
                uint8_t *p_values, uint32_t size);


/**
 * Mandatory function, used to swap a buffer. The buffer size is always a
 * multiple of 4 (4, 8, 12, 16, ...).
 * @param (uint8_t*) buffer : Buffer to swap, generally uint32_t
 * @param (uint16_t) size : Buffer size to swap
 */
void SwapBuffer(uint8_t *buffer, uint16_t size);





// ---- the main API ----
/*
Macro VL53L8CX_RESOLUTION_4X4 or VL53L8CX_RESOLUTION_8X8 allows
 setting sensor in 4x4 mode or 8x8 mode, using function
 vl53l8cx_set_resolution().
*/
#define VL53L8CX_RESOLUTION_4X4     16
#define VL53L8CX_RESOLUTION_8X8     64

/* 
Macro VL53L8CX_TARGET_ORDER_STRONGEST or VL53L8CX_TARGET_ORDER_CLOSEST
      are used to select the target order for data output.
 */
#define VL53L8CX_TARGET_ORDER_CLOSEST           1
#define VL53L8CX_TARGET_ORDER_STRONGEST         2

/* 
Macro VL53L8CX_RANGING_MODE_CONTINUOUS and
 VL53L8CX_RANGING_MODE_AUTONOMOUS are used to change the ranging mode.
 Autonomous mode can be used to set a precise integration time, whereas
 continuous is always maximum.
 */
#define VL53L8CX_RANGING_MODE_CONTINUOUS       1
#define VL53L8CX_RANGING_MODE_AUTONOMOUS       3

/* 
The default power mode is VL53L8CX_POWER_MODE_WAKEUP. User can choose two
 different modes to save power consumption when is the device
 is not used:
 - VL53L8CX_POWER_MODE_SLEEP: This mode retains the firmware and the configuration. It
 is recommended when the device needs to quickly wake-up.
 - VL53L8CX_POWER_MODE_DEEP_SLEEP: This mode clears all memory, by consequence the firmware,
 the configuration and the calibration are lost. It is recommended when the device sleeps during
 a long time as it consumes a very low current consumption.
 Both modes can be changed using function vl53l8cx_set_power_mode().
 */
#define VL53L8CX_POWER_MODE_SLEEP           0
#define VL53L8CX_POWER_MODE_WAKEUP          1
#define VL53L8CX_POWER_MODE_DEEP_SLEEP      2

/* 
Macro VL53L8CX_STATUS_ERROR indicates that something is wrong (value,
 I2C access, ...). Macro VL53L8CX_MCU_ERROR is used to indicate a MCU issue.
 */

#define VL53L8CX_STATUS_OK                      0
#define VL53L8CX_STATUS_TIMEOUT_ERROR           1
#define VL53L8CX_STATUS_CORRUPTED_FRAME         2
#define VL53L8CX_STATUS_LASER_SAFETY            3
#define VL53L8CX_STATUS_XTALK_FAILED            4
#define VL53L8CX_STATUS_FW_CHECKSUM_FAIL        5
#define VL53L8CX_MCU_ERROR                     66
#define VL53L8CX_STATUS_INVALID_PARAM         127
#define VL53L8CX_STATUS_ERROR                 255

/* 
Definitions for Range results block headers
 */
#if VL53L8CX_NB_TARGET_PER_ZONE == 1

#define VL53L8CX_START_BH                       ((uint32_t)0x0000000D)
#define VL53L8CX_METADATA_BH                    ((uint32_t)0x54B400C0)
#define VL53L8CX_COMMONDATA_BH                  ((uint32_t)0x54C00040)
#define VL53L8CX_AMBIENT_RATE_BH                ((uint32_t)0x54D00104)
#define VL53L8CX_SPAD_COUNT_BH                  ((uint32_t)0x55D00404)
#define VL53L8CX_NB_TARGET_DETECTED_BH          ((uint32_t)0xDB840401)
#define VL53L8CX_SIGNAL_RATE_BH                 ((uint32_t)0xDBC40404)
#define VL53L8CX_RANGE_SIGMA_MM_BH              ((uint32_t)0xDEC40402)
#define VL53L8CX_DISTANCE_BH                    ((uint32_t)0xDF440402)
#define VL53L8CX_REFLECTANCE_BH                 ((uint32_t)0xE0440401)
#define VL53L8CX_TARGET_STATUS_BH               ((uint32_t)0xE0840401)
#define VL53L8CX_MOTION_DETECT_BH               ((uint32_t)0xD85808C0)


#define VL53L8CX_METADATA_IDX                   ((uint16_t)0x54B4)
#define VL53L8CX_SPAD_COUNT_IDX                 ((uint16_t)0x55D0)
#define VL53L8CX_AMBIENT_RATE_IDX               ((uint16_t)0x54D0)
#define VL53L8CX_NB_TARGET_DETECTED_IDX         ((uint16_t)0xDB84)
#define VL53L8CX_SIGNAL_RATE_IDX                ((uint16_t)0xDBC4)
#define VL53L8CX_RANGE_SIGMA_MM_IDX             ((uint16_t)0xDEC4)
#define VL53L8CX_DISTANCE_IDX                   ((uint16_t)0xDF44)
#define VL53L8CX_REFLECTANCE_EST_PC_IDX         ((uint16_t)0xE044)
#define VL53L8CX_TARGET_STATUS_IDX              ((uint16_t)0xE084)
#define VL53L8CX_MOTION_DETEC_IDX               ((uint16_t)0xD858)

#else
#define VL53L8CX_START_BH                       ((uint32_t)0x0000000D)
#define VL53L8CX_METADATA_BH                    ((uint32_t)0x54B400C0)
#define VL53L8CX_COMMONDATA_BH                  ((uint32_t)0x54C00040)
#define VL53L8CX_AMBIENT_RATE_BH                ((uint32_t)0x54D00104)
#define VL53L8CX_NB_TARGET_DETECTED_BH          ((uint32_t)0x57D00401)
#define VL53L8CX_SPAD_COUNT_BH                  ((uint32_t)0x55D00404)
#define VL53L8CX_SIGNAL_RATE_BH                 ((uint32_t)0x58900404)
#define VL53L8CX_RANGE_SIGMA_MM_BH              ((uint32_t)0x64900402)
#define VL53L8CX_DISTANCE_BH                    ((uint32_t)0x66900402)
#define VL53L8CX_REFLECTANCE_BH                 ((uint32_t)0x6A900401)
#define VL53L8CX_TARGET_STATUS_BH               ((uint32_t)0x6B900401)
#define VL53L8CX_MOTION_DETECT_BH               ((uint32_t)0xCC5008C0)

#define VL53L8CX_METADATA_IDX                   ((uint16_t)0x54B4)
#define VL53L8CX_SPAD_COUNT_IDX                 ((uint16_t)0x55D0)
#define VL53L8CX_AMBIENT_RATE_IDX               ((uint16_t)0x54D0)
#define VL53L8CX_NB_TARGET_DETECTED_IDX         ((uint16_t)0x57D0)
#define VL53L8CX_SIGNAL_RATE_IDX                ((uint16_t)0x5890)
#define VL53L8CX_RANGE_SIGMA_MM_IDX             ((uint16_t)0x6490)
#define VL53L8CX_DISTANCE_IDX                   ((uint16_t)0x6690)
#define VL53L8CX_REFLECTANCE_EST_PC_IDX         ((uint16_t)0x6A90)
#define VL53L8CX_TARGET_STATUS_IDX              ((uint16_t)0x6B90)
#define VL53L8CX_MOTION_DETEC_IDX               ((uint16_t)0xCC50)
#endif

/* 
Inner Macro for API. Not for user, only for development.
 */
#define VL53L8CX_NVM_DATA_SIZE                  ((uint16_t)492)
#define VL53L8CX_CONFIGURATION_SIZE             ((uint16_t)972)
#define VL53L8CX_OFFSET_BUFFER_SIZE             ((uint16_t)488)
#define VL53L8CX_XTALK_BUFFER_SIZE              ((uint16_t)776)

#define VL53L8CX_DCI_ZONE_CONFIG                ((uint16_t)0x5450)
#define VL53L8CX_DCI_FREQ_HZ                    ((uint16_t)0x5458)
#define VL53L8CX_DCI_INT_TIME                   ((uint16_t)0x545C)
#define VL53L8CX_DCI_FW_NB_TARGET               ((uint16_t)0x5478)
#define VL53L8CX_DCI_RANGING_MODE               ((uint16_t)0xAD30)
#define VL53L8CX_DCI_DSS_CONFIG                 ((uint16_t)0xAD38)
#define VL53L8CX_DCI_VHV_CONFIG                 ((uint16_t)0xAD60)
#define VL53L8CX_DCI_TARGET_ORDER               ((uint16_t)0xAE64)
#define VL53L8CX_DCI_SHARPENER                  ((uint16_t)0xAED8)
#define VL53L8CX_DCI_INTERNAL_CP                ((uint16_t)0xB39C)
#define VL53L8CX_DCI_SYNC_PIN                   ((uint16_t)0xB5F0)
#define VL53L8CX_DCI_MOTION_DETECTOR_CFG        ((uint16_t)0xBFAC)
#define VL53L8CX_DCI_SINGLE_RANGE               ((uint16_t)0xD964)
#define VL53L8CX_DCI_OUTPUT_CONFIG              ((uint16_t)0xD968)
#define VL53L8CX_DCI_OUTPUT_ENABLES             ((uint16_t)0xD970)
#define VL53L8CX_DCI_OUTPUT_LIST                ((uint16_t)0xD980)
#define VL53L8CX_DCI_PIPE_CONTROL               ((uint16_t)0xDB80)

#define VL53L8CX_UI_CMD_STATUS                  ((uint16_t)0x2C00)
#define VL53L8CX_UI_CMD_START                   ((uint16_t)0x2C04)
#define VL53L8CX_UI_CMD_END                     ((uint16_t)0x2FFF)

/* 
Inner values for API. Max buffer size depends of the selected output.
 */

#ifndef VL53L8CX_DISABLE_AMBIENT_PER_SPAD
#define L5CX_AMB_SIZE   260
#else
#define L5CX_AMB_SIZE   0
#endif

#ifndef VL53L8CX_DISABLE_NB_SPADS_ENABLED
#define L5CX_SPAD_SIZE  260
#else
#define L5CX_SPAD_SIZE  0
#endif

#ifndef VL53L8CX_DISABLE_NB_TARGET_DETECTED
#define L5CX_NTAR_SIZE  68
#else
#define L5CX_NTAR_SIZE  0
#endif

#ifndef VL53L8CX_DISABLE_SIGNAL_PER_SPAD
#define L5CX_SPS_SIZE ((256 * VL53L8CX_NB_TARGET_PER_ZONE) + 4)
#else
#define L5CX_SPS_SIZE   0
#endif


#ifndef VL53L8CX_DISABLE_RANGE_SIGMA_MM
#define L5CX_SIGR_SIZE ((128 * VL53L8CX_NB_TARGET_PER_ZONE) + 4)
#else
#define L5CX_SIGR_SIZE  0
#endif

#ifndef VL53L8CX_DISABLE_DISTANCE_MM
#define L5CX_DIST_SIZE ((128 * VL53L8CX_NB_TARGET_PER_ZONE) + 4)
#else
#define L5CX_DIST_SIZE  0
#endif

#ifndef VL53L8CX_DISABLE_REFLECTANCE_PERCENT
#define L5CX_RFLEST_SIZE ((64 *VL53L8CX_NB_TARGET_PER_ZONE) + 4)
#else
#define L5CX_RFLEST_SIZE        0
#endif

#ifndef VL53L8CX_DISABLE_TARGET_STATUS
#define L5CX_STA_SIZE ((64  *VL53L8CX_NB_TARGET_PER_ZONE) + 4)
#else
#define L5CX_STA_SIZE   0
#endif


#ifndef VL53L8CX_DISABLE_MOTION_INDICATOR
#define L5CX_MOT_SIZE   144
#else
#define L5CX_MOT_SIZE   0
#endif


/* 
Macro VL53L8CX_MAX_RESULTS_SIZE indicates the maximum size used by
 output through I2C. Value 40 corresponds to headers + meta-data + common-data
 and 20 corresponds to the footer.
 */
#define VL53L8CX_MAX_RESULTS_SIZE ( 40 \
        + L5CX_AMB_SIZE + L5CX_SPAD_SIZE + L5CX_NTAR_SIZE + L5CX_SPS_SIZE \
        + L5CX_SIGR_SIZE + L5CX_DIST_SIZE + L5CX_RFLEST_SIZE + L5CX_STA_SIZE \
        + L5CX_MOT_SIZE + 20)


/* 
Macro VL53L8CX_TEMPORARY_BUFFER_SIZE can be used to know the size of
 the temporary buffer. The minimum size is 1024, and the maximum depends of
 the output configuration.
 */
#if VL53L8CX_MAX_RESULTS_SIZE < 1024
#define VL53L8CX_TEMPORARY_BUFFER_SIZE ((uint32_t) 1024)
#else
#define VL53L8CX_TEMPORARY_BUFFER_SIZE ((uint32_t) VL53L8CX_MAX_RESULTS_SIZE)
#endif


/* 
Structure VL53L8CX_Configuration contains the sensor configuration.
 User MUST not manually change these field, except for the sensor address.
 */
typedef struct
{
        /* Platform, filled by customer into the 'platform.h' file */
        VL53L8CX_Platform       platform;
        /* Results streamcount, value auto-incremented at each range */
        uint8_t                 streamcount;
        /* Size of data read though I2C */
        uint32_t                data_read_size;
        /* Address of default configuration buffer */
        uint8_t                 *default_configuration;
        /* Address of default Xtalk buffer */
        uint8_t                 *default_xtalk;
        /* Offset buffer */
        uint8_t                 offset_data[VL53L8CX_OFFSET_BUFFER_SIZE];
        /* Xtalk buffer */
        uint8_t                 xtalk_data[VL53L8CX_XTALK_BUFFER_SIZE];
        /* Temporary buffer used for internal driver processing */
        uint8_t                 temp_buffer[VL53L8CX_TEMPORARY_BUFFER_SIZE];
        /* Auto-stop flag for stopping the sensor */
        uint8_t                         is_auto_stop_enabled;
} vl53l8cx_dev_t;
// #define VL53L8CX_Configuration vl53l8cx_dev_t
typedef vl53l8cx_dev_t VL53L8CX_Configuration;

/* 
Structure VL53L8CX_ResultsData contains the ranging results of
 VL53L8CX. If user wants more than 1 target per zone, the results can be split
 into 2 sub-groups :
 - Per zone results. These results are common to all targets (ambient_per_spad
 , nb_target_detected and nb_spads_enabled).
 - Per target results : These results are different relative to the detected
 target (signal_per_spad, range_sigma_mm, distance_mm, reflectance,
 target_status).
 */

typedef struct
{
        /* Internal sensor silicon temperature */
        int8_t silicon_temp_degc;

        /* Ambient noise in kcps/spads */
#ifndef VL53L8CX_DISABLE_AMBIENT_PER_SPAD
        uint32_t ambient_per_spad[VL53L8CX_RESOLUTION_8X8];
#endif

        /* Number of valid target detected for 1 zone */
#ifndef VL53L8CX_DISABLE_NB_TARGET_DETECTED
        uint8_t nb_target_detected[VL53L8CX_RESOLUTION_8X8];
#endif

        /* Number of spads enabled for this ranging */
#ifndef VL53L8CX_DISABLE_NB_SPADS_ENABLED
        uint32_t nb_spads_enabled[VL53L8CX_RESOLUTION_8X8];
#endif

        /* Signal returned to the sensor in kcps/spads */
#ifndef VL53L8CX_DISABLE_SIGNAL_PER_SPAD
        uint32_t signal_per_spad[(VL53L8CX_RESOLUTION_8X8
                                        *VL53L8CX_NB_TARGET_PER_ZONE)];
#endif

        /* Sigma of the current distance in mm */
#ifndef VL53L8CX_DISABLE_RANGE_SIGMA_MM
        uint16_t range_sigma_mm[(VL53L8CX_RESOLUTION_8X8
                                        *VL53L8CX_NB_TARGET_PER_ZONE)];
#endif

        /* Measured distance in mm */
#ifndef VL53L8CX_DISABLE_DISTANCE_MM
        int16_t distance_mm[(VL53L8CX_RESOLUTION_8X8
                                        *VL53L8CX_NB_TARGET_PER_ZONE)];
#endif

        /* Estimated reflectance in percent */
#ifndef VL53L8CX_DISABLE_REFLECTANCE_PERCENT
        uint8_t reflectance[(VL53L8CX_RESOLUTION_8X8
                                        *VL53L8CX_NB_TARGET_PER_ZONE)];
#endif

        /* Status indicating the measurement validity (5 & 9 means ranging OK)*/
#ifndef VL53L8CX_DISABLE_TARGET_STATUS
        uint8_t target_status[(VL53L8CX_RESOLUTION_8X8
                                        *VL53L8CX_NB_TARGET_PER_ZONE)];
#endif


        /* Motion detector results */
#ifndef VL53L8CX_DISABLE_MOTION_INDICATOR
        struct
        {
                uint32_t global_indicator_1;
                uint32_t global_indicator_2;
                uint8_t  status;
                uint8_t  nb_of_detected_aggregates;
                uint8_t  nb_of_aggregates;
                uint8_t  spare;
                uint32_t motion[32];
        } motion_indicator;
#endif

} vl53l8cx_results_data_t;
// #define VL53L8CX_ResultsData vl53l8cx_results_data_t
typedef vl53l8cx_results_data_t VL53L8CX_ResultsData;

union Block_header {
        uint32_t bytes;
        struct {
                uint32_t type : 4;
                uint32_t size : 12;
                uint32_t idx : 16;
        };
};

uint8_t vl53l8cx_is_alive( VL53L8CX_Configuration *p_dev, uint8_t *p_is_alive);


/**
 * Mandatory function used to initialize the sensor. This function must
 * be called after a power on, to load the firmware into the VL53L8CX. It takes
 * a few hundred milliseconds.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @return (uint8_t) status : 0 if initialization is OK.
 */
uint8_t vl53l8cx_init(
                VL53L8CX_Configuration          *p_dev);


// jaremekg@JarPcWin11:~/project/pico2/work$
//  vim VL53L8CX_ULD_driver/VL53L8CX_ULD_API/inc/vl53l8cx_api.h +410

/**
 * This function is used to get the current sensor power mode.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) *p_power_mode : Current power mode. The value of this
 * pointer is equal to 0 if the sensor is in low power,
 * (VL53L8CX_POWER_MODE_SLEEP), or 1 if sensor is in standard mode
 * (VL53L8CX_POWER_MODE_WAKEUP).
 * @return (uint8_t) status : 0 if power mode is OK
 */
uint8_t vl53l8cx_get_power_mode(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_power_mode);

/**
 * This function is used to set the sensor in Low Power mode, for
 * example if the sensor is not used during a long time. The macro
 * VL53L8CX_POWER_MODE_SLEEP can be used to enable the low power mode. When user
 * want to restart the sensor, he can use macro VL53L8CX_POWER_MODE_WAKEUP.
 * Please ensure that the device is not streaming before calling the function.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) power_mode : Selected power mode (VL53L8CX_POWER_MODE_SLEEP
 * or VL53L8CX_POWER_MODE_WAKEUP)
 * @return (uint8_t) status : 0 if power mode is OK, or 127 if power mode
 * requested by user is not valid.
 */
uint8_t vl53l8cx_set_power_mode(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         power_mode);

/**
 * This function starts a ranging session. When the sensor streams, host
 * cannot change settings 'on-the-fly'.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @return (uint8_t) status : 0 if start is OK.
 */
uint8_t vl53l8cx_start_ranging(
                VL53L8CX_Configuration          *p_dev);

/**
 * This function stops the ranging session. It must be used when the
 * sensor streams, after calling vl53l8cx_start_ranging().
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @return (uint8_t) status : 0 if stop is OK
 */
uint8_t vl53l8cx_stop_ranging(
                VL53L8CX_Configuration          *p_dev);


/**
 * This function checks if a new data is ready by polling I2C. If a new
 * data is ready, a flag will be raised.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) *p_isReady : Value of this pointer be updated to 0 if data
 * is not ready, or 1 if a new data is ready.
 * 
*/
uint8_t vl53l8cx_check_data_ready(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_isReady);

/**
 * This function gets the ranging data, using the selected output and the
 * resolution.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (VL53L8CX_ResultsData) *p_results : VL53L5 results structure.
 * @return (uint8_t) status : 0 data are successfully get.
 */
uint8_t vl53l8cx_get_ranging_data(
                VL53L8CX_Configuration          *p_dev,
                VL53L8CX_ResultsData            *p_results);

/**
 * This function gets the current resolution (4x4 or 8x8).
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) *p_resolution : Value of this pointer will be equal to 16
 * for 4x4 mode, and 64 for 8x8 mode.
 * @return (uint8_t) status : 0 if resolution is OK.
 */
uint8_t vl53l8cx_get_resolution(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_resolution);

/**
 * This function sets a new resolution (4x4 or 8x8).
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) resolution : Use macro VL53L8CX_RESOLUTION_4X4 or
 * VL53L8CX_RESOLUTION_8X8 to set the resolution.
 * @return (uint8_t) status : 0 if set resolution is OK.
 */
uint8_t vl53l8cx_set_resolution(
                VL53L8CX_Configuration           *p_dev,
                uint8_t                         resolution);

/**
 * This function gets the current ranging frequency in Hz. Ranging
 * frequency corresponds to the time between each measurement.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) *p_frequency_hz: Contains the ranging frequency in Hz.
 * @return (uint8_t) status : 0 if ranging frequency is OK.
 */
uint8_t vl53l8cx_get_ranging_frequency_hz(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_frequency_hz);

/**
 * This function sets a new ranging frequency in Hz. Ranging frequency
 * corresponds to the measurements frequency. This setting depends of
 * the resolution, so please select your resolution before using this function.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) frequency_hz : Contains the ranging frequency in Hz.
 * - For 4x4, min and max allowed values are : [1;60]
 * - For 8x8, min and max allowed values are : [1;15]
 * @return (uint8_t) status : 0 if ranging frequency is OK, or 127 if the value
 * is not correct.
 */
uint8_t vl53l8cx_set_ranging_frequency_hz(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         frequency_hz);

/**
 * This function gets the current integration time in ms.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint32_t) *p_time_ms: Contains integration time in ms.
 * @return (uint8_t) status : 0 if integration time is OK.
 */
uint8_t vl53l8cx_get_integration_time_ms(
                VL53L8CX_Configuration          *p_dev,
                uint32_t                        *p_time_ms);

/**
 * This function sets a new integration time in ms. Integration time must
 * be computed to be lower than the ranging period, for a selected resolution.
 * Please note that this function has no impact on ranging mode continous.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint32_t) time_ms : Contains the integration time in ms. For all
 * resolutions and frequency, the minimum value is 2ms, and the maximum is
 * 1000ms.
 * @return (uint8_t) status : 0 if set integration time is OK.
 */
uint8_t vl53l8cx_set_integration_time_ms(
                VL53L8CX_Configuration          *p_dev,
                uint32_t                        integration_time_ms);

/**
 * This function gets the current sharpener in percent. Sharpener can be
 * changed to blur more or less zones depending of the application.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint32_t) *p_sharpener_percent: Contains the sharpener in percent.
 * @return (uint8_t) status : 0 if get sharpener is OK.
 */
uint8_t vl53l8cx_get_sharpener_percent(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_sharpener_percent);

/**
 * This function sets a new sharpener value in percent. Sharpener can be
 * changed to blur more or less zones depending of the application. Min value is
 * 0 (disabled), and max is 99.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint32_t) sharpener_percent : Value between 0 (disabled) and 99%.
 * @return (uint8_t) status : 0 if set sharpener is OK.
 */
uint8_t vl53l8cx_set_sharpener_percent(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         sharpener_percent);

/**
 * This function gets the current target order (closest or strongest).
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) *p_target_order: Contains the target order.
 * @return (uint8_t) status : 0 if get target order is OK.
 */
uint8_t vl53l8cx_get_target_order(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_target_order);

/**
 * This function sets a new target order. Please use macros
 * VL53L8CX_TARGET_ORDER_STRONGEST and VL53L8CX_TARGET_ORDER_CLOSEST to define
 * the new output order. By default, the sensor is configured with the strongest
 * output.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) target_order : Required target order.
 * @return (uint8_t) status : 0 if set target order is OK, or 127 if target
 * order is unknown.
 */
uint8_t vl53l8cx_set_target_order(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         target_order);

/**
 * This function is used to get the ranging mode. Two modes are
 * available using ULD : Continuous and autonomous. The default
 * mode is Autonomous.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) *p_ranging_mode : current ranging mode
 * @return (uint8_t) status : 0 if get ranging mode is OK.
 */
uint8_t vl53l8cx_get_ranging_mode(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_ranging_mode);

/**
 * This function is used to set the ranging mode. Two modes are
 * available using ULD : Continuous and autonomous. The default
 * mode is Autonomous.
 * @param (VL53L8CX_Configuration) *p_dev : VL53L8CX configuration structure.
 * @param (uint8_t) ranging_mode : Use macros VL53L8CX_RANGING_MODE_CONTINUOUS,
 * VL53L8CX_RANGING_MODE_CONTINUOUS.
 * @return (uint8_t) status : 0 if set ranging mode is OK.
 */
uint8_t vl53l8cx_set_ranging_mode(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         ranging_mode);

uint8_t vl53l8cx_get_external_sync_pin_enable(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *p_is_sync_pin_enabled);

uint8_t vl53l8cx_set_external_sync_pin_enable(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         enable_sync_pin);

uint8_t vl53l8cx_get_VHV_repeat_count(
                VL53L8CX_Configuration *p_dev,
                uint32_t *p_repeat_count);

uint8_t vl53l8cx_set_VHV_repeat_count(
                VL53L8CX_Configuration *p_dev,
                uint32_t repeat_count);

uint8_t vl53l8cx_dci_read_data(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *data,
                uint32_t                        index,
                uint16_t                        data_size);

uint8_t vl53l8cx_dci_write_data(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *data,
                uint32_t                        index,
                uint16_t                        data_size);

uint8_t vl53l8cx_dci_replace_data(
                VL53L8CX_Configuration          *p_dev,
                uint8_t                         *data,
                uint32_t                        index,
                uint16_t                        data_size,
                uint8_t                         *new_data,
                uint16_t                        new_data_size,
                uint16_t                        new_data_pos);

















