#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"

// WS2812 PIO program
// Generates proper WS2812 timing: bit 1 = long HIGH, bit 0 = short HIGH
// This is the standard WS2812 PIO program from Raspberry Pi examples
const uint16_t ws2812_program_instructions[] = {
    //     .wrap_target
    0x6221, //  0: out    x, 1           side 0 [2] ; Side-set still takes place when instruction stalls
    0x1123, //  1: jmp    !x, 3          side 1 [1] ; Branch on the bit we shifted out. Positive pulse
    0x1400, //  2: jmp    0              side 1 [4] ; Continue driving high, for a long pulse
    0xa442, //  3: nop                   side 0 [4] ; Or drive low, for a short pulse
    //     .wrap
};

const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

// Configuration
#define LED_PIN 28
#define NUM_LEDS_MAX 24
PIO pio = pio1;
uint sm = 0;
uint offset;

// Adjustable parameters
float clock_div = 18.0f;  // Adjusted for your Pico - gives ~1.2μs per bit
int num_leds = 24;
bool test_mode = false;
uint8_t test_pattern = 0;

// LED buffer
uint32_t led_buffer[NUM_LEDS_MAX];

// Initialize PIO for WS2812
void ws2812_pio_init() {
    // Load program
    offset = pio_add_program(pio, &ws2812_program);
    
    // Configure state machine
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    
    // Configure sideset - 1 bit, not optional, no pindirs
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, LED_PIN);
    
    // Set clock divider
    sm_config_set_clkdiv(&c, clock_div);
    
    // Configure shift - shift RIGHT (LSB first), autopull at 24 bits
    sm_config_set_out_shift(&c, true, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    
    // Initialize GPIO for PIO use
    pio_gpio_init(pio, LED_PIN);
    
    // Set pin direction to output
    pio_sm_set_consecutive_pindirs(pio, sm, LED_PIN, 1, true);
    
    // Initialize and start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    
    Serial.println("PIO initialized:");
    Serial.print("  PIO: pio1, SM: ");
    Serial.println(sm);
    Serial.print("  Program offset: ");
    Serial.println(offset);
    Serial.print("  Clock div: ");
    Serial.println(clock_div, 3);
    Serial.print("  Pin: GPIO");
    Serial.println(LED_PIN);
}

// Restart PIO with new parameters
void ws2812_pio_restart() {
    pio_sm_set_enabled(pio, sm, false);
    pio_remove_program(pio, &ws2812_program, offset);
    ws2812_pio_init();
}

// Reverse bits in a byte
uint8_t reverse_byte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// Convert RGB to GRB format (WS2812 order) with bit reversal
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    // Reverse each byte since PIO shifts LSB first
    g = reverse_byte(g);
    r = reverse_byte(r);
    b = reverse_byte(b);
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

// Send LED data
void ws2812_send() {
    for (int i = 0; i < num_leds; i++) {
        pio_sm_put_blocking(pio, sm, led_buffer[i]);
    }
    delayMicroseconds(60);  // RES time >50μs
}

