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

#include <openthread/platform/daps.h>

#include "encoding.h"

#include <string.h>

#define FCS_SIZE 2U

/* Frame Control field, IEEE 802.15.4-2015 general frame format. */
#define FCF_SIZE                 2U
#define FCF_FRAME_TYPE_MASK      0x0007U
#define FCF_FRAME_TYPE_DATA      0x0001U
#define FCF_FRAME_TYPE_MAC_CMD   0x0003U
#define FCF_SECURITY_ENABLED     0x0008U
#define FCF_FRAME_PENDING        0x0010U
#define FCF_ACK_REQUEST          0x0020U
#define FCF_PAN_ID_COMPRESSION   0x0040U
#define FCF_SEQ_NUM_SUPPRESSION  0x0100U
#define FCF_IE_PRESENT           0x0200U
#define FCF_DST_ADDR_MODE_SHIFT  10U
#define FCF_FRAME_VERSION_MASK   0x3000U
#define FCF_FRAME_VERSION_2015   0x2000U
#define FCF_SRC_ADDR_MODE_SHIFT  14U
#define FCF_ADDR_MODE_MASK       0x03U

static uint8_t addrSize(uint8_t aMode)
{
    switch (aMode)
    {
    case OT_DAPS_ADDR_SHORT:
        return sizeof(uint16_t);
    case OT_DAPS_ADDR_EXTENDED:
        return OT_EXT_ADDRESS_SIZE;
    default:
        return 0U;
    }
}

/*
 * When PAN ID Compression is enabled on version 2015 frames, if both src and dst addresses are extended
 * Both PAN ID's are omitted.
 */
static bool frameIsVersion2015AndBothExtended(uint16_t aFcf, const otDapsAddrs *aAddrs)
{
    return (aFcf & FCF_FRAME_VERSION_MASK) == FCF_FRAME_VERSION_2015 &&
           aAddrs->mDstMode == OT_DAPS_ADDR_EXTENDED && aAddrs->mSrcMode == OT_DAPS_ADDR_EXTENDED;
}

static bool dstPanIdIsPresent(uint16_t aFcf, const otDapsAddrs *aAddrs)
{
    if (frameIsVersion2015AndBothExtended(aFcf, aAddrs))
    {
        return (aFcf & FCF_PAN_ID_COMPRESSION) == 0U;
    }

    return true;
}

static bool srcPanIdIsPresent(uint16_t aFcf, const otDapsAddrs *aAddrs)
{
    if (frameIsVersion2015AndBothExtended(aFcf, aAddrs))
    {
        return false;
    }

    return (aFcf & FCF_PAN_ID_COMPRESSION) == 0U;
}

static bool extAddrIsBroadcast(const uint8_t *aAddr)
{
    for (uint8_t i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        if (aAddr[i] != 0xffU)
        {
            return false;
        }
    }

    return true;
}

static bool destIsBroadcast(const otDapsAddrs *aAddrs)
{
    return (aAddrs->mDstMode == OT_DAPS_ADDR_SHORT && aAddrs->mDstShort == OT_PANID_BROADCAST) ||
           (aAddrs->mDstMode == OT_DAPS_ADDR_EXTENDED && extAddrIsBroadcast(aAddrs->mDstExt));
}

/*
 * Parse the addressing fields of a MAC header.
 *
 * On success, @p aPayloadOffset is the offset of the first octet after the addressing fields, i.e.
 * the start of the MAC payload for an unsecured frame.
 *
 * @retval OT_ERROR_NONE       Addressing fields parsed.
 * @retval OT_ERROR_NOT_FOUND  The frame type cannot carry an Alternate PHY exchange.
 * @retval OT_ERROR_PARSE      The addressing fields are truncated or unusable.
 */
