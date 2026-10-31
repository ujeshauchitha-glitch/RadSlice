// The slice-serial register file: the same physical byte slices, remap table, parity and fault hooks as
// sliced_regfile.v, but a read or write moves ONE byte per beat, which is how the slice-serial core uses it.
// One selector per read port instead of eight: this is the version that fits the FPGA.
//
// A word is read by presenting rd_slice = 0..7 on consecutive beats with rd_en high; byte s comes out of
// the remap and the slice's defect. The running parity of the bytes read so far is kept inside, and on
// the beat with rd_slice = 7, rdN_perr says whether the whole word disagreed with its stored parity bit.
// A word is written by presenting wr_slice = 0..7 in order; its parity bit is stored on slice 7.
module sliced_regfile_serial #(parameter SPARES = 8) (
    input  wire        clk,
    input  wire        rst,

    input  wire        rd1_en,
    input  wire [4:0]  rd1_idx,
    input  wire [2:0]  rd1_slice,
    output wire [7:0]  rd1_byte,
    output wire        rd1_perr,      // meaningful with rd1_slice = 7

    input  wire        rd2_en,
    input  wire [4:0]  rd2_idx,
    input  wire [2:0]  rd2_slice,
    output wire [7:0]  rd2_byte,
    output wire        rd2_perr,

    input  wire        wr_en,
    input  wire [4:0]  wr_idx,
    input  wire [2:0]  wr_slice,
    input  wire [7:0]  wr_byte,

    input  wire        remap_we,
    input  wire [7:0]  remap_logical,
    input  wire [8:0]  remap_phys,

    input  wire        def_we,
    input  wire [8:0]  def_phys,
    input  wire [2:0]  def_bit,
    input  wire        def_val,
    input  wire        def_en,

    input  wire        flip_we,
    input  wire [4:0]  flip_word,
    input  wire [5:0]  flip_bit,

    input  wire [8:0]  ph_addr,
    input  wire [7:0]  ph_wdata,
    input  wire        ph_we,
    output wire [7:0]  ph_rdata
);
    localparam NP = 256 + SPARES;

    reg [8*NP-1:0]  phys;
    reg [NP-1:0]    d_en;
    reg [3*NP-1:0]  d_bit;
    reg [NP-1:0]    d_val;
    reg [9*256-1:0] remap;
    reg [31:0]      parity;
    reg             acc1, acc2, wacc;

    integer i;

    // A byte as a read sees it: the selected slice's stored byte with its stuck bit forced, if it has
    // one. The defect is applied after the slice is chosen, once per port, not once per slice.
    function [7:0] force_bit;
        input [7:0] v;
        input       en;
        input [2:0] bit_i;
        input       val;
        reg [7:0] m;
        begin
            m = 8'd1 << bit_i;
            force_bit = en ? (val ? (v | m) : (v & ~m)) : v;
        end
    endfunction

    wire [8:0] pa = remap[9*({rd1_idx, 3'b000} + rd1_slice) +: 9];
    wire [8:0] pb = remap[9*({rd2_idx, 3'b000} + rd2_slice) +: 9];
    assign rd1_byte = force_bit(phys[8*pa +: 8], d_en[pa], d_bit[3*pa +: 3], d_val[pa]);
    assign rd2_byte = force_bit(phys[8*pb +: 8], d_en[pb], d_bit[3*pb +: 3], d_val[pb]);
    assign rd1_perr = ((rd1_slice == 3'd0 ? 1'b0 : acc1) ^ (^rd1_byte)) != parity[rd1_idx];
    assign rd2_perr = ((rd2_slice == 3'd0 ? 1'b0 : acc2) ^ (^rd2_byte)) != parity[rd2_idx];
    assign ph_rdata = (ph_addr < NP)
        ? force_bit(phys[8*ph_addr +: 8], d_en[ph_addr], d_bit[3*ph_addr +: 3], d_val[ph_addr]) : 8'd0;

    wire [8:0] wphys    = remap[9*({wr_idx, 3'b000} + wr_slice) +: 9];
    wire [8:0] fl_slice = remap[9*({flip_word, 3'b000} + flip_bit[5:3]) +: 9];

    always @(posedge clk) begin
        if (rst) begin
            parity <= 32'd0; acc1 <= 1'b0; acc2 <= 1'b0; wacc <= 1'b0;
            phys  <= {8*NP{1'b0}};
            d_en  <= {NP{1'b0}};
            d_bit <= {3*NP{1'b0}};
            d_val <= {NP{1'b0}};
            for (i = 0; i < 256; i = i + 1) remap[9*i +: 9] <= i[8:0];
        end else begin
            if (rd1_en) acc1 <= (rd1_slice == 3'd0 ? 1'b0 : acc1) ^ (^rd1_byte);
            if (rd2_en) acc2 <= (rd2_slice == 3'd0 ? 1'b0 : acc2) ^ (^rd2_byte);
            if (wr_en) begin
                phys[8*wphys +: 8] <= wr_byte;
                wacc <= (wr_slice == 3'd0 ? 1'b0 : wacc) ^ (^wr_byte);
                if (wr_slice == 3'd7) parity[wr_idx] <= (wacc ^ (^wr_byte));
            end
            if (flip_we) phys[8*fl_slice +: 8] <= phys[8*fl_slice +: 8] ^ (8'd1 << flip_bit[2:0]);
            if (ph_we && ph_addr < NP) phys[8*ph_addr +: 8] <= ph_wdata;
            if (remap_we) remap[9*remap_logical +: 9] <= remap_phys;
            if (def_we && def_phys < NP) begin
                d_en[def_phys] <= def_en; d_bit[3*def_phys +: 3] <= def_bit; d_val[def_phys] <= def_val;
            end
        end
    end
endmodule
