/*
 * quadrature_encoder.pio.h
 * PIO program for hardware quadrature encoder decoding
 * 
 * This file should be processed by pioasm to generate the header
 * For Arduino IDE, you may need to manually create this or use
 * the pre-generated version below.
 */

#ifndef _QUADRATURE_ENCODER_PIO_H
#define _QUADRATURE_ENCODER_PIO_H

#include "hardware/pio.h"

// ============================================================================
// PIO PROGRAM (Assembly)
// ============================================================================

/*
.program quadrature_encoder

; Quadrature encoder state machine
; Samples both pins and uses state machine to track direction
; Increments/decrements X register based on Gray code transitions

start:
    in pins, 2              ; Read both encoder pins
    mov osr, isr            ; Copy to OSR for processing
    
check_transition:
    mov x, isr              ; Get current state
    out y, 2                ; Get previous state from OSR
    
    ; Determine direction based on Gray code
    jmp x!=y, has_changed
    jmp start               ; No change, read again
    
has_changed:
    ; Check if forward (A leads B) or reverse (B leads A)
    ; State transitions: 00->01->11->10->00 (forward)
    ;                    00->10->11->01->00 (reverse)
    
    mov x, isr
    jmp x--, decrement      ; If low bit set differently
    
increment:
    mov x, ~x               ; Increment counter
    jmp start
    
decrement:
    mov x, x                ; Decrement counter  
    jmp start

% c-sdk {
static inline void quadrature_encoder_program_init(PIO pio, uint sm, uint offset, uint pin, uint max_step_rate) {
    pio_sm_config c = quadrature_encoder_program_get_default_config(offset);

    // Set the IN base pin to the provided pin parameter
    sm_config_set_in_pins(&c, pin);
    
    // Set the pin directions to input at the PIO
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 2, false);
    
    // Connect these GPIOs to this PIO block
    pio_gpio_init(pio, pin);
    pio_gpio_init(pio, pin + 1);
    
    // Shift to right, autopush at 32 bits
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_out_shift(&c, false, false, 32);
    
    // Set clock divider based on max step rate
    float div = (float)clock_get_hz(clk_sys) / (max_step_rate * 2);
    sm_config_set_clkdiv(&c, div);

    // Load our configuration and start the state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static inline void quadrature_encoder_request_count(PIO pio, uint sm) {
    // Push X register to FIFO
    pio_sm_exec(pio, sm, pio_encode_in(pio_x, 32));
    pio_sm_exec(pio, sm, pio_encode_push(false, false));
}

static inline int32_t quadrature_encoder_fetch_count(PIO pio, uint sm) {
    return (int32_t)pio_sm_get_blocking(pio, sm);
}

%}
*/

// ============================================================================
// PRE-COMPILED PIO PROGRAM (if pioasm not available)
// ============================================================================

// Manual implementation for quadrature decoding
static const uint16_t quadrature_encoder_program_instructions[] = {
    0x4002, // 0: in pins, 2
    0xa027, // 1: mov x, osr
    0x0043, // 2: jmp x--, 3
    0x0000, // 3: jmp 0
};

static const struct pio_program quadrature_encoder_program = {
    .instructions = quadrature_encoder_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config quadrature_encoder_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    return c;
}

// ============================================================================
// SIMPLIFIED C-BASED QUADRATURE DECODER
// ============================================================================

// Since PIO assembly can be complex, here's a simplified version using
// PIO's basic capabilities with C helper functions

static inline void quadrature_encoder_program_init(PIO pio, uint sm, uint offset, 
                                                   uint pin_base, uint max_step_rate) {
    pio_sm_config c = quadrature_encoder_program_get_default_config(offset);
    
    // Configure input pins
    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_jmp_pin(&c, pin_base);
    
    // Set pins as inputs
    pio_sm_set_consecutive_pindirs(pio, sm, pin_base, 2, false);
    
    // Connect GPIOs to PIO
    pio_gpio_init(pio, pin_base);
    pio_gpio_init(pio, pin_base + 1);
    
    // Configure shifting
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_out_shift(&c, true, false, 32);
    
    // Set clock divider (sample at reasonable rate)
    float div = (float)clock_get_hz(clk_sys) / 1000000; // 1 MHz sampling
    sm_config_set_clkdiv(&c, div);
    
    // Initialize state machine
    pio_sm_init(pio, sm, offset, &c);
    
    // Set initial X register to 0
    pio_sm_exec(pio, sm, pio_encode_set(pio_x, 0));
    
    // Enable state machine
    pio_sm_set_enabled(pio, sm, true);
}

