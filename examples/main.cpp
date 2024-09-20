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
#include "I2CMemCard.hpp"

extern "C" {
    void app_main(void);
}

void vTaskLoop(void*);

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_DEBUG); // Enable debug logs to see operation details

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

    AT24C256 at24256(bus_handle, 0x50);

    // Single byte operations
    bool okay = at24256.write(0x0212, 42);

    std::expected<uint8_t, bool> expected_read_result = at24256.read(0x0212);

    uint8_t read_result = at24256.read(0x0212); // If you don't care about error check

    // Multi bytes operations
    std::array<uint8_t, 5> data{ 1, 2, 3, 4, 5 };

    bool res_write = at24256.write(0x017D, data.data(), data.size()); // std::vector<uint8_t> overload also provided

    std::vector<uint8_t> result = at24256.read(0x017D, 5).value(); // If you don't care about error check

    // or

    uint8_t raw_result[5];
    at24256.read(0x017D, raw_result, 5);

    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));

    xTaskCreate(vTaskLoop, "forever_loop", 2048, NULL, 5, NULL);
}

void vTaskLoop(void*)
{
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}   