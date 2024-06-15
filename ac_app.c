#include <furi.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <input/input.h>

// Global variables.
static const char* current_text = "Hello world!";
static const char* alternate_text = "You like pizza!";
static bool text_alternate = false;
static const uint32_t text_change_interval = 6000; // 6000 ms = 6 seconds

// Function to handle GUI events.
static void ac_app_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 32, AlignCenter, AlignCenter, text_alternate ? alternate_text : current_text);
}

// Read the name...
static void change_text_periodically(void* ctx) {
    UNUSED(ctx);
    text_alternate = !text_alternate;
    // Perform GUI update here
    ViewPort* view_port = (ViewPort*)ctx;
    view_port_update(view_port);
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
    FURI_LOG_I("ac_app", "the app started mf.");

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