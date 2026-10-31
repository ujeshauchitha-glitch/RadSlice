#pragma once

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "types.hpp"  // riscv-emu: u8/u32/u64

namespace radslice {

// ---------------------------------------------------------------------------
// A static VLIW scheduler over a retired instruction trace (G17).
//
// What the compiler of a VLIW machine would do, done after the fact on the real trace: take one
// basic block at a time, place its operations on W identical K-bit units without breaking a data
// dependence, and report how many cycles the block takes. Nothing here is a measurement of a chip.
//
// Model, all of it tier C and recorded in docs/results.md:
//   - an operation occupies one unit for `latency` cycles, where latency = base cycles + the
//     slice-serial beats the real sliced ALU spent on it (so one unit, in order, gives exactly
//     instret * base + beats, the number the single-unit model already reports);
//   - a result is available when its operation ends; registers are not renamed, so write-after-read
//     and write-after-write order is kept;
//   - a store waits for every earlier memory operation, a load waits for earlier stores;
//   - a branch, jump or system instruction ends the block, and the next block starts only when
//     the whole of this one has finished (no speculation, no branch prediction).
// ---------------------------------------------------------------------------

enum class OpKind : u8 { Alu, Load, Store, Control, Other };

struct TraceOp {
    u8     rd = 0, rs1 = 0, rs2 = 0;   // register numbers; 0 means "none" (x0 is never a dependence)
    OpKind kind = OpKind::Alu;
    u32    latency = 1;                // cycles the operation occupies a unit
    bool   ends_block() const { return kind == OpKind::Control || kind == OpKind::Other; }
};

enum class IssueMode { InOrder, Reorder };

class VliwScheduler {
public:
    explicit VliwScheduler(unsigned width, IssueMode mode) : w_(width), mode_(mode) { reset_block(); }

    // Feed operations in program order; a block is scheduled when one ends it.
    void push(const TraceOp& op) {
        block_.push_back(op);
        if (op.ends_block()) flush();
    }
    // Schedule a trailing block that was never ended by a branch.
    void finish() { if (!block_.empty()) flush(); }

    u64 cycles() const { return total_; }
    u64 ops() const { return ops_; }
    u64 blocks() const { return blocks_; }

private:
    void reset_block() {
        busy_.assign(w_, {});
        ready_.fill(0); last_read_.fill(0); write_end_.fill(0);
        last_store_end_ = last_mem_end_ = prev_issue_ = makespan_ = 0;
    }

    void flush() {
        reset_block();
        for (const TraceOp& op : block_) place(op);
        total_ += makespan_;
        ops_ += block_.size();
        ++blocks_;
        block_.clear();
    }

    static u64 first_fit(std::vector<std::pair<u64, u64>>& iv, u64 earliest, u32 len) {
        std::sort(iv.begin(), iv.end());
        u64 t = earliest;
        for (const auto& [a, b] : iv) {
            if (t + len <= a) break;
            if (b > t) t = b;
        }
        return t;
    }

    void place(const TraceOp& op) {
        u64 earliest = 0;
        if (op.rs1) earliest = std::max(earliest, ready_[op.rs1]);
        if (op.rs2) earliest = std::max(earliest, ready_[op.rs2]);
        if (op.rd) {
            const u64 war = last_read_[op.rd] + 1 > op.latency ? last_read_[op.rd] + 1 - op.latency : 0;
            const u64 waw = write_end_[op.rd] + 1 > op.latency ? write_end_[op.rd] + 1 - op.latency : 0;
            earliest = std::max({earliest, war, waw});
        }
        if (op.kind == OpKind::Load)  earliest = std::max(earliest, last_store_end_);
        if (op.kind == OpKind::Store) earliest = std::max(earliest, last_mem_end_);
        if (mode_ == IssueMode::InOrder) earliest = std::max(earliest, prev_issue_);

        // The earliest slot of `latency` cycles at or after `earliest` on any unit (gaps left by
        // operations placed earlier are usable, as a list scheduler would use them).
        unsigned best = 0;
        u64      start = ~0ull;
        for (unsigned u = 0; u < w_; ++u) {
            const u64 t = first_fit(busy_[u], earliest, op.latency);
            if (t < start) { start = t; best = u; }
        }
        const u64 end = start + op.latency;

        busy_[best].push_back({start, end});
        prev_issue_ = start;
        if (op.rs1) last_read_[op.rs1] = std::max(last_read_[op.rs1], start);
        if (op.rs2) last_read_[op.rs2] = std::max(last_read_[op.rs2], start);
        if (op.rd) { ready_[op.rd] = end; write_end_[op.rd] = end; }
        if (op.kind == OpKind::Store) last_store_end_ = std::max(last_store_end_, end);
        if (op.kind == OpKind::Load || op.kind == OpKind::Store) last_mem_end_ = std::max(last_mem_end_, end);
        makespan_ = std::max(makespan_, end);
    }

    unsigned           w_;
    IssueMode          mode_;
    std::vector<TraceOp> block_;
    std::vector<std::vector<std::pair<u64, u64>>> busy_;   // per unit: placed [start, end) intervals
    std::array<u64, 32> ready_{}, last_read_{}, write_end_{};
    u64 last_store_end_ = 0, last_mem_end_ = 0, prev_issue_ = 0, makespan_ = 0;
    u64 total_ = 0, ops_ = 0, blocks_ = 0;
};

}  // namespace radslice
