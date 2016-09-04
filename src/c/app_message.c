#include <pebble.h>
#include <pebble-events/pebble-events.h>

static Window *s_main_window;
static Layer *s_bgtime_layer;
static Layer *s_batt_layer;
static TextLayer *s_city_layer;
static TextLayer *s_connection_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_temp_layer;
static TextLayer *s_cond_layer;
static TextLayer *s_sunset_layer;

static BatteryChargeState *charge_state;

static const char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const uint32_t vibe_hour[] = {100, 100, 100};
static const uint32_t vibe_connect[] = {50, 100, 50, 100, 50};
static const uint32_t vibe_disconnect[] = {300, 100, 300, 100, 300};

static AppSync s_sync;

enum WeatherKey {
  WEATHER_TEMP_KEY = 0x0,
  WEATHER_SUN_KEY = 0x1,
  WEATHER_COND_KEY = 0x2,
  WEATHER_CITY_KEY = 0x3,
};

static void bgtime_update_proc(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);
  
  graphics_context_set_fill_color(ctx, GColorWhite);
  GRect rect_bounds = GRect(0, 22, layer_bounds.size.w, 35);
  
  // Draw a rectangle
  graphics_draw_rect(ctx, rect_bounds);
  
  // Fill a rectangle with rounded corners
  int corner_radius = 0;
  
  graphics_fill_rect(ctx, rect_bounds, corner_radius, GCornersAll);
}

static void sbatt_update_proc(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);
  int y_start_point = 19;
    
  if (!charge_state){
    *charge_state = battery_state_service_peek();
  }  
  
  int sz = (layer_bounds.size.w * charge_state->charge_percent) / 100;
  
  graphics_context_set_fill_color(ctx, GColorLightGray);
  GRect rect_fill = GRect(0, y_start_point, layer_bounds.size.w, 3);
  
  graphics_draw_rect(ctx, rect_fill);
  graphics_fill_rect(ctx, rect_fill, 0, GCornerNone);
  
  GRect *rect_batt = (GRect*)malloc(sizeof(GRect));
  
  if(charge_state->is_charging){
    *rect_batt = GRect(0, y_start_point, layer_bounds.size.w, 3);
    graphics_context_set_fill_color(ctx, GColorYellow);
  }else{
    *rect_batt = GRect(0, y_start_point, sz, 3);
    
    graphics_context_set_fill_color(ctx, GColorGreen);
    if(charge_state->charge_percent <= 20){
      graphics_context_set_fill_color(ctx, GColorRed);
    }
  }
  
  graphics_draw_rect(ctx, *rect_batt);
  int corner_radius = 0;
  graphics_fill_rect(ctx, *rect_batt, corner_radius, GCornersAll);
}

/*
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "TUPLE VALUE: %s", new_tuple->value->cstring);
  
  switch (key) {
    case WEATHER_TEMP_KEY:
      text_layer_set_text(s_temp_layer, new_tuple->value->cstring);
      break;

    case WEATHER_SUN_KEY:
      text_layer_set_text(s_sunset_layer, new_tuple->value->cstring);
      break;
    
    case WEATHER_COND_KEY:
      text_layer_set_text(s_cond_layer, new_tuple->value->cstring);
      break;
    
    case WEATHER_CITY_KEY:
      text_layer_set_text(s_city_layer, new_tuple->value->cstring);
      break;
  }
}
*/

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *reply_tuple = dict_find(iter, MESSAGE_KEY_WEATHER_REPLY);
  if(reply_tuple) {
    Tuple *temp_tuple = dict_find(iter, MESSAGE_KEY_WEATHER_TEMP);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "TEMP: %s", temp_tuple->value->cstring);
    text_layer_set_text(s_temp_layer, temp_tuple->value->cstring);
    
    Tuple *cond_tuple = dict_find(iter, MESSAGE_KEY_WEATHER_COND);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "COND: %s", cond_tuple->value->cstring);
    text_layer_set_text(s_cond_layer, cond_tuple->value->cstring);
    
    Tuple *city_tuple = dict_find(iter, MESSAGE_KEY_WEATHER_CITY);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "CITY: %s", city_tuple->value->cstring);
    text_layer_set_text(s_city_layer, city_tuple->value->cstring);
    
    Tuple *sun_tuple = dict_find(iter, MESSAGE_KEY_WEATHER_SUN);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "SUN: %s", sun_tuple->value->cstring);
    text_layer_set_text(s_sunset_layer, sun_tuple->value->cstring);
  }
}

static void handle_battery(BatteryChargeState state) {
  *charge_state = state;
  
  layer_mark_dirty(s_batt_layer);
}

static void request_weather(void) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if(result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error creating outbox message: %d", result);
    return;
  }
  
  dict_write_cstring(iter, MESSAGE_KEY_WEATHER_REQUEST, "1");
  
  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send outbox message: %d", result);
    return;
  }
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
  // Needs to be static because it's used by the system later.
  static char s_time_text[] = "00:00";
  static char s_date_text[] = "24 September";
  static char s_finaldate_text[] = "Sun, 24 September";                                         
                                         
  strftime(s_time_text, sizeof(s_time_text), "%I:%M", tick_time);
  strftime(s_date_text, sizeof(char*[32]), "%d %B", tick_time);
  
  snprintf(s_finaldate_text, sizeof(char*[32]), "%s, %s", weekday[tick_time->tm_wday], s_date_text);
  
  text_layer_set_text(s_time_layer, s_time_text);
  text_layer_set_text(s_date_layer, s_finaldate_text);
  
  if(tick_time->tm_min == 0) {
    request_weather();
    
    if (tick_time->tm_hour > 8){
      VibePattern pat = {
        .durations = vibe_hour,
        .num_segments = ARRAY_LENGTH(vibe_hour),
      };
      
      vibes_enqueue_custom_pattern(pat);
    }
  }
}

