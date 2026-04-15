#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h" 
#include "pico/util/queue.h" 
#include "pico/util/datetime.h"


//IR+LED DEFINES
#define LED_OUT 6
#define IR_IN 21


//WTV AUDIO MODULE

#define WTV_RESET 14
#define WTV_DI 13
#define WTV_CLK  15
#define WTV_BUSY  28

//LCD + I2C
#define I2C_SDA 4
#define I2C_SCL 5

// I2C defines + functions

// Default I2C address for most backpacks is 0x27 (sometimes 0x3F)
static int addr = 0x27;

// --- HD44780 LCD Commands ---
// Base hex commands for LCD operations
const int LCD_CLEARDISPLAY = 0x01;
const int LCD_RETURNHOME = 0x02;
const int LCD_ENTRYMODESET = 0x04;
const int LCD_DISPLAYCONTROL = 0x08;
const int LCD_FUNCTIONSET = 0x20;
const int LCD_SETDDRAMADDR = 0x80;

// --- Flags for display control ---
// Modifiers added to base commands (e.g., turning display on, 2-line mode)
const int LCD_DISPLAYON = 0x04;
const int LCD_2LINE = 0x08;
const int LCD_BACKLIGHT = 0x08;  // I2C backpack pin to keep backlight LED on
const int LCD_ENABLE_BIT = 0x04; // I2C backpack pin for the Enable (EN) pulse

// Register Select (RS) mode: 1 for printing text, 0 for sending commands
#define LCD_CHARACTER  1
#define LCD_COMMAND    0

// Writes 1 single byte over I2C to the LCD's address, false = generate STOP condition
void i2c_write_byte(uint8_t val) {
    i2c_write_blocking(i2c0, addr, &val, 1, false);
}

// Toggles the Enable (EN) pin HIGH then LOW to tell the LCD to read the data lines.
// 600 us delays give the slow LCD processor enough time to register the pulse.
void lcd_toggle_enable(uint8_t val) {
    sleep_us(600);
    i2c_write_byte(val | LCD_ENABLE_BIT);  // Set EN high
    sleep_us(600);
    i2c_write_byte(val & ~LCD_ENABLE_BIT); // Set EN low
    sleep_us(600);
}

// The LCD operates in 4-bit mode, so an 8-bit byte must be split into 2 x 4-bit transfers
void lcd_send_byte(uint8_t val, int mode) {
    uint8_t high = mode | (val & 0xF0) | LCD_BACKLIGHT;        // Extract the first (high) 4 bits
    uint8_t low = mode | ((val << 4) & 0xF0) | LCD_BACKLIGHT;  // Shift and extract the last (low) 4 bits

    i2c_write_byte(high);
    lcd_toggle_enable(high); // Send high bits and pulse
    i2c_write_byte(low);
    lcd_toggle_enable(low);  // Send low bits and pulse
}

// Clears all text from the display
void lcd_clear() {
    lcd_send_byte(LCD_CLEARDISPLAY, LCD_COMMAND);
}

// Moves the cursor to a specific row (0 or 1) and column (position).
// Line 0 starts at memory address 0x80, Line 1 starts at 0xC0.
void lcd_set_cursor(int line, int position) {
    int val = (line == 0) ? 0x80 + position : 0xC0 + position;
    lcd_send_byte(val, LCD_COMMAND);
}

// Iterates through a C-string and sends it character by character to the screen
void lcd_string(const char *s) {
    while (*s) {
        lcd_send_byte(*s++, LCD_CHARACTER); // Send as CHARACTER mode
    }
}

// Initialization "handshake" to wake up the LCD and configure settings
void lcd_init() {
    // 1. Hardware reset sequence to guarantee a known starting state
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    
    // 2. Command to switch the LCD from default 8-bit mode to 4-bit mode
    lcd_send_byte(0x02, LCD_COMMAND);

    // 3. Configure display settings: Auto-increment cursor, 2 lines, Display ON
    lcd_send_byte(LCD_ENTRYMODESET | 0x02, LCD_COMMAND);
    lcd_send_byte(LCD_FUNCTIONSET | LCD_2LINE, LCD_COMMAND);
    lcd_send_byte(LCD_DISPLAYCONTROL | LCD_DISPLAYON, LCD_COMMAND);
    
    // 4. Wipe any random garbage memory from startup
    lcd_clear();
}

// LCD QUEUE
typedef struct {
    char row1[17];
    char row2[17];
}mesaj_lcd;

