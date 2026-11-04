// Checks rtl/mac_array.v against vectors from the integer reference (scripts/g19_mac_vectors.py).
// The lane-to-element map is rotated by a different offset for every tile, and every physical element
// that no lane uses is given a stuck-at-all-ones accumulator, so any result that leaked from an unused
// element, or any mis-routed weight, shows up. With +mutant one USED element gets a stuck bit instead:
// the run must then report mismatches, which checks that the check can fail.
`timescale 1ns/1ps
module tb_mac_array;
    parameter P = 8;
    parameter S = 4;
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

    integer fd, rc, tiles, bad, n, k, l, off, mutant, pe;
    reg [8*200-1:0] path;
    integer xv, wv;
    integer expv;
    integer got;
    reg [PH-1:0] used;
    reg done_all;

    initial begin
        mutant = 0;
        if ($test$plusargs("mutant")) mutant = 1;
        if (!$value$plusargs("vec=%s", path)) begin $display("FAIL no +vec"); $finish; end
        fd = $fopen(path, "r");
        tiles = 0; bad = 0; done_all = 0;
        while (!done_all) begin
            rc = $fscanf(fd, " T %d", n);
            if (rc != 1) begin
                done_all = 1;
            end else begin
                off = tiles % PH;
                used = 0;
                for (l = 0; l < P; l = l + 1) begin
                    map[l*MW +: MW] = (l + off) % PH;
                    used[(l + off) % PH] = 1'b1;
                end
                smask = 0; sval = 0;
                for (pe = 0; pe < PH; pe = pe + 1)
                    if (!used[pe]) begin smask[pe*32 +: 32] = 32'hffffffff; sval[pe*32 +: 32] = 32'hffffffff; end
                if (mutant) begin smask[((0 + off) % PH)*32 +: 32] = 32'h1; sval[((0 + off) % PH)*32 +: 32] = 32'h1; end
                @(negedge clk); clear = 1; step = 0;
                @(negedge clk); clear = 0;
                for (k = 0; k < n; k = k + 1) begin
                    rc = $fscanf(fd, " %d", xv);
                    x = xv;
                    for (l = 0; l < P; l = l + 1) begin
                        rc = $fscanf(fd, " %d", wv);
                        w[l*8 +: 8] = wv;
                    end
                    step = 1;
                    @(negedge clk);
                end
                step = 0;
                #1;
                for (l = 0; l < P; l = l + 1) begin
                    rc = $fscanf(fd, " %d", expv);
                    got = $signed(result[l*32 +: 32]);
                    if (got !== expv) begin
                        bad = bad + 1;
                        if (bad <= 5) $display("MISMATCH tile %0d lane %0d: got %0d expected %0d", tiles, l, got, expv);
                    end
                end
                tiles = tiles + 1;
            end
        end
        $fclose(fd);
        if (mutant) begin
            if (bad > 0) $display("PASS mutant caught (%0d mismatches over %0d tiles)", bad, tiles);
            else $display("FAIL mutant not caught");
        end else if (bad == 0 && tiles > 0) $display("PASS P=%0d S=%0d %0d tiles, 0 mismatches", P, S, tiles);
        else $display("FAIL P=%0d S=%0d %0d mismatches over %0d tiles", P, S, bad, tiles);
        $finish;
    end
endmodule
