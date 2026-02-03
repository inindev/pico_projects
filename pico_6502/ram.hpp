//
//  RAM implementation for W65C02S emulator in C++
//
//  Copyright 2018-2026, John Clark
//
//  Released under the GNU General Public License
//  https://www.gnu.org/licenses/gpl.html
//

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>

// ============================================================================
//  Hook types and storage
// ============================================================================

using ReadHook = std::function<uint8_t(uint16_t addr)>;
using WriteHook = std::function<void(uint16_t addr, uint8_t val)>;

struct PageHandler {
    ReadHook read;
    WriteHook write;
};

namespace detail {
    struct EmptyHookStorage {};

    struct PageTableStorage {
        std::array<PageHandler, 256> pages_{};
    };
}

// ============================================================================
//  Ram - 64KB memory with optional page-based hooks for memory-mapped I/O
// ============================================================================
//
//  Ram<false> (default): Simple memory, no hooks, zero overhead
//  Ram<true>:            Page-table hooks for multiple I/O regions
//
//  Usage:
//    Ram<> simple_ram;                    // No hooks
//    Ram<true> hooked_ram;                // With hooks
//    hooked_ram.set_read_hook(0xD000, 0xD0FF, keyboard_handler);
//    hooked_ram.set_write_hook(0xD400, 0xD4FF, video_handler);
//

template<bool HasHooks = false>
class Ram : private std::conditional_t<HasHooks, detail::PageTableStorage, detail::EmptyHookStorage> {
public:
    Ram() = default;

    // Non-copyable, non-movable (std::array is large)
    Ram(const Ram&) = delete;
    Ram& operator=(const Ram&) = delete;
    Ram(Ram&&) = delete;
    Ram& operator=(Ram&&) = delete;

    // ========================================================================
    //  Core read/write operations
    // ========================================================================

    uint8_t read(uint16_t addr) const {
        if constexpr (HasHooks) {
            const auto& page = this->pages_[addr >> 8];
            if (page.read) return page.read(addr);
        }
        return mem_[addr];
    }

    void write(uint16_t addr, uint8_t val) {
        mem_[addr] = val;
        if constexpr (HasHooks) {
            const auto& page = this->pages_[addr >> 8];
            if (page.write) page.write(addr, val);
        }
    }

    uint16_t read_word(uint16_t addr) const {
        return read(addr) | (static_cast<uint16_t>(read(addr + 1)) << 8);
    }

    void write_word(uint16_t addr, uint16_t val) {
        write(addr, val & 0xff);
        write(addr + 1, (val >> 8) & 0xff);
    }

    // Direct memory access (bypasses hooks)
    uint8_t* data() { return mem_.data(); }
    const uint8_t* data() const { return mem_.data(); }
    static constexpr size_t size() { return 0x10000; }

    uint8_t& operator[](uint16_t addr) { return mem_[addr]; }
    const uint8_t& operator[](uint16_t addr) const { return mem_[addr]; }

    // ========================================================================
    //  Hook management (only available when HasHooks=true)
    // ========================================================================

    // Set read hook for address range (applies to all pages in range)
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void set_read_hook(uint16_t addr_begin, uint16_t addr_end, ReadHook hook) {
        uint8_t page_begin = addr_begin >> 8;
        uint8_t page_end = addr_end >> 8;
        for (unsigned page = page_begin; page <= page_end; ++page) {
            this->pages_[page].read = hook;
        }
    }

    // Set write hook for address range (applies to all pages in range)
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void set_write_hook(uint16_t addr_begin, uint16_t addr_end, WriteHook hook) {
        uint8_t page_begin = addr_begin >> 8;
        uint8_t page_end = addr_end >> 8;
        for (unsigned page = page_begin; page <= page_end; ++page) {
            this->pages_[page].write = hook;
        }
    }

    // Set read hook for a single page
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void set_read_hook(uint8_t page, ReadHook hook) {
        this->pages_[page].read = std::move(hook);
    }