static otError macAddrsParse(const uint8_t *aMpdu, uint8_t aMpduLen, otDapsAddrs *aAddrs, uint8_t *aPayloadOffset)
{
    uint16_t fcf;
    uint16_t frameType;
    uint8_t  offset;

    /* Every offset below is validated against the payload budget, which excludes the FCS. */
    if (aMpduLen < FCF_SIZE + FCS_SIZE)
    {
        return OT_ERROR_NOT_FOUND;
    }

    memset(aAddrs, 0, sizeof(*aAddrs));

    fcf    = otEncodingReadUint16Le(aMpdu);
    offset = FCF_SIZE;

    frameType = fcf & FCF_FRAME_TYPE_MASK;
    if (frameType != FCF_FRAME_TYPE_DATA && frameType != FCF_FRAME_TYPE_MAC_CMD)
    {
        return OT_ERROR_NOT_FOUND;
    }

    aAddrs->mDstMode = (fcf >> FCF_DST_ADDR_MODE_SHIFT) & FCF_ADDR_MODE_MASK;
    aAddrs->mSrcMode = (fcf >> FCF_SRC_ADDR_MODE_SHIFT) & FCF_ADDR_MODE_MASK;

    /* An Alternate PHY exchange is unicast in both directions, so both addresses are present. */
    if (aAddrs->mDstMode == OT_DAPS_ADDR_NONE || aAddrs->mSrcMode == OT_DAPS_ADDR_NONE)
    {
        return OT_ERROR_PARSE;
    }

    if ((fcf & FCF_SEQ_NUM_SUPPRESSION) == 0U)
    {
        offset += 1U;
    }

    aAddrs->mPanIdPresent = dstPanIdIsPresent(fcf, aAddrs);

    if (aAddrs->mPanIdPresent)
    {
        if (aMpduLen < offset + sizeof(uint16_t) + FCS_SIZE)
        {
            return OT_ERROR_PARSE;
        }
        aAddrs->mPanId = otEncodingReadUint16Le(&aMpdu[offset]);
        offset += sizeof(uint16_t);
    }

    if (aMpduLen < offset + addrSize(aAddrs->mDstMode) + FCS_SIZE)
    {
        return OT_ERROR_PARSE;
    }

    if (aAddrs->mDstMode == OT_DAPS_ADDR_SHORT)
    {
        aAddrs->mDstShort = otEncodingReadUint16Le(&aMpdu[offset]);
    }
    else
    {
        memcpy(aAddrs->mDstExt, &aMpdu[offset], OT_EXT_ADDRESS_SIZE);
    }
    offset += addrSize(aAddrs->mDstMode);

    if (srcPanIdIsPresent(fcf, aAddrs))
    {
        if (aMpduLen < offset + sizeof(uint16_t) + FCS_SIZE)
        {
            return OT_ERROR_PARSE;
        }
        offset += sizeof(uint16_t);
    }

    if (aMpduLen < offset + addrSize(aAddrs->mSrcMode) + FCS_SIZE)
    {
        return OT_ERROR_PARSE;
    }

    if (aAddrs->mSrcMode == OT_DAPS_ADDR_SHORT)
    {
        aAddrs->mSrcShort = otEncodingReadUint16Le(&aMpdu[offset]);
    }
    else
    {
        memcpy(aAddrs->mSrcExt, &aMpdu[offset], OT_EXT_ADDRESS_SIZE);
    }
    offset += addrSize(aAddrs->mSrcMode);

    if (aPayloadOffset != NULL)
    {
        *aPayloadOffset = offset;
    }

    return OT_ERROR_NONE;
}

otError otDapsBuild(const uint8_t *aMpdu,
                    uint8_t        aMpduLen,
                    uint8_t        aPhyId,
                    uint8_t        aChannel,
                    uint8_t       *aDaps,
                    uint8_t        aDapsCap,
                    uint8_t       *aDapsLen)
{
    otDapsAddrs addrs;
    otError     error;
    uint8_t     payloadLen;
    uint8_t     frameLen;
    uint16_t    fcf;
    uint8_t    *cursor;
    bool        emitPanId;

    if (aMpdu == NULL || aDaps == NULL || aDapsLen == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }

    error = macAddrsParse(aMpdu, aMpduLen, &addrs, NULL);
    if (error == OT_ERROR_NOT_FOUND)
    {
        return OT_ERROR_PARSE;
    }
    else if (error != OT_ERROR_NONE)
    {
        return error;
    }

    if (destIsBroadcast(&addrs))
    {
        return OT_ERROR_INVALID_ARGS;
    }

    fcf = FCF_FRAME_TYPE_MAC_CMD | FCF_FRAME_VERSION_2015 | FCF_PAN_ID_COMPRESSION | FCF_SEQ_NUM_SUPPRESSION |
          ((uint16_t)addrs.mDstMode << FCF_DST_ADDR_MODE_SHIFT) |
          ((uint16_t)addrs.mSrcMode << FCF_SRC_ADDR_MODE_SHIFT);

    emitPanId = dstPanIdIsPresent(fcf, &addrs);

    /* A short-addressed DAPS frame still needs a PAN ID, so the announced frame has to supply one. */
    if (emitPanId && !addrs.mPanIdPresent)
    {
        return OT_ERROR_PARSE;
    }

    /* Command ID and Sub-ID are always emitted; PHY ID and Channel might me omitted */
    payloadLen = 2U + ((aChannel == OT_ALTERNATE_PHY_CHANNEL_SAME) ? (aPhyId == 0U ? 0U : 1U): 2U);
    frameLen   = (uint8_t)(FCF_SIZE + (emitPanId ? sizeof(uint16_t) : 0U) + addrSize(addrs.mDstMode) +
                         addrSize(addrs.mSrcMode) + payloadLen + FCS_SIZE);

    if (aDapsCap < frameLen)
    {
        return OT_ERROR_NO_BUFS;
    }

    cursor = aDaps;

    otEncodingWriteUint16Le(cursor, fcf);
    cursor += FCF_SIZE;

    if (emitPanId)
    {
        otEncodingWriteUint16Le(cursor, addrs.mPanId);
        cursor += sizeof(uint16_t);
    }

    if (addrs.mDstMode == OT_DAPS_ADDR_SHORT)
    {
        otEncodingWriteUint16Le(cursor, addrs.mDstShort);
    }
    else
    {
        memcpy(cursor, addrs.mDstExt, OT_EXT_ADDRESS_SIZE);
    }
    cursor += addrSize(addrs.mDstMode);

    if (addrs.mSrcMode == OT_DAPS_ADDR_SHORT)
    {
        otEncodingWriteUint16Le(cursor, addrs.mSrcShort);
    }
    else
    {
        memcpy(cursor, addrs.mSrcExt, OT_EXT_ADDRESS_SIZE);
    }
    cursor += addrSize(addrs.mSrcMode);

    *cursor++ = OT_THREAD_MAC_COMMAND_ID;
    *cursor++ = OT_DAPS_SUB_ID;

    if (aChannel != OT_ALTERNATE_PHY_CHANNEL_SAME)
    {
        *cursor++ = aPhyId;
        *cursor++ = aChannel;
    }
    else if (aPhyId != 0U)
    {
        *cursor++ = aPhyId;
    }

    /* The radio appends the real FCS; reserve the octets so the length is right. */
    memset(cursor, 0, FCS_SIZE);

    *aDapsLen = frameLen;

    return OT_ERROR_NONE;
}

