/** @file fatfsadapter.h
 * @author team-04
 * @brief Adapter from the STM32Cube FatFS dispatcher to block storage.
 */
#ifndef TEAM04_FATFSADAPTER_H_
#define TEAM04_FATFSADAPTER_H_
#include "ff.h"
#include "ff_gen_drv.h"
#include "storagedrv.h"

#ifndef LBA_t
typedef DWORD LBA_t;
#endif
namespace team04 {
class FatFsAdapter final {
public:
    explicit FatFsAdapter(StorageDrv& storage, uint8_t lun = 0U);
    FatFsAdapter(const FatFsAdapter&) = delete;
    FatFsAdapter& operator=(const FatFsAdapter&) = delete;
    /* Call before FATFS_LinkDriver; only one bound adapter is supported. */
    bool bind();
    uint32_t errorCount() const { return errorCount_; }
    static const Diskio_drvTypeDef kDriver;
private:
    static DSTATUS initialize(BYTE lun);
    static DSTATUS status(BYTE lun);
    static DRESULT read(BYTE lun, BYTE* buffer, LBA_t sector, UINT count);
    static DRESULT write(BYTE lun, const BYTE* buffer, LBA_t sector, UINT count);
    static DRESULT ioctl(BYTE lun, BYTE command, void* buffer);
    static DRESULT map(IoStatus status);
    static FatFsAdapter* active_;
    StorageDrv& storage_;
    uint8_t lun_;
    bool ready_;
    uint32_t errorCount_;
};
}
#endif
/***************************** END OF FILE ************************************/
