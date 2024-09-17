/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#include "wakeup_coord_table.hpp"

#include <openthread/platform/time.h>

#include "common/encoding.hpp"
#include "common/locator.hpp"
#include "common/log.hpp"

#if OPENTHREAD_CONFIG_MAC_CSL_PERIPHERAL_ENABLE

namespace ot {

namespace Mac {

RegisterLogModule("CoordTable");

Error WakeupCoordTable::DetectReplay(const RxFrame &aFrame)
{
    Error        error = kErrorNone;
    WakeupCoord *coord;
    Address      frameSrcAddr;
    uint32_t     frameKeySequence;
    uint32_t     frameCounter;

    IgnoreError(aFrame.GetSrcAddr(frameSrcAddr));
    frameKeySequence = BigEndian::ReadUint32(aFrame.GetKeySource());
    IgnoreError(aFrame.GetFrameCounter(frameCounter));
    coord = mWakeupCoords.FindMatching(frameSrcAddr.GetExtended());

    if (coord != nullptr)
    {
        VerifyOrExit(frameKeySequence >= coord->GetKeySequence(), error = kErrorSecurity);

        if (frameKeySequence == coord->GetKeySequence())
        {
            VerifyOrExit(frameCounter > coord->GetFrameCounter(), error = kErrorSecurity);
        }
    }
    else
    {
        Evict();
        coord = mWakeupCoords.PushBack();
        VerifyOrExit(coord != nullptr, error = kErrorNoBufs);
    }

    coord->SetExtAddress(frameSrcAddr.GetExtended());
    coord->SetKeySequence(frameKeySequence);
    coord->SetFrameCounter(frameCounter);
    coord->SetLastUpdated(GetNowInSecs());

exit:
    if (error == kErrorSecurity)
    {
        LogWarn("Received replayed wake-up with source address %s!", coord->GetExtAddress().ToString().AsCString());
    }
    else if (error == kErrorNoBufs)
    {
        LogInfo("Received a wake-up frame while the WC table was full");
    }

    return error;
}

void WakeupCoordTable::Evict(void)
{
    uint32_t     now         = GetNowInSecs();
    WakeupCoord *oldestCoord = nullptr;
    uint32_t     oldestUpdated;

    VerifyOrExit(now > kWakeupCoordinatorEvictAge);
    oldestUpdated = now - kWakeupCoordinatorEvictAge;

    for (WakeupCoord &coord : mWakeupCoords)
    {
        if (coord.GetLastUpdated() < oldestUpdated)
        {
            oldestUpdated = coord.GetLastUpdated();
            oldestCoord   = &coord;
        }
    }

    if (oldestCoord != nullptr)
    {
        LogInfo("Evicting WC %s", oldestCoord->GetExtAddress().ToString().AsCString());
        mWakeupCoords.Remove(*oldestCoord);
    }

exit:
    return;
}

uint32_t WakeupCoordTable::GetNowInSecs(void) { return static_cast<uint32_t>(otPlatTimeGet() / 1000000); }

} // namespace Mac

} // namespace ot

#endif // OPENTHREAD_FTD
