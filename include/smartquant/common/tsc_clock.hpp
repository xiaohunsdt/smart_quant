#pragma once

#include <cstdint>
#include <ctime>
#include <stdexcept>

namespace sq {

// TSC-based nanosecond clock.
//
// Must call TscClock::calibrate() once at process startup before any call to
// now_ns().  After that, now_ns() is a single rdtsc + multiply, with no system
// calls.  The process must be pinned to a single core (isolcpus) to guarantee
// that rdtsc values are monotonic and consistent.
class TscClock {
public:
    // Calibrate TSC frequency by comparing two clock_gettime samples.
    // Sleeps for ~10 ms to get a stable reading.
    static void calibrate(int sleep_ms = 10) {
        struct timespec t0{}, t1{};
        uint64_t tsc0, tsc1;

        ::clock_gettime(CLOCK_REALTIME, &t0);
        tsc0 = rdtsc();

        // Busy-wait for sleep_ms milliseconds
        struct timespec req{ 0, static_cast<long>(sleep_ms) * 1'000'000L };
        ::nanosleep(&req, nullptr);

        tsc1 = rdtsc();
        ::clock_gettime(CLOCK_REALTIME, &t1);

        const uint64_t wall_ns =
            (static_cast<uint64_t>(t1.tv_sec  - t0.tv_sec)  * 1'000'000'000ULL) +
            (static_cast<uint64_t>(t1.tv_nsec) - static_cast<uint64_t>(t0.tv_nsec));

        if (wall_ns == 0 || tsc1 <= tsc0)
            throw std::runtime_error("TscClock::calibrate failed: bad samples");

        // tsc_freq_ghz = ticks_per_ns
        s_tsc_freq_ghz = static_cast<double>(tsc1 - tsc0) / static_cast<double>(wall_ns);

        // Record wall-clock epoch so now_ns() returns real Unix nanoseconds
        s_epoch_tsc     = tsc0;
        s_epoch_wall_ns = static_cast<uint64_t>(t0.tv_sec) * 1'000'000'000ULL +
                          static_cast<uint64_t>(t0.tv_nsec);
        s_calibrated = true;
    }

    // Returns Unix nanoseconds (wall clock aligned).
    [[nodiscard]] static uint64_t now_ns() noexcept {
        const uint64_t tsc = rdtsc();
        return s_epoch_wall_ns +
               static_cast<uint64_t>(
                   static_cast<double>(tsc - s_epoch_tsc) / s_tsc_freq_ghz);
    }

    // Returns raw TSC ticks (useful for latency measurement without division).
    [[nodiscard]] static uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
#else
        // Fallback for non-x86 (e.g. ARM): use clock_gettime
        struct timespec ts{};
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
               static_cast<uint64_t>(ts.tv_nsec);
#endif
    }

    [[nodiscard]] static bool is_calibrated() noexcept { return s_calibrated; }
    [[nodiscard]] static double freq_ghz()    noexcept { return s_tsc_freq_ghz; }

private:
    inline static double   s_tsc_freq_ghz{1.0};
    inline static uint64_t s_epoch_tsc{0};
    inline static uint64_t s_epoch_wall_ns{0};
    inline static bool     s_calibrated{false};
};

}  // namespace sq
