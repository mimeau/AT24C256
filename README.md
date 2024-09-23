# AT24C256

[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mimeau/library/AT24C256.svg)](https://registry.platformio.org/libraries/mimeau/AT24C256) [![PlatformIO CI](https://github.com/mimeau/AT24C256/actions/workflows/main.yml/badge.svg)](https://github.com/mimeau/AT24C256/actions/workflows/main.yml/badge.svg)

This small C++ library provides read/write operations on AT24C256 EEPROM chip using the Espressif IDF and its I2C lib.

It aims to provide easy to use C++ interface and comprehensive logging. It does not aim to be as performant as possible.

Features:
 - Read / write a single byte
 - Read / write contiguous data
   - Any container meeting the requirements of ```std::ranges::contiguous_range``` : ```std::vector```, ```std::array```, ```std::span```, ...
   - C-style array
 - Read / write arbitrary objects (shallow copy, including padding)
 - Write limited to a single page
 - A safe mode enabled by default. When enabled :
   - some functions return ```std::optional<T>``` instead of just ```T```, the optional being empty on failures
   - additonal checks are performed : 
     - number of elements to write against container size
     - address to write to (check validity)
     - page overlap for ```write_page()```
 - Comprehensive logging to check operations (enable debug logs for max details)

To create an AT24C256 object, user must first create a valid I2C bus handle from ESPIDF framework. Then, this handle and the AT24C256 chip address is given to the constructor.

AT24C256 objects can be instanciated with safe_mode enable (by default) or disable using a boolean template parameter. When safe_mode is enable, additional safety checks are performed, and additional logging is outputted. Some function will also return an std::optional instead of directly the result.

## Interface

### Constructors

```cpp 
template<bool safe_mode = true>
AT24C256::AT24C256(i2c_master_bus_handle_t bus, uint8_t address);
```
Takes an i2c bus handle constructed using ESP IDF I2C library `driver/i2c_master.h`, and the device physical I2C address. For AT24C256 chips, this address should be between 0x50 and 0x57.

Safe mode is enabled by default and adds additional checks & logs.

```cpp 
AT24C256(const AT24C256 &other) = delete;
AT24C256(AT24C256 &&other);
~AT24C256();

AT24C256& operator=(const AT24C256 &other) = delete;
AT24C256& operator=(AT24C256 &&other);
```

AT24C256 objects can be moved, but can't be copied. Internally, each object holds its `i2c_master_dev_handle_t`, a handle that needs to be freed only once. To prevent use-after-free or multiple free issues, copy is disabled for the object.

### Write operations

```cpp 
bool AT24C256::write(uint16_t address, uint8_t byte) const;
bool AT24C256::write(uint16_t address, uint8_t* buffer, uint16_t size) const;
```

Write a byte or an array of bytes at specified address. Valid addresses range from 0x0000 to 0x7FFF.

```cpp 
template<typename T> requires (!std::ranges::contiguous_range<T>) 
bool write(uint16_t address, const T& value) const;
```

Write arbitrary data at specified address. If `T` is an object, it writes a shallow copy including the padding bytes that may be present. `T` must be an std::contiguous_range, as there is special overloads for this kind of types, see below.   

```cpp 
template<typename R>
    requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
bool write(uint16_t address, R&& range, size_t count) const;

template<typename R>
    requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
bool write(uint16_t address, R&& range) const;
```

These overloads provide convenient ways to write data from any contiguous and sized range such as `std::vector`, `std::array`, `std::span`, etc. C.f. usage examples below. Return success/error.

```cpp 
bool write_page(uint16_t address, uint8_t* buffer, uint16_t size) const;
```

Memory on AT24C256 chips is segmented in 512 pages of 64 bytes each. A single write OP (as understood by the chip) is restricted to a single page (and will loop back to the beginning of the page if the write continues). This function is mostly used by other write functions that will distribute the write on multiple pages if the data is big enough to warrant it.

Return success/error.

### Read operations

```cpp
auto read(uint16_t address) const -> typename std::conditional<safe_mode, std::optional<uint8_t>, uint8_t>::type;
```

Read a single byte at specified address. If safe_mode is enabled, this function returns an `std::optional<uint8_t>` that is empty on failures. If safe_mode is disabled, returns the byte directly, without any guarentees that it contains the correct value.

```cpp
bool read(uint16_t address, uint8_t* buffer, size_t size) const;
```

Read multiples bytes at specified address, into a C-style array. Return success/error.

```cpp
template<typename T>
    requires (!std::ranges::contiguous_range<T>) 
auto read(uint16_t address) const -> typename std::conditional<safe_mode, std::optional<T>, T>::type
```

Read arbitrary data at specified address. As with its equivalent `write()` function, `T` must be an std::contiguous_range, as there is special overloads for this kind of types, see below.

```cpp
template<typename R>
requires std::ranges::contiguous_range<R> 
auto read(uint16_t address, size_t count) const -> typename std::conditional<safe_mode, std::optional<R>, R>::type;

template<typename R>
requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
auto read(uint16_t address) const -> typename std::conditional<safe_mode, std::optional<R>, R>::type;
```

Construct a container of type `R` that must be an `std::ranges::contiguous_range` (and an `std::ranges::sized_range` in the second case), and will read \<count> bytes at the specified address (first case) or will read as many bytes as the container holds (second case). Works with arbitrary elements type (`uint8_t`, `float`, structs, ...).

Note that the count parameter designates an amount of elements. The actual amount of bytes read will be equal to `count * sizeof(<element type>)`. 

With safe_mode enabled, functions will return an std::optional<R>.

Allows to call:

```cpp
auto array = at24256.read<std::array<float, 4>>(0x20A); // No need to indicate a count if we want to fill the whole array

auto vector = at24256.read<std::vector<float>>(0x20A, 4); // Need to indicate a count with std::vector or else it will read 0 bytes (empty default-constructed vector)
```

In these examples, `4 * sizeof(float)` = 16 bytes are read.

```cpp
template<typename R>
requires std::ranges::contiguous_range<R>
        && std::ranges::sized_range<R> 
bool read(uint16_t address, R& range, size_t count) const;

template<typename R>
requires std::ranges::contiguous_range<R> 
        && std::ranges::sized_range<R>
bool read(uint16_t address, R& range) const;
```

Same as above, except it reads into the given container directly, and returns a boolean indicating success or failure.

```cpp
std::array<uint8_t, 3> array;
bool result = at24256.read(0x017D, array, 3);
// or at24256.read(0x017D, array);
```

## Usage
```cpp
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

```

## Limitations

Objects calls functions such as `i2c_master_transmit()` and `i2c_master_transmit_receive()` which are **not thread safe**.