// Test patterns
void generate_test_pattern() {
    static uint8_t anim_offset = 0;  // For animations
    
    switch (test_pattern) {
        case 0:  // All off
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                led_buffer[i] = 0;
            }
            break;
            
        case 1:  // Single LED red
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                led_buffer[i] = (i == 0) ? rgb_to_grb(255, 0, 0) : 0;
            }
            break;
            
        case 2:  // Single LED green
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                led_buffer[i] = (i == 0) ? rgb_to_grb(0, 255, 0) : 0;
            }
            break;
            
        case 3:  // Single LED blue
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                led_buffer[i] = (i == 0) ? rgb_to_grb(0, 0, 255) : 0;
            }
            break;
            
        case 4:  // All LEDs dim white
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                led_buffer[i] = rgb_to_grb(10, 10, 10);
            }
            break;
            
        case 5:  // Rainbow (static)
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                uint8_t pos = (i * 256 / NUM_LEDS_MAX) & 0xFF;
                if (pos < 85) {
                    led_buffer[i] = rgb_to_grb(pos * 3, 255 - pos * 3, 0);
                } else if (pos < 170) {
                    pos -= 85;
                    led_buffer[i] = rgb_to_grb(255 - pos * 3, 0, pos * 3);
                } else {
                    pos -= 170;
                    led_buffer[i] = rgb_to_grb(0, pos * 3, 255 - pos * 3);
                }
                // Dim for safety
                led_buffer[i] = ((led_buffer[i] >> 16) & 0xFF) / 10 << 16 |
                               ((led_buffer[i] >> 8) & 0xFF) / 10 << 8 |
                               (led_buffer[i] & 0xFF) / 10;
            }
            break;
            
        case 6:  // Rainbow chase (animated)
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                uint8_t pos = ((i * 256 / NUM_LEDS_MAX) + anim_offset) & 0xFF;
                if (pos < 85) {
                    led_buffer[i] = rgb_to_grb(pos * 3, 255 - pos * 3, 0);
                } else if (pos < 170) {
                    pos -= 85;
                    led_buffer[i] = rgb_to_grb(255 - pos * 3, 0, pos * 3);
                } else {
                    pos -= 170;
                    led_buffer[i] = rgb_to_grb(0, pos * 3, 255 - pos * 3);
                }
                // Dim for safety
                led_buffer[i] = ((led_buffer[i] >> 16) & 0xFF) / 8 << 16 |
                               ((led_buffer[i] >> 8) & 0xFF) / 8 << 8 |
                               (led_buffer[i] & 0xFF) / 8;
            }
            anim_offset += 2;
            break;
            
        case 7:  // Theater chase - red
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                if ((i + anim_offset / 8) % 3 == 0) {
                    led_buffer[i] = rgb_to_grb(100, 0, 0);
                } else {
                    led_buffer[i] = 0;
                }
            }
            anim_offset++;
            break;
            
        case 8:  // Color wipe (animated)
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                if (i <= (anim_offset / 2)) {
                    led_buffer[i] = rgb_to_grb(50, 0, 50);
                } else {
                    led_buffer[i] = 0;
                }
            }
            anim_offset++;
            if (anim_offset > NUM_LEDS_MAX * 2) anim_offset = 0;
            break;
            
        case 9:  // Sparkle
            // Random sparkle effect
            for (int i = 0; i < NUM_LEDS_MAX; i++) {
                if (random(100) < 5) {  // 5% chance per LED
                    led_buffer[i] = rgb_to_grb(255, 255, 255);
                } else {
                    // Fade existing
                    uint32_t c = led_buffer[i];
                    uint8_t r = ((c >> 16) & 0xFF) * 95 / 100;
                    uint8_t g = ((c >> 8) & 0xFF) * 95 / 100;
                    uint8_t b = (c & 0xFF) * 95 / 100;
                    led_buffer[i] = rgb_to_grb(r, g, b);
                }
            }
            break;
            
        case 10:  // Breathing (all LEDs)
            {
                static uint8_t breathe = 0;
                static int8_t direction = 1;
                
                breathe += direction * 2;
                if (breathe >= 250) direction = -1;
                if (breathe <= 5) direction = 1;
                
                for (int i = 0; i < NUM_LEDS_MAX; i++) {
                    led_buffer[i] = rgb_to_grb(0, breathe / 4, breathe / 2);
                }
            }
            break;
            
        case 11:  // Larson scanner (Knight Rider)
            {
                static int pos = 0;
                static int direction = 1;
                
                // Clear all
                for (int i = 0; i < NUM_LEDS_MAX; i++) {
                    led_buffer[i] = 0;
                }
                
                // Set the scanning LED and trail
                for (int i = 0; i < NUM_LEDS_MAX; i++) {
                    int dist = abs(pos - i);
                    if (dist == 0) {
                        led_buffer[i] = rgb_to_grb(100, 0, 0);
                    } else if (dist == 1) {
                        led_buffer[i] = rgb_to_grb(30, 0, 0);
                    } else if (dist == 2) {
                        led_buffer[i] = rgb_to_grb(10, 0, 0);
                    }
                }
                
                pos += direction;
                if (pos >= num_leds - 1) direction = -1;
                if (pos <= 0) direction = 1;
            }
            break;
    }
}

// Print menu
void print_menu() {
    Serial.println("\n=== WS2812 PIO Debug Menu ===");
    Serial.println("d <value>  - Set clock divider (current: " + String(clock_div, 3) + ")");
    Serial.println("n <1-24>   - Set number of LEDs (current: " + String(num_leds) + ")");
    Serial.println("t <0-11>   - Set test pattern (current: " + String(test_pattern) + ")");
    Serial.println("             0=Off, 1=Red, 2=Green, 3=Blue, 4=White");
    Serial.println("             5=Rainbow, 6=Rainbow Chase, 7=Theater Chase");
    Serial.println("             8=Color Wipe, 9=Sparkle, 10=Breathing, 11=Scanner");
    Serial.println("s          - Send current pattern once");
    Serial.println("a          - Auto-send mode toggle (current: " + String(test_mode ? "ON" : "OFF") + ")");
    Serial.println("r <r> <g> <b> - Set LED 0 to RGB values (0-255)");
    Serial.println("p          - Toggle pin manually (test GPIO)");
    Serial.println("i          - Show PIO status");
    Serial.println("h          - Show this menu");
    Serial.println("==============================\n");
}

