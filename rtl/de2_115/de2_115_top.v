// Board wrapper for the Terasic DE2-115 (Cyclone IV E): the slice-serial RV64I core with on-chip memory, LEDs,
// switches and seven-segment displays, and the fault-injection hooks of the register file on the switches and keys.
//
// Memory map seen by the program (the core's data bus):
//   0x80000000 .. 0x80001fff   8 KiB RAM, initialised from the program image (code is fetched from a ROM copy)
//   0x10000000  LEDR[17:0]  write      0x10000008  LEDG[7:0] write (LEDG shows the parity-error count, see below)
//   0x10000010  HEX3..HEX0 (16 bits, four hex digits)  write
//   0x10000018  SW[17:0] read          0x10000020  KEY[3:0] read (1 = pressed)
//
// Fault hooks (the model's inject calls), each fired by a key press:
//   KEY1  flip one stored bit:  word = SW[4:0], bit = SW[10:5]
//   KEY2  stuck-bit defect on physical slice SW[8:0], bit SW[11:9], value SW[12];  with SW[17] = 1 it clears the slice's defect
//   KEY0  reset
// LEDG[7:0] shows the core's parity-error count (low 8 bits); LEDG[8] is lit when the core has halted.
module de2_115_top #(
    parameter K        = 16,
    parameter SPARES   = 8,
    parameter CLK_DIV  = 3,                 // core clock = 50 MHz / 2^CLK_DIV
    parameter IMAGE    = "demo.hex",        // 32-bit words, one per line, from address 0x80000000
    parameter DEBOUNCE = 500000             // key debounce, in 50 MHz cycles (a few in simulation)
) (
    input  wire        CLOCK_50,
    input  wire [3:0]  KEY,
    input  wire [17:0] SW,
    output wire [17:0] LEDR,
    output wire [8:0]  LEDG,
    output wire [6:0]  HEX0, HEX1, HEX2, HEX3
);
    // ---- clock and reset ---------------------------------------------------------------------------------------
    reg [CLK_DIV-1:0] div = 0;
    always @(posedge CLOCK_50) div <= div + 1'b1;
    wire clk = div[CLK_DIV-1];

    // debounce the keys (about 10 ms at 50 MHz) and bring them into the core clock domain
    reg [18:0] dcnt0 = 0, dcnt1 = 0, dcnt2 = 0, dcnt3 = 0;
    reg [3:0]  key_db = 4'b1111;
    always @(posedge CLOCK_50) begin
        if (KEY[0] == key_db[0]) dcnt0 <= 0; else begin dcnt0 <= dcnt0 + 1'b1; if (dcnt0 == DEBOUNCE) key_db[0] <= KEY[0]; end
        if (KEY[1] == key_db[1]) dcnt1 <= 0; else begin dcnt1 <= dcnt1 + 1'b1; if (dcnt1 == DEBOUNCE) key_db[1] <= KEY[1]; end
        if (KEY[2] == key_db[2]) dcnt2 <= 0; else begin dcnt2 <= dcnt2 + 1'b1; if (dcnt2 == DEBOUNCE) key_db[2] <= KEY[2]; end
        if (KEY[3] == key_db[3]) dcnt3 <= 0; else begin dcnt3 <= dcnt3 + 1'b1; if (dcnt3 == DEBOUNCE) key_db[3] <= KEY[3]; end
    end
    reg [3:0] key_s1 = 4'b1111, key_s2 = 4'b1111, key_prev = 4'b1111;
    always @(posedge clk) begin key_s1 <= key_db; key_s2 <= key_s1; key_prev <= key_s2; end
    wire [3:0] pressed    = ~key_s2;                   // active-low keys
    wire [3:0] press_edge = key_prev & ~key_s2;        // one core-clock pulse per press

    reg [3:0] rst_sh = 4'b1111;
    always @(posedge clk) rst_sh <= {rst_sh[2:0], pressed[0]};
    wire rst = rst_sh[3];

    // ---- core ----------------------------------------------------------------------------------------------------
    wire [63:0] imem_addr, dmem_addr, dmem_wdata, dmem_rdata;
    wire [31:0] imem_data, parity_errors;
    wire        dmem_en, dmem_we, halted, illegal;
    wire [2:0]  dmem_funct3;

    rv64i_core #(.K(K), .SPARES(SPARES), .MEM_SYNC(1)) core (
        .clk(clk), .rst(rst),
        .imem_addr(imem_addr), .imem_data(imem_data),
        .dmem_en(dmem_en), .dmem_we(dmem_we), .dmem_addr(dmem_addr), .dmem_funct3(dmem_funct3),
        .dmem_wdata(dmem_wdata), .dmem_rdata(dmem_rdata),
        .ret_valid(), .ret_pc(), .ret_inst(), .ret_rd(), .ret_rdval(), .ret_st(), .ret_st_addr(),
        .ret_st_size(), .ret_st_val(), .ret_next_pc(),
        .halted(halted), .illegal(illegal), .parity_errors(parity_errors), .cycles(),
        .remap_we(1'b0), .remap_logical(8'd0), .remap_phys(9'd0),
        .def_we(press_edge[2]), .def_phys(SW[8:0]), .def_bit(SW[11:9]), .def_val(SW[12]), .def_en(~SW[17]),
        .flip_we(press_edge[1]), .flip_word(SW[4:0]), .flip_bit(SW[10:5]));

    // ---- instruction ROM: 2048 x 32 bits, synchronous read (block RAM) -----------------------------------------
    reg [31:0] rom [0:2047];
    reg [31:0] rom_q;
    initial $readmemh(IMAGE, rom);
    always @(posedge clk) rom_q <= rom[imem_addr[12:2]];
    assign imem_data = rom_q;

    // ---- data RAM: 1024 dwords as a 64-bit word with byte-enabled write, synchronous read ----------------------
    // Initialised from the same image, so the program's constants and initial data are visible.
    wire        is_mmio = dmem_addr[28];
    wire [9:0]  widx    = dmem_addr[12:3];
    wire [2:0]  lane    = dmem_addr[2:0];
    reg  [63:0] ram [0:1023];
    reg  [63:0] ram_q;
    integer ii;
    initial for (ii = 0; ii < 1024; ii = ii + 1) ram[ii] = {rom[2*ii + 1], rom[2*ii]};

    wire [63:0] sdata = dmem_wdata << {lane, 3'b000};
    reg  [7:0]  wen;
    always @* begin
        case (dmem_funct3[1:0])
            2'd0:    wen = 8'h01 << lane;
            2'd1:    wen = 8'h03 << lane;
            2'd2:    wen = 8'h0f << lane;
            default: wen = 8'hff;
        endcase
    end
    wire we_ram = dmem_en && dmem_we && !is_mmio;
    always @(posedge clk) begin
        if (we_ram) begin
            if (wen[0]) ram[widx][7:0]   <= sdata[7:0];
            if (wen[1]) ram[widx][15:8]  <= sdata[15:8];
            if (wen[2]) ram[widx][23:16] <= sdata[23:16];
            if (wen[3]) ram[widx][31:24] <= sdata[31:24];
            if (wen[4]) ram[widx][39:32] <= sdata[39:32];
            if (wen[5]) ram[widx][47:40] <= sdata[47:40];
            if (wen[6]) ram[widx][55:48] <= sdata[55:48];
            if (wen[7]) ram[widx][63:56] <= sdata[63:56];
        end
        ram_q <= ram[widx];
    end
    // the loaded dword, shifted down by the byte offset (the core extends from the access size)
    reg [2:0] lane_q;
    reg       mmio_q;
    always @(posedge clk) begin lane_q <= lane; mmio_q <= is_mmio; end
    wire [63:0] ram_rdata = ram_q >> {lane_q, 3'b000};

    // ---- memory-mapped I/O ---------------------------------------------------------------------------------------
    reg [17:0] ledr_r = 0;
    reg [15:0] hex_r  = 0;
    always @(posedge clk) begin
        if (rst) begin ledr_r <= 0; hex_r <= 0; end
        else if (dmem_en && dmem_we && is_mmio) begin
            case (dmem_addr[5:3])
                3'd0:    ledr_r <= dmem_wdata[17:0];
                3'd2:    hex_r  <= dmem_wdata[15:0];
                default: ;
            endcase
        end
    end
    reg [2:0]  mmio_sel_q;
    always @(posedge clk) mmio_sel_q <= dmem_addr[5:3];
    reg [63:0] mmio_rdata;
    always @* begin
        case (mmio_sel_q)
            3'd3:    mmio_rdata = {46'd0, SW};
            3'd4:    mmio_rdata = {60'd0, pressed};
            default: mmio_rdata = 64'd0;
        endcase
    end
    assign dmem_rdata = mmio_q ? mmio_rdata : ram_rdata;

    // ---- display ---------------------------------------------------------------------------------------------------
    function [6:0] seg;       // active-low segments a..g on bits 0..6
        input [3:0] d;
        case (d)
            4'h0: seg = 7'b1000000; 4'h1: seg = 7'b1111001; 4'h2: seg = 7'b0100100; 4'h3: seg = 7'b0110000;
            4'h4: seg = 7'b0011001; 4'h5: seg = 7'b0010010; 4'h6: seg = 7'b0000010; 4'h7: seg = 7'b1111000;
            4'h8: seg = 7'b0000000; 4'h9: seg = 7'b0010000; 4'ha: seg = 7'b0001000; 4'hb: seg = 7'b0000011;
            4'hc: seg = 7'b1000110; 4'hd: seg = 7'b0100001; 4'he: seg = 7'b0000110; default: seg = 7'b0001110;
        endcase
    endfunction
    assign HEX0 = seg(hex_r[3:0]);
    assign HEX1 = seg(hex_r[7:4]);
    assign HEX2 = seg(hex_r[11:8]);
    assign HEX3 = seg(hex_r[15:12]);
    assign LEDR = ledr_r;
    assign LEDG = {halted | illegal, parity_errors[7:0]};
endmodule
