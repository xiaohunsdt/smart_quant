#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sys/mman.h>  // MAP_FAILED

namespace sq {

// ── Constants ─────────────────────────────────────────────────────────────────

// Magic bytes spelling "SQFIXLOG" in little-endian
static constexpr uint64_t kLogMagic   = 0x474F4C584946'5153ULL;
static constexpr uint32_t kLogVersion = 1;
// Header occupies a full OS page so data starts on a page boundary
static constexpr size_t   kHeaderSize = 4096;

// ── On-disk structures ────────────────────────────────────────────────────────

// Placed at offset 0 of the mmap'd file.
// alignas(64) ensures write_pos sits on its own cache line.
struct alignas(64) LogFileHeader {
    uint64_t magic;                    //  8 B
    uint32_t version;                  //  4 B
    uint32_t reserved;                 //  4 B
    uint64_t buffer_size;              //  8 B  — usable ring bytes after header
    std::atomic<uint64_t> write_pos;   //  8 B  — monotonic, never wraps
    // Padding to fill the rest of the 4096-byte header page (ensures the data
    // ring starts on a page boundary and no false sharing with ring data).
    char _pad[kHeaderSize - 32];
};
static_assert(sizeof(LogFileHeader) == kHeaderSize, "header size mismatch");

// Prepended to every record in the ring buffer.
// Packed to exactly 12 bytes — no padding between timestamp_ns and length.
#pragma pack(push, 1)
struct RecordHeader {
    uint64_t timestamp_ns;  // 8 B — from TscClock::now_ns()
    uint32_t length;        // 4 B — payload byte count
    // followed immediately by `length` payload bytes (raw FIX message)
};
#pragma pack(pop)
static constexpr size_t kRecordHeaderSize = sizeof(RecordHeader);
static_assert(kRecordHeaderSize == 12, "unexpected RecordHeader size");

// ── BinaryLogger ─────────────────────────────────────────────────────────────

// mmap-backed ring-buffer logger for raw FIX messages.
//
// Single-writer design: write() must only be called from one thread.
// The Reader inner class is safe to open in a separate process/thread
// (read-only mmap of the same file).
class BinaryLogger {
public:
    // Opens (or creates) `path` and maps it.
    // `buffer_size` must be > 0; it is rounded up to a multiple of the OS page
    // size.  The file is truncated to kHeaderSize + buffer_size on every open.
    explicit BinaryLogger(const std::string& path,
                          size_t buffer_size = 512ULL << 20);
    ~BinaryLogger();

    BinaryLogger(const BinaryLogger&)            = delete;
    BinaryLogger& operator=(const BinaryLogger&) = delete;
    BinaryLogger(BinaryLogger&&)                 = delete;
    BinaryLogger& operator=(BinaryLogger&&)      = delete;

    // Write one raw FIX message to the ring.  Never allocates.
    // ts_ns should come from TscClock::now_ns().
    void write(uint64_t ts_ns, const char* data, uint32_t len) noexcept;
    void write(uint64_t ts_ns, std::string_view sv) noexcept {
        write(ts_ns, sv.data(), static_cast<uint32_t>(sv.size()));
    }

    // Non-blocking async flush (msync MS_ASYNC).  Call periodically.
    void flush() noexcept;

    // Total bytes written so far (monotonic).
    [[nodiscard]] uint64_t total_written() const noexcept;

    // ── Replay reader (for backtest) ──────────────────────────────────────────
    class Reader {
    public:
        struct Entry {
            uint64_t    timestamp_ns;
            uint32_t    length;
            const char* data;   // valid while this Reader is alive
        };

        // Opens `path` read-only.  Snapshots write_pos at construction time
        // so the reader sees a consistent view of data written up to that
        // point.  Does not follow new writes after construction.
        explicit Reader(const std::string& path);
        ~Reader();

        Reader(const Reader&)            = delete;
        Reader& operator=(const Reader&) = delete;

        // Advances to the next record.  Returns false when all records have
        // been consumed or an incomplete record is encountered.
        bool next(Entry& entry) noexcept;

        // Reset read cursor to the oldest record still in the ring.
        void rewind() noexcept;

        [[nodiscard]] uint64_t total_records() const noexcept { return total_records_; }
        [[nodiscard]] uint64_t bytes_available() const noexcept {
            return end_pos_ - read_pos_;
        }

    private:
        // Copies n bytes from the ring starting at read_pos_, advancing it.
        void ring_read_into(char* dst, size_t n) noexcept;

        int          fd_{-1};
        void*        mapping_{MAP_FAILED};
        size_t       file_size_{0};

        const LogFileHeader* header_{nullptr};
        const char*          buf_{nullptr};
        size_t               buf_size_{0};

        uint64_t read_pos_{0};      // monotonic, same space as write_pos
        uint64_t end_pos_{0};       // snapshot of write_pos at Reader open time
        uint64_t total_records_{0};

        // Bounce buffer for records that straddle the ring boundary.
        // Only used at the ring-wrap point — never on the hot path.
        std::vector<char> bounce_;
    };

private:
    // Writes n bytes from src into the ring at the current `off` position,
    // handling wrap-around.  Updates `off` in-place.
    static void ring_write(char* buf, size_t buf_size,
                           size_t& off,
                           const char* src, size_t n) noexcept;

    int            fd_{-1};
    void*          mapping_{MAP_FAILED};
    size_t         file_size_{0};

    LogFileHeader* header_{nullptr};
    char*          buf_{nullptr};
    size_t         buf_size_{0};
};

}  // namespace sq
