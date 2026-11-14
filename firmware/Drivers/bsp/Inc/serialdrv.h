/** @file serialdrv.h
 * @author team-04
 * @brief Hardware-independent nonblocking serial transport interface.
 */
#ifndef TEAM04_SERIALDRV_H_
#define TEAM04_SERIALDRV_H_
#include <stdint.h>
#include "spidrv.h"
namespace team04 {
class SerialDrv {
public:
    SerialDrv() = default;
    SerialDrv(const SerialDrv&) = delete;
    SerialDrv& operator=(const SerialDrv&) = delete;
    virtual uint16_t write(const uint8_t* data, uint16_t length) = 0;
    virtual uint16_t txFree() const = 0;
protected:
    ~SerialDrv() = default;
};
}
#endif
/***************************** END OF FILE ************************************/
