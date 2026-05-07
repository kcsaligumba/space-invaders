`timescale 1 ns / 1 ps

module score_hex_display (
    input  logic        clk,
    input  logic        reset,
    input  logic [23:0] score_bcd,

    output logic [7:0]  hex_segA,
    output logic [3:0]  hex_gridA,
    output logic [7:0]  hex_segB,
    output logic [3:0]  hex_gridB
);

    logic [3:0] digits_a [4];
    logic [3:0] digits_b [4];

    // Lower four score digits: thousands, hundreds, tens, ones.
    assign digits_b[0] = score_bcd[3:0];
    assign digits_b[1] = score_bcd[7:4];
    assign digits_b[2] = score_bcd[11:8];
    assign digits_b[3] = score_bcd[15:12];

    // Upper two score digits, padded with leading zeroes for an 8-digit display.
    assign digits_a[0] = score_bcd[19:16];
    assign digits_a[1] = score_bcd[23:20];
    assign digits_a[2] = 4'h0;
    assign digits_a[3] = 4'h0;

    hex_driver HexA (
        .clk      (clk),
        .reset    (reset),
        .in       (digits_a),
        .hex_seg  (hex_segA),
        .hex_grid (hex_gridA)
    );

    hex_driver HexB (
        .clk      (clk),
        .reset    (reset),
        .in       (digits_b),
        .hex_seg  (hex_segB),
        .hex_grid (hex_gridB)
    );

endmodule
