#pragma once

#include <cstddef>
#include <optional>
#include <ranges>

#include "driver/i2c_master.h"
#include "esp_log.h"

/**
 * An AT24C256 EEPROM chip from Atmel
 * capable of storing 262144 bits at 32768 distinct addresses
 * 
 * safe_mode: if enabled, performs additional checks and logs (bound checks, addresses checks, ...)
 * It is enable by default
 */
template<bool safe_mode = true>
class AT24C256
{
public:
    static constexpr int I2C_MASTER_FREQ_HZ = 100000;

    static constexpr int PAGE_COUNT = 512;
    static constexpr int PAGE_SIZE = 64;
    static constexpr int MEMORY_SIZE = PAGE_COUNT * PAGE_SIZE;

    // First address: 0x0000
    // Last address:  0x7FFF
    // (0b111111111'111111, 9 + 6 = 15 bits)

public:
    AT24C256(i2c_master_bus_handle_t bus, uint8_t address);

    AT24C256(const AT24C256 &other) = delete;
    AT24C256(AT24C256 &&other);
    ~AT24C256();

    AT24C256& operator=(const AT24C256 &other) = delete;
    AT24C256& operator=(AT24C256 &&other);

    /**
     * Write a single byte anywhere on the chip
     * 
     * Return true on success, false on faillure
     * On faillure, check logs for more info
     */
    bool write(uint16_t address, uint8_t byte) const;

    /**
     * Write a sequence of bytes anywhere on the chip
     * 
     * Return true on success, false on faillure
     * On faillure, check logs for more info
     */
    bool write(uint16_t address, uint8_t* buffer, uint16_t size) const;

    /**
     * Write a sequence of bytes anywhere on the chip,
     * from a contiguous and sized arbitrary range
     * 
     * count is the number of elements in the range to write (starting from the beginning)
     * 
     * Return true on success, false on faillure
     * On faillure, check logs for more info
     */
    template<typename R>
    requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
    bool write(uint16_t address, R&& range, size_t count) const
    {
        if constexpr(safe_mode)
        {
            if(count > std::ranges::size(range))
            {
                ESP_LOGE("AT24C256::write", "[0x%02x] - Given size (%zu) is greater than range size (%zu)", _address, count, std::ranges::size(range));
                return false;
            }
        }

        return write(address, (uint8_t*) std::ranges::data(range), count * sizeof(std::ranges::range_value_t<R>));
    }

    /**
     * Write a sequence of bytes anywhere on the chip,
     * from a contiguous and sized arbitrary range
     * 
     * Return true on success, false on faillure
     * On faillure, check logs for more info
     */
    template<typename R>
    requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
    bool write(uint16_t address, R&& range) const
    {
        return write(address, (uint8_t*) std::ranges::data(range), std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>));
    }

    /**
     * Multi-byte write inside the same page.
     * The function can write as many bytes as there is between address
     * and the last page byte.
     * A page being 64 bytes, this function can write at most 64 bytes at once. 
     * 
     * Return true on success, false on faillure
     * On faillure, check logs for more info
     */
    bool write_page(uint16_t address, uint8_t* buffer, uint16_t size) const;

    /**
     * Read a single byte at given address
     */
    auto read(uint16_t address) const -> typename std::conditional<safe_mode, std::optional<uint8_t>, uint8_t>::type;

    /**
     * Read a sequence of bytes
     * If size is > than remaining bytes between address and last addressable byte,
     * it loops back to address 0x0000
     */
    bool read(uint16_t address, uint8_t* buffer, size_t size) const;

    /**
     * Construct a container of type R that must represent
     * a contiguous and sized range, read <count> elements
     * into the container and return it. The actual amount 
     * of bytes read will depend on the element's size.
     */
    template<typename R>
    requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
    auto read(uint16_t address, size_t count) const -> typename std::conditional<safe_mode, std::optional<R>, R>::type
    {
        R buffer(count);
        bool result = read(address, std::ranges::data(buffer), std::ranges::size(buffer) * sizeof(std::ranges::range_value_t<R>));
        
        if constexpr (safe_mode) 
        {
            if(!result)
                return std::nullopt;
        }

        return buffer;
    }

    /**
     * Read <count> element into the container of type R that must represent
     * a contiguous and sized range. The actual amount of bytes read will depend on
     * the element size.
     */
    template<typename R>
    requires std::ranges::contiguous_range<R> 
    bool read(uint16_t address, R& range, size_t count) const
    {
        if constexpr(safe_mode)
        {
            if(count > std::ranges::size(range))
            {
                ESP_LOGE("AT24C256::read", "[0x%02x] - Given size (%zu) is greater than range size (%zu)", _address, count, std::ranges::size(range));
                return false;
            }
        }

        return read(address, std::ranges::data(range), count * sizeof(std::ranges::range_value_t<R>));
    }

    /**
     * Read elements into the container of type R that must represent
     * a contiguous and sized range. The actual amount of bytes read will depend on
     * the range size and element size.
     */
    template<typename R>
    requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
    bool read(uint16_t address, R& range) const
    {
        return read(address, std::ranges::data(range), std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>));
    }



private:
    uint8_t _address;
    i2c_device_config_t  _dev_cfg;
    i2c_master_dev_handle_t _dev_handle;
};