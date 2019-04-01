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

const short int BOX_COLOUR = 0x9ac4;
const short int BOX_BORDER_COLOUR = 0x9e84;
const short int WALLS_COLOUR = 0x8430;
const short int PADDLE_COLOUR = 0x411e;


// 0 for empty space, 1 for brick, 2 for wall
int boardStatus[240][320];

void plot_boxes(int xx, int yy, int x_size, int y_size, short int box_colour);
void clear_screen();
void writeScoreAndLife(int score, int lives);
void swap(int *n0, int *n1);
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

    while (lives > 0){
        padLeft = false;
        padRight = false;

        short int keyPressCurrent = (*key_ptr);
        short int keyPressEdge = *(key_ptr + 3);
        *(key_ptr + 3) = keyPressEdge;
        short int keyPress = (keyPressCurrent | keyPressEdge); // so either if the key is being pressed or if it was pressed while drawing

        // read the key values
        if(keyPress == 0b100){
            padLeft = true;
        } else if(keyPress == 0b001){
            padRight = true;
        }

        // update paddle location
        if(padLeft){
            paddleX -=4;
        } else if(padRight){
            paddleX +=4;
        }

        if(paddleX <= MIN_X){
            paddleX = MIN_X;
        } else if((paddleX+PADDLE_X) >= MAX_X){
            paddleX = MAX_X-PADDLE_X;
        }

        // Erase old buffer
        clear_screen();

        // draw the score and the lives
        writeScoreAndLife(score, lives);

        // redraw the boxes and paddle
        int x,y;
        for(x=0; x<320; x++){
            for(y=0; y<240; y++){
                if(x<=MIN_X || x>=MAX_X || y<=MIN_Y || y>=MAX_Y){
                    boardStatus[y][x] = 2;
                } else {
                    boardStatus[y][x] = 0;
                }
            }
        }

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
    }
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