#include "stdlib.h"
#include "stdbool.h"

volatile int pixel_buffer_start; // global variable

//extern short MYIMAGE [240][320];
extern short SCORELIFE [24][165];
// y = 0-11 is for the score, x= 0-43 for the title, ( number * 11 ) + 44 
// y= 12-23 is for the lives, x= 0-45 for the title, ( 46-60 for a heart)

const int MIN_X = 15;
const int MAX_X = 304;
const int MIN_Y = 20;
const int MAX_Y = 234;

const int PADDLE_X = 64;
const int PADDLE_Y = 10;
const int PADDLE_POSITION_Y = 210;

const int PADDING = 1;

const int BOX_X = 35; //310 - 2 pixels (left/right pad) - 7 spaces = 301/8 = 
const int BOX_Y = 15;

const int TITLE_HEIGHT = 12;

const int NUM_BOX_PER_ROW = 8;
const int NUM_ROW_OF_BOX = 4;

const int BALL_SIZE = 6;

const short int BOX_COLOUR = 0x9ac4;
const short int BOX_BORDER_COLOUR = 0x9e84;
const short int WALLS_COLOUR = 0x8430;
const short int PADDLE_COLOUR = 0x411e;
const short int BALL_COLOUR = 0xFFFF;

const int SIMULATOR_ADJUSTMENT = 8;

// 0 for empty space, 1 for brick, 2 for wall
int boardStatus[240][320];

void plot_boxes(int xx, int yy, int x_size, int y_size, short int box_colour);
void drawBall(int left_x, int top_y, int size);
void clear_screen();
void writeScoreAndLife(int score, int lives);
void swap(int *n0, int *n1);
int abs(int x);
void plot_pixel(int x, int y, short int line_color);
void wait_for_vsync();

