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
 *   This file includes core definitions for Alternate PHY capabilities.
 */

#ifndef RADIO_ALTERNATE_PHY_HPP_
#define RADIO_ALTERNATE_PHY_HPP_

#include <stdint.h>

#include <openthread/platform/alternate_phy.h>
#include <openthread/platform/radio.h>

#include "common/array.hpp"
#include "common/code_utils.hpp"
#include "common/encoding.hpp"
#include "common/timer.hpp"
#include "openthread-core-config.h"
#include "thread/link_quality.hpp"

namespace ot {
namespace AlternatePhy {

typedef uint8_t                  PhyId;
typedef otAlternatePhyCapability Capability;

static constexpr uint8_t kMaxPhyCount    = OT_ALTERNATE_PHY_MAX_COUNT;
static constexpr uint8_t kParameterCount = OT_ALTERNATE_PHY_PARAMETER_COUNT;
static constexpr PhyId   kInvalidPhyId   = 0xff;

namespace Tl3Gfsk {

static constexpr PhyId kPhyId = OT_ALTERNATE_PHY_ID_TL3_GFSK;

inline uint16_t GetMaxPsdu(const Capability &aCapability)
{
    return LittleEndian::ReadUint16(&aCapability.mParameters[OT_ALTERNATE_PHY_TL3_GFSK_PARAMETER_MAX_PSDU_LSB]);
}

inline void SetMaxPsdu(Capability &aCapability, uint16_t aMaxPsdu)
{
    LittleEndian::WriteUint16(aMaxPsdu, &aCapability.mParameters[OT_ALTERNATE_PHY_TL3_GFSK_PARAMETER_MAX_PSDU_LSB]);
}

inline bool IsValid(const Capability &aCapability)
{
    const uint16_t maxPsdu = GetMaxPsdu(aCapability);

    return (maxPsdu >= OT_ALTERNATE_PHY_TL3_GFSK_MAX_PSDU_MIN) &&
           (maxPsdu <= OT_ALTERNATE_PHY_TL3_GFSK_MAX_PSDU_MAX);
}

} // namespace Tl3Gfsk

inline bool IsValidPhyId(PhyId aPhyId)
{
    switch (aPhyId)
    {
    case Tl3Gfsk::kPhyId:
        return true;

    default:
        return false;
    }
}

inline bool IsValid(const Capability &aCapability)
{
    switch (aCapability.mPhyId)
    {
    case Tl3Gfsk::kPhyId:
        return Tl3Gfsk::IsValid(aCapability);

    default:
        return false;
    }
}

inline uint16_t GetMaxPsdu(const Capability &aCapability)
{
    uint16_t maxPsdu = 0;

    switch (aCapability.mPhyId)
    {
    case Tl3Gfsk::kPhyId:
        maxPsdu = Tl3Gfsk::GetMaxPsdu(aCapability);
        break;
    }

    return maxPsdu;
}

inline bool RequiresDaps(const Capability &aCapability)
{
    bool requiresDaps = true;

    switch (aCapability.mPhyId)
    {
    case Tl3Gfsk::kPhyId:
        requiresDaps = (aCapability.mFlags & OT_ALTERNATE_PHY_TL3_GFSK_FLAG_CONCURRENT_LISTENING) == 0;
        break;

    default:
        break;
    }

    return requiresDaps;
}

/**
 * Per-PHY link quality and usage state for a neighbor.
 *
 */
struct LinkState
{
    PhyId           mPhyId;
    bool            mInUse;
    TimeMilli       mNextProbeTime;
    LinkQualityInfo mLinkInfo;
};

/**
 * Stores per-PHY Alternate PHY link state (parallel to @ref Capabilities).
 *
 */
class LinkStates : public Array<LinkState, kMaxPhyCount>
{
public:
    void Clear(void) { Array<LinkState, kMaxPhyCount>::Clear(); }

    LinkState *Find(PhyId aPhyId)
    {
        LinkState *match = nullptr;

        for (LinkState &state : *this)
        {
            if (state.mPhyId == aPhyId)
            {
                match = &state;
                break;
            }
        }

        return match;
    }

    const LinkState *Find(PhyId aPhyId) const
    {
        const LinkState *match = nullptr;

        for (const LinkState &state : *this)
        {
            if (state.mPhyId == aPhyId)
            {
                match = &state;
                break;
            }
        }

        return match;
    }

    bool IsInUse(PhyId aPhyId) const
    {
        const LinkState *state = Find(aPhyId);

        return (state != nullptr) && state->mInUse;
    }

    void SetInUse(PhyId aPhyId, bool aInUse)
    {
        LinkState *state = Find(aPhyId);

        if (state != nullptr)
        {
            state->mInUse = aInUse;

            if (!aInUse)
            {
                state->mNextProbeTime = TimerMilli::GetNow() + OPENTHREAD_CONFIG_ALTERNATE_PHY_RETRY_INTERVAL;
            }
        }
    }

    LinkQuality GetLinkQuality(PhyId aPhyId) const
    {
        LinkQuality     quality = kLinkQuality0;
        const LinkState *state  = Find(aPhyId);

        VerifyOrExit(state != nullptr);
        VerifyOrExit(state->mLinkInfo.GetAverageRss() != OT_RADIO_RSSI_INVALID);
        quality = state->mLinkInfo.GetLinkQuality();

    exit:
        return quality;
    }

