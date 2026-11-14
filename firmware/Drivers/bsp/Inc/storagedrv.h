/** @file storagedrv.h
 * @author team-04
 * @brief Hardware-independent 512-byte block storage interface.
 */
#ifndef TEAM04_STORAGEDRV_H_
#define TEAM04_STORAGEDRV_H_
#include "spidrv.h"
namespace team04 {
class StorageDrv {
public:
    static constexpr uint16_t kBlockSize = 512U;
    StorageDrv() = default;
    StorageDrv(const StorageDrv&) = delete;
    StorageDrv& operator=(const StorageDrv&) = delete;
    virtual IoStatus initialize() = 0;
    virtual IoStatus read(uint32_t lba, uint8_t* buffer) = 0;
    virtual IoStatus write(uint32_t lba, const uint8_t* buffer) = 0;
    virtual IoStatus sectorCount(uint64_t& count) = 0;
protected:
    ~StorageDrv() = default;
};
}
#endif
/***************************** END OF FILE ************************************/
