// The slice-serial RV64 ALU: ONE K-bit slice unit reused for every slice position, one slice per
// clock ("beat"). This is the single-unit organisation of G10, and it follows SlicedAlu::exec in
// src/sliced_alu.cpp step for step, so that the model's beat counts are the hardware's cycle counts.
//
// Interface: pulse `start` for one cycle with op/a/b valid while !busy. `busy` stays high for exactly
// the number of beats (64/K for a 64-bit operation, 32/K for a W form). The cycle after the last beat
// `done` is high for one cycle and `result` is valid; it stays valid until the next start.
//
// op codes are the AluOp enum of riscv-emu (include/backends.hpp):
//   0 Add 1 Sub 2 And 3 Or 4 Xor 5 Sll 6 Srl 7 Sra 8 Slt 9 Sltu 10 AddW 11 SubW 12 SllW 13 SrlW 14 SraW
module sliced_alu #(parameter K = 16) (
    input  wire        clk,
    input  wire        rst,
    input  wire        start,
    input  wire [3:0]  op,
    input  wire [63:0] a,
    input  wire [63:0] b,
    output reg         busy,
    output reg         done,
    output wire [63:0] result
);
    localparam N  = 64 / K;     // slices in a full operation
    localparam NW = 32 / K;     // slices in the low word

    reg [3:0]  op_r;
    reg [63:0] a_r, b_r, res;
    reg        carry;
    reg [5:0]  cnt;

    wire is_w     = (op_r >= 4'd10);
    wire [5:0] nsl = is_w ? NW[5:0] : N[5:0];
    wire sub_like = (op_r == 4'd1) || (op_r == 4'd8) || (op_r == 4'd9) || (op_r == 4'd11);
    wire is_arith = (op_r == 4'd0) || (op_r == 4'd1) || (op_r == 4'd8) || (op_r == 4'd9) ||
                    (op_r == 4'd10) || (op_r == 4'd11);
    wire is_left  = (op_r == 4'd5) || (op_r == 4'd12);
    wire is_sra   = (op_r == 4'd7) || (op_r == 4'd14);

    wire [63:0] b_eff = sub_like ? ~b_r : b_r;
    wire [5:0]  amount = is_w ? {1'b0, b_r[4:0]} : b_r[5:0];
    wire [5:0]  q = amount / K;
    wire [5:0]  r = amount % K;
    wire        fill_bit = is_w ? a_r[31] : a_r[63];
    wire [K-1:0] fill = (is_sra && fill_bit) ? {K{1'b1}} : {K{1'b0}};

    // Operands for this beat.
    integer hi, lo;
    reg [2:0]   sop;
    reg [K-1:0] ux, uy;
    reg [5:0]   uaux;
    always @* begin
        hi = 0; lo = 0; sop = 3'd0; ux = {K{1'b0}}; uy = {K{1'b0}}; uaux = 6'd0;
        if (is_arith) begin
            sop = 3'd0; ux = a_r[cnt*K +: K]; uy = b_eff[cnt*K +: K]; uaux = {5'd0, carry};
        end else if (op_r == 4'd2) begin
            sop = 3'd1; ux = a_r[cnt*K +: K]; uy = b_r[cnt*K +: K];
        end else if (op_r == 4'd3) begin
            sop = 3'd2; ux = a_r[cnt*K +: K]; uy = b_r[cnt*K +: K];
        end else if (op_r == 4'd4) begin
            sop = 3'd3; ux = a_r[cnt*K +: K]; uy = b_r[cnt*K +: K];
        end else if (is_left) begin
            sop = 3'd4; uaux = r;
            hi = $signed({1'b0, cnt}) - $signed({1'b0, q});
            lo = hi - 1;
            ux = (hi >= 0) ? a_r[hi*K +: K] : {K{1'b0}};
            uy = (lo >= 0) ? a_r[lo*K +: K] : {K{1'b0}};
        end else begin    // right shifts, logical and arithmetic
            sop = 3'd5; uaux = r;
            hi = cnt + q;
            lo = cnt + q + 1;
            ux = (hi < nsl) ? a_r[hi*K +: K] : fill;
            uy = (lo < nsl) ? a_r[lo*K +: K] : fill;
        end
    end

    wire [K-1:0] uval;
    wire         ucarry;
    slice_unit #(.K(K)) u (.sop(sop), .x(ux), .y(uy), .aux(uaux), .value(uval), .carry(ucarry));

    always @(posedge clk) begin
        if (rst) begin
            busy <= 1'b0; done <= 1'b0; cnt <= 6'd0; carry <= 1'b0; res <= 64'd0;
            op_r <= 4'd0; a_r <= 64'd0; b_r <= 64'd0;
        end else begin
            done <= 1'b0;
            if (!busy) begin
                if (start) begin
                    op_r <= op; a_r <= a; b_r <= b; res <= 64'd0; cnt <= 6'd0;
                    carry <= (op == 4'd1) || (op == 4'd8) || (op == 4'd9) || (op == 4'd11);
                    busy <= 1'b1;
                end
            end else begin
                res[cnt*K +: K] <= uval;
                if (is_arith) carry <= ucarry;
                if (cnt == nsl - 6'd1) begin
                    busy <= 1'b0; done <= 1'b1;
                end
                cnt <= cnt + 6'd1;
            end
        end
    end

    // Final selection. Sltu/Slt come from the carry out of a + ~b + 1; a W form sign-extends
    // from bit 31 on the write-back path.
    wire sa  = a_r[63];
    wire sb  = b_r[63];
    wire ltu = ~carry;
    reg [63:0] out;
    always @* begin
        case (op_r)
            4'd8:    out = (sa != sb) ? {63'd0, sa} : {63'd0, ltu};
            4'd9:    out = {63'd0, ltu};
            default: out = is_w ? {{32{res[31]}}, res[31:0]} : res;
        endcase
    end
    assign result = out;
endmodule
