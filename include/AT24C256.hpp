#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <expected>

#include "driver/i2c_master.h"

/**
 * An AT24C256 EEPROM chip from Atmel
 * capable of storing 262144 bits at 32768 distinct addresses
 */
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
    bool write(uint16_t address, std::vector<uint8_t> buffer) const;

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
    std::expected<uint8_t, bool> read(uint16_t address) const;
    
    uint8_t read(uint16_t address);

    /**
     * Read a sequence of bytes
     * If size is > than remaining bytes between address and last addressable byte,
     * it loops back to address 0x0000
     */
    bool read(uint16_t address, uint8_t* buffer, uint8_t size) const;
    std::expected<std::vector<uint8_t>, bool> read(uint16_t address, uint8_t size) const;

private:
    uint8_t _address;
    i2c_device_config_t  _dev_cfg;
    i2c_master_dev_handle_t _dev_handle;
};