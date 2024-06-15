#include <furi.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <input/input.h>
#include "infrared_signal.h"

// Global variables.
static const char* current_text = "Hello world!";
static const char* alternate_text = "You like pizza!";
static bool text_alternate = false;
static const uint32_t text_change_interval = 6000; // 6000 ms = 6 seconds
static uint32_t received_timings[256];
static size_t received_timings_size = 0;
static bool signal_received = false;

// Define the infrared signal parameters
static const uint32_t ir_address = 0x00006F98;
static const uint32_t ir_command = 0x0000E619;

// Function to handle GUI events.
static void ac_app_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 32, AlignCenter, AlignCenter, text_alternate ? alternate_text : current_text);
}

// Define a function to send the infrared signal
static void send_ir_signal() {
    InfraredSignal* signal = infrared_signal_alloc();
    InfraredMessage message = {
        .protocol = InfraredProtocolNECext,
        .address = ir_address,
        .command = ir_command,
    };
    infrared_signal_set_message(signal, &message);
    infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    FURI_LOG_I("ir_tx", "Started infrared transmission");
}

// Callback function for infrared signal capture
static void capture_isr_callback(void* context, bool complete, uint32_t timing) {
    UNUSED(context);
    if(complete) {
        signal_received = true;
        FURI_LOG_I("ir_rx", "Infrared signal capture complete");
    } else {
        if(received_timings_size < 256) {
            received_timings[received_timings_size++] = timing;
        }
    }
}

// Callback function for infrared receive timeout
static void timeout_isr_callback(void* context) {
    UNUSED(context);
    signal_received = false;
    FURI_LOG_I("ir_rx", "Infrared receive timeout");
}

// Function to decode and log infrared signals
static void decode_ir_signal() {
    if(signal_received) {
        FURI_LOG_I("ir_rx", "Received raw signal: timings_size=%zu", received_timings_size);
        for(size_t i = 0; i < received_timings_size; i++) {
            FURI_LOG_I("ir_rx", "Timing[%zu]: %lu", i, received_timings[i]);
        }

        // Decode NEC protocol
        if(received_timings_size >= 67) { // NEC protocol should have at least 67 timings
            uint32_t address = 0;
            uint32_t command = 0;

            // Decode address
            for(size_t i = 0; i < 16; i++) {
                if(received_timings[2 * i + 3] > 1000) { // Long space means '1'
                    address |= (1 << i);
                }
            }

            // Decode command
            for(size_t i = 0; i < 16; i++) {
                if(received_timings[2 * i + 35] > 1000) { // Long space means '1'
                    command |= (1 << i);
                }
            }

            FURI_LOG_I(
                "ir_rx", "Decoded NEC signal: address=0x%08lX, command=0x%08lX", address, command);
        } else {
            FURI_LOG_I("ir_rx", "Received signal does not match NEC protocol");
        }
    } else {
        FURI_LOG_I("ir_rx", "No infrared signal received");
    }
}

// Function to read and log infrared signals
static void read_ir_signal() {
    received_timings_size = 0;
    signal_received = false;
    furi_hal_infrared_async_rx_set_capture_isr_callback(capture_isr_callback, NULL);
    furi_hal_infrared_async_rx_set_timeout_isr_callback(timeout_isr_callback, NULL);
    furi_hal_infrared_async_rx_start();

    // Wait for signal reception or timeout
    furi_delay_ms(1000);

    furi_hal_infrared_async_rx_stop();

    decode_ir_signal();
}

// Change text on screen, send IR signal, and read IR signal.
static void change_text_periodically(void* ctx) {
    UNUSED(ctx);
    text_alternate = !text_alternate;
    // Perform GUI update here
    ViewPort* view_port = (ViewPort*)ctx;
    view_port_update(view_port);
    send_ir_signal();
    read_ir_signal();
    FURI_LOG_I("ac_app", "Changed text, sent IR signal, and read IR signal");
}

// Handle input.
static void ac_app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FURI_LOG_I(
        "ac_app", "Input event received: type=%d, key=%d", input_event->type, input_event->key);
    if(input_event->key == InputKeyBack) {
        FURI_LOG_I("ac_app", "Received input to close the application.");
        // Stop the event loop to exit the application
        FuriMessageQueue* event_queue = (FuriMessageQueue*)ctx;
        InputEvent exit_event = {.type = InputTypeShort, .key = InputKeyBack};
        furi_message_queue_put(event_queue, &exit_event, FuriWaitForever);
    }
}

int32_t ac_app_app(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FURI_LOG_I("ac_app", "The app started.");

    // Creating and configuring a ViewPort.
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, ac_app_render_callback, view_port);
    view_port_input_callback_set(view_port, ac_app_input_callback, event_queue);

    // Get the default GUI instance and add the ViewPort to it.
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Create a timer for periodic text updates.
    FuriTimer* text_update_timer =
        furi_timer_alloc(change_text_periodically, FuriTimerTypePeriodic, view_port);
    furi_timer_start(text_update_timer, text_change_interval);

    // Run the event loop so the app doesn't stop until we say so.
    InputEvent event;
    while(true) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            // Close the application when the back button is pressed.
            if(event.key == InputKeyBack) {
                FURI_LOG_I("ac_app", "Closing the application!");
                break;
            }
        }
    }

    // Cleanup.
    furi_timer_stop(text_update_timer);
    furi_timer_free(text_update_timer);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(event_queue);

    return 0;
}