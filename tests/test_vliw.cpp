#include <vector>

#include "radslice/vliw.hpp"
#include "test_util.hpp"

// ---------------------------------------------------------------------------
// The static VLIW scheduler (G17), on small blocks whose cycle counts are worked out by hand.
// ---------------------------------------------------------------------------

using namespace radslice;

namespace {

TraceOp alu(u8 rd, u8 rs1, u8 rs2, u32 lat = 1) {
    TraceOp o; o.kind = OpKind::Alu; o.rd = rd; o.rs1 = rs1; o.rs2 = rs2; o.latency = lat; return o;
}
TraceOp load(u8 rd, u8 rs1, u32 lat = 1) {
    TraceOp o; o.kind = OpKind::Load; o.rd = rd; o.rs1 = rs1; o.latency = lat; return o;
}
TraceOp store(u8 rs1, u8 rs2, u32 lat = 1) {
    TraceOp o; o.kind = OpKind::Store; o.rs1 = rs1; o.rs2 = rs2; o.latency = lat; return o;
}
TraceOp branch(u8 rs1, u8 rs2, u32 lat = 1) {
    TraceOp o; o.kind = OpKind::Control; o.rs1 = rs1; o.rs2 = rs2; o.latency = lat; return o;
}

u64 run(unsigned w, IssueMode m, const std::vector<TraceOp>& ops) {
    VliwScheduler s(w, m);
    for (const auto& o : ops) s.push(o);
    s.finish();
    return s.cycles();
}

// One unit, in order, is the plain sum of latencies: the number the single-unit model reports.
void test_one_unit_is_the_sum() {
    const std::vector<TraceOp> ops = {alu(1, 2, 3, 5), alu(4, 5, 6, 3), load(7, 8, 2), store(9, 10, 4), branch(1, 2, 1)};
    CHECK(run(1, IssueMode::InOrder, ops) == 5 + 3 + 2 + 4 + 1);
}

// A chain of dependent operations cannot use more units.
void test_dependent_chain_gains_nothing() {
    std::vector<TraceOp> ops;
    for (int i = 0; i < 8; ++i) ops.push_back(alu(1, 1, 2, 3));   // x1 = x1 + x2, eight times
    for (unsigned w = 1; w <= 4; ++w) {
        CHECK(run(w, IssueMode::InOrder, ops) == 8 * 3);
        CHECK(run(w, IssueMode::Reorder, ops) == 8 * 3);
    }
}

// Independent operations spread across the units: ceil(n / w) rounds of the same latency.
void test_independent_ops_spread() {
    std::vector<TraceOp> ops;
    for (u8 r = 1; r <= 8; ++r) ops.push_back(alu(r, 20, 21, 4));
    CHECK(run(1, IssueMode::InOrder, ops) == 32);
    CHECK(run(2, IssueMode::InOrder, ops) == 16);
    CHECK(run(4, IssueMode::InOrder, ops) == 8);
    CHECK(run(3, IssueMode::InOrder, ops) == 12);      // 3 + 3 + 2 operations
}

// Reordering lets a ready operation overtake a stalled one; in order it cannot.
void test_reorder_beats_in_order() {
    // 1 is slow, 2 needs 1, 3 is independent of both. In order, 3 may not issue before 2.
    const std::vector<TraceOp> ops = {alu(1, 20, 21, 6), alu(2, 1, 21, 2), alu(3, 20, 21, 2)};
    CHECK(run(2, IssueMode::InOrder, ops) == 8);       // 3 waits for 2's issue time (cycle 6), ends at 8
    CHECK(run(2, IssueMode::Reorder, ops) == 8);       // unit 1: op1 then op2 (8); unit 2: op3 ends at 2
    const std::vector<TraceOp> ops2 = {alu(1, 20, 21, 6), alu(2, 1, 21, 2), alu(3, 20, 21, 6)};
    CHECK(run(2, IssueMode::InOrder, ops2) == 12);     // op3 starts at 6 (in order), ends at 12
    CHECK(run(2, IssueMode::Reorder, ops2) == 8);      // op3 starts at 0 on the other unit
}

// Memory order: a store waits for earlier memory operations, a load waits for earlier stores.
void test_memory_order() {
    const std::vector<TraceOp> sl = {store(1, 2, 5), load(3, 4, 5)};
    CHECK(run(2, IssueMode::Reorder, sl) == 10);       // the load cannot pass the store
    const std::vector<TraceOp> ll = {load(3, 4, 5), load(5, 6, 5)};
    CHECK(run(2, IssueMode::Reorder, ll) == 5);        // two loads are independent
    const std::vector<TraceOp> ls = {load(3, 4, 5), store(1, 2, 5)};
    CHECK(run(2, IssueMode::Reorder, ls) == 10);       // the store waits for the earlier load
}

// Write-after-read and write-after-write keep their order (no renaming).
void test_false_dependences() {
    // op1 reads x5 slowly; op2 overwrites x5 quickly. The overwrite must not land before op1 has read.
    const std::vector<TraceOp> war = {alu(1, 5, 6, 4), alu(5, 20, 21, 1)};
    CHECK(run(2, IssueMode::Reorder, war) >= 4);
    // Two writers of x7: the later one must end after the earlier one.
    const std::vector<TraceOp> waw = {alu(7, 20, 21, 5), alu(7, 20, 21, 1)};
    CHECK(run(2, IssueMode::Reorder, waw) == 6);       // second ends at 6, one cycle after the first
}

// A branch ends the block; the next block starts only when the whole of this one has finished.
void test_branch_is_a_barrier() {
    const std::vector<TraceOp> ops = {alu(1, 20, 21, 4), branch(1, 2, 1), alu(3, 20, 21, 4), alu(4, 20, 21, 4)};
    // block 1: alu 4 then branch (needs x1) 1 = 5; block 2: two independent alus on 2 units = 4
    CHECK(run(2, IssueMode::Reorder, ops) == 5 + 4);
    VliwScheduler s(2, IssueMode::Reorder);
    for (const auto& o : ops) s.push(o);
    s.finish();
    CHECK(s.blocks() == 2);
    CHECK(s.ops() == 4);
}

// More units never make it slower.
void test_more_units_never_hurt() {
    std::vector<TraceOp> ops;
    for (int i = 0; i < 40; ++i) {
        ops.push_back(alu(static_cast<u8>(1 + i % 7), static_cast<u8>(1 + (i * 3) % 7), static_cast<u8>(8 + i % 5), 1 + i % 4));
        if (i % 9 == 8) ops.push_back(load(static_cast<u8>(1 + i % 6), 2, 2));
        if (i % 13 == 12) ops.push_back(branch(1, 2));
    }
    for (IssueMode m : {IssueMode::InOrder, IssueMode::Reorder}) {
        u64 prev = ~0ull;
        for (unsigned w = 1; w <= 4; ++w) {
            const u64 c = run(w, m, ops);
            CHECK(c <= prev);
            prev = c;
        }
    }
    for (unsigned w = 1; w <= 4; ++w) CHECK(run(w, IssueMode::Reorder, ops) <= run(w, IssueMode::InOrder, ops));
}

}  // namespace

int main() {
    test_one_unit_is_the_sum();
    test_dependent_chain_gains_nothing();
    test_independent_ops_spread();
    test_reorder_beats_in_order();
    test_memory_order();
    test_false_dependences();
    test_branch_is_a_barrier();
    test_more_units_never_hurt();
    return tu::summary("vliw");
}