    bool IsLinkAcceptable(PhyId aPhyId) const
    {
        bool             acceptable = false;
        const LinkState *state      = Find(aPhyId);
        const bool       inUse      = (state != nullptr) && state->mInUse;
        const LinkQuality minQuality =
            static_cast<LinkQuality>(inUse ? OPENTHREAD_CONFIG_ALTERNATE_PHY_MIN_LINK_QUALITY_CONTINUE
                                           : OPENTHREAD_CONFIG_ALTERNATE_PHY_MIN_LINK_QUALITY_START);

        // Without any state yet, allow a first probe if its result can be tracked.
        if (state == nullptr)
        {
            ExitNow(acceptable = !IsFull());
        }

        if (state->mLinkInfo.GetMessageErrorRate() > OPENTHREAD_CONFIG_ALTERNATE_PHY_MAX_MESSAGE_FAILURE_RATE)
        {
            VerifyOrExit(TimerMilli::GetNow() >= state->mNextProbeTime);
        }

        if (state->mLinkInfo.GetAverageRss() == OT_RADIO_RSSI_INVALID)
        {
            acceptable = !inUse;
            ExitNow();
        }

        acceptable = (state->mLinkInfo.GetLinkQuality() >= minQuality);

    exit:
        return acceptable;
    }
};

/**
 * Stores a set of Alternate PHY capabilities.
 *
 */
class Capabilities : public Array<Capability, kMaxPhyCount>
{
public:
    /**
     * Finds a capability by PHY Identifier.
     *
     * @param[in] aPhyId  The PHY Identifier to find.
     *
     * @returns A pointer to the matching capability, or `nullptr` if it is not present.
     *
     */
    const Capability *Find(PhyId aPhyId) const
    {
        const Capability *match = nullptr;

        for (const Capability &capability : *this)
        {
            if (capability.mPhyId == aPhyId)
            {
                match = &capability;
                break;
            }
        }

        return match;
    }

    /**
     * Indicates whether a capability for a given PHY Identifier is present.
     *
     * @param[in] aPhyId  The PHY Identifier to check.
     *
     * @retval TRUE   A matching capability is present.
     * @retval FALSE  A matching capability is not present.
     *
     */
    bool Contains(PhyId aPhyId) const { return Find(aPhyId) != nullptr; }

    /**
     * Adds or replaces a capability.
     *
     * @param[in] aCapability  The capability to add or replace.
     *
     * @retval kErrorNone    The capability was stored.
     * @retval kErrorNoBufs  The array is full and no existing capability has the same PHY Identifier.
     *
     */
    Error Upsert(const Capability &aCapability)
    {
        for (Capability &capability : *this)
        {
            if (capability.mPhyId == aCapability.mPhyId)
            {
                capability = aCapability;
                return kErrorNone;
            }
        }

        return PushBack(aCapability);
    }
};

/**
 * Gets the Alternate PHY capabilities of the local device.
 *
 * @param[in] aInstance  OpenThread instance passed to the platform capability API.
 *
 * @returns The local Alternate PHY capabilities.
 *
 */
inline Capabilities GetCapabilities(otInstance *aInstance)
{
    Capabilities capabilities;
    uint8_t      count;

    count = otPlatAlternatePhyGetCapabilities(aInstance, capabilities.GetArrayBuffer(), capabilities.GetMaxSize());
    capabilities.SetLength((count < capabilities.GetMaxSize()) ? count : capabilities.GetMaxSize());

    return capabilities;
}

/**
 * Selects the best Alternate PHY for transmission toward a neighbor.
 *
 * Iterates the intersection of @p aLocal and @p aNeighbor capabilities and returns the capability with the
 * highest local platform priority that passes @ref LinkStates::IsLinkAcceptable(). Link quality breaks a priority tie.
 * The Primary Link participates in the same comparison and is represented by a `nullptr` return value.
 *
 * @param[in] aInstance         OpenThread instance passed to the platform priority API.
 * @param[in] aLocal            Alternate PHY capabilities of the local device.
 * @param[in] aNeighbor         Alternate PHY capabilities advertised by the neighbor.
 * @param[in] aLinkStates       Per-PHY link state for the neighbor.
 * @param[in] aPrimaryLinkInfo  Link quality information for the Primary Link.
 *
 * @returns A pointer to the selected neighbor capability, or `nullptr` if the Primary Link is selected.
 *
 */
inline const Capability *SelectBest(otInstance               *aInstance,
                                    const Capabilities       &aLocal,
                                    const Capabilities       &aNeighbor,
                                    const LinkStates         &aLinkStates,
                                    const LinkQualityInfo    &aPrimaryLinkInfo)
{
    const Capability *best         = nullptr;
    uint8_t           bestPriority = otPlatAlternatePhyGetPriority(aInstance, OT_ALTERNATE_PHY_ID_PRIMARY_LINK);
    LinkQuality       bestQuality  = aPrimaryLinkInfo.GetLinkQuality();

    for (const Capability &localCap : aLocal)
    {
        const Capability *neighborCap = aNeighbor.Find(localCap.mPhyId);
        uint8_t           priority;
        LinkQuality       quality;

        if (neighborCap == nullptr)
        {
            continue;
        }

        if (!aLinkStates.IsLinkAcceptable(localCap.mPhyId))
        {
            continue;
        }

        priority = otPlatAlternatePhyGetPriority(aInstance, localCap.mPhyId);
        quality = aLinkStates.GetLinkQuality(localCap.mPhyId);

        if ((priority > bestPriority) || ((priority == bestPriority) && (quality > bestQuality)))
        {
            best         = neighborCap;
            bestPriority = priority;
            bestQuality  = quality;
        }
    }

    return best;
}

} // namespace AlternatePhy
} // namespace ot

#endif // RADIO_ALTERNATE_PHY_HPP_