queue_t coada_lcd;

void lcd_task(){
    mesaj_lcd msg;
    if (queue_try_remove(&coada_lcd, &msg)) {
        lcd_clear();
        if (strlen(msg.row1) > 0) {
            lcd_set_cursor(0, 0); 
            lcd_string(msg.row1);
            lcd_set_cursor(1, 0); 
            lcd_string(msg.row2);
        }
    }
}

//DETECTIE PE ZILE
typedef enum{
    Azi=1,
    Maine=2,
    Poimaine=3,
    LimitaAtinsa=4
}zile;

void procesare_detectie(int n, mesaj_lcd *m){
    switch(n) {
        case Azi:
            strcpy(m->row1, "Ziua: Azi");
            break;
        case Maine:
            strcpy(m->row1, "Ziua: Maine");
            break;
        case Poimaine:
            strcpy(m->row1, "Ziua: Poimaine");
            break;
        case LimitaAtinsa:
            strcpy(m->row1, "Limita atinsa");
                break;
        default:
            strcpy(m->row1, "Input zile");
                break;
        }

}
int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 100Khz.
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);


    //***IR + LED CONFIGURATION
    gpio_init(LED_OUT);
    gpio_set_dir(LED_OUT, GPIO_OUT);

    gpio_init(IR_IN);
    gpio_set_dir(IR_IN, GPIO_IN);
    gpio_pull_up(IR_IN); 
    //*** 

    lcd_init();
    //QUEUE
    queue_init(&coada_lcd,sizeof(mesaj_lcd),10); //max 10 mesaje
   
    int nr_detectari=0;
    bool stare_ir = true;

    while (true) {
        //***IR + LED CODE
        bool ir_val= gpio_get(IR_IN);
        printf("IR Sensor Value: %d \n", ir_val);   
        mesaj_lcd mesaj_nou= {"", ""};

        lcd_task();
        //daca s-a schimbat starea fata de cea dinainte
        if(ir_val != stare_ir){
            //detectie, pana in 3 pentru azi maine poimaine!!!!!
            if(ir_val==0 && nr_detectari<3){
                
                printf("S-a detectat miscare. \n");
                gpio_put(LED_OUT, 1);
                printf("S-a aprins LED-ul. \n");
                
                nr_detectari++;
                 
                procesare_detectie(nr_detectari, &mesaj_nou);
                snprintf(mesaj_nou.row2, 17, "Detectia nr: %d", nr_detectari);
                queue_add_blocking(&coada_lcd, &mesaj_nou);
                printf("S-a trimis mesajul. \n");

               
                absolute_time_t timer = make_timeout_time_ms(3000);
                // Asteptare 3s non-blocking
                while (!time_reached(timer)) {
                    tight_loop_contents(); 
                }
                
                // Confirmare prezenta dupa cele 3 secunde
                if(gpio_get(IR_IN) == 0 && nr_detectari < 3){
                printf("NOUA MISCARE DETECTATA \n");

                nr_detectari++; 
                
                procesare_detectie(nr_detectari, &mesaj_nou);
                snprintf(mesaj_nou.row2, 17, "INCA AICI: %d", nr_detectari);
                queue_add_blocking(&coada_lcd, &mesaj_nou);
            }

            }else{
                gpio_put(LED_OUT, 0);
                //resetare dupa atingerea pragului de 3 secunde
                if(nr_detectari ==3){
                    nr_detectari++;
                    printf("S-a atins limita de zile. \n");
                    procesare_detectie(nr_detectari, &mesaj_nou);
                    strcpy(mesaj_nou.row2, "Resetare...");
                    queue_add_blocking(&coada_lcd, &mesaj_nou);

                    sleep_ms(2000); 
                    nr_detectari = 0;

                    printf("S-a resetat numaratoarea. \n");
                    strcpy(mesaj_nou.row1, "Gata de start");
                    queue_add_blocking(&coada_lcd, &mesaj_nou);
                }
                strcpy(mesaj_nou.row1, "Asteptare...");
                snprintf(mesaj_nou.row2, 17, "Total: %d/3", nr_detectari);
                queue_add_blocking(&coada_lcd, &mesaj_nou);
            }

            printf("Nr detectari: %d \n", nr_detectari);
            sleep_ms(100);
        }
        stare_ir=ir_val;
        sleep_ms(500);
       
    }
    return 0;
}