int main(void) {

    volatile int * pixel_ctrl_ptr = (int *) 0xFF203020;
    volatile int * key_ptr = (int *) 0xFF200050;

    int score = 0;
    int lives = 3;

    bool drawBrick[NUM_ROW_OF_BOX][NUM_BOX_PER_ROW];
    int i,j;
    for(i=0; i<NUM_ROW_OF_BOX; i++){
        for(j=0; j<NUM_BOX_PER_ROW; j++){
            drawBrick[i][j] = true;
        } 
    }

    bool padLeft = false;
    bool padRight = false;

    int paddleX = (MAX_X + MIN_X)/2 - PADDLE_X/2;

    /* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                        // back buffer

    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();

    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer

    /* set back pixel buffer to start of SDRAM memory */
    // without double buffer, sometimes near the top/left the image gets cutoff/missing(black)
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer

    int ball_x = paddleX + PADDLE_X/2 - BALL_SIZE/2; // paddleX+PADDLE_X/2 = center of paddle; - BALL_SIZE/2 to get left of ball
    int ball_y = PADDLE_POSITION_Y - BALL_SIZE; // PADDLE_POSITION_Y = top of box; + BALL_SIZE to have box sitting on paddle
    int ball_dx = rand()%3-1;
    int ball_dy = -2; // minus is up

    
    bool round_started = false;
    while (true){
        // Read key values
        short int keyPressCurrent = (*key_ptr);
        short int keyPressEdge = *(key_ptr + 3);
        *(key_ptr + 3) = keyPressEdge; // reset edge 
        short int keyPress = (keyPressCurrent | keyPressEdge); // so either if the key is being pressed or if it was pressed while drawing

        int x,y;
        
        // Handle waiting for KEY1 to be pressed to start a life
        if (!round_started) {
            if (keyPress == 0b0010 && lives > 0) {
                round_started = true;
            } else if (keyPress == 0b1000 && lives == 0){ // if key3, reset everything
                round_started = false;

                for(y=0; y<NUM_ROW_OF_BOX; y++){
                    for(x=0; x<NUM_BOX_PER_ROW; x++){
                        drawBrick[y][x] = true;
                    }
                }

                score = 0;
                lives = 3;

                paddleX = (MAX_X + MIN_X)/2 - PADDLE_X/2;
                ball_x = paddleX + PADDLE_X/2 - BALL_SIZE/2; // paddleX+PADDLE_X/2 = center of paddle; - BALL_SIZE/2 to get left of ball
                ball_y = PADDLE_POSITION_Y - BALL_SIZE; // PADDLE_POSITION_Y = top of box; + BALL_SIZE to have box sitting on paddle
                ball_dx = rand()%3-1;
                ball_dy = -2; // minus is up
            } else {
                // Initial Draw
                clear_screen();
                writeScoreAndLife(score, lives);
                drawBall(ball_x, ball_y, BALL_SIZE);
                
                for(y=0; y<NUM_ROW_OF_BOX; y++){
                    for(x=0; x<NUM_BOX_PER_ROW; x++){
                        if(drawBrick[y][x]){
                            plot_boxes(MIN_X + PADDING + x*(BOX_X+PADDING), MIN_Y + PADDING + y*(BOX_Y+PADDING), BOX_X, BOX_Y, BOX_COLOUR);
                        }
                    }
                }
                plot_boxes(paddleX, PADDLE_POSITION_Y, PADDLE_X, PADDLE_Y, PADDLE_COLOUR);
                wait_for_vsync(); // swap front and back buffers on VGA vertical sync
                pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
                continue;
            }
        }

        /* Check whether the ball has hit anything */
        // If it's hit a brick, remove the brick
        for(y=0; y<NUM_ROW_OF_BOX; y++) {
            for(x=0; x<NUM_BOX_PER_ROW; x++){
                if(drawBrick[y][x]) {
                    int box_left = MIN_X+PADDING+x*(BOX_X+PADDING); 
                    int box_right = box_left + BOX_X;
                    int box_top = MIN_Y+PADDING+y*(BOX_Y+PADDING);
                    int box_bot = box_top + BOX_Y;
                    if (ball_x+BALL_SIZE >= box_left && ball_x < box_right) {
                        // ball is within X range of box
                        if (ball_y+BALL_SIZE >= box_top && ball_y < box_bot) {
                            // ball is within Y range of box
                            // break box! 
                            drawBrick[y][x] = false;
                            // bounce ball
                            // if bottom of ball is below box: flip dy
                            // if top of ball is above box: flip dy
                            // if completely in middle (y-wise): keep dy direction, change speed
                            if (ball_y+BALL_SIZE > box_bot || ball_y < box_top) {
                                ball_dy *= -1;
                            } else {
                                ball_dy += (rand()%2)*(ball_dy/abs(ball_dy)); // a number 0,1,2 in direction of dy
                                // if it's too large (>4), halve it
                                if (abs(ball_dy) > 4) {
                                    ball_dy /= 2;
                                }
                            }

                            // if left of ball is left of box: flip dx
                            // if right of ball is right of box: flip dx
                            // if completely in middle (x-ise): keep dx direction
                            if (ball_x+BALL_SIZE > box_right || ball_x < box_left) {
                                ball_dx *= -1;
                            } else {
                                ball_dx += (rand()%2)*(ball_dx/abs(ball_dx)); // a number 0,1,2 in direction of dy
                                // if it's too large (>6), halve it
                                if (abs(ball_dx) > 6) {
                                    ball_dx /= 2;
                                }
                            }
                            // THUS, |dx| must be less than BOX_X, |dy| must be less than BOX_Y, I think?
                            // exit, you can only break one box per hit
                            score += 1;
                            break;
                        }
                    }
                }
            }
        }
        // If hit a side wall, flip dx
        if (ball_x < MIN_X || ball_x+BALL_SIZE >= MAX_X) {
            ball_dx *= -1;
            // add rand() too?
        }
        // If hit the top wall or paddle, flip dy
        if (ball_y < MIN_Y || ((ball_y + BALL_SIZE) > PADDLE_POSITION_Y && ball_y < PADDLE_POSITION_Y+PADDLE_Y && ball_x > paddleX && ball_x < paddleX+PADDLE_X)) {
            ball_dy *= -1;
            // add rand() too?
        }
        // If went below paddle & off screen, lose a life && reset
        if (ball_y+BALL_SIZE >= MAX_Y) {
            lives--;
            round_started = false;
            paddleX = (MAX_X + MIN_X)/2 - PADDLE_X/2;
            ball_x = paddleX + PADDLE_X/2 - BALL_SIZE/2; // paddleX+PADDLE_X/2 = center of paddle; - BALL_SIZE/2 to get left of ball
            ball_y = PADDLE_POSITION_Y - BALL_SIZE; // PADDLE_POSITION_Y = top of box; + BALL_SIZE to have box sitting on paddle
            ball_dx = rand()%3-1;
            ball_dy = -2; // minus is up
            continue;
        }

        /* Move paddle based on KEY presses */
        padLeft = false;
        padRight = false;
        // Set whether moving left or right
        if(keyPress == 0b100){
            padLeft = true;
        } else if(keyPress == 0b001){
            padRight = true;
        }
        // Update paddle location
        if(padLeft){
            paddleX -=4;
        } else if(padRight){
            paddleX +=4;
        }
        // Don't go too far!
        if(paddleX <= MIN_X){
            paddleX = MIN_X;
        } else if((paddleX+PADDLE_X) >= MAX_X){
            paddleX = MAX_X-PADDLE_X;
        }

        /* Draw new screen */
        // Erase old buffer
        clear_screen();

        // draw the score and the lives
        writeScoreAndLife(score, lives);

        // redraw the boxes and paddle
        // TODO: assign paddle & ball here. Or get rid of this? I think it's unnecessary
        for(x=0; x<320; x++){
            for(y=0; y<240; y++){
                if(x<=MIN_X || x>=MAX_X || y<=MIN_Y || y>=MAX_Y){
                    boardStatus[y][x] = 2;
                } else {
                    boardStatus[y][x] = 0;
                }
            }
        }

        drawBall(ball_x, ball_y, BALL_SIZE);

        for(y=0; y<NUM_ROW_OF_BOX; y++){
            for(x=0; x<NUM_BOX_PER_ROW; x++){
                if(drawBrick[y][x]){
                    plot_boxes(MIN_X + PADDING + x*(BOX_X+PADDING), MIN_Y + PADDING + y*(BOX_Y+PADDING), BOX_X, BOX_Y, BOX_COLOUR);
                }
            }
        }

        plot_boxes(paddleX, PADDLE_POSITION_Y, PADDLE_X, PADDLE_Y, PADDLE_COLOUR);

        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
        
        /* Move ball */
        // TODO: Slope & travel
        ball_x = ball_x + ball_dx;//*SIMULATOR_ADJUSTMENT;
        ball_y = ball_y + ball_dy;//*SIMULATOR_ADJUSTMENT;
    }
}

