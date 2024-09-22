#define UNITY_INCLUDE_DOUBLE
#include <unity.h>
#include <array>
#include <vector>

#include "esp_log.h"
#include "driver/i2c_master.h"

#include "AT24C256.hpp"

extern "C" {
    void app_main(void);
}

i2c_master_bus_handle_t g_bus_handle;

void setUp(void) {
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

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &g_bus_handle));
}

void tearDown(void) {
    i2c_del_master_bus(g_bus_handle);
}

void test_AT24C256_simple_read_write()
{
    AT24C256 at24256(g_bus_handle, 0x51);

    TEST_ASSERT_TRUE(at24256.write(0x0212, 42));

    TEST_ASSERT_EQUAL(42, *at24256.read(0x0212));

    TEST_ASSERT_FALSE(at24256.write(0x8221, 42));

    auto res_read = at24256.read(0x822F);
    TEST_ASSERT_FALSE(res_read);
}

void test_AT24C256_multi_read_write()
{
    AT24C256 at24256(g_bus_handle, 0x51);

    std::array<uint8_t, 12> data;
    for(size_t i=0; i<data.size(); ++i)
    {
        data[i] = i;
    }

    uint16_t addr = 0x0AB0;

    at24256.write_page(addr, data.data(), data.size());

    std::vector<uint8_t> result = at24256.read<std::vector<uint8_t>>(addr, 12).value();

    TEST_ASSERT_EQUAL(data.size(), result.size());

    for(size_t i=0; i<data.size(); ++i)
    {
        TEST_ASSERT_EQUAL(data[i], result[i]);
    }

    bool res_write = at24256.write_page(0x822F, data.data(), data.size());
    TEST_ASSERT_FALSE(res_write);

    // Page overlap test
    // Page 5 = 0x0140 to 0x017F
    // Page 6 = 0x0180 to 0x01BF
    // Start 0x017D, size 5 = overlap 
    std::array<uint8_t, 5> data_overlap_pages;

    bool res_write_overlap = at24256.write_page(0x017D, data_overlap_pages.data(), data_overlap_pages.size());
    TEST_ASSERT_FALSE(res_write_overlap);

    auto res_read = at24256.read<std::vector<uint8_t>>(0x822F, 12);
    TEST_ASSERT_FALSE(res_read);
}

void test_AT24C256_multi_read_write_big()
{
    AT24C256 at24256(g_bus_handle, 0x51);

    std::array<uint8_t, 5> data;
    for(size_t i=0; i<data.size(); ++i)
    {
        data[i] = i;
    }

    // Page 5 to page 6 overlap
    bool res_write = at24256.write(0x017D, data.data(), data.size());
    TEST_ASSERT_TRUE(res_write);

    std::vector<uint8_t> result = at24256.read<std::vector<uint8_t>>(0x017D, 5).value();

    TEST_ASSERT_EQUAL(0, at24256.read(0x017D).value());
    TEST_ASSERT_EQUAL(1, at24256.read(0x017E).value());
    TEST_ASSERT_EQUAL(2, at24256.read(0x017F).value());
    TEST_ASSERT_EQUAL(3, at24256.read(0x0180).value());
    TEST_ASSERT_EQUAL(4, at24256.read(0x0181).value());

    std::array<uint8_t, 100> big_data;
    for(size_t i=0; i<big_data.size(); ++i)
    {
        big_data[i] = 120;
    }

    // Page 2 to page 4, 100 bytes write
    res_write = at24256.write(0x00B0, big_data.data(), big_data.size());
    TEST_ASSERT_TRUE(res_write);

    TEST_ASSERT_EQUAL(120, at24256.read(0x00B6).value()); // Page 2
    TEST_ASSERT_EQUAL(120, at24256.read(0x00DF).value()); // Page 3
    TEST_ASSERT_EQUAL(120, at24256.read(0x010F).value()); // Page 4

    result = at24256.read<std::vector<uint8_t>>(0x00B0, big_data.size()).value();
    
    TEST_ASSERT_EQUAL(big_data.size(), result.size());
    
    for(size_t i=0; i<big_data.size(); ++i)
    {
        TEST_ASSERT_EQUAL(big_data[i], result[i]);
    }

    TEST_ASSERT_TRUE(at24256.write(0x017D, std::vector<uint8_t>{0x10, 0x11, 0x12}));

    TEST_ASSERT_TRUE(at24256.write(0x019D, std::vector<uint8_t>{0x13, 0x14, 0x15}, 2));

    TEST_ASSERT_FALSE(at24256.write(0x019D, std::vector<uint8_t>{0x13, 0x14, 0x15, 0x16}, 5));

    auto result2 = at24256.read<std::vector<uint8_t>>(0x017D, 3);

    TEST_ASSERT_TRUE(result2);

    TEST_ASSERT_EQUAL(3, result2->size());
    for(size_t i=0; i<result2->size(); ++i)
    {
        TEST_ASSERT_EQUAL(0x10, (*result2)[0]);
        TEST_ASSERT_EQUAL(0x11, (*result2)[1]);
        TEST_ASSERT_EQUAL(0x12, (*result2)[2]);
    }

    std::array<uint8_t, 3> array;
    TEST_ASSERT_TRUE(at24256.read(0x017D, array, 3));

    for(size_t i=0; i<array.size(); ++i)
    {
        TEST_ASSERT_EQUAL(0x10, array[0]);
        TEST_ASSERT_EQUAL(0x11, array[1]);
        TEST_ASSERT_EQUAL(0x12, array[2]);
    }

    TEST_ASSERT_TRUE(at24256.read(0x017D, array));

    uint8_t raw[3];
    std::span span(raw);

    TEST_ASSERT_TRUE(at24256.read(0x017D, span));

    TEST_ASSERT_TRUE(at24256.write(0x018B, span));
}

void test_AT24C256_multi_read_write_edge()
{
    AT24C256 at24256(g_bus_handle, 0x51);

    std::array<uint8_t, 5> data;
    for(size_t i=0; i<data.size(); ++i)
    {
        data[i] = i+1;
    }

    // Last address: 0x7FFF
    
    bool res_write = at24256.write(0x7FFE, data.data(), data.size()); // Past last address
    TEST_ASSERT_FALSE(res_write);

    res_write = at24256.write(0x7FFA, data.data(), data.size()); // Right at the edge (last 5 bytes)
    TEST_ASSERT_TRUE(res_write);
}

void test_AT24C256_read_write_arbitrary_type()
{
    AT24C256 at24256(g_bus_handle, 0x51);

    TEST_ASSERT_TRUE(at24256.write(0x05A, 5.0f));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, at24256.read<float>(0x05A).value());

    TEST_ASSERT_TRUE(at24256.write(0x06E, 4800.84));
    TEST_ASSERT_EQUAL_DOUBLE(4800.84, at24256.read<double>(0x06E).value());

    struct S
    {
        int a;
        double b;
        long c;
        bool d;
        char s[5];
    };

    S s1{10, 42.356, 1345898, true, "abcd"};

    TEST_ASSERT_TRUE(at24256.write(0x10A, s1));

    S s2 = at24256.read<S>(0x10A).value();
    TEST_ASSERT_EQUAL_MEMORY(&s1, &s2, sizeof(S));
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_DEBUG);

    UNITY_BEGIN();

    RUN_TEST(test_AT24C256_simple_read_write);
    RUN_TEST(test_AT24C256_multi_read_write);
    RUN_TEST(test_AT24C256_multi_read_write_big);
    RUN_TEST(test_AT24C256_multi_read_write_edge);
    RUN_TEST(test_AT24C256_read_write_arbitrary_type);

    UNITY_END();
}
