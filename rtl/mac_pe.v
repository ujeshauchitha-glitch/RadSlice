// One multiply-accumulate processing element: acc <= acc + x*w, signed int8 x int8 into a 32-bit accumulator.
//
// Fault hooks (G19). `stuck_mask`/`stuck_val` force accumulator bits: a set mask bit holds that bit of the
// STORED accumulator at stuck_val, so the wrong value also feeds back into later additions, as a stuck
// flip-flop would. `flip` XORs bits once, for one cycle, to model an upset.
module mac_pe #(parameter ACC = 32) (
    input  wire                 clk,
    input  wire                 clear,      // acc <= 0 (takes priority over step)
    input  wire                 step,       // acc <= acc + x*w
    input  wire signed [7:0]    x,
    input  wire signed [7:0]    w,
    input  wire [ACC-1:0]       stuck_mask,
    input  wire [ACC-1:0]       stuck_val,
    input  wire [ACC-1:0]       flip,
    output reg  [ACC-1:0]       acc
);
    wire signed [15:0]    prod = x * w;
    wire signed [ACC-1:0] sum  = $signed(acc) + prod;     // prod is sign-extended
    wire [ACC-1:0]        nxt  = (clear ? {ACC{1'b0}} : (step ? sum : acc)) ^ flip;

    always @(posedge clk)
        acc <= (nxt & ~stuck_mask) | (stuck_val & stuck_mask);

    initial acc = {ACC{1'b0}};
endmodule
