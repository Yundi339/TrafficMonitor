#pragma once

#include <Windows.h>

class CHistoryTrafficFullSaveRetrySchedule
{
public:
    static constexpr ULONGLONG INITIAL_INTERVAL_MS = 15ull * 1000;
    static constexpr ULONGLONG MAX_INTERVAL_MS = 5ull * 60 * 1000;

    void Reset(ULONGLONG current_tick)
    {
        m_last_attempt_tick = current_tick;
        m_retry_interval_ms = INITIAL_INTERVAL_MS;
    }

    bool ShouldRetry(ULONGLONG current_tick) const
    {
        const ULONGLONG elapsed = current_tick >= m_last_attempt_tick
            ? current_tick - m_last_attempt_tick
            : m_retry_interval_ms;
        return elapsed >= m_retry_interval_ms;
    }

    void MarkFailed(ULONGLONG current_tick)
    {
        m_last_attempt_tick = current_tick;
        if (m_retry_interval_ms >= MAX_INTERVAL_MS / 2)
            m_retry_interval_ms = MAX_INTERVAL_MS;
        else
            m_retry_interval_ms *= 2;
    }

    ULONGLONG GetRetryInterval() const { return m_retry_interval_ms; }

private:
    ULONGLONG m_last_attempt_tick{};
    ULONGLONG m_retry_interval_ms{ INITIAL_INTERVAL_MS };
};
