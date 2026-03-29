#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h" 
// I2C defines + functions

// Default I2C address for most backpacks is 0x27 (sometimes 0x3F)
static int addr = 0x27;

// LCD Commands
const int LCD_CLEARDISPLAY = 0x01;
const int LCD_RETURNHOME = 0x02;
const int LCD_ENTRYMODESET = 0x04;
const int LCD_DISPLAYCONTROL = 0x08;
const int LCD_FUNCTIONSET = 0x20;
const int LCD_SETDDRAMADDR = 0x80;

// Flags for display control
const int LCD_DISPLAYON = 0x04;
const int LCD_2LINE = 0x08;
const int LCD_BACKLIGHT = 0x08;
const int LCD_ENABLE_BIT = 0x04;

#define LCD_CHARACTER  1
#define LCD_COMMAND    0

void i2c_write_byte(uint8_t val) {
    i2c_write_blocking(i2c0, addr, &val, 1, false);
}

void lcd_toggle_enable(uint8_t val) {
    sleep_us(600);
    i2c_write_byte(val | LCD_ENABLE_BIT);
    sleep_us(600);
    i2c_write_byte(val & ~LCD_ENABLE_BIT);
    sleep_us(600);
}

void lcd_send_byte(uint8_t val, int mode) {
    uint8_t high = mode | (val & 0xF0) | LCD_BACKLIGHT;
    uint8_t low = mode | ((val << 4) & 0xF0) | LCD_BACKLIGHT;

    i2c_write_byte(high);
    lcd_toggle_enable(high);
    i2c_write_byte(low);
    lcd_toggle_enable(low);
}

void lcd_clear() {
    lcd_send_byte(LCD_CLEARDISPLAY, LCD_COMMAND);
}

void lcd_set_cursor(int line, int position) {
    int val = (line == 0) ? 0x80 + position : 0xC0 + position;
    lcd_send_byte(val, LCD_COMMAND);
}

void lcd_string(const char *s) {
    while (*s) {
        lcd_send_byte(*s++, LCD_CHARACTER);
    }
}

void lcd_init() {
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x02, LCD_COMMAND);

    lcd_send_byte(LCD_ENTRYMODESET | 0x02, LCD_COMMAND);
    lcd_send_byte(LCD_FUNCTIONSET | LCD_2LINE, LCD_COMMAND);
    lcd_send_byte(LCD_DISPLAYCONTROL | LCD_DISPLAYON, LCD_COMMAND);
    lcd_clear();
}

//IR+LED DEFINES
#define LED_OUT 16
#define IR_IN 21


int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 100Khz.
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    lcd_init();

    //***IR + LED CONFIGURATION
    gpio_init(LED_OUT);
    gpio_set_dir(LED_OUT, GPIO_OUT);

    gpio_init(IR_IN);
    gpio_set_dir(IR_IN, GPIO_IN);
    gpio_pull_up(IR_IN); 
    //*** 

    while (true) {
        //***IR + LED CODE
        bool ir_val= gpio_get(IR_IN);
        printf("Hello, world!\n");
        printf("IR Sensor Value: %d\n", ir_val);   
       
        if(ir_val==0){
            gpio_put(LED_OUT, 1);
        }else{
            gpio_put(LED_OUT, 0);
        }
        //*** 


        //***LCD I2C
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_string("Hello Pico 2W!");
        lcd_set_cursor(1, 0);
        lcd_string("I2C LCD Test");
        //*** 

        
        sleep_ms(2000);
    }
    return 0;
}

