/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "speaker.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(speaker, LOG_LEVEL_INF);

// Board-specific speaker node selection
#if DT_NODE_EXISTS(DT_NODELABEL(speaker_pwm))
    #define PWM_SPEAKER_NODE DT_NODELABEL(speaker_pwm)
#elif DT_NODE_EXISTS(DT_NODELABEL(speaker))
    #define PWM_SPEAKER_NODE DT_NODELABEL(speaker)
#else
    #error "No speaker PWM node found (speaker_pwm or speaker)"
#endif

static const struct pwm_dt_spec speaker_pwm = PWM_DT_SPEC_GET(PWM_SPEAKER_NODE);

// Athan-like melody (frequencies in Hz)
static const int athan_melody[] = { 440, 494, 523, 440, 392, 440, 523, 494 };
static const int athan_durations[] = { 500, 500, 500, 500, 500, 500, 500, 500 }; // milliseconds
static const int athan_notes = 8;

int speaker_init(void)
{
    if (!device_is_ready(speaker_pwm.dev)) {
        LOG_ERR("PWM device %s is not ready", speaker_pwm.dev->name);
        return -ENODEV;
    }

    LOG_INF("Speaker initialized on P0.12");
    return 0;
}

void speaker_play_tone(uint32_t frequency_hz, uint32_t duration_ms)
{
    if (frequency_hz == 0) {
        // Silence - turn off PWM
        pwm_set_pulse_dt(&speaker_pwm, 0);
        k_msleep(duration_ms);
        return;
    }

    // Calculate period in nanoseconds from frequency
    // Period = 1 / frequency (in seconds) = 1,000,000,000 / frequency (in nanoseconds)
    uint32_t period_ns = 1000000000U / frequency_hz;

    // 50% duty cycle for square wave
    uint32_t pulse_ns = period_ns / 2;

    int ret = pwm_set_dt(&speaker_pwm, period_ns, pulse_ns);
    if (ret) {
        LOG_ERR("Error setting PWM: %d", ret);
        return;
    }

    k_msleep(duration_ms);
}

void speaker_stop(void)
{
    pwm_set_pulse_dt(&speaker_pwm, 0);
}

void speaker_play_athan(void)
{
    LOG_INF("Playing Athan melody (repeating for 1 minute)...");

    // Calculate melody duration (sum of all note durations + pauses)
    int melody_duration_ms = 0;
    for (int i = 0; i < athan_notes; i++) {
        melody_duration_ms += athan_durations[i];
        melody_duration_ms += athan_durations[i] / 5;  // Add pause between notes
    }

    // Repeat for approximately 1 minute (60,000 ms)
    const int total_duration_ms = 60000;
    const int pause_between_repeats_ms = 3000;  // 3 second pause
    int elapsed_ms = 0;

    while (elapsed_ms < total_duration_ms) {
        // Play the melody once
        for (int i = 0; i < athan_notes; i++) {
            speaker_play_tone(athan_melody[i], athan_durations[i]);

            // Pause between notes (20% longer than note duration)
            k_msleep(athan_durations[i] / 5);
        }

        elapsed_ms += melody_duration_ms;

        // Add 3 second pause between repetitions (if not finished yet)
        if (elapsed_ms < total_duration_ms) {
            speaker_stop();
            k_msleep(pause_between_repeats_ms);
            elapsed_ms += pause_between_repeats_ms;
            LOG_INF("Repeating Athan melody... (%d ms elapsed)", elapsed_ms);
        }
    }

    speaker_stop();
    LOG_INF("Athan melody complete (1 minute).");
}
