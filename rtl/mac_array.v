// A row of P logical MAC lanes served by P+S physical processing elements (G19).
//
// Output-stationary matrix-vector engine: each cycle one input element `x` is broadcast and every lane gets
// its own weight; lane l accumulates sum(x * w[l]) over the steps. A layer with more outputs than lanes is
// run in passes by the caller.
//
// Sparing: `map` gives, for each logical lane, the physical element that serves it (must be distinct). Weights
// are routed to that element and its accumulator is read back as the lane's result. Physical elements nobody
// maps stay idle (weight 0), so a defective element is repaired by mapping a spare in its place. The routing
// here is an ideal any-to-any crossbar; its own cost and defects are not modelled (see G10 and G14).
//
// Packed ports: w is P x 8 bits (lane l in bits [8l+7:8l]); map is P x MW bits; result is P x 32 bits.
module mac_array #(parameter P = 8, parameter S = 4, parameter ACC = 32,
                   parameter PH = P + S,                          // physical elements (derived)
                   parameter MW = (P + S <= 2) ? 1 : $clog2(P + S)) ( // bits of a physical index (derived)
    input  wire                         clk,
    input  wire                         clear,
    input  wire                         step,
    input  wire signed [7:0]            x,
    input  wire [P*8-1:0]               w,
    input  wire [P*MW-1:0]              map,
    // fault hooks, per physical element
    input  wire [PH*ACC-1:0]            stuck_mask,
    input  wire [PH*ACC-1:0]            stuck_val,
    input  wire                         flip_en,
    input  wire [MW-1:0]                flip_pe,
    input  wire [5:0]                   flip_bit,
    output wire [P*ACC-1:0]             result
);
    wire [PH*ACC-1:0] acc_all;

    genvar p, l;
    generate
        for (p = 0; p < PH; p = p + 1) begin : pe
            // the weight of whichever lane maps to this element, or 0
            wire [7:0] wsel [0:P];
            assign wsel[0] = 8'd0;
            for (l = 0; l < P; l = l + 1) begin : route
                assign wsel[l+1] = wsel[l] | ((map[l*MW +: MW] == p) ? w[l*8 +: 8] : 8'd0);
            end
            wire [ACC-1:0] flip = (flip_en && flip_pe == p) ? ({{(ACC-1){1'b0}}, 1'b1} << flip_bit) : {ACC{1'b0}};
            mac_pe #(.ACC(ACC)) u (
                .clk(clk), .clear(clear), .step(step), .x(x), .w(wsel[P]),
                .stuck_mask(stuck_mask[p*ACC +: ACC]), .stuck_val(stuck_val[p*ACC +: ACC]),
                .flip(flip), .acc(acc_all[p*ACC +: ACC])
            );
        end
        for (l = 0; l < P; l = l + 1) begin : outsel
            assign result[l*ACC +: ACC] = acc_all[map[l*MW +: MW]*ACC +: ACC];
        end
    endgenerate
endmodule
