// Testbench: feed the vectors written by radslice_alu_vectors to rtl/sliced_alu.v and check both the
// result and the number of busy cycles (the model's beat count).
//
//   iverilog -g2012 -o tb -P tb_sliced_alu.K=16 rtl/slice_unit.v rtl/sliced_alu.v rtl/tb/tb_sliced_alu.v
//   vvp tb +vec=vectors.txt
`timescale 1ns/1ps
module tb_sliced_alu;
    parameter K = 16;

    reg         clk = 0, rst = 1, start = 0;
    reg  [3:0]  op = 0;
    reg  [63:0] a = 0, b = 0;
    wire        busy, done;
    wire [63:0] result;

    sliced_alu #(.K(K)) dut (.clk(clk), .rst(rst), .start(start), .op(op), .a(a), .b(b),
                             .busy(busy), .done(done), .result(result));

    always #5 clk = ~clk;

    integer fd, n, rc, beats_got, bad, shown;
    reg [3:0]  vop;
    reg [63:0] va, vb, vexp;
    integer    vbeats;
    reg [1023:0] path;

    initial begin
        bad = 0; shown = 0; n = 0;
        if (!$value$plusargs("vec=%s", path)) begin
            $display("FAIL: pass +vec=<vector file>");
            $finish;
        end
        fd = $fopen(path, "r");
        if (fd == 0) begin
            $display("FAIL: cannot open %0s", path);
            $finish;
        end
        repeat (3) @(posedge clk);
        rst <= 0;
        @(posedge clk);

        rc = $fscanf(fd, "%h %h %h %h %d\n", vop, va, vb, vexp, vbeats);
        while (rc == 5) begin
            op <= vop; a <= va; b <= vb; start <= 1;
            @(posedge clk);
            start <= 0;
            beats_got = 0;
            // busy is high from the cycle after start until the last beat has run
            @(negedge clk);
            while (busy) begin
                beats_got = beats_got + 1;
                @(negedge clk);
            end
            // done is high in the cycle after busy falls; the result is valid then
            if (!done) begin
                bad = bad + 1;
                if (shown < 10) begin shown = shown + 1; $display("FAIL: no done for op %0d", vop); end
            end
            if (result !== vexp || beats_got != vbeats) begin
                bad = bad + 1;
                if (shown < 10) begin
                    shown = shown + 1;
                    $display("FAIL: op %0d a=%h b=%h got %h (beats %0d) want %h (beats %0d)",
                             vop, va, vb, result, beats_got, vexp, vbeats);
                end
            end
            n = n + 1;
            @(posedge clk);
            rc = $fscanf(fd, "%h %h %h %h %d\n", vop, va, vb, vexp, vbeats);
        end
        $fclose(fd);
        if (bad == 0) $display("PASS K=%0d: %0d vectors, results and beat counts match the model", K, n);
        else          $display("FAIL K=%0d: %0d of %0d vectors differ", K, bad, n);
        $finish;
    end

    initial begin
        #200000000;
        $display("FAIL: timeout");
        $finish;
    end
endmodule
