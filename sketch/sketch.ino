#include <Arduino_RouterBridge.h>
#include <Arduino_LED_Matrix.h>

Arduino_LED_Matrix matrix;

void setup() {
    matrix.begin();
    matrix.setGrayscaleBits(1);
    Bridge.begin();
    Bridge.provide("set_led_column", set_led_column);
}

void loop() {}

void set_led_column(int colPos) {
    uint8_t frame[104] = {0};
    const int row_num = 52;
    frame[row_num + colPos] = 1;
    matrix.draw(frame);
}
