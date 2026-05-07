`timescale 1ns / 1ps

// Generic sprite ROM. One row per address; bit [WIDTH-1] is leftmost pixel.
// INIT_FILE is a $readmemh-compatible text file with one hex row per line.
// Registered read: data appears one clock after addr is latched.
module sprite_rom #(
    parameter int WIDTH     = 16,
    parameter int HEIGHT    = 8,
    parameter     INIT_FILE = ""
) (
    input  logic                          clk,
    input  logic [$clog2(HEIGHT)-1:0]     row,
    output logic [WIDTH-1:0]              row_bits
);
    logic [WIDTH-1:0] mem [0:HEIGHT-1];

    initial begin
        if (INIT_FILE != "")
            $readmemh(INIT_FILE, mem);
    end

    always_ff @(posedge clk)
        row_bits <= mem[row];
endmodule