// Show PIO status
void show_status() {
    Serial.println("\n=== PIO Status ===");
    Serial.print("TX FIFO Level: ");
    Serial.println(pio_sm_get_tx_fifo_level(pio, sm));
    Serial.print("TX FIFO Full: ");
    Serial.println(pio_sm_is_tx_fifo_full(pio, sm) ? "YES" : "NO");
    Serial.print("TX FIFO Empty: ");
    Serial.println(pio_sm_is_tx_fifo_empty(pio, sm) ? "YES" : "NO");
    Serial.print("SM Enabled: ");
    Serial.println((pio->ctrl & (1u << sm)) ? "YES" : "NO");
    
    float sys_clk = clock_get_hz(clk_sys);
    float pio_clk = sys_clk / clock_div;
    Serial.print("System Clock: ");
    Serial.print(sys_clk / 1000000.0, 2);
    Serial.println(" MHz");
    Serial.print("PIO Clock: ");
    Serial.print(pio_clk / 1000000.0, 2);
    Serial.println(" MHz");
    Serial.print("Bit time: ");
    Serial.print(1000000.0 / pio_clk, 3);
    Serial.println(" μs");
    Serial.println("==================\n");
}

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial connection
    
    Serial.println("\n\nWS2812 PIO Debugger Starting...");
    Serial.println("Pin: GPIO28 (Physical pin 34)");
    Serial.println("PIO: pio1, State Machine: 0");
    
    // Initialize LED buffer to off
    for (int i = 0; i < NUM_LEDS_MAX; i++) {
        led_buffer[i] = 0;
    }
    
    // Initialize PIO
    ws2812_pio_init();
    
    print_menu();
}

void loop() {
    // Handle serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.length() == 0) return;
        
        char command = cmd.charAt(0);
        
        switch (command) {
            case 'd': {
                float new_div = cmd.substring(2).toFloat();
                if (new_div >= 1.0 && new_div <= 65535.0) {
                    clock_div = new_div;
                    Serial.print("Setting clock divider to: ");
                    Serial.println(clock_div, 3);
                    ws2812_pio_restart();
                } else {
                    Serial.println("Invalid divider (must be 1.0-65535.0)");
                }
                break;
            }
            
            case 'n': {
                int new_num = cmd.substring(2).toInt();
                if (new_num >= 1 && new_num <= NUM_LEDS_MAX) {
                    num_leds = new_num;
                    Serial.print("Number of LEDs set to: ");
                    Serial.println(num_leds);
                } else {
                    Serial.println("Invalid number (must be 1-24)");
                }
                break;
            }
            
            case 't': {
                int pattern = cmd.substring(2).toInt();
                if (pattern >= 0 && pattern <= 11) {
                    test_pattern = pattern;
                    Serial.print("Test pattern set to: ");
                    Serial.println(test_pattern);
                    generate_test_pattern();
                    ws2812_send();
                } else {
                    Serial.println("Invalid pattern (must be 0-11)");
                }
                break;
            }
            
            case 's':
                Serial.println("Sending pattern...");
                generate_test_pattern();
                ws2812_send();
                break;
            
            case 'a':
                test_mode = !test_mode;
                Serial.print("Auto-send mode: ");
                Serial.println(test_mode ? "ON" : "OFF");
                break;
            
            case 'r': {
                int r, g, b;
                if (sscanf(cmd.c_str(), "r %d %d %d", &r, &g, &b) == 3) {
                    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                        led_buffer[0] = rgb_to_grb(r, g, b);
                        Serial.printf("LED 0 set to RGB(%d, %d, %d)\n", r, g, b);
                        ws2812_send();
                    } else {
                        Serial.println("RGB values must be 0-255");
                    }
                } else {
                    Serial.println("Usage: r <r> <g> <b>");
                }
                break;
            }
            
            case 'i':
                show_status();
                break;
            
            case 'p': {
                // Manual pin toggle test - bypasses PIO
                Serial.println("Toggling pin manually (10 times)...");
                pio_sm_set_enabled(pio, sm, false);  // Disable PIO
                pinMode(LED_PIN, OUTPUT);
                for (int i = 0; i < 10; i++) {
                    digitalWrite(LED_PIN, HIGH);
                    delayMicroseconds(500);
                    digitalWrite(LED_PIN, LOW);
                    delayMicroseconds(500);
                }
                Serial.println("Done. Re-initializing PIO...");
                ws2812_pio_restart();
                break;
            }
            
            case 'h':
                print_menu();
                break;
            
            default:
                Serial.println("Unknown command. Type 'h' for help.");
                break;
        }
    }
    
    // Auto-send mode
    if (test_mode) {
        generate_test_pattern();
        ws2812_send();
        delay(50);  // 20 Hz refresh
    }
}