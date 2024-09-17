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

#ifndef WAKEUP_COORD_TABLE_HPP_
#define WAKEUP_COORD_TABLE_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_MAC_CSL_PERIPHERAL_ENABLE

#include "common/array.hpp"
#include "common/non_copyable.hpp"
#include "mac/mac_frame.hpp"
#include "mac/mac_types.hpp"

namespace ot {

namespace Mac {

constexpr uint8_t  kMaxWakeupCoords           = OPENTHREAD_CONFIG_MAC_MAX_WAKEUP_COORDS;
constexpr uint32_t kWakeupCoordinatorEvictAge = OPENTHREAD_CONFIG_MAC_WC_EVICT_AGE;

/**
 * Represents a trusted Wake-up Coordinator.
 *
 */
class WakeupCoord
{
public:
    /**
     * Returns the Extended Address.
     *
     * @returns A const reference to the Extended Address.
     *
     */
    const ExtAddress &GetExtAddress(void) const { return mExtAddr; }

    /**
     * Returns the Extended Address.
     *
     * @returns A reference to the Extended Address.
     *
     */
    ExtAddress &GetExtAddress(void) { return mExtAddr; }

    /**
     * Sets the Extended Address.
     *
     * @param[in]  aAddress  The Extended Address value to set.
     *
     */
    void SetExtAddress(const ExtAddress &aAddress) { mExtAddr = aAddress; }

    /**
     * Gets the key sequence value.
     *
     * @returns The key sequence value.
     *
     */
    uint32_t GetKeySequence(void) const { return mKeySequence; }

    /**
     * Sets the key sequence value.
     *
     * @param[in]  aKeySequence  The key sequence value.
     *
     */
    void SetKeySequence(uint32_t aKeySequence) { mKeySequence = aKeySequence; }

    /**
     * Returns Frame Counter value.
     *
     * @returns The Frame Counter value.
     *
     */
    uint32_t GetFrameCounter(void) const { return mFrameCounter; }

    /**
     * Sets the Frame Counter value.
     *
     * @param[in] aFrameCounter  The Frame Counter value.
     *
     */
    void SetFrameCounter(uint32_t aFrameCounter) { mFrameCounter = aFrameCounter; }

    /**
     * Returns last updated value.
     *
     * @returns The last updated value.
     *
     */
    uint32_t GetLastUpdated(void) const { return mLastUpdated; }

    /**
     * Sets the last updated value.
     *
     * @param[in] aLastUpdated  The last updated value.
     *
     */
    void SetLastUpdated(uint32_t aLastUpdated) { mLastUpdated = aLastUpdated; }

    /**
     * Indicates whether a given ExtAddress matches the WakeupCoord.
     *
     * @param[in] aExtAddress  A ExtAddress to match with the WakeupCoord.
     *
     * @returns TRUE if the ExtAddress matches the WakeupCoord, FALSE otherwise.
     *
     */
    bool Matches(const ExtAddress &aExtAddress) const { return mExtAddr == aExtAddress; };

private:
    ExtAddress mExtAddr;
    uint32_t   mKeySequence;
    uint32_t   mFrameCounter;
    uint32_t   mLastUpdated;
};

class WakeupCoordTable : private NonCopyable
{
public:
    /**
     * Constructor.
     *
     */
    explicit WakeupCoordTable(void) = default;

    /**
     * This method clears the router table.
     *
     */
    void Clear(void) { mWakeupCoords.Clear(); }

    /**
     * Detects if the frame is a replay by verifying that no entry exists in the table with the same
     * extended source address and stale security information (key sequence and frame counter).
     *
     * If the table is full, tries to evict the oldest entry that exeeced `kWakeupCoordinatorEvictAge`.
     *
     * @param[in]  aFrame  A reference to the incoming frame.
     *
     * @returns A pointer to the router or `nullptr` if the router could not be found.
     *
     */
    Error DetectReplay(const RxFrame &aFrame);

private:
    void     Evict(void);
    uint32_t GetNowInSecs(void);

    Array<WakeupCoord, kMaxWakeupCoords> mWakeupCoords;
};

} // namespace Mac

} // namespace ot

#endif // OPENTHREAD_CONFIG_MAC_CSL_PERIPHERAL_ENABLE

#endif // WAKEUP_COORD_TABLE_HPP_
