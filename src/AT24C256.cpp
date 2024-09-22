#include "AT24C256.hpp"

#include <array>
#include <vector>
#include "freertos/FreeRTOS.h"

template<bool safe_mode>
AT24C256<safe_mode>::AT24C256(i2c_master_bus_handle_t bus, uint8_t address) : _address(address)
{
    _dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 0,
        .flags = { .disable_ack_check = false }
    };

    ESP_LOGD("AT24C256::AT24C256", "[0x%02x] - Registering device", _address);
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &_dev_cfg, &_dev_handle));
}

template<bool safe_mode>
AT24C256<safe_mode>::AT24C256(AT24C256<safe_mode> &&other)
{
    _address = other._address;
    other._address = 0;

    _dev_handle = other._dev_handle;
    other._dev_handle = nullptr;
}

template<bool safe_mode>
AT24C256<safe_mode>::~AT24C256()
{
    if(_dev_handle)
    {
        ESP_LOGD("AT24C256::~AT24C256", "[0x%02x] - Deregistering device", _address);
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(_dev_handle));
    } 
}

template<bool safe_mode>
AT24C256<safe_mode>& AT24C256<safe_mode>::operator=(AT24C256<safe_mode> &&other)
{
    _address = other._address;
    other._address = 0;

    _dev_handle = other._dev_handle;
    other._dev_handle = nullptr;

    return *this;
}

template<bool safe_mode>
bool AT24C256<safe_mode>::write(uint16_t address, uint8_t byte) const
{
    if constexpr (safe_mode)
    {
        if(address > MEMORY_SIZE-1)
        {
            ESP_LOGE("AT24C256::write", "[0x%02x] - Address 0x%02x is too big", _address, address);
            return false;
        }
    }

    std::array<uint8_t, 3> payload{
        (uint8_t)(address >> 8),        // First word address
        (uint8_t)address,               // Second word address
        byte  
    };

    esp_err_t err = i2c_master_transmit(_dev_handle, payload.data(), payload.size(), -1);
    
    if (err != ESP_OK) 
    {
        ESP_LOGD("AT24C256::write", "[0x%02x] - Write failed: [%u] %s", _address, err, esp_err_to_name(err));
        return false;
    }

    ESP_LOGD("AT24C256::write", "[0x%02x] - Wrote byte 0x%02x @ 0x%04x", _address, byte, address);
    vTaskDelay(25 / portTICK_PERIOD_MS);  // 25 milliseconds

    return true;
}

template<bool safe_mode>
bool AT24C256<safe_mode>::write(uint16_t address, uint8_t* buffer, uint16_t size) const
{
    if constexpr (safe_mode)
    {
        if((address+size) > MEMORY_SIZE-1)
        {
            ESP_LOGE("AT24C256::write", "[0x%02x] - Address 0x%02x is too big (max address: 0x%02x)", _address, address, MEMORY_SIZE-1);
            return false;
        }
    }

    ESP_LOGD("AT24C256::write", "[0x%02x] - Size: %u", _address, size);
    ESP_LOG_BUFFER_HEXDUMP("AT24C256::write", buffer, size, ESP_LOG_DEBUG);

    uint16_t start_page = address >> 6;
    uint16_t end_page = (address+size-1) >> 6;

    uint16_t page_count = (end_page - start_page) + 1;

    uint16_t remaining_size = size;
    uint16_t current_addr = address;
    uint8_t* current_buffer = buffer;

    ESP_LOGD("AT24C256::write", "[0x%02x] - start_page: %u, end_page: %u, page_count: %u", _address, start_page, end_page, page_count);

    for(int i=0; i<page_count; ++i)
    {
        ESP_LOGD("AT24C256::write", "[0x%02x] - remaining_size: %u, current_addr: Ox%02x, current_buffer: %p", _address, remaining_size, current_addr, current_buffer);

        uint16_t next_page_addr = ((start_page+i+1) << 6);
        uint16_t page_byte_remaining = next_page_addr - current_addr;
        uint8_t byte_count = std::min(remaining_size, page_byte_remaining);

        bool result = write_page(current_addr, current_buffer, byte_count);

        if(!result)
        {
            ESP_LOGE("AT24C256::write", "[0x%02x] - Write failled - remaining_size: %u, current_addr: Ox%02x, current_buffer: %p", _address, remaining_size, current_addr, current_buffer);
            return false;
        }

        remaining_size -= byte_count;
        current_addr += byte_count;
        current_buffer += byte_count;
    }

    return true;
}

