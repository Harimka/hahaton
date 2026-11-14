/** @file fatfsadapter.cpp
 * @author team-04
 * @brief FatFS diskio driver table for statically bound block storage.
 */
#include "fatfsadapter.h"
namespace team04 {
FatFsAdapter* FatFsAdapter::active_ = nullptr;
const Diskio_drvTypeDef FatFsAdapter::kDriver = {initialize, status, read, write, ioctl};
FatFsAdapter::FatFsAdapter(StorageDrv& storage, uint8_t lun)
    : storage_(storage)
    , lun_(lun)
    , ready_(false)
    , errorCount_(0U)
{
}
bool FatFsAdapter::bind() {
    if (active_ && active_ != this) return false;
    active_ = this; ready_ = false; return true;
}
DRESULT FatFsAdapter::map(IoStatus s) {
    if (s == IoStatus::kOk) return RES_OK;
    if (active_) ++active_->errorCount_;
    if (s == IoStatus::kNotReady) return RES_NOTRDY;
    if (s == IoStatus::kInvalid) return RES_PARERR;
    return RES_ERROR;
}
DSTATUS FatFsAdapter::initialize(BYTE lun) {
    if (!active_ || lun != active_->lun_) return STA_NOINIT;
    active_->ready_ = active_->storage_.initialize() == IoStatus::kOk;
    if (!active_->ready_) ++active_->errorCount_;
    return active_->ready_ ? 0U : STA_NOINIT;
}
DSTATUS FatFsAdapter::status(BYTE lun) {
    return active_ && lun == active_->lun_ && active_->ready_ ? 0U : STA_NOINIT;
}
DRESULT FatFsAdapter::read(BYTE lun, BYTE* buffer, LBA_t sector, UINT count) {
    if (status(lun)) return RES_NOTRDY;
    if (!buffer || !count || static_cast<uint64_t>(sector) + count - 1U > UINT32_MAX) return RES_PARERR;
    for (UINT i = 0; i < count; ++i) {
        const IoStatus s = active_->storage_.read(static_cast<uint32_t>(sector + i), buffer + 512U * i);
        if (s != IoStatus::kOk) return map(s);
    }
    return RES_OK;
}
DRESULT FatFsAdapter::write(BYTE lun, const BYTE* buffer, LBA_t sector, UINT count) {
#if FF_FS_READONLY == 0
    if (status(lun)) return RES_NOTRDY;
    if (!buffer || !count || static_cast<uint64_t>(sector) + count - 1U > UINT32_MAX) return RES_PARERR;
    for (UINT i = 0; i < count; ++i) {
        const IoStatus s = active_->storage_.write(static_cast<uint32_t>(sector + i), buffer + 512U * i);
        if (s != IoStatus::kOk) return map(s);
    }
    return RES_OK;
#else
    (void)lun; (void)buffer; (void)sector; (void)count; return RES_WRPRT;
#endif
}
DRESULT FatFsAdapter::ioctl(BYTE lun, BYTE command, void* buffer) {
    if (status(lun)) return RES_NOTRDY;
    if (command == CTRL_SYNC) return RES_OK;
    if (!buffer) return RES_PARERR;
    if (command == GET_SECTOR_SIZE) { *static_cast<WORD*>(buffer) = 512U; return RES_OK; }
    if (command == GET_BLOCK_SIZE) { *static_cast<DWORD*>(buffer) = 1U; return RES_OK; }
    if (command == GET_SECTOR_COUNT) {
        uint64_t sectors = 0U; const IoStatus s = active_->storage_.sectorCount(sectors);
        if (s != IoStatus::kOk) return map(s);
        const LBA_t converted = static_cast<LBA_t>(sectors);
        if (static_cast<uint64_t>(converted) != sectors) return RES_PARERR;
        *static_cast<LBA_t*>(buffer) = converted; return RES_OK;
    }
    return RES_PARERR;
}
}
/***************************** END OF FILE ************************************/
