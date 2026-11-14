/**
 * @file    serialdrv.h
 * @author  team-04
 * @brief   Abstract serial communication interface.
 */

#ifndef TEAM04_SERIALDRV_H_
#define TEAM04_SERIALDRV_H_

#include <stdint.h>

namespace team04 {

/**
 * @brief Abstract serial transport interface.
 */
class SerialDrv {
public:
    SerialDrv() = default;
    virtual ~SerialDrv() = default;

    SerialDrv(const SerialDrv&) = delete;
    SerialDrv& operator=(const SerialDrv&) = delete;

    /**
     * @brief Transmit raw bytes.
     * @param data Pointer to buffer.
     * @param length Number of bytes to transmit.
     * @return Number of bytes actually written.
     */
    virtual uint16_t write(const uint8_t* data, uint16_t length) = 0;

    /**
     * @brief Transmit null-terminated string.
     * @param str C-string pointer.
     */
    virtual void print(const char* str) = 0;
};

} // namespace team04

#endif // TEAM04_SERIALDRV_H_

/***************************** END OF FILE ************************************/