template<bool safe_mode>
bool AT24C256<safe_mode>::write_page(uint16_t address, uint8_t* buffer, uint16_t size) const
{
    if constexpr (safe_mode)
    {
        if(address & 0x8000) [[unlikely]]
        {
            ESP_LOGE("AT24C256::write_page", "[0x%02x] - Address 0x%02x is too big", _address, address);
            return false;
        }

        if( (address >> 6) != ((address+size-1) >> 6)) [[unlikely]]
        {
            ESP_LOGE("AT24C256::write_page", "[0x%02x] - Write overlap page border", _address);
            return false;
        }
    }

    std::vector<uint8_t> payload(2+size);
    payload[0] = (uint8_t)(address >> 8);
    payload[1] = (uint8_t)address;
    std::copy(buffer, buffer+size, payload.begin()+2);

    ESP_LOG_BUFFER_HEXDUMP("AT24C256::write_page", payload.data(), payload.size(), ESP_LOG_DEBUG);

    esp_err_t err = i2c_master_transmit(_dev_handle, payload.data(), payload.size(), -1);
    if (err != ESP_OK) 
    {
        ESP_LOGD("AT24C256::write_page", "[0x%02x] - Multi-write failed: [%u] %s", _address, err, esp_err_to_name(err));
        return false;
    }

    ESP_LOGD("AT24C256::write_page", "[0x%02x] - Wrote %u bytes @ 0x%04x", _address, size, address);
    vTaskDelay(25 / portTICK_PERIOD_MS);  // 25 milliseconds

    return true;
}

template<bool safe_mode>
auto AT24C256<safe_mode>::read(uint16_t address) const -> typename std::conditional<safe_mode, std::optional<uint8_t>, uint8_t>::type
{
    if constexpr (safe_mode)
    {
        if(address & 0x8000)
        {
            ESP_LOGE("AT24C256::write", "[0x%02x] - Address 0x%02x is too big", _address, address);
            return std::nullopt;
        }
    }

    uint8_t data = 0;

    std::array<uint8_t, 2> payload{
        (uint8_t)(address >> 8),            // First word address
        (uint8_t)address,                   // Second word address
    };

    [[maybe_unused]] esp_err_t err = i2c_master_transmit_receive(_dev_handle, payload.data(), payload.size(), &data, 1, -1);

    if constexpr (safe_mode)
    {
        if (err != ESP_OK) 
        {
            ESP_LOGD("AT24C256::read", "[0x%02x] - Read failed @ 0x%04x: [%u] %s", _address, address, err, esp_err_to_name(err));
            return std::nullopt;
        }
    }

    ESP_LOGD("AT24C256::read", "[0x%02x] - Read byte 0x%02x @ 0x%04x", _address, data, address);
    vTaskDelay(25 / portTICK_PERIOD_MS);  // 25 milliseconds

    return data;    
}

template<bool safe_mode>
bool AT24C256<safe_mode>::read(uint16_t address, uint8_t* buffer, size_t size) const
{
    if constexpr (safe_mode)
    {
        if(address & 0x8000)
        {
            ESP_LOGE("AT24C256::write", "[0x%02x] - Address 0x%02x is too big", _address, address);
            return false;
        }
    }

    std::array<uint8_t, 2> payload{
        (uint8_t)(address >> 8),            // First word address
        (uint8_t)address,                   // Second word address
    };

    esp_err_t err = i2c_master_transmit_receive(_dev_handle, payload.data(), payload.size(), buffer, size, -1);
    if (err != ESP_OK) 
    {
        ESP_LOGD("AT24C256::read", "[0x%02x] - Multi-read failed @ 0x%04x: [%u] %s", _address, address, err, esp_err_to_name(err));
        return false;
    }

    ESP_LOGD("AT24C256::read", "[0x%02x] - Read %u bytes @ 0x%04x", _address, size, address);
    vTaskDelay(25 / portTICK_PERIOD_MS);  // 25 milliseconds
    
    return true;
}

template class AT24C256<true>;
template class AT24C256<false>;
