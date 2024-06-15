#include <furi.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <input/input.h>
#include "infrared_signal.h"

// Global variables.
static const char* current_text = "Hello world!";
static const char* alternate_text = "You like pizza!";
static bool text_alternate = false;
static const uint32_t one_hour_interval = 3600000; // 1 hour in milliseconds
static const uint32_t three_hour_interval = 10800000; // 3 hours in milliseconds

// Define the infrared signal parameters for both signals
static const uint32_t ir_address_1 = 0x00006F98;
static const uint32_t ir_command_1 = 0x0000E619;
static const uint32_t ir_command_2 = 0x0000F708;

// Function to handle GUI events.
static void ac_app_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 32, AlignCenter, AlignCenter, text_alternate ? alternate_text : current_text);
}

// Define a function to send the infrared signal
static void send_ir_signal(uint32_t address, uint32_t command) {
    InfraredSignal* signal = infrared_signal_alloc();
    InfraredMessage message = {
        .protocol = InfraredProtocolNECext,
        .address = address,
        .command = command,
    };
    infrared_signal_set_message(signal, &message);
    infrared_signal_transmit(signal);
    infrared_signal_free(signal);
    FURI_LOG_I(
        "ir_tx",
        "Started infrared transmission: address=0x%08lX, command=0x%08lX",
        address,
        command);
}

// Function to send signals and update text based on the current state
static void send_signals_and_update_text(void* ctx) {
    ViewPort* view_port = (ViewPort*)ctx;

    if(text_alternate) {
        // Send "You like pizza!" signal
        send_ir_signal(ir_address_1, ir_command_1);
        text_alternate = false;
        current_text = alternate_text;
    } else {
        // Send "Hello world!" signals
        send_ir_signal(ir_address_1, ir_command_1);
        furi_delay_ms(100); // Short delay between signals
        send_ir_signal(ir_address_1, ir_command_2);
        furi_delay_ms(100); // Short delay between signals
        send_ir_signal(ir_address_1, ir_command_2);
        text_alternate = true;
        current_text = "Hello world!";
    }

    // Update the text on the screen
    view_port_update(view_port);

    // Schedule the next signal based on the current state
    FuriTimer* timer =
        furi_timer_alloc(send_signals_and_update_text, FuriTimerTypeOnce, view_port);
    if(text_alternate) {
        furi_timer_start(timer, one_hour_interval);
    } else {
        furi_timer_start(timer, three_hour_interval);
    }
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

    // Send the initial "Hello world!" signals as soon as the app opens
    send_ir_signal(ir_address_1, ir_command_1);
    furi_delay_ms(100); // Short delay between signals
    send_ir_signal(ir_address_1, ir_command_2);
    furi_delay_ms(100); // Short delay between signals
    send_ir_signal(ir_address_1, ir_command_2);

    // Schedule the first "You like pizza!" signal to be sent in one hour
    FuriTimer* timer =
        furi_timer_alloc(send_signals_and_update_text, FuriTimerTypeOnce, view_port);
    furi_timer_start(timer, one_hour_interval);

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
    furi_timer_stop(timer);
    furi_timer_free(timer);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(event_queue);

    return 0;
}