    // Set write hook for a single page
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void set_write_hook(uint8_t page, WriteHook hook) {
        this->pages_[page].write = std::move(hook);
    }

    // Clear read hook for a single page
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void clear_read_hook(uint8_t page) {
        this->pages_[page].read = nullptr;
    }

    // Clear write hook for a single page
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void clear_write_hook(uint8_t page) {
        this->pages_[page].write = nullptr;
    }

    // Clear all hooks
    template<bool H = HasHooks, typename = std::enable_if_t<H>>
    void clear_hooks() {
        for (auto& page : this->pages_) {
            page.read = nullptr;
            page.write = nullptr;
        }
    }

    // ========================================================================
    //  Utility functions
    // ========================================================================

    void reset() {
        mem_.fill(0);
        if constexpr (HasHooks) {
            clear_hooks();
        }
    }

    void fill(uint8_t val, uint16_t addr_begin, uint16_t addr_end) {
        if (addr_begin > addr_end) return;
        for (uint32_t addr = addr_begin; addr <= addr_end; ++addr) {
            write(static_cast<uint16_t>(addr), val);
        }
    }

    void apply(uint16_t offset, const uint8_t* src, size_t len) {
        for (size_t i = 0; i < len && (offset + i) < size(); ++i) {
            write(static_cast<uint16_t>(offset + i), src[i]);
        }
    }

    // Load data directly (bypasses write hooks)
    void load(uint16_t offset, const uint8_t* src, size_t len) {
        size_t copy_len = std::min(len, size() - offset);
        std::copy_n(src, copy_len, mem_.data() + offset);
    }

    std::string hexdump(uint16_t addr_begin, uint16_t addr_end, bool ascii = true) const {
        if (addr_begin > addr_end) return {};

        std::string out;
        out.reserve(((addr_end - addr_begin) / 16 + 1) * 80);

        for (uint32_t i = addr_begin; i <= addr_end; i += 16) {
            char line[128];
            int pos = snprintf(line, sizeof(line), "%04X  ", i & 0xFFFF);

            char asc[17] = {};
            int asc_idx = 0;

            for (uint32_t j = i; j < i + 16; ++j) {
                if (j <= addr_end) {
                    uint8_t val = read(static_cast<uint16_t>(j));
                    pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", val);
                    asc[asc_idx++] = (val >= 0x20 && val <= 0x7e) ? static_cast<char>(val) : '.';
                } else {
                    pos += snprintf(line + pos, sizeof(line) - pos, "   ");
                    asc[asc_idx++] = ' ';
                }
                if (j == i + 7) {
                    line[pos++] = ' ';
                    line[pos] = '\0';
                }
            }

            if (ascii) {
                snprintf(line + pos, sizeof(line) - pos, " |%s|\n", asc);
            } else {
                snprintf(line + pos, sizeof(line) - pos, "\n");
            }

            out += line;
        }

        return out;
    }

    // ========================================================================
    //  CPU integration via static binding
    // ========================================================================
    //
    // Since W65C02S uses plain function pointers for ram_read/ram_write,
    // we use a static instance pointer.
    //
    // Usage:
    //   Ram<>::set_instance(&ram);
    //   cpu.ram_read = &Ram<>::static_read;
    //   cpu.ram_write = &Ram<>::static_write;
    //

    static void set_instance(Ram* ram) { instance_ = ram; }

    static uint8_t static_read(uint16_t addr) {
        return instance_ ? instance_->read(addr) : 0;
    }

    static void static_write(uint16_t addr, uint8_t val) {
        if (instance_) instance_->write(addr, val);
    }

private:
    std::array<uint8_t, 0x10000> mem_{};
    static inline Ram* instance_{};
};

// ============================================================================
//  Type aliases for convenience
// ============================================================================

using SimpleRam = Ram<false>;
using HookedRam = Ram<true>;
