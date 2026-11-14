/** @file imudrv.h
 * @author team-04
 * @brief Raw sample contract and abstract sensor interface.
 */
#ifndef TEAM04_IMUDRV_H_
#define TEAM04_IMUDRV_H_
#include "spidrv.h"
namespace team04 {
#pragma pack(push, 1)
struct ImuRawSample {
    uint32_t timestampUs;
    uint32_t sampleIndex;
    int16_t rawX;
    int16_t rawY;
    int16_t rawZ;
    uint8_t flags;
};
#pragma pack(pop)
static_assert(sizeof(ImuRawSample) == 15U, "ImuRawSample layout must match docs/interfaces.md");
enum class ImuFlag : uint8_t { kOverrun = 1U, kSaturation = 2U, kSpiError = 4U };
class ImuDrv {
public:
    ImuDrv() = default;
    ImuDrv(const ImuDrv&) = delete;
    ImuDrv& operator=(const ImuDrv&) = delete;
    virtual IoStatus configure() = 0;
    virtual IoStatus readRaw(int16_t& x, int16_t& y, int16_t& z) = 0;
protected:
    ~ImuDrv() = default;
};
}
#endif
/***************************** END OF FILE ************************************/
