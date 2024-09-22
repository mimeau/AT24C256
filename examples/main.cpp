#include <array>
#include <stdio.h>
#include <inttypes.h>
#include <vector>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>
#include "driver/i2c_master.h"

#include "AT24C256.hpp"

extern "C" {
    void app_main(void);
}

void vTaskLoop(void*);

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_DEBUG); // Enable debug logs to see operation details

    // Setup I2C bus
    i2c_master_bus_config_t i2c_mst_config = {
        .i2c_port = -1,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = { .enable_internal_pullup = true}
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    {
        AT24C256 at24256(bus_handle, 0x51); // Equivalent to AT24C256<true> (safe_mode enabled)
        // AT24C256 at24256<false>(bus_handle, 0x51); // safe_mode disabled

        if(at24256.write(0x0212, 42)) // Write one byte at specified address
        {
            // Read back what we wrote
            std::optional<uint8_t> value = at24256.read(0x0212);    // safe_mode enabled
            // uint8_t value = at24256.read(0x0212);                // safe_mode disabled
        }

        // Read / write any kind of data
        at24256.write(0x05A, 5.0f); // Write a float

        float f = at24256.read<float>(0x05A).value(); // Read a float, returns an std::optional<float> because safe_mode is on
        // float f = at24256.read<float>(0x05A); when safe_mode is off

        struct S
        {
            int a;
            double b;
            long c;
            bool d;
            char s[5];
        };

        S s1{10, 42.356, 1345898, true, "abcd"};

        at24256.write(0x10A, s1); // Write a whole struct (including its potential padding)

        S s2 = at24256.read<S>(0x10A).value(); // .value() because safe_mode returned an std::optional
        // S s2 = at24256.read<S>(0x10A); when safe_mode is off

        // Write multiple bytes
        at24256.write(0x017D, std::vector<uint8_t>{0x10, 0x11, 0x12}); // Accepts any contiguous and sized std::ranges, like std::vector

        at24256.write(0x017D, std::array<uint8_t, 3>{0x10, 0x11, 0x12}, 2); // Write only the first two elements

        // at24256.write(0x017D, std::vector<uint8_t>{0x10, 0x11, 0x12}, 5); // Error, size is too big

        // Write C-style array directly
        uint8_t data[3] = {0x10, 0x11, 0x12};
        at24256.write(0x017D, data, 3);

        // or through an std::span wrapper
        at24256.write(0x017D, std::span(data));

        // Read data into arbitrary contiguous sized container (must be able to construct the container using T t(size) where size is the amount of bytes to read)
        auto buffer = at24256.read<std::vector<uint8_t>>(0x017D, 3); // safe_mode enabled: returns an std::optional<std::vector<uint8_t>>
                                                                    // safe_mode disabled: returns an std::vector<uint8_t>

        // Read directly into the buffer
        std::array<uint8_t, 3> array;
        bool result = at24256.read(0x017D, array, 3);   // return success or failure

        // or, if the container is already at the right size, simply
        result = at24256.read(0x017D, array);

        // Works for arbitrary types
        at24256.write(0x20A, std::vector<float>{59.6, 12.44, 126.9, 0.00023});

        // Read back into an array, for instance
        auto vec2 = at24256.read<std::array<float, 4>>(0x20A).value(); // .value() because safe_mode returned an std::optional   
    }

    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle)); // Make sure to delete the I2C bus after all at24256 objects went out of scope / were deleted

    xTaskCreate(vTaskLoop, "forever_loop", 2048, NULL, 5, NULL);
}

void vTaskLoop(void*)
{
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}   