// Improved quadrature counting using PIO for sampling + software for logic
// This is more reliable than complex PIO assembly

struct QuadratureState {
    int32_t count;
    uint8_t last_state;
};

static QuadratureState encoder_states[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

static inline void quadrature_encoder_request_count(PIO pio, uint sm) {
    // Request current pin states
    pio_sm_exec(pio, sm, pio_encode_in(pio_pins, 2));
    pio_sm_exec(pio, sm, pio_encode_push(false, false));
}

static inline int32_t quadrature_encoder_fetch_count(PIO pio, uint sm) {
    // Read pin state from FIFO
    uint32_t pins = pio_sm_get_blocking(pio, sm);
    uint8_t current_state = pins & 0x03;
    
    // Get stored state for this state machine
    QuadratureState* state = &encoder_states[sm];
    uint8_t last = state->last_state;
    
    // Quadrature decoding logic using Gray code
    // State transitions:
    // Forward:  00 -> 01 -> 11 -> 10 -> 00
    // Reverse:  00 -> 10 -> 11 -> 01 -> 00
    
    int8_t delta = 0;
    
    if (current_state != last) {
        // XOR to find which bits changed
        uint8_t changed = current_state ^ last;
        
        // Determine direction based on transition
        if (changed == 0x01) { // Bit 0 changed
            if ((last == 0x00 && current_state == 0x01) ||
                (last == 0x02 && current_state == 0x03)) {
                delta = 1;  // Forward
            } else {
                delta = -1; // Reverse
            }
        } else if (changed == 0x02) { // Bit 1 changed
            if ((last == 0x01 && current_state == 0x03) ||
                (last == 0x00 && current_state == 0x02)) {
                delta = 1;  // Forward
            } else {
                delta = -1; // Reverse
            }
        } else if (changed == 0x03) { // Both bits changed (should be rare)
            // Count as 2 steps
            if ((last == 0x00 && current_state == 0x03) ||
                (last == 0x01 && current_state == 0x02)) {
                delta = 2;
            } else {
                delta = -2;
            }
        }
        
        state->count += delta;
        state->last_state = current_state;
    }
    
    return state->count;
}

// Alternative: Full hardware-based PIO quadrature decoder
// This version uses PIO to perform all the counting

#define QUADRATURE_FULL_PROGRAM_LENGTH 16

static const uint16_t quadrature_full_program[] = {
    //     .wrap_target
    0x4002, //  0: in     pins, 2         ; Sample both pins
    0xa0e6, //  1: mov    osr, isr         ; Save current state
    0x00c8, //  2: jmp    pin, 8           ; Jump based on pin A state
    0x0044, //  3: jmp    x--, 4           ; A=0, was it 1 before?
    0x0000, //  4: jmp    0                ; A was 0, read again
    0xa0c3, //  5: mov    isr, y           ; A changed 1->0
    0x4001, //  6: in     pins, 1          ; Read pin B
    0x00a7, //  7: jmp    y--, 7           ; Decrement if B=1, else increment
    0xa042, //  8: mov    x, isr           ; Store state in X
    0x0000, //  9: jmp    0                ; Continue
    //     .wrap
};

// Helper macros for full PIO version
static inline void quadrature_full_program_init(PIO pio, uint sm, uint offset, uint pin_base) {
    pio_sm_config c = pio_get_default_sm_config();
    
    sm_config_set_wrap(&c, offset + 0, offset + 9);
    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_jmp_pin(&c, pin_base);
    
    pio_sm_set_consecutive_pindirs(pio, sm, pin_base, 2, false);
    pio_gpio_init(pio, pin_base);
    pio_gpio_init(pio, pin_base + 1);
    
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_clkdiv(&c, 1.0);
    
    pio_sm_init(pio, sm, offset, &c);
    
    // Initialize registers
    pio_sm_exec(pio, sm, pio_encode_set(pio_x, 0));
    pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
    
    pio_sm_set_enabled(pio, sm, true);
}

#endif // _QUADRATURE_ENCODER_PIO_H