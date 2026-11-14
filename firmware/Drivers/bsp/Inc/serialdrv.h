/*******************************************************************************
 * @file    serialdrv.h
 * @author  team-04 (Hackathon 2026)
 * @brief   Abstract serial bus peripheral driver interface (like ztlib).
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @brief Abstract peripheral bus driver (pure virtual interface)
 */
class SerialDrv {
public:
    virtual ~SerialDrv() = default;

    /**
     * @brief Open bus driver with hardware setup
     */
    virtual bool open() = 0;

    /**
     * @brief Close bus driver
     */
    virtual void close() = 0;

    /**
     * @brief Raw write data buffer to bus
     */
    virtual int32_t write(const void* buf, uint32_t len) = 0;

    /**
     * @brief Raw read data buffer from bus
     */
    virtual int32_t read(void* buf, uint32_t len) = 0;

    /**
     * @brief Write data to specific register
     */
    virtual int32_t write(uint8_t reg, const void* buf, uint32_t len) = 0;

    /**
     * @brief Read data from specific register
     */
    virtual int32_t read(uint8_t reg, void* buf, uint32_t len) = 0;

    /**
     * @brief Full-duplex synchronous transfer
     */
    virtual int32_t transfer(const void* tx, void* rx, uint32_t len) = 0;
};

/***************************** END OF FILE ************************************/
