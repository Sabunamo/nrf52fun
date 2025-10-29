/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SPEAKER_H
#define SPEAKER_H

#include <zephyr/kernel.h>
#include <stdint.h>

/**
 * @brief Initialize the speaker PWM
 *
 * @return 0 on success, negative error code on failure
 */
int speaker_init(void);

/**
 * @brief Play a tone at a specific frequency
 *
 * @param frequency_hz Frequency in Hz (0 for silence)
 * @param duration_ms Duration in milliseconds
 */
void speaker_play_tone(uint32_t frequency_hz, uint32_t duration_ms);

/**
 * @brief Stop the speaker (silence)
 */
void speaker_stop(void);

/**
 * @brief Play the Athan (call to prayer) melody
 */
void speaker_play_athan(void);

#endif /* SPEAKER_H */
