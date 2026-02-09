#pragma once

#include <stdint.h>
#include <stdbool.h>

/** Type definition for a SRAMLink Header. Always sent unecrypted.
 *  Header is the same for all versions of SRAMLink that currently
 *  exist.
 */
typedef struct
{
    uint32_t master_id;     /**< Master ID of the sending device */
    uint32_t device_id;     /**< Device ID of the sending device */
    uint8_t  device_type;   /**< Device type of the sending device */
    uint8_t  message_type;  /**< Message type of payload */

    // note that when this gets packed/unpacked into the message buffer,
    // it goes at the beginning of the message. The order in the struct
    // is to preserve memory and alignment of this struct in memory.
    uint8_t version;        /**< Version of the SRAMLink Protocol */
    bool    is_targeted;    /**< True if the message is targeted */
}slmsg_header_t;


/** Length of the header when packed */
#define SRAMLINK_HEADER_LENGTH slmsg_get_header_length()

/**
 *  Enumeration type definition for SRAMLink message types.
 */
typedef enum
{
    SLMSGTYPE_NOP                                 = 0,
    SLMSGTYPE_POWER_ON                            = 1,
    SLMSGTYPE_WAKE                                = 2,
    SLMSGTYPE_SYNC_ADVERTISE                      = 10,
    SLMSGTYPE_SYNC_ADVERTISE_ENCRYPTED            = 11,
    SLMSGTYPE_SYNC_BECOME_PAIRABLE                = 12,
    SLMSGTYPE_SYNC_PAIR                           = 13,
    SLMSGTYPE_SYNC_HOLD                           = 14,
    SLMSGTYPE_SYNC_FIRMWARE                       = 15,
    SLMSGTYPE_PUBLIC_KEY_SHARE                    = 16,
    SLMSGTYPE_NETWORK_SETTINGS                    = 17,
    SLMSGTYPE_DEVICE_INFO                         = 18,
    SLMSGTYPE_BUTTONS                             = 20,
    SLMSGTYPE_STATUS_REQUEST                      = 21,
    SLMSGTYPE_STATUS_REPORT                       = 22,
    SLMSGTYPE_DIRECT_ADJUST                       = 23,
    SLMSGTYPE_EXTENDED_STATUS                     = 24,
    SLMSGTYPE_MONITOR_VALUES                      = 25,
    SLMSGTYPE_MONITOR_KEYS                        = 26,
    SLMSGTYPE_DTYPE_ASSIGN                        = 27,
    SLMSGTYPE_DEVCOMM                             = 102,
    SLMSGTYPE_DEVCOMM_ENABLE                      = 103,
    SLMSGTYPE_ROSTER_LIST                         = 104,
    SLMSGTYPE_ROSTER_REQUEST                      = 105,
    SLMSGTYPE_DISCOVERY_REQUEST                   = 106,
    SLMSGTYPE_DISCOVERY_RESP                      = 107,
    SLMSGTYPE_VALUE_SET                           = 108,
    SLMSGTYPE_VALUE_GET                           = 109,
    SLMSGTYPE_VALUE_ACK                           = 110,
    SLMSGTYPE_NOTIFICATION                        = 111,
    SLMSGTYPE_DU_STATUS                           = 112,
    SLMSGTYPE_BATT_STATUS                         = 113,
    SLMSGTYPE_POWER_ON_PAIRING_STATUS             = 114,
    SLMSGTYPE_POWER_ON_PAIRING_INSTRUCTION        = 115,
    SLMSGTYPE_POWER_ON_PAIRING_PUBLIC_INSTRUCTION = 116,
    SLMSGTYPE_SMACK_MSG                           = 117,
    SLMSGTYPE_TIME_REPORT                         = 118,
    SLMSGTYPE_BATT_DEBUG_2                        = 119,
    SLMSGTYPE_BATT_DEBUG_1                        = 120,
    SLMSGTYPE_SPACEFORCE_STATE                    = 121,
    SLMSGTYPE_SPACEFORCE_ERROR_SUBSYS             = 122,
    SLMSGTYPE_SPACEFORCE_ERROR_LIST               = 123,
    SLMSGTYPE_DU_STATUS_CALIBRATION               = 125,
    SLMSGTYPE_SPACEFORCE_PD_STATUS                = 126,
    SLMSGTYPE_TEST                                = 127,
    SLMSGTYPE_AUTOSHIFT_LOG                       = 101,
    //no message types > 127, 0x80 is used for targeting flag
    SLMSGTYPE_INVALID                             = 255,
}slmsg_msg_type_t;

