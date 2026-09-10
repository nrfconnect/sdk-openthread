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
 * @brief
 *   Helpers for building and parsing Dynamic Alternate PHY Switch (DAPS) frames.
 */

#ifndef OPENTHREAD_PLATFORM_DAPS_H_
#define OPENTHREAD_PLATFORM_DAPS_H_

#include <openthread/error.h>
#include <openthread/platform/alternate_phy.h>
#include <openthread/platform/radio.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** IEEE 802.15.4 addressing modes, as encoded in the Frame Control field. */
enum
{
    OT_DAPS_ADDR_NONE     = 0,
    OT_DAPS_ADDR_SHORT    = 2,
    OT_DAPS_ADDR_EXTENDED = 3,
};

/** Addressing fields of a MAC frame header. */
typedef struct otDapsAddrs
{
    uint8_t  mDstMode;      /**< One of `OT_DAPS_ADDR_*`. */
    uint8_t  mSrcMode;      /**< One of `OT_DAPS_ADDR_*`. */
    bool     mPanIdPresent; /**< Whether the frame carries a Destination PAN ID. */
    uint16_t mPanId;        /**< Destination PAN ID, valid only when @p mPanIdPresent. */
    uint16_t mDstShort;
    uint8_t  mDstExt[OT_EXT_ADDRESS_SIZE]; /**< Over-the-air byte order. */
    uint16_t mSrcShort;
    uint8_t  mSrcExt[OT_EXT_ADDRESS_SIZE]; /**< Over-the-air byte order. */
} otDapsAddrs;

/** Contents of a received DAPS frame. */
typedef struct otDapsInfo
{
    otDapsAddrs mAddrs;
    uint8_t     mPhyId;  /**< Alternate PHY the sender is about to use. */
    uint8_t     mChannel; /**< Target channel, or `OT_ALTERNATE_PHY_CHANNEL_SAME` when absent. */
} otDapsInfo;

/**
 * Builds the Dynamic Alternate PHY Switch frame that precedes an Alternate PHY transmission.
 *
 * @param[in]  aMpdu      MPDU of the upcoming Alternate PHY frame (PSDU bytes, no PHR).
 * @param[in]  aMpduLen   Length of @p aMpdu, including the FCS octets.
 * @param[in]  aPhyId     Alternate PHY identifier to announce.
 * @param[in]  aChannel   Target channel, or `OT_ALTERNATE_PHY_CHANNEL_SAME` to keep the current one.
 * @param[out] aDaps      Output buffer for the DAPS MPDU (no PHR).
 * @param[in]  aDapsCap   Capacity of @p aDaps in octets.
 * @param[out] aDapsLen   Length of the generated MPDU, including the FCS octets.
 *
 * @retval OT_ERROR_NONE          DAPS frame generated.
 * @retval OT_ERROR_PARSE         @p aMpdu is not a unicast frame this codec understands.
 * @retval OT_ERROR_NO_BUFS       @p aDapsCap is too small.
 * @retval OT_ERROR_INVALID_ARGS  Invalid arguments, or a broadcast destination.
 */
otError otDapsBuild(const uint8_t *aMpdu,
                    uint8_t        aMpduLen,
                    uint8_t        aPhyId,
                    uint8_t        aChannel,
                    uint8_t       *aDaps,
                    uint8_t        aDapsCap,
                    uint8_t       *aDapsLen);

/**
 * Parses a received frame as a DAPS frame.
 *
 * @param[in]  aMpdu     Received MPDU (no PHR).
 * @param[in]  aMpduLen  Length of @p aMpdu, including the FCS octets.
 * @param[out] aInfo     Parsed contents, valid only on `OT_ERROR_NONE`.
 *
 * @retval OT_ERROR_NONE          @p aMpdu is a well-formed DAPS frame.
 * @retval OT_ERROR_NOT_FOUND     @p aMpdu is not a DAPS frame and must be handled normally.
 * @retval OT_ERROR_PARSE         @p aMpdu claims to be a DAPS frame but is malformed.
 * @retval OT_ERROR_INVALID_ARGS  @p aMpdu or @p aInfo is NULL.
 */
otError otDapsParse(const uint8_t *aMpdu, uint8_t aMpduLen, otDapsInfo *aInfo);

/**
 * Checks whether a parsed DAPS frame is addressed to this device.
 *
 * @param[in] aInfo         Parsed DAPS frame.
 * @param[in] aShortAddress This device's short address.
 * @param[in] aExtAddress   This device's extended address, in over-the-air byte order. May be NULL
 *                          only when @p aInfo carries a short destination address.
 *
 * @retval OT_ERROR_NONE                           The DAPS frame targets this device.
 * @retval OT_ERROR_DESTINATION_ADDRESS_FILTERED   The DAPS frame targets someone else.
 * @retval OT_ERROR_PARSE                          @p aInfo has no usable destination address.
 * @retval OT_ERROR_INVALID_ARGS                   A required argument is NULL.
 */
otError otDapsIsAddressedTo(const otDapsInfo *aInfo, otShortAddress aShortAddress, const otExtAddress *aExtAddress);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENTHREAD_PLATFORM_DAPS_H_