int abs(int x) {
    if (x < 0)
        return -1*x;
    else
        return x;
}

// code for subroutines (not shown)
void writeScoreAndLife(int score, int lives){
    int scoreDigit[3];
    scoreDigit[0] = (score/100);
    scoreDigit[1] = (score/10) % 10;
    scoreDigit[2] = (score) % 10;

    int startY = MIN_Y / 2 - TITLE_HEIGHT / 2 - 1;
    int startXScore = MIN_X;
    int startXLife = 200;

    // draw the score title
    int x,y;
    for(x=0; x<45; x++){
        for(y=0; y<TITLE_HEIGHT; y++){
            plot_pixel(x + startXScore, y+startY, SCORELIFE[y][x+2]);
        }
    }

    // draw the score
    int i;
    for(i=0; i<3; i++){
        for(x=0; x<11; x++){
            for(y=0; y<TITLE_HEIGHT; y++){
                plot_pixel(x + startXScore + 45 + (11*i), y+startY, SCORELIFE[y][x + 45 + (11*scoreDigit[i])]);
            }
        }
    }

    // draw the life title
    for(x=0; x<47; x++){
        for(y=0; y<TITLE_HEIGHT; y++){
            plot_pixel(x + startXLife, y+startY, SCORELIFE[TITLE_HEIGHT + y][x]);
        }
    }

    // draw the life hearts
    for(i=0; i<lives; i++){
        for(x=0; x<16; x++){
            for(y=0; y<TITLE_HEIGHT; y++){
                plot_pixel(x + startXLife + 47 + (i*16), y+startY, SCORELIFE[TITLE_HEIGHT + y][x + 47]);
            }
        }
    }
}



// write all addresses in buffer to draw the background picture
void clear_screen(){
    int x,y;
    for(x=0 ; x<320 ; x++){
        for(y=0 ; y<240 ; y++){
            if((x<MIN_X || x>=MAX_X || y<MIN_Y || y>=MAX_Y)){
                boardStatus[y][x] = 2;
                if(MIN_Y / 2 + TITLE_HEIGHT / 2 > y){
                    plot_pixel(x,y,SCORELIFE[1][0]);
                } else {
                    plot_pixel(x,y,WALLS_COLOUR);
                }
            } else {
                boardStatus[y][x] = 0;
                plot_pixel(x,y,0);
            }
        }
    }
}

// swap n0 and n1
void swap(int *n0, int *n1){
    int temp = *n1;
    *n1 = *n0;
    *n0 = temp;
}

// plots a x_size by y_size rectangle
void plot_boxes(int xx, int yy, int x_size, int y_size, short int box_colour){
    int x,y;

    for(x=0 ; x < x_size ; x++){
        for(y=0 ; y < y_size ; y++){
            boardStatus[y][x] = 1;
            if((x<2 || x>=x_size-2 || y<2 || y>=y_size-2)){
                plot_pixel(xx + x,yy + y,BOX_BORDER_COLOUR);
            } else {
                plot_pixel(xx + x,yy + y,box_colour);
            }
        }
    }
}

// Draws the ball
void drawBall(int left_x, int top_y, int size) {
    int x,y;
    for (x = 0; x < size; x++) {
        for (y = 0; y < size; y++) {
            boardStatus[y][x] = 3;
            plot_pixel(left_x + x, top_y + y, BALL_COLOUR);
        }
    }
}

// plot the pixel in the buffer
void plot_pixel(int x, int y, short int pixel_colour){
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = pixel_colour;
}

// wait for the vsync to happen
void wait_for_vsync(){
    volatile int * pixel_ctrl_ptr = (int *) 0xFF203020; // pixel controller
    register int status;

    *pixel_ctrl_ptr = 1; // start the sync process

    status = *(pixel_ctrl_ptr+3);
    while((status & 0x01) != 0){
        status = *(pixel_ctrl_ptr + 3);
    }
}
