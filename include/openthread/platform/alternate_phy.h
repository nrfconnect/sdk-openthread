/*
 *  Copyright (c) 2026, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes the platform abstraction for Alternate PHY (HDR) capability discovery.
 */

#ifndef OPENTHREAD_PLATFORM_ALTERNATE_PHY_H_
#define OPENTHREAD_PLATFORM_ALTERNATE_PHY_H_

#include <stdbool.h>
#include <stdint.h>

#include <openthread/instance.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup plat-alternate-phy
 *
 * @brief
 *   This module includes the platform abstraction for Alternate PHY capability discovery.
 *
 *   An "Alternate PHY" is a secondary physical layer (for example, 2 Mbps GFSK PHY) that a device may
 *   support in addition to the primary IEEE 802.15.4 PHY. OpenThread advertises the supported Alternate PHYs to
 *   neighbors during MLE. This module lets the platform report which alternate PHYs the hardware supports.
 *
 * @{
 *
 */

/**
 * PHY Identifier for the 2 Mbps GFSK Alternate PHY.
 *
 * This value is used as the Alternate PHY Sub-TLV Type.
 *
 */
#define OT_ALTERNATE_PHY_ID_TL3_GFSK 0

/**
 * PHY Identifier used when querying the local priority of the Primary Link.
 *
 * This value is not an Alternate PHY identifier and must not be advertised in an Alternate PHY Capability TLV.
 *
 */
#define OT_ALTERNATE_PHY_ID_PRIMARY_LINK 0xff

/**
 * Maximum number of Alternate PHYs OpenThread queries the platform for.
 *
 */
#define OT_ALTERNATE_PHY_MAX_COUNT 4

/**
 * Number of parameter octets in an Alternate PHY capability.
 *
 */
#define OT_ALTERNATE_PHY_PARAMETER_COUNT 4

/**
 * Identifies a parameter octet in an Alternate PHY capability.
 *
 */
typedef enum otAlternatePhyParameter
{
    OT_ALTERNATE_PHY_PARAMETER_0 = 0,
    OT_ALTERNATE_PHY_PARAMETER_1 = 1,
    OT_ALTERNATE_PHY_PARAMETER_2 = 2,
    OT_ALTERNATE_PHY_PARAMETER_3 = 3,
} otAlternatePhyParameter;

/**
 * Identifies the parameters defined for the TL3 2 Mbps GFSK PHY.
 *
 */
typedef enum otAlternatePhyTl3GfskParameter
{
    OT_ALTERNATE_PHY_TL3_GFSK_PARAMETER_SETTLING_DELAY = OT_ALTERNATE_PHY_PARAMETER_0,
    OT_ALTERNATE_PHY_TL3_GFSK_PARAMETER_AIFS           = OT_ALTERNATE_PHY_PARAMETER_1,
    OT_ALTERNATE_PHY_TL3_GFSK_PARAMETER_MAX_PSDU_LSB   = OT_ALTERNATE_PHY_PARAMETER_2,
    OT_ALTERNATE_PHY_TL3_GFSK_PARAMETER_MAX_PSDU_MSB   = OT_ALTERNATE_PHY_PARAMETER_3,
} otAlternatePhyTl3GfskParameter;

/** Minimum TL3 max PSDU (including FCS). */
#define OT_ALTERNATE_PHY_TL3_GFSK_MAX_PSDU_MIN 127

/** Maximum TL3 max PSDU (including FCS). */
#define OT_ALTERNATE_PHY_TL3_GFSK_MAX_PSDU_MAX 255

/**
 * Defines the flags for the TL3 2 Mbps GFSK PHY.
 *
 */
typedef enum otAlternatePhyTl3GfskFlag
{
    OT_ALTERNATE_PHY_TL3_GFSK_FLAG_CONCURRENT_LISTENING = (1 << 0),
} otAlternatePhyTl3GfskFlag;

/**
 * Represents the capabilities of a single Alternate PHY supported by the device.
 *
 * The interpretation of the parameter octets and flags is determined by @c mPhyId.
 *
 */
typedef struct otAlternatePhyCapability
{
    uint8_t mPhyId;                                        ///< PHY Identifier.
    uint8_t mFlags;                                        ///< PHY-specific capability flags.
    uint8_t mParameters[OT_ALTERNATE_PHY_PARAMETER_COUNT]; ///< PHY-specific parameter values.
} otAlternatePhyCapability;

/** Thread MAC Command identifier for Dynamic Alternate PHY Switch (DAPS). */
#define OT_THREAD_MAC_COMMAND_ID 0x34

/** DAPS Sub-ID within the Thread MAC Command payload. */
#define OT_DAPS_SUB_ID 0x00

/**
 * Indicates that an Alternate PHY transmission stays on the current Primary Link channel.
 *
 */
#define OT_ALTERNATE_PHY_CHANNEL_SAME 0

/**
 * Parameters the TL3 2 Mbps GFSK PHY needs in order to transmit a frame.
 *
 */
typedef struct otAlternatePhyTl3GfskTxInfo
{
    uint8_t mSettlingDelay; ///< TL3_SETTLING_DELAY in microseconds, as advertised by the receiver.
    uint8_t mAifs;          ///< TL3_AIFS in microseconds, as advertised by the receiver.
} otAlternatePhyTl3GfskTxInfo;

/**
 * Describes the Alternate PHY selected for a single transmission.
 *
 */
typedef struct otAlternatePhyTxInfo
{
    uint8_t mPhyId;        ///< Selected Alternate PHY identifier (`OT_ALTERNATE_PHY_ID_*`).
    uint8_t mChannel;      ///< Target channel, or `OT_ALTERNATE_PHY_CHANNEL_SAME` to keep the Primary Link channel.
    bool    mRequiresDaps; ///< Send a DAPS frame on the Primary Link before this frame.

    /**
     * PHY-specific transmission parameters.
     *
     */
    union
    {
        otAlternatePhyTl3GfskTxInfo mTl3Gfsk;
        uint8_t                     mParameters[OT_ALTERNATE_PHY_PARAMETER_COUNT];
    } mParams;

    uint16_t mMaxPsdu; ///< Maximum PSDU for this transmission, including FCS.
} otAlternatePhyTxInfo;

/**
 * Gets the set of Alternate PHYs supported by the platform.
 *
 * OpenThread calls this while building the Alternate PHY Capability TLV and the Connectivity TLV during MLE. The
 * platform reports each Alternate PHY it supports together with the associated timing parameters and flags.
 *
 * @note This is an optional platform API. A weak default implementation returns 0, meaning the device supports no
 *       Alternate PHY. A platform that supports one or more
 *       Alternate PHYs must provide a strong implementation of this function.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 * @param[out] aCaps      A pointer to an array to be filled with the supported Alternate PHY capabilities.
 * @param[in]  aMaxCount  The maximum number of entries that @p aCaps can hold.
 *
 * @returns The number of Alternate PHY capability entries written to @p aCaps (0 if none are supported).
 *
 */
uint8_t otPlatAlternatePhyGetCapabilities(otInstance *aInstance, otAlternatePhyCapability *aCaps, uint8_t aMaxCount);

/**
 * Gets the local selection priority for a PHY.
 *
 * OpenThread selects the acceptable PHY with the highest priority for transmission. Link quality is used to break a
 * priority tie.
 *
 * @note This is an optional platform API. The weak default implementation gives Alternate PHYs priority 1 and the
 *       Primary Link priority 0, preserving the default preference for an available Alternate PHY.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 * @param[in] aPhyId     An `OT_ALTERNATE_PHY_ID_*` value or `OT_ALTERNATE_PHY_ID_PRIMARY_LINK`.
 *
 * @returns The local selection priority. A larger value indicates a stronger preference.
 *
 */
uint8_t otPlatAlternatePhyGetPriority(otInstance *aInstance, uint8_t aPhyId);

/**
 * @}
 *
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENTHREAD_PLATFORM_ALTERNATE_PHY_H_