/**
 * Enumeration type definition for SRAMLink extended status event types.
 */
typedef enum
{
    EVT_TYPE_UNKNOWN,
    EVT_TYPE_POWER_ON,
    EVT_TYPE_WAKE,
    EVT_TYPE_BL_ENTRY,
}slmsg_evt_type_t;

/** @brief Bits for flag byte of slmsg_v2_0_sync_advertise_t
 *  @name Sync Advertise Flag Bits
 *  @{
 */
/** @brief tells that device is in sync hold mode (times out after 2
    seconds if not refreshed by host) */
#define     SYNC_ADVERTISE_FLAG_HOLD_BIT            0
/** @brief tells if the user is currently pressing the sync button */
#define     SYNC_ADVERTISE_FLAG_SYNC_BIT            1
/** @brief tells that user released, then pressed the button again in
    hold mode (acknowledging that he's enabled FW upgrade) */
#define     SYNC_ADVERTISE_FLAG_USER_BIT            2
/** @brief set whenever the device updates its pairing information
    during sync */
#define     SYNC_ADVERTISE_FLAG_NEW_PAIRING_BIT     3
/** @} */

/** @brief Bits of button_mask in slmsg_v2_0_buttons_t
 *  @name 'button_mask' Bits
 *  @{
 */
/** @brief sender's button 0 is pressed */
#define     SHIFT_BUTTON_0_BIT                      0
/** @brief sender's button 1 is pressed */
#define     SHIFT_BUTTON_1_BIT                      1
/** @brief sender's button 2 is pressed */
#define     SHIFT_BUTTON_2_BIT                      2
/** @brief sender's button 3 is pressed */
#define     SHIFT_BUTTON_3_BIT                      3
/** @brief sender's button 4 is pressed */
#define     SHIFT_BUTTON_4_BIT                      4
/** @brief sender's button 5 is pressed */
#define     SHIFT_BUTTON_5_BIT                      5
/** @brief sender's mod button is pressed */
#define     MOD_BUTTON_PRESS_BIT                    6
/** @brief sender's calibration input 0 is active */
#define     SHIFT_CALIBRATE_0_BIT                   6
/** @brief sender's calibration input 1 is active */
#define     SHIFT_CALIBRATE_1_BIT                   7
/** @} */


/** @brief Masks for buttons in slmsg_v2_0_buttons_t
 *  @name 'button_mask' Masks
 *  @{
 */
/** @brief shifter's button 0 is pressed */
#define     SHIFT_BUTTON_0_MASK         (1 << SHIFT_BUTTON_0_BIT)
/** @brief shifter's button 1 is pressed */
#define     SHIFT_BUTTON_1_MASK         (1 << SHIFT_BUTTON_1_BIT)
/** @brief shifter's button 2 is pressed */
#define     SHIFT_BUTTON_2_MASK         (1 << SHIFT_BUTTON_2_BIT)
/** @brief shifter's button 3 is pressed */
#define     SHIFT_BUTTON_3_MASK         (1 << SHIFT_BUTTON_3_BIT)
/** @brief shifter's button 4 is pressed */
#define     SHIFT_BUTTON_4_MASK         (1 << SHIFT_BUTTON_4_BIT)
/** @brief shifter's button 5 is pressed */
#define     SHIFT_BUTTON_5_MASK         (1 << SHIFT_BUTTON_5_BIT)
/** @brief shifter's calibration input 0 is active */
#define     SHIFT_CALIBRATE_0_MASK      (1 << SHIFT_CALIBRATE_0_BIT)
/** @brief shifter's calibration input 1 is active */
#define     SHIFT_CALIBRATE_1_MASK      (1 << SHIFT_CALIBRATE_1_BIT)
/** @brief shifter's mod button is pressed */
#define     MOD_BUTTON_MASK             (1 << MOD_BUTTON_PRESS_BIT)
/** @} */

