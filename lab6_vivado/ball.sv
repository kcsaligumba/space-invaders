//-------------------------------------------------------------------------
//    Ball.sv                                                            --
//    Viral Mehta                                                        --
//    Spring 2005                                                        --
//                                                                       --
//    Modified by Stephen Kempf     03-01-2006                           --
//                                  03-12-2007                           --
//    Translated by Joe Meng        07-07-2013                           --
//    Modified by Zuofu Cheng       08-19-2023                           --
//    Modified by Satvik Yellanki   12-17-2023                           --
//    Fall 2024 Distribution                                             --
//                                                                       --
//    For use with ECE 385 USB + HDMI Lab                                --
//    UIUC ECE Department                                                --
//-------------------------------------------------------------------------


module  ball 
( 
    input  logic        Reset, 
    input  logic        frame_clk,
    input  logic [7:0]  keycode,

    output logic [9:0]  BallX, 
    output logic [9:0]  BallY, 
    output logic [9:0]  BallS 
);
    

	 
    parameter [9:0] Ball_X_Center=320;  // Center position on the X axis
    parameter [9:0] Ball_Y_Center=240;  // Center position on the Y axis
    parameter [9:0] Ball_X_Min=0;       // Leftmost point on the X axis
    parameter [9:0] Ball_X_Max=639;     // Rightmost point on the X axis
    parameter [9:0] Ball_Y_Min=0;       // Topmost point on the Y axis
    parameter [9:0] Ball_Y_Max=479;     // Bottommost point on the Y axis
    parameter [9:0] Ball_X_Step=1;      // Step size on the X axis
    parameter [9:0] Ball_Y_Step=1;      // Step size on the Y axis

    logic [9:0] Ball_X_Motion;
    logic [9:0] Ball_X_Motion_next;
    logic [9:0] Ball_Y_Motion;
    logic [9:0] Ball_Y_Motion_next;

    logic [9:0] Ball_X_next;
    logic [9:0] Ball_Y_next;
    
    localparam [7:0] KEY_W = 8'h1A;
    localparam [7:0] KEY_A = 8'h04;
    localparam [7:0] KEY_S = 8'h16;
    localparam [7:0] KEY_D = 8'h07;

    localparam [9:0] NEG_X_STEP = (~Ball_X_Step) + 10'd1;
    localparam [9:0] NEG_Y_STEP = (~Ball_Y_Step) + 10'd1;

    always_comb begin
        Ball_Y_Motion_next = Ball_Y_Motion; // set default motion to be same as prev clock cycle 
        Ball_X_Motion_next = Ball_X_Motion;

        //modify to control ball motion with the keycode
        unique case (keycode)
            KEY_W: begin
                Ball_X_Motion_next = 10'd0;
                Ball_Y_Motion_next = NEG_Y_STEP;
            end

            KEY_A: begin
                Ball_X_Motion_next = NEG_X_STEP;
                Ball_Y_Motion_next = 10'd0;
            end

            KEY_S: begin
                Ball_X_Motion_next = 10'd0;
                Ball_Y_Motion_next = Ball_Y_Step;
            end

            KEY_D: begin
                Ball_X_Motion_next = Ball_X_Step;
                Ball_Y_Motion_next = 10'd0;
            end

            default: begin
                
            end
        endcase

        // Bounce vertically only when the ball is actually traveling vertically.
        if ((Ball_Y_Motion_next != 10'd0) &&
            (Ball_Y_Motion_next[9] == 1'b0) &&
            ((BallY + BallS) >= Ball_Y_Max))
        begin
            Ball_Y_Motion_next = NEG_Y_STEP;
        end
        else if ((Ball_Y_Motion_next[9] == 1'b1) &&
                 ((BallY - BallS) <= Ball_Y_Min))
        begin
            Ball_Y_Motion_next = Ball_Y_Step;
        end

        // Bounce horizontally only when the ball is actually traveling horizontally.
        if ((Ball_X_Motion_next != 10'd0) &&
            (Ball_X_Motion_next[9] == 1'b0) &&
            ((BallX + BallS) >= Ball_X_Max))
        begin
            Ball_X_Motion_next = NEG_X_STEP;
        end
        else if ((Ball_X_Motion_next[9] == 1'b1) &&
                 ((BallX - BallS) <= Ball_X_Min))
        begin
            Ball_X_Motion_next = Ball_X_Step;
        end
    end

    assign BallS = 16;  // default ball size
    assign Ball_X_next = (BallX + Ball_X_Motion_next);
    assign Ball_Y_next = (BallY + Ball_Y_Motion_next);
   
    always_ff @(posedge frame_clk) //make sure the frame clock is instantiated correctly
    begin: Move_Ball
        if (Reset)
        begin 
            Ball_Y_Motion <= 10'd0; //Ball_Y_Step;
			Ball_X_Motion <= 10'd0; //Ball_X_Step;
            
			BallY <= Ball_Y_Center;
			BallX <= Ball_X_Center;
        end
        else 
        begin 

			Ball_Y_Motion <= Ball_Y_Motion_next; 
			Ball_X_Motion <= Ball_X_Motion_next; 

            BallY <= Ball_Y_next;  // Update ball position
            BallX <= Ball_X_next;
			
		end  
    end


    
      
endmodule
