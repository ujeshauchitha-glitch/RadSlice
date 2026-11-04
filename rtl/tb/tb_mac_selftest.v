// G19 step 3: defective chips, boot self-test and remap, end to end on rtl/mac_array.v.
//
// For each chip in +chips (scripts/g19_selftest_vectors.py):
//   1. RAW   with the identity map, the defective array's outputs on NRAW tiles must equal the expected values
//            computed by the separately written Python model (checks the stuck-bit semantics of the Verilog).
//   2. TEST  the boot self-test: clear, add 1*0, read, add -1*1, read. A healthy element reads 0 then -1; a bit
//            stuck at 1 shows on the first read, a bit stuck at 0 on the second. Every element is tested, P at a
//            time through the lane map. The testbench plays the controller here.
//   3. MAP   the first P healthy elements serve the P lanes. If fewer than P are healthy the chip is rejected;
//            that decision must equal the expected one (defective elements <= S).
//   4. RUN   an accepted chip must now reproduce the exact sums on all tiles in +vec.
//   5. FLIP  on a fault-free array one accumulator bit is flipped at a chosen step; outputs must equal the model.
`timescale 1ns/1ps
module tb_mac_selftest;
    parameter P = 8;
    parameter S = 4;
    parameter NT = 3;           // tiles read from +vec (the last one is also the flip tile)
    parameter NRAW = 2;
    parameter MAXN = 80;
    localparam PH = P + S;
    localparam MW = (PH <= 2) ? 1 : $clog2(PH);

    reg clk = 0;
    always #5 clk = ~clk;

    reg clear = 0, step = 0;
    reg signed [7:0] x = 0;
    reg [P*8-1:0] w = 0;
    reg [P*MW-1:0] map = 0;
    reg [PH*32-1:0] smask = 0, sval = 0;
    reg flip_en = 0;
    reg [MW-1:0] flip_pe = 0;
    reg [5:0] flip_bit = 0;
    wire [P*32-1:0] result;

    mac_array #(.P(P), .S(S)) dut (.clk(clk), .clear(clear), .step(step), .x(x), .w(w), .map(map),
        .stuck_mask(smask), .stuck_val(sval), .flip_en(flip_en), .flip_pe(flip_pe), .flip_bit(flip_bit),
        .result(result));

    // tiles
    integer tn [0:NT-1];
    integer txs [0:NT*MAXN-1];
    reg [P*8-1:0] tws [0:NT*MAXN-1];
    integer texp [0:NT*P-1];

    integer fd, rc, t, k, l, e, c, r, i;
    reg [8*200-1:0] vpath, cpath;
    integer wv, xv, expv, got;
    integer nchips, bad_raw, bad_dec, bad_run, bad_flip, accepted, rejected;
    integer acc_flag, ftile, fstep, fel, fbit, nhealthy, rounds, base_e;
    reg [PH-1:0] healthy;
    reg [31:0] mk, vl;
    reg [31:0] a_read [0:P-1];
    integer idx;

    // run one tile with the current map and optional flip; leaves results in `result`
    task run_tile(input integer tile, input integer do_flip, input integer fs, input integer fe, input integer fb);
        integer kk, ll;
        begin
            @(negedge clk); clear = 1; step = 0; flip_en = 0;
            @(negedge clk); clear = 0;
            for (kk = 0; kk < tn[tile]; kk = kk + 1) begin
                x = txs[tile*MAXN + kk];
                w = tws[tile*MAXN + kk];
                step = 1;
                flip_en = (do_flip != 0) && (kk == fs);
                flip_pe = fe;
                flip_bit = fb;
                @(negedge clk);
            end
            step = 0; flip_en = 0;
            #1;
        end
    endtask

    initial begin
        if (!$value$plusargs("vec=%s", vpath)) begin $display("FAIL no +vec"); $finish; end
        if (!$value$plusargs("chips=%s", cpath)) begin $display("FAIL no +chips"); $finish; end
        // load tiles
        fd = $fopen(vpath, "r");
        for (t = 0; t < NT; t = t + 1) begin
            rc = $fscanf(fd, " T %d", tn[t]);
            for (k = 0; k < tn[t]; k = k + 1) begin
                rc = $fscanf(fd, " %d", xv);
                txs[t*MAXN + k] = xv;
                for (l = 0; l < P; l = l + 1) begin
                    rc = $fscanf(fd, " %d", wv);
                    tws[t*MAXN + k][l*8 +: 8] = wv;
                end
            end
            for (l = 0; l < P; l = l + 1) begin
                rc = $fscanf(fd, " %d", expv);
                texp[t*P + l] = expv;
            end
        end
        $fclose(fd);

        fd = $fopen(cpath, "r");
        nchips = 0; bad_raw = 0; bad_dec = 0; bad_run = 0; bad_flip = 0; accepted = 0; rejected = 0;
        rc = $fscanf(fd, " C %d", acc_flag);
        while (rc == 1) begin
            smask = 0; sval = 0;
            for (e = 0; e < PH; e = e + 1) begin
                rc = $fscanf(fd, " %h %h", mk, vl);
                smask[e*32 +: 32] = mk;
                sval[e*32 +: 32] = vl;
            end
            // 1. RAW
            for (l = 0; l < P; l = l + 1) map[l*MW +: MW] = l;
            for (t = 0; t < NRAW; t = t + 1) begin
                run_tile(t, 0, 0, 0, 0);
                for (l = 0; l < P; l = l + 1) begin
                    rc = $fscanf(fd, " %d", expv);
                    got = $signed(result[l*32 +: 32]);
                    if (got !== expv) begin
                        bad_raw = bad_raw + 1;
                        if (bad_raw <= 3) $display("RAW MISMATCH chip %0d tile %0d lane %0d: got %0d expected %0d", nchips, t, l, got, expv);
                    end
                end
            end
            // 2. TEST: every element, P at a time
            healthy = {PH{1'b1}};
            rounds = (PH + P - 1) / P;
            for (r = 0; r < rounds; r = r + 1) begin
                for (l = 0; l < P; l = l + 1) begin
                    idx = r*P + l;
                    if (idx >= PH) idx = PH - 1;        // spare lanes of the last round repeat an element
                    map[l*MW +: MW] = idx;
                end
                @(negedge clk); clear = 1; step = 0;
                @(negedge clk); clear = 0;
                x = 1; for (l = 0; l < P; l = l + 1) w[l*8 +: 8] = 0;
                step = 1; @(negedge clk); step = 0; #1;
                for (l = 0; l < P; l = l + 1) a_read[l] = result[l*32 +: 32];
                x = -1; for (l = 0; l < P; l = l + 1) w[l*8 +: 8] = 1;
                step = 1; @(negedge clk); step = 0; #1;
                for (l = 0; l < P; l = l + 1) begin
                    idx = r*P + l;
                    if (idx < PH)
                        if (a_read[l] !== 32'd0 || result[l*32 +: 32] !== 32'hffffffff) healthy[idx] = 1'b0;
                end
            end
            nhealthy = 0;
            for (e = 0; e < PH; e = e + 1) if (healthy[e]) nhealthy = nhealthy + 1;
            // 3. MAP and decide
            if ((nhealthy >= P) !== (acc_flag == 1)) begin
                bad_dec = bad_dec + 1;
                if (bad_dec <= 3) $display("DECISION MISMATCH chip %0d: healthy %0d expected accept=%0d", nchips, nhealthy, acc_flag);
            end
            if (nhealthy >= P) begin
                accepted = accepted + 1;
                l = 0;
                for (e = 0; e < PH; e = e + 1)
                    if (healthy[e] && l < P) begin map[l*MW +: MW] = e; l = l + 1; end
                // 4. RUN
                for (t = 0; t < NT; t = t + 1) begin
                    run_tile(t, 0, 0, 0, 0);
                    for (l = 0; l < P; l = l + 1) begin
                        got = $signed(result[l*32 +: 32]);
                        if (got !== texp[t*P + l]) begin
                            bad_run = bad_run + 1;
                            if (bad_run <= 3) $display("RUN MISMATCH chip %0d tile %0d lane %0d: got %0d expected %0d", nchips, t, l, got, texp[t*P + l]);
                        end
                    end
                end
            end else rejected = rejected + 1;
            // 5. FLIP on a fault-free array with the identity map
            smask = 0; sval = 0;
            for (l = 0; l < P; l = l + 1) map[l*MW +: MW] = l;
            rc = $fscanf(fd, " F %d %d %d %d", ftile, fstep, fel, fbit);
            run_tile(ftile, 1, fstep, fel, fbit);
            for (l = 0; l < P; l = l + 1) begin
                rc = $fscanf(fd, " %d", expv);
                got = $signed(result[l*32 +: 32]);
                if (got !== expv) begin
                    bad_flip = bad_flip + 1;
                    if (bad_flip <= 3) $display("FLIP MISMATCH chip %0d lane %0d: got %0d expected %0d", nchips, l, got, expv);
                end
            end
            nchips = nchips + 1;
            rc = $fscanf(fd, " C %d", acc_flag);
        end
        $fclose(fd);
        if (nchips > 0 && bad_raw == 0 && bad_dec == 0 && bad_run == 0 && bad_flip == 0)
            $display("PASS P=%0d S=%0d chips=%0d accepted=%0d rejected=%0d (raw, decision, repaired run and flip all match)", P, S, nchips, accepted, rejected);
        else
            $display("FAIL P=%0d S=%0d chips=%0d raw=%0d decision=%0d run=%0d flip=%0d", P, S, nchips, bad_raw, bad_dec, bad_run, bad_flip);
        $finish;
    end
endmodule