/** @brief bits for flags in slmsg_v2_0_der_status_report_t
 *  @name 'flags' bits
 *  @{
 */
/** @brief the device is currently moving toward the indicated position
    when this is set */
#define     DERAILLEUR_STATUS_SHIFT_BUSY_BIT        0
/** @brief indicates a device is about to enter bl for FW update */
#define     BLE_EVNT_JUMPING_TO_BL_ACKNOWLEDGE_BIT    1
/** @} */

/** @brief Bits for flags in slmsg_v1_2_device_adjusment_t.
 *  @name Device Adjust flag bits
 *  @{
 */
/** @brief set to adjust the gear */
#define     DIRECT_ADJUST_V1_2_FLAG_ADJ_GEAR_BIT          0
/** @brief if adjusting the gear, set to indicate relative adjustment,
    otherwise absolute */
#define     DIRECT_ADJUST_V1_2_FLAG_GEAR_RELATIVE_BIT     1
/** @brief set to adjust the trim */
#define     DIRECT_ADJUST_V1_2_FLAG_ADJ_TRIM_BIT          2
/** @brief if adjusting the trim, set to indicate relative adjustment,
    otherwise absolute */
#define     DIRECT_ADJUST_V1_2_FLAG_TRIM_RELATIVE_BIT     3
/** @} */

/** @brief Bits for flags in slmsg_v2_0_device_adjusment_t.
 *  @name Device Adjust flag bits
 *  @{
 */
/** @brief set to indicate adjustment should be acted on */
#define     DIRECT_ADJUST_V2_0_FLAG_ACTIVE_BIT            0

/** @brief set to indicate adjustment is relative */
#define     DIRECT_ADJUST_V2_0_FLAG_RELATIVE_BIT          1
/** @} */


#define MAX_ADJUSTMENTS 4   /**< maximum number of adjustments elements
                                 we'll support */
#define DEVCOMM_PAYLOAD_MAX_LENGTH 100

#define MAX_NUM_MONITOR_VALUES 8 /** maximum number of values to send in
                                  monitor message */

/**
 * Maximum SMACK fragment size for transport over SRAMLink
 *
 * This is set to maintain a balance between TX and RX intervals.
 * Larger values tend to overrun the RX time, reducing the full-duplex
 * capabilities of the protocol.
 */
#define SRAMLINK_SMACK_MTU  43

/** @brief Bits of event flags in slmsg_v2_0_spaceforce_state_t
 *  @name 'event_flag' Bits
 *  @{
 */
/** @brief magnet is detected - battery insertion (set by battery) */
#define     BMS_MAGNET_DETECTION_BIT 0
/** @brief bms state monitor disable (set by battery) */
#define     BMS_STATE_MONITOR_DISABLED_BIT 1
/** @brief sfu mode has been requested by bms */
#define     BMS_SFU_REQUESTED_BIT 2

/** @brief du idle (set by du) */
#define     DU_IDLE_BIT 0
/** @brief du state monitor disable (set by du) */
#define     DU_STATE_MONITOR_DISABLED_BIT 1
/** @brief sfu mode has been requested by du */
#define     DU_SFU_REQUESTED_BIT 2

/** @brief power button has been pressed (set by display) */
#define     DISPLAY_POWER_BTN_BIT 0
/** @brief diagnostic mode has been requested by display */
#define     DISPLAY_DIAGNOSTIC_BIT 1
/** @brief display state monitor disable (set by display) */
#define     DISPLAY_STATE_MONITOR_DISABLED_BIT 2
/** @brief charger has been connected to the display */
#define     DISPLAY_CHARGER_CONNECTED_BIT 3
/** @brief sfu mode has been requested by display */
#define     DISPLAY_SFU_REQUESTED_BIT 4

/** @} */

/** @brief Masks for event flags in slmsg_v2_0_spaceforce_state_t
 *  @name 'event_flag' Masks
 *  @{
 */
/** @brief magnet is detected - battery insertion (set by battery) */
#define     BMS_MAGNET_DETECTION_MASK (1 << BMS_MAGNET_DETECTION_BIT)
/** @brief bms state monitor disable (set by bms) */
#define     BMS_STATE_MONITOR_DISABLED_MASK (1 << \
                                             BMS_STATE_MONITOR_DISABLED_BIT)
