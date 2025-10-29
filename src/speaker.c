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

#define PWM_SPEAKER_NODE DT_NODELABEL(speaker)

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
    LOG_INF("Playing Athan melody...");

    for (int i = 0; i < athan_notes; i++) {
        speaker_play_tone(athan_melody[i], athan_durations[i]);

        // Pause between notes (20% longer than note duration)
        k_msleep(athan_durations[i] / 5);
    }

    speaker_stop();
    LOG_INF("Athan melody complete.");
}
