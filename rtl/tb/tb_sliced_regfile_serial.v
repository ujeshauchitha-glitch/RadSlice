// Testbench: replay the operation sequences written by radslice_rf_vectors against rtl/sliced_regfile_serial.v
// and compare every read (value and parity flag) and every physical read with the model's answer.
//
//   iverilog -g2012 -o tb -P tb_sliced_regfile_serial.SPARES=8 rtl/sliced_regfile_serial.v rtl/tb/tb_sliced_regfile_serial.v
//   vvp tb +vec=vectors.txt
`timescale 1ns/1ps
module tb_sliced_regfile_serial;
    parameter SPARES = 8;

    reg clk = 0, rst = 1;
    reg  [4:0]  rd1_idx = 0, rd2_idx = 0, wr_idx = 0, flip_word = 0;
    reg  [2:0]  rd1_slice = 0, rd2_slice = 0, wr_slice = 0, def_bit = 0;
    reg         rd1_en = 0, rd2_en = 0, wr_en = 0, remap_we = 0, def_we = 0, flip_we = 0, ph_we = 0, def_val = 0, def_en = 0;
    reg  [7:0]  wr_byte = 0, remap_logical = 0, ph_wdata = 0;
    reg  [8:0]  remap_phys = 0, def_phys = 0, ph_addr = 0;
    reg  [5:0]  flip_bit = 0;
    wire [7:0]  rd1_byte, rd2_byte, ph_rdata;
    wire        rd1_perr, rd2_perr;

    sliced_regfile_serial #(.SPARES(SPARES)) dut (
        .clk(clk), .rst(rst),
        .rd1_en(rd1_en), .rd1_idx(rd1_idx), .rd1_slice(rd1_slice), .rd1_byte(rd1_byte), .rd1_perr(rd1_perr),
        .rd2_en(rd2_en), .rd2_idx(rd2_idx), .rd2_slice(rd2_slice), .rd2_byte(rd2_byte), .rd2_perr(rd2_perr),
        .wr_en(wr_en), .wr_idx(wr_idx), .wr_slice(wr_slice), .wr_byte(wr_byte),
        .remap_we(remap_we), .remap_logical(remap_logical), .remap_phys(remap_phys),
        .def_we(def_we), .def_phys(def_phys), .def_bit(def_bit), .def_val(def_val), .def_en(def_en),
        .flip_we(flip_we), .flip_word(flip_word), .flip_bit(flip_bit),
        .ph_addr(ph_addr), .ph_wdata(ph_wdata), .ph_we(ph_we), .ph_rdata(ph_rdata));

    always #5 clk = ~clk;

    integer fd, rc, n, bad, shown, reads, sl;
    reg [63:0] got1, got2;
    reg        pe1, pe2;
    reg [3:0]  op;
    reg [63:0] a, b, c;
    reg [1023:0] path;

    task fail;
        input [255:0] what;
        begin
            bad = bad + 1;
            if (shown < 10) begin
                shown = shown + 1;
                $display("FAIL op %0d (%0s): a=%h b=%h c=%h  read1=%h perr1=%b read2=%h perr2=%b ph=%h",
                         op, what, a, b, c, got1, pe1, got2, pe2, ph_rdata);
            end
        end
    endtask

    initial begin
        bad = 0; shown = 0; n = 0; reads = 0;
        if (!$value$plusargs("vec=%s", path)) begin $display("FAIL: pass +vec=<file>"); $finish; end
        fd = $fopen(path, "r");
        if (fd == 0) begin $display("FAIL: cannot open %0s", path); $finish; end
        repeat (3) @(posedge clk);
        rst <= 0;
        @(posedge clk);

        rc = $fscanf(fd, "%h %h %h %h\n", op, a, b, c);
        while (rc == 4) begin
            case (op)
                0: begin
                    for (sl = 0; sl < 8; sl = sl + 1) begin
                        wr_idx <= a[4:0]; wr_slice <= sl[2:0]; wr_byte <= b[8*sl +: 8]; wr_en <= 1;
                        @(posedge clk);
                    end
                    wr_en <= 0;
                end
                1: begin
                    // both ports read the same word on the same beats
                    for (sl = 0; sl < 8; sl = sl + 1) begin
                        rd1_idx <= a[4:0]; rd2_idx <= a[4:0]; rd1_slice <= sl[2:0]; rd2_slice <= sl[2:0];
                        rd1_en <= 1; rd2_en <= 1;
                        @(negedge clk);
                        got1[8*sl +: 8] = rd1_byte; got2[8*sl +: 8] = rd2_byte;
                        pe1 = rd1_perr; pe2 = rd2_perr;
                        @(posedge clk);
                    end
                    rd1_en <= 0; rd2_en <= 0;
                    if (got1 !== b || pe1 !== ~c[0]) fail("read");
                    if (got2 !== got1 || pe2 !== pe1) fail("port2");
                    reads = reads + 1;
                end
                2: begin def_phys <= a[8:0]; def_bit <= b[2:0]; def_val <= c[0]; def_en <= 1; def_we <= 1; @(posedge clk); def_we <= 0; end
                3: begin remap_logical <= a[7:0]; remap_phys <= b[8:0]; remap_we <= 1; @(posedge clk); remap_we <= 0; end
                4: begin flip_word <= a[4:0]; flip_bit <= b[5:0]; flip_we <= 1; @(posedge clk); flip_we <= 0; end
                5: begin ph_addr <= a[8:0]; @(negedge clk); if (ph_rdata !== b[7:0]) fail("phys read"); reads = reads + 1; end
                6: begin ph_addr <= a[8:0]; ph_wdata <= b[7:0]; ph_we <= 1; @(posedge clk); ph_we <= 0; end
                7: begin def_phys <= a[8:0]; def_bit <= 3'd0; def_val <= 1'b0; def_en <= 0; def_we <= 1; @(posedge clk); def_we <= 0; end
            endcase
            n = n + 1;
            rc = $fscanf(fd, "%h %h %h %h\n", op, a, b, c);
        end
        $fclose(fd);
        if (bad == 0) $display("PASS SPARES=%0d: %0d operations, %0d checked reads (value and parity) match the model", SPARES, n, reads);
        else          $display("FAIL SPARES=%0d: %0d mismatches in %0d operations", SPARES, bad, n);
        $finish;
    end

    initial begin #500000000; $display("FAIL: timeout"); $finish; end
endmodule
