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

#include "wakeup_tx_scheduler.hpp"

#if OPENTHREAD_CONFIG_MAC_CSL_CENTRAL_ENABLE

#include "core/instance/instance.hpp"
#include "common/locator_getters.hpp"
#include "common/log.hpp"
#include "common/time.hpp"
#include "common/code_utils.hpp"
#include "common/num_utils.hpp"
#include "radio/radio.hpp"

#include <openthread/platform/radio.h>

namespace ot {

namespace {

// Frame lengths including SHR
constexpr uint32_t kWakeupFrameLength   = 54;
constexpr uint32_t kParentRequestLength = 78;
// This value has been determined experimentally to ensure that a wake-up frame is received
// by the radio co-processor early enough to be scheduled on time. That is, it is not
// exactly the length of data that is sent over the RCP transport, such as USB.
constexpr uint32_t kWakeupFrameDataLength = 100;

uint16_t CalcTxRequestAheadTimeUs(Instance &aInstance)
{
    constexpr uint32_t kBitsPerByte = 8;
    constexpr uint32_t kUsPerSecond = 1000000;

    uint16_t aheadTimeUs = OPENTHREAD_CONFIG_MAC_CSL_REQUEST_AHEAD_US;
    uint32_t busSpeedHz  = otPlatRadioGetBusSpeed(&aInstance);

    if (busSpeedHz > 0)
    {
        aheadTimeUs +=
            static_cast<uint16_t>((kWakeupFrameDataLength * kBitsPerByte * kUsPerSecond + busSpeedHz - 1) / busSpeedHz);
    }

    return aheadTimeUs;
}

TimeMicro GetRadioNow(Instance &aInstance)
{
    return TimeMicro(static_cast<uint32_t>(otPlatRadioGetNow(&aInstance)));
}

} // namespace

RegisterLogModule("WakeupTxSched");

WakeupTxScheduler::WakeupTxScheduler(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mTxTimeUs(0)
    , mTxEndTimeUs(0)
    , mTxRequestAheadTimeUs(CalcTxRequestAheadTimeUs(aInstance))
    , mTimer(aInstance)
    , mSequenceOngoing(false)
{
}

Error WakeupTxScheduler::WakeUp(const Mac::ExtAddress &aTarget, uint16_t aIntervalUs, uint16_t aDurationMs)
{
    Error     error = kErrorNone;
    TimeMicro nowUs;

    VerifyOrExit(mSequenceOngoing == false, error = kErrorInvalidState);

    mTarget           = aTarget;
    nowUs             = TimerMicro::GetNow();
    mTxTimeUs         = nowUs + mTxRequestAheadTimeUs;
    mTxEndTimeUs      = mTxTimeUs + aDurationMs * 1000 + aIntervalUs;
    mIntervalUs       = aIntervalUs;
    mSequenceOngoing  = true;

    LogInfo("Started wake-up sequence to %s", aTarget.ToString().AsCString());

    ScheduleNext(/* aIsFirstFrame */ true);

exit:
    return error;
}

#if OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE

Mac::TxFrame *WakeupTxScheduler::PrepareWakeupFrame(Mac::TxFrames &aTxFrames)
{
    Mac::TxFrame      *frame = nullptr;
    Mac::Address       target;
    Mac::Address       source;
    TimeMicro          radioTxUs;
    uint32_t           rendezvousTimeUs;
    Mac::ConnectionIe *connectionIe;

    VerifyOrExit(mSequenceOngoing == true);

    target.SetExtended(mTarget);
    source.SetExtended(Get<Mac::Mac>().GetExtAddress());
    radioTxUs = GetRadioNow(GetInstance()) + (mTxTimeUs - TimerMicro::GetNow());

#if OPENTHREAD_CONFIG_MULTI_RADIO
    frame = &aTxFrames.GetTxFrame(Mac::kRadioTypeIeee802154);
#else
    frame = &aTxFrames.GetTxFrame();
#endif

    VerifyOrExit(frame->GenerateWakeupFrame(Get<Mac::Mac>().GetPanId(), target, source) == kErrorNone, frame = nullptr);
    frame->SetTxDelayBaseTime(0);
    frame->SetTxDelay(radioTxUs.GetValue());
    frame->SetCsmaCaEnabled(false);
    frame->SetMaxCsmaBackoffs(0);
    frame->SetMaxFrameRetries(0);

    // Rendezvous Time is the time between the end of transmission of a wake-up
    // frame and the start of transmission of the first payload frame, in the
    // units of 10 symbols.
    // Aligning the expected reception of Parent Request in the middle of the
    // next empty slot in between wake-up frames.
    rendezvousTimeUs = (mIntervalUs - (kWakeupFrameLength + kParentRequestLength) * kOctetDuration) / 2;
    rendezvousTimeUs += mIntervalUs;
    frame->GetRendezvousTimeIe()->SetRendezvousTime(rendezvousTimeUs / kUsPerTenSymbols);

    connectionIe = frame->GetConnectionIe();
    connectionIe->SetRetryInterval(kConnectionRetryInterval);
    connectionIe->SetRetryCount(kConnectionRetryCount);

    // Schedule the next timer right away before waiting for the transmission completion
    // to keep up with the high rate of the wake-up frames in the RCP architecture.
    ScheduleNext();

exit:
    return frame;
}

#else // OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE

Mac::TxFrame *WakeupTxScheduler::PrepareWakeupFrame(Mac::TxFrames &) { return nullptr; }

#endif // OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE

void WakeupTxScheduler::ScheduleNext(bool aIsFirstFrame)
{
    if (!aIsFirstFrame)
    {
        // Advance to the time of the next wake-up frame, but make sure we're not late already.
        mTxTimeUs = Max(mTxTimeUs + mIntervalUs, TimerMicro::GetNow() + mTxRequestAheadTimeUs);
    }

    // It is sufficient to just exit early when the wake-up sequence is over because this method
    // is called either at the beginning of the wake-up sequence or right after sending a wake-up
    // frame, so no frame is scheduled at this moment yet.
    if (mTxTimeUs >= mTxEndTimeUs)
    {
        mSequenceOngoing = false;
        LogInfo("Stopped wake-up sequence");
        ExitNow();
    }

    mTimer.FireAt(mTxTimeUs - mTxRequestAheadTimeUs);

exit:
    return;
}

void WakeupTxScheduler::Stop(void)
{
    mSequenceOngoing = false;
    mTimer.Stop();
}

} // namespace ot

#endif // OPENTHREAD_CONFIG_MAC_CSL_CENTRAL_ENABLE
