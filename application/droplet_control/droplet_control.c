#include "droplet_control.h"
#include "main.h"

// 'pca9685' is the handle initialized in main.c
extern pca9685_handle_t pca9685;

// Clockwise path (0-indexed channels for PCA9685)
// Magnet array: [1,2,3,4; 5,6,7,8; 9,10,11,12; 13,14,15,16]
// Path: 1 -> 2 -> 3 -> 4 -> 8 -> 12 -> 16 -> 15 -> 14 -> 13 -> 9 -> 5 -> 1
const uint8_t clockwise_path[] = {0, 1, 2, 3, 7, 11, 15, 14, 13, 12, 8, 4};
const uint8_t path_len = sizeof(clockwise_path) / sizeof(clockwise_path[0]);

#define TRANSITION_STEPS 100 // Number of steps for smooth transition
#define STEP_DELAY_MS 10     // Delay in milliseconds for each step

/**
 * @brief Rotates the ferrofluid droplet clockwise on the electromagnet array.
 * 
 * This function continuously cycles through a predefined path of electromagnets,
 * creating a rotating magnetic field to move the droplet.
 * It uses a smooth transition by gradually decreasing the power of the current
 * electromagnet while increasing the power of the next one.
 */
void droplet_rotate_clockwise(void)
{
    uint8_t current_magnet_path_index = 0;
    uint8_t next_magnet_path_index = 1;

    // Ensure all magnets are off initially
    for (int i = 0; i < 16; i++) {
        pca9685_set_channel_duty_cycle(&pca9685, i, 0.0f, false);
    }

    while (1)
    {
        uint8_t current_magnet_channel = clockwise_path[current_magnet_path_index];
        uint8_t next_magnet_channel = clockwise_path[next_magnet_path_index];

        // Smooth transition from current magnet to the next
        for (int i = 0; i <= TRANSITION_STEPS; i++)
        {
            float next_strength = (float)i / TRANSITION_STEPS;
            float current_strength = 1.0f - next_strength;

            pca9685_set_channel_duty_cycle(&pca9685, current_magnet_channel, current_strength, false);
            pca9685_set_channel_duty_cycle(&pca9685, next_magnet_channel, next_strength, false);
            HAL_Delay(STEP_DELAY_MS);
        }

        // Move to the next segment of the path
        current_magnet_path_index = (current_magnet_path_index + 1) % path_len;
        next_magnet_path_index = (next_magnet_path_index + 1) % path_len;
    }
}

/**
 * @brief Sets all 16 electromagnets to 100% power.
 * 
 * This function is for testing the HAL layer and ensuring all electromagnets
 * can be activated at full strength.
 */
void test_all_magnets_on(void)
{
    for (int i = 0; i < 16; i++) {
        pca9685_set_channel_duty_cycle(&pca9685, i, 1.0f, false);
    }
}
