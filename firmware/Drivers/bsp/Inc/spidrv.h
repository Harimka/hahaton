/*******************************************************************************
 * @file    spidrv.h
 * @author  team-04 (Hackathon 2026)
 * @brief   Abstract SPI peripheral driver interface (like ztlib).
 ******************************************************************************/

#pragma once

#include "serialdrv.h"

/**
 * @brief SPI bus peripheral driver
 */
class SpiDrv : public SerialDrv {
public:
    enum class CsState {
        kInactive = 0,
        kActive   = 1
    };

    /**
     * @brief Control Chip Select line
     */
    virtual void setCs(CsState state) = 0;
};

/***************************** END OF FILE ************************************/
