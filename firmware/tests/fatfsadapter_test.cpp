/** @file fatfsadapter_test.cpp
 * @author team-04
 * @brief Host checks for the Cube FatFS adapter and drive table.
 */
#include "fatfsadapter.h"
#include <cassert>
using namespace team04;
namespace {
class MockStorage final : public StorageDrv {
public:
    uint32_t reads = 0U, writes = 0U;
    IoStatus initialize() override { return IoStatus::kOk; }
    IoStatus read(uint32_t, uint8_t*) override { ++reads; return IoStatus::kOk; }
    IoStatus write(uint32_t, const uint8_t*) override { ++writes; return IoStatus::kOk; }
    IoStatus sectorCount(uint64_t& count) override { count = 131072U; return IoStatus::kOk; }
};
}
int main() {
    MockStorage storage; FatFsAdapter adapter(storage); assert(adapter.bind());
    assert(FatFsAdapter::kDriver.disk_initialize(0U) == 0U);
    uint8_t buffer[1024]{};
    assert(FatFsAdapter::kDriver.disk_read(0U, buffer, 5U, 2U) == RES_OK && storage.reads == 2U);
    assert(FatFsAdapter::kDriver.disk_write(0U, buffer, 5U, 2U) == RES_OK && storage.writes == 2U);
    LBA_t sectors = 0U;
    assert(FatFsAdapter::kDriver.disk_ioctl(0U, GET_SECTOR_COUNT, &sectors) == RES_OK && sectors == 131072U);
    assert(FatFsAdapter::kDriver.disk_read(1U, buffer, 0U, 1U) == RES_NOTRDY);
}
/***************************** END OF FILE ************************************/
