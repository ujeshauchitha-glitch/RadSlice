// One physical slice unit: the only place an ALU operation touches data, K bits at a time.
// Mirrors SlicedAlu::ideal_slice in src/sliced_alu.cpp operation for operation.
//
//   sop 0 ADD      x + y + aux[0], with carry out
//   sop 1 AND      sop 2 OR      sop 3 XOR
//   sop 4 FUNNEL_L (x << aux | y >> (K - aux)) & mask     one output slice of a left shift
//   sop 5 FUNNEL_R (x >> aux | y << (K - aux)) & mask     one output slice of a right shift
//
// This module is plain combinational logic and nothing else. A defective unit is modelled in
// the simulation model by replacing exactly this function; in hardware it is where a spare
// unit would take over.
module slice_unit #(parameter K = 16) (
    input  wire [2:0]   sop,
    input  wire [K-1:0] x,
    input  wire [K-1:0] y,
    input  wire [5:0]   aux,
    output reg  [K-1:0] value,
    output reg          carry
);
    reg [K:0]     sum;
    reg [2*K-1:0] wide;

    always @* begin
        sum   = {1'b0, x} + {1'b0, y} + {{K{1'b0}}, aux[0]};
        wide  = 0;
        carry = 1'b0;
        value = {K{1'b0}};
        case (sop)
            3'd0: begin value = sum[K-1:0]; carry = sum[K]; end
            3'd1: value = x & y;
            3'd2: value = x | y;
            3'd3: value = x ^ y;
            3'd4: begin wide = {x, y} << aux; value = wide[2*K-1:K]; end
            3'd5: begin wide = {y, x} >> aux; value = wide[K-1:0]; end
            default: value = {K{1'b0}};
        endcase
    end
endmodule
