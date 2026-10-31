// An in-order, multi-cycle RV64I core built from the slice-serial parts: one K-bit slice unit reused per beat
// (sliced_alu) and a byte-per-beat register file with remap and parity (sliced_regfile_serial).
//
// It executes the same instructions the golden core runs in the RadSlice workloads (RV64I, 32-bit encodings, no
// CSRs, no M/A/F/D/C). The ALU is used where the golden core uses it (include/backends.hpp): arithmetic and
// logic, branch comparisons (Xor for BEQ/BNE, Slt for BLT/BGE, Sltu for BLTU/BGEU), data addresses and the JALR
// target (Add). LUI, AUIPC, JAL and the pc + 4 link value use a plain adder, as in the model.
//
// One instruction takes: 1 fetch, 8 register-read beats (both source ports in parallel), the ALU beats, a
// memory cycle for loads and stores, 8 write-back beats. Those fixed costs are what the model calls `base`; the
// testbench reports the measured value.
//
// Memory is outside: a combinational instruction fetch and a combinational data port. ECALL, EBREAK and any
// encoding this core does not implement stop it (halted, with `illegal` set for the latter).
module rv64i_core #(parameter K = 16, parameter SPARES = 8, parameter RESET_PC = 64'h0000000080000000) (
    input  wire        clk,
    input  wire        rst,

    output wire [63:0] imem_addr,
    input  wire [31:0] imem_data,

    output reg         dmem_en,
    output reg         dmem_we,
    output reg  [63:0] dmem_addr,
    output reg  [2:0]  dmem_funct3,     // size and signedness as in the instruction (0 b, 1 h, 2 w, 3 d, 4 bu, 5 hu, 6 wu)
    output reg  [63:0] dmem_wdata,
    input  wire [63:0] dmem_rdata,      // loaded value, zero-extended from the access size

    // one pulse per retired instruction
    output reg         ret_valid,
    output reg  [63:0] ret_pc,
    output reg  [31:0] ret_inst,
    output reg  [4:0]  ret_rd,          // 0 when nothing was written to a register
    output reg  [63:0] ret_rdval,
    output reg         ret_st,
    output reg  [63:0] ret_st_addr,
    output reg  [2:0]  ret_st_size,     // log2 bytes
    output reg  [63:0] ret_st_val,
    output reg  [63:0] ret_next_pc,

    output reg         halted,
    output reg         illegal,
    output reg  [31:0] parity_errors,
    output reg  [63:0] cycles,

    // register-file fault hooks, passed straight through
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
    input  wire [5:0]  flip_bit
);
    localparam S_FETCH = 4'd0, S_READ = 4'd1, S_EXEC = 4'd2, S_WAIT = 4'd3, S_MEM = 4'd4, S_WB = 4'd5, S_RETIRE = 4'd6, S_HALT = 4'd7;

    localparam OPC_LOAD = 7'h03, OPC_MISC = 7'h0f, OPC_OPIMM = 7'h13, OPC_AUIPC = 7'h17, OPC_OPIMM32 = 7'h1b,
               OPC_STORE = 7'h23, OPC_OP = 7'h33, OPC_LUI = 7'h37, OPC_OP32 = 7'h3b, OPC_BRANCH = 7'h63,
               OPC_JALR = 7'h67, OPC_JAL = 7'h6f, OPC_SYSTEM = 7'h73;

    reg [3:0]  state;
    reg [63:0] pc;
    reg [31:0] inst;
    reg [3:0]  cnt;
    reg [63:0] opa, opb, res, next_pc;

    assign imem_addr = pc;

    // ---- decode -----------------------------------------------------------------------------------------
    wire [6:0] opcode = inst[6:0];
    wire [4:0] rd     = inst[11:7];
    wire [2:0] funct3 = inst[14:12];
    wire [4:0] rs1    = inst[19:15];
    wire [4:0] rs2    = inst[24:20];
    wire [6:0] funct7 = inst[31:25];

    wire [63:0] imm_i = {{52{inst[31]}}, inst[31:20]};
    wire [63:0] imm_s = {{52{inst[31]}}, inst[31:25], inst[11:7]};
    wire [63:0] imm_b = {{51{inst[31]}}, inst[31], inst[7], inst[30:25], inst[11:8], 1'b0};
    wire [63:0] imm_u = {{32{inst[31]}}, inst[31:12], 12'd0};
    wire [63:0] imm_j = {{43{inst[31]}}, inst[31], inst[19:12], inst[20], inst[30:21], 1'b0};

    // ---- which ALU operation, if any ----------------------------------------------------------------------
    // alu_op uses the AluOp numbering of riscv-emu: 0 Add 1 Sub 2 And 3 Or 4 Xor 5 Sll 6 Srl 7 Sra 8 Slt 9 Sltu
    // 10 AddW 11 SubW 12 SllW 13 SrlW 14 SraW
    reg        uses_alu, uses_b_reg, bad;
    reg [3:0]  alu_op;
    always @* begin
        uses_alu = 1'b1; uses_b_reg = 1'b0; bad = 1'b0; alu_op = 4'd0;
        case (opcode)
            OPC_OPIMM: begin
                case (funct3)
                    3'd0: alu_op = 4'd0;
                    3'd1: begin alu_op = 4'd5; if (inst[31:26] != 6'd0) bad = 1'b1; end
                    3'd2: alu_op = 4'd8;
                    3'd3: alu_op = 4'd9;
                    3'd4: alu_op = 4'd4;
                    3'd5: begin
                        if (inst[31:26] == 6'd0) alu_op = 4'd6;
                        else if (inst[31:26] == 6'b010000) alu_op = 4'd7;
                        else bad = 1'b1;
                    end
                    3'd6: alu_op = 4'd3;
                    3'd7: alu_op = 4'd2;
                endcase
            end
            OPC_OP: begin
                uses_b_reg = 1'b1;
                if (funct7 != 7'd0 && !(funct7 == 7'h20 && (funct3 == 3'd0 || funct3 == 3'd5))) bad = 1'b1;
                case (funct3)
                    3'd0: alu_op = (funct7 == 7'h20) ? 4'd1 : 4'd0;
                    3'd1: alu_op = 4'd5;
                    3'd2: alu_op = 4'd8;
                    3'd3: alu_op = 4'd9;
                    3'd4: alu_op = 4'd4;
                    3'd5: alu_op = (funct7 == 7'h20) ? 4'd7 : 4'd6;
                    3'd6: alu_op = 4'd3;
                    3'd7: alu_op = 4'd2;
                endcase
            end
            OPC_OPIMM32: begin
                case (funct3)
                    3'd0: alu_op = 4'd10;
                    3'd1: begin alu_op = 4'd12; if (funct7 != 7'd0) bad = 1'b1; end
                    3'd5: begin
                        if (funct7 == 7'd0) alu_op = 4'd13;
                        else if (funct7 == 7'h20) alu_op = 4'd14;
                        else bad = 1'b1;
                    end
                    default: bad = 1'b1;
                endcase
            end
            OPC_OP32: begin
                uses_b_reg = 1'b1;
                if (funct7 != 7'd0 && !(funct7 == 7'h20 && (funct3 == 3'd0 || funct3 == 3'd5))) bad = 1'b1;
                case (funct3)
                    3'd0: alu_op = (funct7 == 7'h20) ? 4'd11 : 4'd10;
                    3'd1: alu_op = 4'd12;
                    3'd5: alu_op = (funct7 == 7'h20) ? 4'd14 : 4'd13;
                    default: bad = 1'b1;
                endcase
            end
            OPC_BRANCH: begin
                uses_b_reg = 1'b1;
                case (funct3)
                    3'd0, 3'd1: alu_op = 4'd4;
                    3'd4, 3'd5: alu_op = 4'd8;
                    3'd6, 3'd7: alu_op = 4'd9;
                    default: bad = 1'b1;
                endcase
            end
            OPC_LOAD: begin
                alu_op = 4'd0;
                if (funct3 == 3'd7) bad = 1'b1;
            end
            OPC_STORE: begin
                uses_b_reg = 1'b1; alu_op = 4'd0;
                if (funct3 > 3'd3) bad = 1'b1;
            end
            OPC_JALR: begin alu_op = 4'd0; if (funct3 != 3'd0) bad = 1'b1; end
            OPC_LUI, OPC_AUIPC, OPC_JAL, OPC_MISC: uses_alu = 1'b0;
            default: begin uses_alu = 1'b0; bad = 1'b1; end
        endcase
    end

    // the second ALU operand: a register, or the immediate for everything that has one
    reg [63:0] b_imm;
    always @* begin
        case (opcode)
            OPC_STORE: b_imm = imm_s;
            default:   b_imm = imm_i;
        endcase
    end
    // Stores and branches read rs2 as a register but the ALU's second operand differs: a store adds the
    // immediate, a branch compares the two registers.
    wire alu_b_is_reg = (opcode == OPC_OP) || (opcode == OPC_OP32) || (opcode == OPC_BRANCH);

    // ---- register file and ALU -----------------------------------------------------------------------------
    wire [7:0] rf_b1, rf_b2;
    wire       rf_pe1, rf_pe2;
    reg        rf_wr_en;
    reg [2:0]  rf_wr_slice;
    reg [7:0]  rf_wr_byte;
    wire       rf_rd_en    = (state == S_READ);   // the byte shown is for this beat
    wire [2:0] rf_rd_slice = cnt[2:0];

    sliced_regfile_serial #(.SPARES(SPARES)) rf (
        .clk(clk), .rst(rst),
        .rd1_en(rf_rd_en), .rd1_idx(rs1), .rd1_slice(rf_rd_slice), .rd1_byte(rf_b1), .rd1_perr(rf_pe1),
        .rd2_en(rf_rd_en), .rd2_idx(rs2), .rd2_slice(rf_rd_slice), .rd2_byte(rf_b2), .rd2_perr(rf_pe2),
        .wr_en(rf_wr_en), .wr_idx(rd), .wr_slice(rf_wr_slice), .wr_byte(rf_wr_byte),
        .remap_we(remap_we), .remap_logical(remap_logical), .remap_phys(remap_phys),
        .def_we(def_we), .def_phys(def_phys), .def_bit(def_bit), .def_val(def_val), .def_en(def_en),
        .flip_we(flip_we), .flip_word(flip_word), .flip_bit(flip_bit),
        .ph_addr(9'd0), .ph_wdata(8'd0), .ph_we(1'b0), .ph_rdata());

    reg        alu_start;
    reg [63:0] alu_a, alu_b;
    wire       alu_busy, alu_done;
    wire [63:0] alu_res;
    sliced_alu #(.K(K)) alu (.clk(clk), .rst(rst), .start(alu_start), .op(alu_op), .a(alu_a), .b(alu_b),
                             .busy(alu_busy), .done(alu_done), .result(alu_res));

    // ---- control ---------------------------------------------------------------------------------------------
    wire [63:0] pc_plus4 = pc + 64'd4;
    reg  [63:0] mem_addr_r;
    reg         is_load, is_store, wb_en;

    // load data, sign- or zero-extended from the access size
    // branch condition from the ALU result: BEQ taken when Xor is zero, BNE when not; BLT/BLTU when the
    // comparison is 1, BGE/BGEU when it is 0
    reg branch_cond;
    always @* begin
        case (funct3)
            3'd0:    branch_cond = (alu_res == 64'd0);
            3'd1:    branch_cond = (alu_res != 64'd0);
            3'd4:    branch_cond = (alu_res != 64'd0);
            3'd5:    branch_cond = (alu_res == 64'd0);
            3'd6:    branch_cond = (alu_res != 64'd0);
            default: branch_cond = (alu_res == 64'd0);
        endcase
    end

    reg [63:0] load_val;
    always @* begin
        case (funct3)
            3'd0: load_val = {{56{dmem_rdata[7]}},  dmem_rdata[7:0]};
            3'd1: load_val = {{48{dmem_rdata[15]}}, dmem_rdata[15:0]};
            3'd2: load_val = {{32{dmem_rdata[31]}}, dmem_rdata[31:0]};
            3'd3: load_val = dmem_rdata;
            3'd4: load_val = {56'd0, dmem_rdata[7:0]};
            3'd5: load_val = {48'd0, dmem_rdata[15:0]};
            default: load_val = {32'd0, dmem_rdata[31:0]};
        endcase
    end

    always @(posedge clk) begin
        if (rst) begin
            state <= S_FETCH; pc <= RESET_PC; inst <= 32'd0; cnt <= 4'd0;
            opa <= 64'd0; opb <= 64'd0; res <= 64'd0; next_pc <= 64'd0;
            dmem_en <= 1'b0; dmem_we <= 1'b0; dmem_addr <= 64'd0; dmem_funct3 <= 3'd0; dmem_wdata <= 64'd0;
            ret_valid <= 1'b0; ret_pc <= 64'd0; ret_inst <= 32'd0; ret_rd <= 5'd0; ret_rdval <= 64'd0;
            ret_st <= 1'b0; ret_st_addr <= 64'd0; ret_st_size <= 3'd0; ret_st_val <= 64'd0; ret_next_pc <= 64'd0;
            halted <= 1'b0; illegal <= 1'b0; parity_errors <= 32'd0; cycles <= 64'd0;
            rf_wr_en <= 1'b0; rf_wr_slice <= 3'd0; rf_wr_byte <= 8'd0;
            alu_start <= 1'b0; alu_a <= 64'd0; alu_b <= 64'd0;
            is_load <= 1'b0; is_store <= 1'b0; wb_en <= 1'b0; 
            mem_addr_r <= 64'd0;
        end else begin
            ret_valid <= 1'b0;
            alu_start <= 1'b0;
            dmem_en <= 1'b0; dmem_we <= 1'b0;
            rf_wr_en <= 1'b0;
            if (!halted) cycles <= cycles + 64'd1;

            case (state)
                S_FETCH: begin
                    inst <= imem_data;
                    cnt <= 4'd0;
                    state <= S_READ;
                end

                // 8 beats: byte cnt of rs1 and rs2 from the register file (x0 reads as zero)
                S_READ: begin
                    // the register file answers combinationally for the slice shown this beat
                    opa[8*cnt[2:0] +: 8] <= (rs1 == 5'd0) ? 8'd0 : rf_b1;
                    opb[8*cnt[2:0] +: 8] <= (rs2 == 5'd0) ? 8'd0 : rf_b2;
                    if (cnt == 4'd7) begin
                        // a parity mismatch is counted once per register actually read, never repaired here
                        if (rs1 != 5'd0 && rf_pe1 && opcode != OPC_LUI && opcode != OPC_AUIPC && opcode != OPC_JAL && opcode != OPC_MISC)
                            parity_errors <= parity_errors + 32'd1;
                        if (rs2 != 5'd0 && rf_pe2 && (opcode == OPC_OP || opcode == OPC_OP32 || opcode == OPC_STORE || opcode == OPC_BRANCH))
                            parity_errors <= parity_errors + 32'd1;
                        cnt <= 4'd0;
                        state <= S_EXEC;
                    end else cnt <= cnt + 4'd1;
                end

                S_EXEC: begin
                    is_load <= (opcode == OPC_LOAD);
                    is_store <= (opcode == OPC_STORE);
                    wb_en <= 1'b0;
                    next_pc <= pc_plus4;
                    if (bad || opcode == OPC_SYSTEM) begin
                        illegal <= bad && opcode != OPC_SYSTEM;
                        halted <= 1'b1;
                        state <= S_HALT;
                    end else if (uses_alu) begin
                        alu_a <= opa;
                        alu_b <= alu_b_is_reg ? opb : b_imm;
                        alu_start <= 1'b1;
                        state <= S_WAIT;
                    end else begin
                        // LUI, AUIPC, JAL, FENCE: plain adders
                        case (opcode)
                            OPC_LUI:   begin res <= imm_u; wb_en <= 1'b1; end
                            OPC_AUIPC: begin res <= pc + imm_u; wb_en <= 1'b1; end
                            OPC_JAL:   begin res <= pc_plus4; next_pc <= pc + imm_j; wb_en <= 1'b1; end
                            default: ;
                        endcase
                        cnt <= 4'd0;
                        state <= S_WB;
                    end
                end

                S_WAIT: begin
                    if (alu_done) begin
                        cnt <= 4'd0;
                        case (opcode)
                            OPC_OPIMM, OPC_OP, OPC_OPIMM32, OPC_OP32: begin res <= alu_res; wb_en <= 1'b1; state <= S_WB; end
                            OPC_JALR: begin res <= pc_plus4; next_pc <= alu_res & ~64'd1; wb_en <= 1'b1; state <= S_WB; end
                            OPC_BRANCH: begin
                                if (branch_cond) next_pc <= pc + imm_b;
                                state <= S_WB;
                            end
                            default: begin   // load or store: alu_res is the address
                                mem_addr_r <= alu_res;
                                dmem_addr <= alu_res;
                                dmem_funct3 <= funct3;
                                dmem_wdata <= opb;
                                dmem_en <= 1'b1;
                                dmem_we <= (opcode == OPC_STORE);
                                state <= S_MEM;
                            end
                        endcase
                    end
                end

                S_MEM: begin
                    // the combinational data port has answered in the cycle dmem_en was high
                    if (is_load) begin res <= load_val; wb_en <= 1'b1; end
                    cnt <= 4'd0;
                    state <= S_WB;
                end

                // 8 beats of write-back, or none if there is nothing to write
                S_WB: begin
                    if (wb_en && rd != 5'd0) begin
                        rf_wr_en <= 1'b1;
                        rf_wr_slice <= cnt[2:0];
                        rf_wr_byte <= res[8*cnt[2:0] +: 8];
                        if (cnt == 4'd7) state <= S_RETIRE; else cnt <= cnt + 4'd1;
                    end else state <= S_RETIRE;
                end

                S_RETIRE: begin
                    ret_valid <= 1'b1;
                    ret_pc <= pc; ret_inst <= inst;
                    ret_rd <= (wb_en && rd != 5'd0) ? rd : 5'd0;
                    ret_rdval <= res;
                    ret_st <= is_store; ret_st_addr <= mem_addr_r; ret_st_size <= funct3[1:0]; ret_st_val <= opb;
                    ret_next_pc <= next_pc;
                    pc <= next_pc;
                    state <= S_FETCH;
                end

                S_HALT: ;
                default: state <= S_HALT;
            endcase
        end
    end
endmodule
