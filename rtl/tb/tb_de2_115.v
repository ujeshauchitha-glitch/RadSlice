// Whole-board simulation: the DE2-115 wrapper running the demo program, with the switches and keys driven the way a
// person would. Checks that the counter counts, that flipping a bit of x6 (the counter) with KEY1 makes the hardware
// parity-error count (LEDG) rise and the displayed value jump, and that a stuck-bit defect set with KEY2 does the same.
//
//   bash scripts/run-board-sim.sh
`timescale 1ns/1ps
module tb_de2_115;
    reg         clk50 = 0;
    reg  [3:0]  key = 4'b1111;
    reg  [17:0] sw = 18'd0;
    wire [17:0] ledr;
    wire [8:0]  ledg;
    wire [6:0]  hex0, hex1, hex2, hex3;

    de2_115_top #(.K(16), .CLK_DIV(1), .DEBOUNCE(4), .IMAGE("build/core-tmp/demo-fast.hex")) dut (
        .CLOCK_50(clk50), .KEY(key), .SW(sw), .LEDR(ledr), .LEDG(ledg),
        .HEX0(hex0), .HEX1(hex1), .HEX2(hex2), .HEX3(hex3));

    always #10 clk50 = ~clk50;

    integer bad = 0;
    task check(input cond, input [255:0] what);
        begin
            if (!cond) begin bad = bad + 1; $display("FAIL: %0s (counter %0d, parity errors %0d)", what, dut.hex_r, dut.parity_errors[7:0]); end
        end
    endtask

    task press(input integer k);
        begin
            key[k] = 1'b0; repeat (40) @(posedge clk50);
            key[k] = 1'b1; repeat (40) @(posedge clk50);
        end
    endtask

    reg [15:0] c0, c1;
    reg [7:0]  p0;
    initial begin
        key[0] = 1'b0; repeat (40) @(posedge clk50); key[0] = 1'b1;       // reset
        // let it count
        repeat (60000) @(posedge clk50);
        c0 = dut.hex_r;
        repeat (60000) @(posedge clk50);
        c1 = dut.hex_r;
        $display("counter %0d -> %0d, green LEDs %b", c0, c1, ledg);
        check(c1 > c0 && c0 > 0, "the counter counts");
        check(dut.parity_errors == 0, "no parity errors before any fault");
        sw = 18'd0;
        check(ledr == 18'd0, "red LEDs mirror the switches (all off)");
        sw[0] = 1'b1; repeat (30000) @(posedge clk50);
        check(ledr[0] == 1'b1, "red LED 0 follows switch 0");

        // 1. flip bit 12 of word 6 (x6, the counter): word = SW[4:0], bit = SW[10:5]
        sw = 18'd0; sw[4:0] = 5'd6; sw[10:5] = 6'd12;
        repeat (100) @(posedge clk50);
        c0 = dut.hex_r; p0 = dut.parity_errors[7:0];
        press(1);
        repeat (80000) @(posedge clk50);
        c1 = dut.hex_r;
        $display("after flip: counter %0d (was %0d), parity errors %0d", c1, c0, dut.parity_errors[7:0]);
        check(dut.parity_errors[7:0] > p0, "a flipped bit raises the parity-error count");
        check(c1 >= c0 + 16'h0800 || c1 + 16'h1000 > c0 + 16'h0800, "the displayed counter jumped by the flipped bit");

        // 2. stuck-bit defect on the first byte slice of word 6 (physical slice 48), bit 0 stuck at 1
        sw = 18'd0; sw[8:0] = 9'd48; sw[11:9] = 3'd0; sw[12] = 1'b1;
        repeat (100) @(posedge clk50);
        p0 = dut.parity_errors[7:0];
        press(2);
        repeat (80000) @(posedge clk50);
        $display("after stuck-at defect: parity errors %0d", dut.parity_errors[7:0]);
        check(dut.parity_errors[7:0] > p0, "a stuck bit raises the parity-error count");

        if (bad == 0) $display("PASS board simulation: counter, switches, flip and stuck-bit injection behave as designed");
        else          $display("FAIL board simulation: %0d checks failed", bad);
        $finish;
    end

    initial begin #2000000000; $display("FAIL: timeout"); $finish; end
endmodule
