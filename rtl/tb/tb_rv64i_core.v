// Lockstep testbench: run rtl/rv64i_core.v on a program and compare every retired instruction with the golden
// trace written by radslice_trace (pc, instruction, register written and its value, store address/size/data, next pc).
//
//   iverilog -g2012 -o tb -P tb_rv64i_core.K=16 rtl/slice_unit.v rtl/sliced_alu.v rtl/sliced_regfile_serial.v \
//            rtl/rv64i_core.v rtl/tb/tb_rv64i_core.v
//   vvp tb +mem=program.hex +trace=trace.txt +count=N       (program.hex: one byte per line from 0x80000000)
`timescale 1ns/1ps
module tb_rv64i_core;
    parameter K = 16;
    localparam MEMSZ = 2 * 1024 * 1024;
    localparam [63:0] BASE = 64'h0000000080000000;

    reg clk = 0, rst = 1;
    always #5 clk = ~clk;

    reg [7:0] mem [0:MEMSZ-1];

    wire [63:0] imem_addr;
    wire [31:0] imem_data = {mem[imem_addr[20:0] + 3], mem[imem_addr[20:0] + 2], mem[imem_addr[20:0] + 1], mem[imem_addr[20:0]]};

    wire        dmem_en, dmem_we;
    wire [63:0] dmem_addr, dmem_wdata;
    wire [2:0]  dmem_funct3;
    wire [20:0] da = dmem_addr[20:0];
    // Continuous assignments, not an always @* block: a combinational block that reads a 2 MiB array makes the
    // simulator build a sensitivity entry per element, which does not finish.
    wire [63:0] raw = {mem[da + 7], mem[da + 6], mem[da + 5], mem[da + 4], mem[da + 3], mem[da + 2], mem[da + 1], mem[da]};
    wire [63:0] dmem_rdata =
        (dmem_funct3 == 3'd0 || dmem_funct3 == 3'd4) ? {56'd0, raw[7:0]} :
        (dmem_funct3 == 3'd1 || dmem_funct3 == 3'd5) ? {48'd0, raw[15:0]} :
        (dmem_funct3 == 3'd2 || dmem_funct3 == 3'd6) ? {32'd0, raw[31:0]} : raw;
    integer wi;
    always @(posedge clk) begin
        if (dmem_en && dmem_we)
            for (wi = 0; wi < (1 << dmem_funct3[1:0]); wi = wi + 1) mem[da + wi] <= dmem_wdata[8*wi +: 8];
    end

    wire        ret_valid, ret_st, halted, illegal;
    wire [63:0] ret_pc, ret_rdval, ret_st_addr, ret_st_val, ret_next_pc, cycles;
    wire [31:0] ret_inst, parity_errors;
    wire [4:0]  ret_rd;
    wire [2:0]  ret_st_size;

    rv64i_core #(.K(K)) dut (
        .clk(clk), .rst(rst),
        .imem_addr(imem_addr), .imem_data(imem_data),
        .dmem_en(dmem_en), .dmem_we(dmem_we), .dmem_addr(dmem_addr), .dmem_funct3(dmem_funct3),
        .dmem_wdata(dmem_wdata), .dmem_rdata(dmem_rdata),
        .ret_valid(ret_valid), .ret_pc(ret_pc), .ret_inst(ret_inst), .ret_rd(ret_rd), .ret_rdval(ret_rdval),
        .ret_st(ret_st), .ret_st_addr(ret_st_addr), .ret_st_size(ret_st_size), .ret_st_val(ret_st_val),
        .ret_next_pc(ret_next_pc), .halted(halted), .illegal(illegal), .parity_errors(parity_errors), .cycles(cycles),
        .remap_we(1'b0), .remap_logical(8'd0), .remap_phys(9'd0),
        .def_we(1'b0), .def_phys(9'd0), .def_bit(3'd0), .def_val(1'b0), .def_en(1'b0),
        .flip_we(1'b0), .flip_word(5'd0), .flip_bit(6'd0));

    integer fd, rc, n, bad, i;
    reg [1023:0] memfile, tracefile;
    reg [63:0] e_pc, e_rdval, e_staddr, e_stval, e_next;
    reg [31:0] e_inst;
    reg [4:0]  e_rd;
    reg [3:0]  e_st, e_stsize;
    reg [63:0] mask;
    reg  [4:0] e_rd5;
    integer count;

    initial begin
        bad = 0; n = 0;
        for (i = 0; i < MEMSZ; i = i + 1) mem[i] = 8'd0;
        if (!$value$plusargs("mem=%s", memfile) || !$value$plusargs("trace=%s", tracefile) || !$value$plusargs("count=%d", count)) begin
            $display("FAIL: pass +mem=<hex> +trace=<trace> +count=<lines>"); $finish;
        end
        $readmemh(memfile, mem);
        fd = $fopen(tracefile, "r");
        if (fd == 0) begin $display("FAIL: cannot open %0s", tracefile); $finish; end
        repeat (3) @(posedge clk);
        rst <= 0;
    end

    always @(posedge clk) begin
        if (!rst && ret_valid && bad == 0) begin
            rc = $fscanf(fd, "%h %h %h %h %h %h %h %h %h\n", e_pc, e_inst, e_rd, e_rdval, e_st, e_staddr, e_stsize, e_stval, e_next);
            if (rc != 9) begin
                $display("FAIL: the core retired more instructions than the golden trace has (%0d)", n);
                bad = 1; $finish;
            end else begin
                e_rd5 = {1'b0, e_rd};
                mask = (e_stsize == 3) ? 64'hffffffffffffffff : ((64'd1 << (8 * (1 << e_stsize))) - 64'd1);
                if (ret_pc !== e_pc || ret_inst !== e_inst || ret_rd !== e_rd5 ||
                    (e_rd != 0 && ret_rdval !== e_rdval) || ret_st !== e_st[0] ||
                    (e_st[0] && (ret_st_addr !== e_staddr || ret_st_size !== e_stsize[2:0] || (ret_st_val & mask) !== (e_stval & mask))) ||
                    ret_next_pc !== e_next) begin
                    bad = 1;
                    $display("FAIL at instruction %0d: pc core %h golden %h", n, ret_pc, e_pc);
                    $display("   inst core %h golden %h; rd core %0d golden %0d; value core %h golden %h", ret_inst, e_inst, ret_rd, e_rd, ret_rdval, e_rdval);
                    $display("   store core %b %h size %0d val %h; golden %b %h size %0d val %h; next pc core %h golden %h",
                             ret_st, ret_st_addr, ret_st_size, ret_st_val, e_st[0], e_staddr, e_stsize, e_stval, ret_next_pc, e_next);
                    $finish;
                end
                n = n + 1;
            end
        end
        if (!rst && halted && bad == 0) begin
            $display("core halted after %0d instructions (illegal=%b)", n, illegal);
            $finish;
        end
    end

    // The trace length comes from the command line (+count=N): finish when that many instructions have matched.
    always @(posedge clk) begin
        if (!rst && ret_valid && bad == 0 && n == count) begin
            $display("PASS K=%0d: %0d instructions match the golden trace; %0d cycles (%0d.%02d per instruction); parity errors %0d",
                     K, n, cycles, cycles / n, ((cycles * 100) / n) % 100, parity_errors);
            $finish;
        end
    end

    initial begin #2000000000; $display("FAIL: timeout after %0d instructions", n); $finish; end
endmodule
