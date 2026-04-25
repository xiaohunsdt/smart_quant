#include "smartquant/log/binary_logger.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sq {

// ── Helpers ───────────────────────────────────────────────────────────────────

static size_t round_up_page(size_t n) {
    const long page = ::sysconf(_SC_PAGE_SIZE);
    const size_t ps = (page > 0) ? static_cast<size_t>(page) : 4096UL;
    return ((n + ps - 1) / ps) * ps;
}

// ── BinaryLogger ─────────────────────────────────────────────────────────────

BinaryLogger::BinaryLogger(const std::string& path, size_t buffer_size) {
    if (buffer_size == 0)
        throw std::invalid_argument("BinaryLogger: buffer_size must be > 0");

    buf_size_  = round_up_page(buffer_size);
    file_size_ = kHeaderSize + buf_size_;

    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0)
        throw std::system_error(errno, std::system_category(),
                                "BinaryLogger: open " + path);

    if (::ftruncate(fd_, static_cast<off_t>(file_size_)) < 0) {
        ::close(fd_);
        throw std::system_error(errno, std::system_category(),
                                "BinaryLogger: ftruncate");
    }

    mapping_ = ::mmap(nullptr, file_size_,
                      PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        ::close(fd_);
        throw std::system_error(errno, std::system_category(),
                                "BinaryLogger: mmap");
    }

    // Hint: sequential write pattern
    ::madvise(mapping_, file_size_, MADV_SEQUENTIAL);

    // Initialise header via placement new
    header_ = new (mapping_) LogFileHeader{};
    header_->magic       = kLogMagic;
    header_->version     = kLogVersion;
    header_->reserved    = 0;
    header_->buffer_size = buf_size_;
    header_->write_pos.store(0, std::memory_order_relaxed);

    buf_ = static_cast<char*>(mapping_) + kHeaderSize;
}

BinaryLogger::~BinaryLogger() {
    if (mapping_ != MAP_FAILED) {
        ::msync(mapping_, file_size_, MS_SYNC);
        ::munmap(mapping_, file_size_);
    }
    if (fd_ >= 0)
        ::close(fd_);
}

// static helper
void BinaryLogger::ring_write(char* buf, size_t buf_size,
                              size_t& off,
                              const char* src, size_t n) noexcept {
    const size_t avail = buf_size - off;
    if (n <= avail) {
        std::memcpy(buf + off, src, n);
        off += n;
    } else {
        std::memcpy(buf + off, src, avail);
        std::memcpy(buf,       src + avail, n - avail);
        off = n - avail;
    }
}

void BinaryLogger::write(uint64_t ts_ns, const char* data, uint32_t len) noexcept {
    const size_t total = kRecordHeaderSize + len;

    // Single-writer: relaxed load is safe here
    const uint64_t pos = header_->write_pos.load(std::memory_order_relaxed);
    size_t off = static_cast<size_t>(pos % buf_size_);

    const RecordHeader rh{ts_ns, len};
    ring_write(buf_, buf_size_, off,
               reinterpret_cast<const char*>(&rh), kRecordHeaderSize);
    ring_write(buf_, buf_size_, off, data, len);

    // release: consumer observing write_pos sees the complete record
    header_->write_pos.store(pos + total, std::memory_order_release);
}

void BinaryLogger::flush() noexcept {
    if (mapping_ != MAP_FAILED)
        ::msync(mapping_, file_size_, MS_ASYNC);
}

uint64_t BinaryLogger::total_written() const noexcept {
    return header_->write_pos.load(std::memory_order_relaxed);
}

// ── BinaryLogger::Reader ─────────────────────────────────────────────────────

BinaryLogger::Reader::Reader(const std::string& path) {
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0)
        throw std::system_error(errno, std::system_category(),
                                "BinaryLogger::Reader: open " + path);

    struct stat st{};
    if (::fstat(fd_, &st) < 0) {
        ::close(fd_);
        throw std::system_error(errno, std::system_category(),
                                "BinaryLogger::Reader: fstat");
    }
    file_size_ = static_cast<size_t>(st.st_size);

    if (file_size_ < kHeaderSize)
        throw std::runtime_error("BinaryLogger::Reader: file too small");

    mapping_ = ::mmap(nullptr, file_size_, PROT_READ, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        ::close(fd_);
        throw std::system_error(errno, std::system_category(),
                                "BinaryLogger::Reader: mmap");
    }
    ::madvise(mapping_, file_size_, MADV_SEQUENTIAL);

    header_ = static_cast<const LogFileHeader*>(mapping_);
    if (header_->magic != kLogMagic)
        throw std::runtime_error("BinaryLogger::Reader: bad magic — "
                                 "not a valid sq binary log file");

    buf_      = static_cast<const char*>(mapping_) + kHeaderSize;
    buf_size_ = header_->buffer_size;

    // Snapshot write_pos so the reader sees data written up to this point
    end_pos_ = header_->write_pos.load(std::memory_order_acquire);

    // If more data has been written than the ring can hold, skip stale bytes
    read_pos_ = (end_pos_ > buf_size_) ? (end_pos_ - buf_size_) : 0;
}

BinaryLogger::Reader::~Reader() {
    if (mapping_ != MAP_FAILED)
        ::munmap(mapping_, file_size_);
    if (fd_ >= 0)
        ::close(fd_);
}

void BinaryLogger::Reader::ring_read_into(char* dst, size_t n) noexcept {
    const size_t off   = static_cast<size_t>(read_pos_ % buf_size_);
    const size_t avail = buf_size_ - off;
    if (n <= avail) {
        std::memcpy(dst, buf_ + off, n);
    } else {
        std::memcpy(dst,        buf_ + off, avail);
        std::memcpy(dst + avail, buf_,      n - avail);
    }
    read_pos_ += n;
}

bool BinaryLogger::Reader::next(Entry& entry) noexcept {
    if (read_pos_ + kRecordHeaderSize > end_pos_)
        return false;

    RecordHeader rh{};
    ring_read_into(reinterpret_cast<char*>(&rh), kRecordHeaderSize);

    if (read_pos_ + rh.length > end_pos_) {
        // Incomplete record — revert and signal end-of-stream
        read_pos_ -= kRecordHeaderSize;
        return false;
    }

    const size_t data_off = static_cast<size_t>(read_pos_ % buf_size_);
    const size_t avail    = buf_size_ - data_off;

    if (rh.length <= avail) {
        // Fast path: payload is contiguous in the ring — zero-copy pointer
        entry.data = buf_ + data_off;
        read_pos_ += rh.length;
    } else {
        // Slow path: payload straddles the ring boundary — copy to bounce buf
        bounce_.resize(rh.length);
        ring_read_into(bounce_.data(), rh.length);
        entry.data = bounce_.data();
    }

    entry.timestamp_ns = rh.timestamp_ns;
    entry.length       = rh.length;
    ++total_records_;
    return true;
}

void BinaryLogger::Reader::rewind() noexcept {
    end_pos_       = header_->write_pos.load(std::memory_order_acquire);
    read_pos_      = (end_pos_ > buf_size_) ? (end_pos_ - buf_size_) : 0;
    total_records_ = 0;
}

}  // namespace sq