otError otDapsParse(const uint8_t *aMpdu, uint8_t aMpduLen, otDapsInfo *aInfo)
{
    otError  error;
    uint16_t fcf;
    uint8_t  offset;
    uint8_t  payloadLen;

    if (aMpdu == NULL || aInfo == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }

    memset(aInfo, 0, sizeof(*aInfo));

    if (aMpduLen < FCF_SIZE + FCS_SIZE)
    {
        return OT_ERROR_NOT_FOUND;
    }

    fcf = otEncodingReadUint16Le(aMpdu);

    if ((fcf & FCF_FRAME_TYPE_MASK) != FCF_FRAME_TYPE_MAC_CMD)
    {
        return OT_ERROR_NOT_FOUND;
    }

    if ((fcf & FCF_FRAME_VERSION_MASK) != FCF_FRAME_VERSION_2015)
    {
        return OT_ERROR_NOT_FOUND;
    }

    if ((fcf & (FCF_PAN_ID_COMPRESSION | FCF_SEQ_NUM_SUPPRESSION)) !=
        (FCF_PAN_ID_COMPRESSION | FCF_SEQ_NUM_SUPPRESSION))
    {
        return OT_ERROR_NOT_FOUND;
    }

    if ((fcf & (FCF_SECURITY_ENABLED | FCF_FRAME_PENDING | FCF_ACK_REQUEST | FCF_IE_PRESENT)) != 0U)
    {
        return OT_ERROR_NOT_FOUND;
    }

    error = macAddrsParse(aMpdu, aMpduLen, &aInfo->mAddrs, &offset);
    if (error != OT_ERROR_NONE)
    {
        return error;
    }

    payloadLen = (uint8_t)(aMpduLen - FCS_SIZE - offset);

    /* Command ID and Sub-ID are mandatory; PHY ID and Channel are optional, in that order. */
    if (payloadLen < 2U)
    {
        return OT_ERROR_NOT_FOUND;
    }

    if (aMpdu[offset++] != OT_THREAD_MAC_COMMAND_ID)
    {
        return OT_ERROR_NOT_FOUND;
    }

    if (aMpdu[offset++] != OT_DAPS_SUB_ID)
    {
        return OT_ERROR_NOT_FOUND;
    }

    if (payloadLen > 4U)
    {
        return OT_ERROR_PARSE;
    }

    /* An omitted PHY ID is implicitly 0 (TL3 GFSK). */
    aInfo->mPhyId   = (payloadLen >= 3U) ? aMpdu[offset++] : 0U;
    aInfo->mChannel = (payloadLen == 4U) ? aMpdu[offset] : OT_ALTERNATE_PHY_CHANNEL_SAME;

    return OT_ERROR_NONE;
}

otError otDapsIsAddressedTo(const otDapsInfo *aInfo, otShortAddress aShortAddress, const otExtAddress *aExtAddress)
{
    if (aInfo == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }

    switch (aInfo->mAddrs.mDstMode)
    {
    case OT_DAPS_ADDR_SHORT:
        return (aInfo->mAddrs.mDstShort == aShortAddress) ? OT_ERROR_NONE : OT_ERROR_DESTINATION_ADDRESS_FILTERED;

    case OT_DAPS_ADDR_EXTENDED:
        if (aExtAddress == NULL)
        {
            return OT_ERROR_INVALID_ARGS;
        }
        return (memcmp(aInfo->mAddrs.mDstExt, aExtAddress->m8, OT_EXT_ADDRESS_SIZE) == 0)
                   ? OT_ERROR_NONE
                   : OT_ERROR_DESTINATION_ADDRESS_FILTERED;

    default:
        return OT_ERROR_PARSE;
    }
}