/** @brief sfu has been requested by bms */
#define     BMS_SFU_REQUESTED_MASK (1 << BMS_SFU_REQUESTED_BIT)

/** @brief du idle (set by du) */
#define     DU_IDLE_MASK (1 << DU_IDLE_BIT)
/** @brief du state monitor disable (set by du) */
#define     DU_STATE_MONITOR_DISABLED_MASK (1 << DU_STATE_MONITOR_DISABLED_BIT)
/** @brief sfu has been requested by du */
#define     DU_SFU_REQUESTED_MASK (1 << DU_SFU_REQUESTED_BIT)

/** @brief power button has been pressed (set by display) */
#define     POWER_BTN_MASK (1 << DISPLAY_POWER_BTN_BIT)
/** @brief power button has been pressed (set by display) */
#define     DISPLAY_DIAGNOSTIC_MASK (1 << DISPLAY_DIAGNOSTIC_BIT)
/** @brief display state monitor disable (set by display) */
#define     DISPLAY_STATE_MONITOR_DISABLED_MASK (1 << \
                                                 DISPLAY_STATE_MONITOR_DISABLED_BIT)
/** @brief charger has been connected to the display (set by display) */
#define     DISPLAY_CHARGER_CONNECTED_MASK (1 << DISPLAY_CHARGER_CONNECTED_BIT)
/** @brief sfu has been requested by display */
#define     DISPLAY_SFU_REQUESTED_MASK (1 << DISPLAY_SFU_REQUESTED_BIT)
/** @} */


/** @brief Bits to indicate flex submenu status (set by DU)
 *  @name 'flex submenu' Bits
 *  @{
 */
/** @brief flex adjust submenu */
#define SUBMENU_FLEX_ADJUST_BIT 0
/** @brief autoshift submenu */
#define SUBMENU_AUTOSHIFT_BIT 1
/** @brief autoshift adjust submenu */
#define SUBMENU_AUTOSHIFT_ADJUST_BIT 2
/** @brief no active submenu */
#define SUBMENU_NONE_BIT 3
/** @} */

/** @brief Masks for submenu status (set by DU)
 *  @name 'submenu' Masks
 *  @{
 */
/** @brief flex adjust submenu */
#define SUBMENU_FLEX_ADJUST_MASK (1 << SUBMENU_FLEX_ADJUST_BIT)
/** @brief autoshift submenu */
#define SUBMENU_AUTOSHIFT_MASK (1 << SUBMENU_AUTOSHIFT_BIT)
/** @brief autoshift adjust submenu */
#define SUBMENU_AUTOSHIFT_ADJUST_MASK (1 << SUBMENU_AUTOSHIFT_ADJUST_BIT)
/** @brief no active submenu */
#define SUBMENU_NONE_MASK (1 << SUBMENU_NONE_BIT)
/** @} */


/** @brief Bits to indicate derating status
 *  @name 'derating_flag' Bits
 *  @{
 */
/** @brief pcba temperature derating */
#define PCBA_TEMP_DERATING_BIT 7
/** @brief windings temperature derating */
#define WINDINGS_TEMP_DERATING_BIT 6
/** @brief imax derating */
#define IMAX_DERATING_BIT 5
/** @} */


/** @brief Masks for derating status
 *  @name 'derating_flag' Masks
 *  @{
 */
/** @brief pcba temperature derating */
#define PCBA_TEMP_DERATING_MASK (1 << PCBA_TEMP_DERATING_BIT)
/** @brief windings temperature derating */
#define WINDINGS_TEMP_DERATING_MASK (1 << WINDINGS_TEMP_DERATING_BIT)
/** @brief imax derating */
#define IMAX_DERATING_MASK (1 << IMAX_DERATING_BIT)
/** @brief all du derating */
#define DU_DERATING_MASK (PCBA_TEMP_DERATING_MASK | \
                          WINDINGS_TEMP_DERATING_MASK | IMAX_DERATING_MASK)
/** @} */


/** @brief Mask for assist1 ridemode status
 *  @name 'ride_mode' Mask
 *  @{
 */
/** @brief ride mode */
#define RIDE_MODE_MASK ~(DU_DERATING_MASK)
/** @} */
