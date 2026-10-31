// The register file as physical byte slices: 32 words x 8 byte slices = 256 primary slices, plus SPARES
// spare slices. A logical slice (word w, byte b) reaches its physical slice through a remap table that
// resets to the identity. Each word carries one parity bit, written with the word and checked on read.
// Mirrors SlicedRegFile in src/sliced_regfile.cpp (x0 is not special here: the core never reads or writes it).
//
// Hooks the model has and the board needs:
//   - a hard stuck-bit defect per physical slice (set_defect), applied on every read of that slice;
//   - a single-event upset: flip one stored bit of a word, found through the remap (flip_bit);
//   - direct physical access that bypasses the remap, which the boot self-test uses;
//   - a remap write port.
// A parity mismatch is flagged on the read that sees it; it is never repaired here.
module sliced_regfile #(parameter SPARES = 8) (
    input  wire        clk,
    input  wire        rst,

    // two read ports (combinational) and one write port (clocked)
    input  wire [4:0]  rd1_idx,
    output wire [63:0] rd1_data,
    output wire        rd1_perr,
    input  wire [4:0]  rd2_idx,
    output wire [63:0] rd2_data,
    output wire        rd2_perr,
    input  wire        wr_en,
    input  wire [4:0]  wr_idx,
    input  wire [63:0] wr_data,

    // remap: logical slice (word*8 + byte) -> physical slice
    input  wire        remap_we,
    input  wire [7:0]  remap_logical,
    input  wire [8:0]  remap_phys,

    // stuck-bit defect on a physical slice (def_en = 0 clears it)
    input  wire        def_we,
    input  wire [8:0]  def_phys,
    input  wire [2:0]  def_bit,
    input  wire        def_val,
    input  wire        def_en,

    // single-event upset
    input  wire        flip_we,
    input  wire [4:0]  flip_word,
    input  wire [5:0]  flip_bit,

    // direct physical access (self-test); ph_rdata applies the slice's defect, like a normal read
    input  wire [8:0]  ph_addr,
    input  wire [7:0]  ph_wdata,
    input  wire        ph_we,
    output wire [7:0]  ph_rdata
);
    localparam NP = 256 + SPARES;

    // Everything is held as packed vectors, and the read path is plain continuous assignments over
    // them, so a change to any stored bit, remap entry or defect reaches the read ports at once.
    reg [8*NP-1:0]  phys;      // physical slices, byte p at [8p +: 8]
    reg [NP-1:0]    d_en;      // stuck-bit defect present on slice p
    reg [3*NP-1:0]  d_bit;     // which bit is stuck
    reg [NP-1:0]    d_val;     // the stuck value
    reg [9*256-1:0] remap;     // logical slice l -> physical slice, at [9l +: 9]
    reg [31:0]      parity;

    integer i;

    // Each physical slice as a read sees it: the stored byte with a stuck bit forced, if it has one.
    wire [8*NP-1:0] seen;
    genvar p;
    generate
        for (p = 0; p < NP; p = p + 1) begin : g_seen
            wire [7:0] stored = phys[8*p +: 8];
            wire [7:0] bitm   = 8'd1 << d_bit[3*p +: 3];
            assign seen[8*p +: 8] = d_en[p] ? (d_val[p] ? (stored | bitm) : (stored & ~bitm)) : stored;
        end
    endgenerate

    // Read ports: byte s of word idx comes from the physical slice remap[idx*8 + s].
    genvar s;
    generate
        for (s = 0; s < 8; s = s + 1) begin : g_rd
            wire [8:0] pa = remap[9*({rd1_idx, 3'b000} + s) +: 9];
            wire [8:0] pb = remap[9*({rd2_idx, 3'b000} + s) +: 9];
            assign rd1_data[8*s +: 8] = seen[8*pa +: 8];
            assign rd2_data[8*s +: 8] = seen[8*pb +: 8];
        end
    endgenerate

    assign rd1_perr = (^rd1_data) != parity[rd1_idx];
    assign rd2_perr = (^rd2_data) != parity[rd2_idx];
    assign ph_rdata = (ph_addr < NP) ? seen[8*ph_addr +: 8] : 8'd0;

    wire [8:0] fl_slice = remap[9*({flip_word, 3'b000} + flip_bit[5:3]) +: 9];

    always @(posedge clk) begin
        if (rst) begin
            parity <= 32'd0;
            phys   <= {8*NP{1'b0}};
            d_en   <= {NP{1'b0}};
            d_bit  <= {3*NP{1'b0}};
            d_val  <= {NP{1'b0}};
            for (i = 0; i < 256; i = i + 1) remap[9*i +: 9] <= i[8:0];
        end else begin
            if (wr_en) begin
                for (i = 0; i < 8; i = i + 1) phys[8*remap[9*({wr_idx, 3'b000} + i) +: 9] +: 8] <= wr_data[8*i +: 8];
                parity[wr_idx] <= ^wr_data;
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