static void handle_bluetooth(bool connected) {
  VibePattern *pat = malloc(sizeof(VibePattern));
  
  if(connected){
    pat->durations = vibe_connect;
    pat->num_segments = ARRAY_LENGTH(vibe_connect);
  }else{
    pat->durations = vibe_disconnect;
    pat->num_segments = ARRAY_LENGTH(vibe_disconnect);
  };
    
  vibes_enqueue_custom_pattern(*pat);
  
  text_layer_set_text(s_connection_layer, connected ? "" : "X");
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  s_bgtime_layer = layer_create(bounds);
  layer_set_update_proc(s_bgtime_layer, bgtime_update_proc);  
  
  s_batt_layer = layer_create(bounds);
  layer_set_update_proc(s_batt_layer, sbatt_update_proc);  
  
  s_city_layer = text_layer_create(GRect(0, 0, bounds.size.w, 16));
  text_layer_set_text_color(s_city_layer, GColorWhite);
  text_layer_set_background_color(s_city_layer, GColorClear);
  text_layer_set_font(s_city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_city_layer, GTextAlignmentCenter);
  text_layer_set_text(s_city_layer, "Loading...");
  
  s_time_layer = text_layer_create(GRect(0, 12, bounds.size.w, 42));
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  s_date_layer = text_layer_create(GRect(0, 55, bounds.size.w, 22));
  text_layer_set_text_color(s_date_layer, GColorLightGray);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  s_connection_layer = text_layer_create(GRect(0, 0, bounds.size.w, 16));
  text_layer_set_text_color(s_connection_layer, GColorWhite);
  text_layer_set_background_color(s_connection_layer, GColorClear);
  text_layer_set_font(s_connection_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_connection_layer, GTextAlignmentRight);
  handle_bluetooth(connection_service_peek_pebble_app_connection());

  s_temp_layer = text_layer_create(GRect(0, bounds.size.h - 90, bounds.size.w, 42));
  text_layer_set_text_color(s_temp_layer, GColorWhite);
  text_layer_set_background_color(s_temp_layer, GColorClear);
  text_layer_set_font(s_temp_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_temp_layer, GTextAlignmentCenter);
  text_layer_set_text(s_temp_layer, "0.0");
  
  s_cond_layer = text_layer_create(GRect(0, bounds.size.h - 41, bounds.size.w, 22));
  text_layer_set_text_color(s_cond_layer, GColorLightGray);
  text_layer_set_background_color(s_cond_layer, GColorClear);
  text_layer_set_font(s_cond_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_cond_layer, GTextAlignmentCenter);
  text_layer_set_text(s_cond_layer, "Not good");
    
  s_sunset_layer = text_layer_create(GRect(0, bounds.size.h - 16, bounds.size.w, 16));
  text_layer_set_text_color(s_sunset_layer, GColorLightGray);
  text_layer_set_background_color(s_sunset_layer, GColorClear);
  text_layer_set_font(s_sunset_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_sunset_layer, GTextAlignmentCenter);
  text_layer_set_text(s_sunset_layer, "unavailable");
  
  // Ensures time is displayed immediately (will break if NULL tick event accessed).
  // (This is why it's a good idea to have a separate routine to do the update itself.)
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  handle_minute_tick(current_time, MINUTE_UNIT);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  battery_state_service_subscribe(handle_battery);

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });

  layer_add_child(window_layer, s_bgtime_layer);
  layer_add_child(window_layer, s_batt_layer);
  
  layer_add_child(window_layer, text_layer_get_layer(s_city_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_temp_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_cond_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_sunset_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_connection_layer));  
  /*
  
  Tuplet initial_values[]= {
    TupletCString(WEATHER_TEMP_KEY, "-24.3"),
    TupletCString(WEATHER_SUN_KEY, "6:11-17:59"),
    TupletCString(WEATHER_COND_KEY, "Breeze"),
    TupletCString(WEATHER_CITY_KEY, "Itajubá"),  
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
                initial_values, ARRAY_LENGTH(initial_values),
                sync_tuple_changed_callback, sync_error_callback, NULL);
  */
  
}

static void main_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  layer_destroy(s_bgtime_layer);
  layer_destroy(s_batt_layer);
  text_layer_destroy(s_city_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_temp_layer);
  text_layer_destroy(s_cond_layer);
  text_layer_destroy(s_sunset_layer);
  text_layer_destroy(s_connection_layer);
}

static void js_ready_handler(void *context) {
  request_weather();
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
  
  events_app_message_request_inbox_size(2026);
  events_app_message_request_outbox_size(656);
  events_app_message_register_inbox_received(inbox_received_handler, NULL);
  events_app_message_open();
  
  charge_state = (BatteryChargeState*)malloc(sizeof(BatteryChargeState));  
  *charge_state = battery_state_service_peek();
  
  app_timer_register(3000, js_ready_handler, NULL);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}