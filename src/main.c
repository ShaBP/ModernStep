/*
ModernCalendar Plus is an integration by ShaBP of two separate watchfaces:
  Modern Minimal by JWay5929
  ModernCalendar by Fowler/Baeumle, which works with Baeumle's popular Smartwatch Pro
Both of those watchfaces are based on the popular Modern watchface by Lukasz Zalewski
Besides for the integration, week of year indication was added.
The end result is a cleaner minimal design analog watchface, which presents calendar events read from Smartwatch Pro.
This release is an effort to integrate activity tracking from On11 by Qian He.
*/

#include "common.h"
#include "recognizer.h"


// Window
static Window *window;
static Layer *window_layer;
static GRect window_bounds;
// Background
static BitmapLayer *background_layer;  //BitmapLayer *background_image_container;
static GBitmap *background_image;
// Time
static struct tm *current_time;
// Hands
static Layer *hands_layer;
static GPath *minute_hand; //Layer *minute_display_layer;
static GPath *hour_hand; //Layer *hour_display_layer;
const GPathInfo MINUTE_HAND_POINTS = { 4, (GPoint[] ) { { -4, 12 }, { 4, 12 }, { 4, -69 }, { -4, -69 }, } };
const GPathInfo HOUR_HAND_POINTS = { 4, (GPoint[] ) { { -4, 12 }, { 4, 12 }, { 4, -43 }, { -4, -43 }, } };
// Date Window
// static BitmapLayer *date_window_layer;
// static GBitmap *image_date_window;
// Date Text
static TextLayer *date_layer;
static char date_text[] = "w99, ZZZ 99";
static TextLayer *event_layer;
static TextLayer *event_layer2; // from ModernCalendar
//static char date_text[] = "Wed 13 ";
//static char empty_str[] = ""; // from ModernCalendar
// Week Text status
static TextLayer *steps_layer;
static char steps_text[] = "steps: 99999";
static TextLayer *sit_layer;
static char sit_text[] = "s99:99";
static TextLayer *walk_layer;
static char walk_text[] = "w99:99";
static TextLayer *jog_layer;
static char jog_text[] = "j99:99";
// Week Text status
//static TextLayer *week_layer;
//static char week_text[] = "w99";
// Small time layer (for shake screen)
static TextLayer *small_time_layer;
static char small_time_text[] = "99:99";
// Status
static bool status_showing = false;
static char event_text[21]; // from ModernCalendar
static char event_text2[21]; // from ModernCalendar
static GBitmap *icon_status_1; // from ModernCalendar
static GBitmap *icon_status_2; // from ModernCalendar
static GBitmap *icon_status_3; // from ModernCalendar
static Layer *event_status_layer; // from ModernCalendar
static int event_status_display = 0; // from ModernCalendar
// Bluetooth
static BitmapLayer *bt_layer;
static GBitmap *icon_bt_connected;
static GBitmap *icon_bt_disconnected;
static bool bt_status = false;
static bool already_vibrated = false;
// Pebble Battery Icon
static BitmapLayer *battery_layer;
static GBitmap *icon_battery_charging;
static GBitmap *icon_battery_100;
static GBitmap *icon_battery_90;
static GBitmap *icon_battery_80;
static GBitmap *icon_battery_70;
static GBitmap *icon_battery_60;
static GBitmap *icon_battery_50;
static GBitmap *icon_battery_40;
static GBitmap *icon_battery_30;
static GBitmap *icon_battery_20;
static GBitmap *icon_battery_10;
static uint8_t battery_level;
static bool battery_plugged = false;
static bool battery_charging = false;
static bool battery_hide = false;
// Timers
static AppTimer *display_timer;
static AppTimer *vibrate_timer;
///
/// Easy set items for version management (and future settings screen)
///
static bool badge = false;
//static bool hide_date = true;

// On11 Config
static bool mIsDriving = false;
static int32_t mPedometerSensitivity = 10;	// Range: [0, 100]

// On11 Classification
static LowPassFilter mFilter;
static uint32_t mCurrentType = 1;

static Counter mCounter;

//static uint32_t steps;


///
/// Draw Callbacks
///
// Draw/Update/Hide Battery icon. Loads appropriate icon per battery state. Sets appropriate hide/unhide status for state. Activates hide status. -- DONE
void draw_battery_icon() {
	if (battery_plugged && !status_showing) {
		if (battery_charging) bitmap_layer_set_bitmap(battery_layer, icon_battery_charging);
		else bitmap_layer_set_bitmap(battery_layer, icon_battery_100);
	}
	else {
		if (battery_level == 100) bitmap_layer_set_bitmap(battery_layer, icon_battery_100);
		else if (battery_level == 90) bitmap_layer_set_bitmap(battery_layer, icon_battery_90);
		else if (battery_level == 80) bitmap_layer_set_bitmap(battery_layer, icon_battery_80);
		else if (battery_level == 70) bitmap_layer_set_bitmap(battery_layer, icon_battery_70);
		else if (battery_level == 60) bitmap_layer_set_bitmap(battery_layer, icon_battery_60);
		else if (battery_level == 50) bitmap_layer_set_bitmap(battery_layer, icon_battery_50);
		else if (battery_level == 40) bitmap_layer_set_bitmap(battery_layer, icon_battery_40);
		else if (battery_level == 30) bitmap_layer_set_bitmap(battery_layer, icon_battery_30);
		else if (battery_level == 20) bitmap_layer_set_bitmap(battery_layer, icon_battery_20);
		else if (battery_level == 10) bitmap_layer_set_bitmap(battery_layer, icon_battery_10);
	}
	if (battery_plugged) battery_hide = false;
	else if (battery_level < 20) battery_hide = false;
	else if (status_showing) battery_hide = false;
	else battery_hide = true;
	layer_set_hidden(bitmap_layer_get_layer(battery_layer), battery_hide);
}
// Draw/Update/Hide bluetooth status icon. Loads appropriate icon. Sets appropriate hide/unhide status per status. ("draw_bt_icon" is separate from "bt_connection_handler" as to not vibrate if not connected on init)
void draw_bt_icon() {
	if (bt_status) bitmap_layer_set_bitmap(bt_layer, icon_bt_connected);
	else bitmap_layer_set_bitmap(bt_layer, icon_bt_disconnected);
	if (status_showing) layer_set_hidden(bitmap_layer_get_layer(bt_layer), false);
	else layer_set_hidden(bitmap_layer_get_layer(bt_layer), bt_status);
}
// Draw/Update/Hide steps counter text, as part of status info
void draw_steps_text() {
  int32_t sit_hours = mCounter.sitTime / 60 / 60;
  int32_t sit_min = mCounter.sitTime / 60 - (sit_hours * 60);
  int32_t walk_hours = mCounter.walkTime / 60 / 60;
  int32_t walk_min = mCounter.walkTime / 60 - (walk_hours * 60);
  int32_t jog_hours = mCounter.jogTime / 60 / 60;
  int32_t jog_min = mCounter.jogTime / 60 - (jog_hours * 60);
	snprintf(steps_text, sizeof(steps_text), "steps: %d", (int)(mCounter.steps));
	text_layer_set_text(steps_layer, steps_text);  
	snprintf(sit_text, sizeof(sit_text), "s%2d:%2d", (int)sit_hours, (int)sit_min);
	text_layer_set_text(sit_layer, sit_text);  
	snprintf(walk_text, sizeof(walk_text), "w%2d:%2d", (int)walk_hours, (int)walk_min);
	text_layer_set_text(walk_layer, walk_text);  
	snprintf(jog_text, sizeof(jog_text), "j%2d:%2d", (int)jog_hours, (int)jog_min);
	text_layer_set_text(jog_layer, jog_text);  
  switch(mCurrentType) {
    case 1:
    text_layer_set_font(sit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(walk_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_font(jog_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    break;
    case 2:
    text_layer_set_font(sit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_font(walk_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(jog_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    break;
    case 3:
    text_layer_set_font(sit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_font(walk_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_font(jog_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    break;
  }
  if (status_showing){
    layer_set_hidden(text_layer_get_layer(steps_layer), false);
    layer_set_hidden(text_layer_get_layer(sit_layer), false);
    layer_set_hidden(text_layer_get_layer(walk_layer), false);
    layer_set_hidden(text_layer_get_layer(jog_layer), false);    
  }
	else{
    layer_set_hidden(text_layer_get_layer(steps_layer), true);
    layer_set_hidden(text_layer_get_layer(sit_layer), true);
    layer_set_hidden(text_layer_get_layer(walk_layer), true);
    layer_set_hidden(text_layer_get_layer(jog_layer), true);
  } 
}
// Draw/Update/Hide small time, as part of status info
void draw_small_time() {
  strftime(small_time_text, sizeof(small_time_text), "%H:%M", current_time);
  text_layer_set_text(small_time_layer, small_time_text);
  if (status_showing) layer_set_hidden(text_layer_get_layer(small_time_layer), false);
  else layer_set_hidden(text_layer_get_layer(small_time_layer), true);
}
// Draw/Update/Hide date text, as part of status info
void draw_date_text() {
	strftime(date_text, sizeof(date_text), "w%V, %b %2d", current_time);
	text_layer_set_text(date_layer, date_text); 
	if (status_showing) layer_set_hidden(text_layer_get_layer(date_layer), false);
	else layer_set_hidden(text_layer_get_layer(date_layer), true);
}
// Draw/Update/Hide event status icons. Sets appropriate hide/unhide status per status.
void draw_event_status() {
	if (status_showing) layer_set_hidden(event_status_layer, false);
	else layer_set_hidden(event_status_layer, true);
}
// Draw/Update/Hide Event text - from ModernCalendar
void draw_event_text(){
  if (status_showing) {
    layer_set_hidden(text_layer_get_layer(event_layer), false);
    layer_set_hidden(text_layer_get_layer(event_layer2), false);
  }
  else {
    layer_set_hidden(text_layer_get_layer(event_layer), true);
    layer_set_hidden(text_layer_get_layer(event_layer2), true);
  }
}

void display_event_text(char *text, char *relative) {
  strncpy(event_text2, text, sizeof(event_text2));
  text_layer_set_text(event_layer2, event_text2);
  strncpy(event_text, relative, sizeof(event_text));
  text_layer_set_text(event_layer, event_text);
}
// Draw/Update Date
/*
void draw_date() {
	strftime(date_text, sizeof(date_text), "%d", current_time);
	text_layer_set_text(date_layer, date_text);
}
*/
void draw_clock() {
  if (status_showing) {
    layer_set_hidden(bitmap_layer_get_layer(background_layer), true);
    layer_set_hidden(hands_layer, true);
  }
  else {
    layer_set_hidden(bitmap_layer_get_layer(background_layer), false);
    layer_set_hidden(hands_layer, false);
  }
}
// Draw/Update Clock Hands
void draw_hands(Layer *layer, GContext *ctx){
   // Hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);	
  gpath_rotate_to(hour_hand, (((TRIG_MAX_ANGLE * current_time->tm_hour) / 12) + (((TRIG_MAX_ANGLE * current_time->tm_min) / 60) / 12))); //Moves hour hand 1/60th between hour indicators.  Only updates per minute.
  gpath_draw_filled(ctx, hour_hand);
  gpath_draw_outline(ctx, hour_hand);
  // Minite hand
  gpath_rotate_to(minute_hand, TRIG_MAX_ANGLE * current_time->tm_min / 60);
  gpath_draw_filled(ctx, minute_hand);
  gpath_draw_outline(ctx, minute_hand);
  // Dot in the middle
  const GPoint center = grect_center_point(&window_bounds);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 2);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 1);	  
}
// Hides status icons. Call draw of default battery/bluetooth icons (which will show or hide icon based on set logic). Hides date/window if selected
void hide_status() {
	status_showing = false;
  draw_clock();
  draw_small_time();
	draw_battery_icon();
	draw_bt_icon();
  draw_steps_text();
  draw_date_text();
  draw_event_status();
  draw_event_text();
//	layer_set_hidden(text_layer_get_layer(date_layer), hide_date);
//	layer_set_hidden(bitmap_layer_get_layer(date_window_layer), hide_date);
}
// Shows status icons. Call draw of default battery/bluetooth icons (which will show or hide icon based on set logic). Shows date/window.
void show_status() {
  if (status_showing){
    app_timer_cancel(display_timer); // Cancels previous timer if another show_status is called within the 4000ms
  }
  status_showing = true;
	// Hide analog clock face
  draw_clock();
  // Draw small (digital) time
  draw_small_time();
  // Redraws battery icon
	draw_battery_icon();
	// Shows current bluetooth status icon
	draw_bt_icon();
  // Shows steps count
  draw_steps_text();
  // Shows week of year text
  draw_date_text();
  // Shows event status icons
  draw_event_status();
  // Shows event text
  draw_event_text();
	// Show Date Window & Date
//	layer_set_hidden(text_layer_get_layer(date_layer), false);
//	layer_set_hidden(bitmap_layer_get_layer(date_window_layer), false);
	// 4 Sec timer then call "hide_status"
	display_timer = app_timer_register(4000, hide_status, NULL);
}


///
/// PEBBLE HANDLERS
///
// Battery state handler. Updates battery level, plugged, charging states. Calls "draw_battery_icon".
void battery_state_handler(BatteryChargeState c) {
	battery_level = c.charge_percent;
	battery_plugged = c.is_plugged;
	battery_charging = c.is_charging;
	draw_battery_icon();
}
void short_pulse(){
	vibes_short_pulse();
}
// If bluetooth is still not connected after 5 sec delay, vibrate. (double vibe was too easily confused with a signle short vibe.  Two short vibes was easier to distinguish from a notification)
void bt_vibrate(){
	if (!bt_status && !already_vibrated) {
		already_vibrated = true;
		app_timer_register(0, short_pulse, NULL);
		app_timer_register(350, short_pulse, NULL);
	}
}
// Bluetooth connection status handler.  Updates bluetooth status. Calls "draw_bt_icon". If bluetooth not connected, wait 5 seconds then call "bt_vibrate". Cancel any vibrate timer if status change in 5 seconds (minimizes repeat vibration alerts)
void bt_connection_handler(bool bt) {
	bt_status = bt;
	draw_bt_icon();
	app_timer_cancel(vibrate_timer);
	if (!bt_status) vibrate_timer = app_timer_register(5000, bt_vibrate, NULL);
	else if (bt_status && already_vibrated) already_vibrated = false;
}
// Shake/Tap Handler. On shake/tap... call "show_status"
void tap_handler(AccelAxisType axis, int32_t direction) {
	show_status();	
}
// Time Update Handler. Set current time, redraw date (to update when changed not at 2359) and update hands layer
void update_time(struct tm *t, TimeUnits units_changed) {
	current_time = t;
	layer_mark_dirty(hands_layer);
	draw_date_text();
}
// Status icon callback handler - from ModernCalendar
void event_status_layer_update_callback(Layer *layer, GContext *ctx) {
  
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	
  if (event_status_display == STATUS_REQUEST) {
     graphics_draw_bitmap_in_rect(ctx, icon_status_1, GRect(0, 0, 38, 9));
     //app_log(APP_LOG_LEVEL_INFO, "main.c", 352, "status 1");
  } else if (event_status_display == STATUS_REPLY) {
     graphics_draw_bitmap_in_rect(ctx, icon_status_2, GRect(0, 0, 38, 9));
     //app_log(APP_LOG_LEVEL_INFO, "main.c", 355, "status 2");
  } else if (event_status_display == STATUS_ALERT_SET) {
     graphics_draw_bitmap_in_rect(ctx, icon_status_3, GRect(0, 0, 38, 9));
     //app_log(APP_LOG_LEVEL_INFO, "main.c", 358, "status 3");
  }
}
// Setting event status - from ModernCalendar
void set_event_status(int new_event_status_display) {
  //app_log(APP_LOG_LEVEL_INFO, "event status", 362, "Entered event status handler");
  event_status_display = new_event_status_display;
	layer_mark_dirty(event_status_layer);
}

// On11 handle accleration data
void processAccelerometerData(AccelData* acceleration, uint32_t size) {
	Counter counter = mCounter;
//  app_log(APP_LOG_LEVEL_INFO, "Steps change", 320, "Entered accel handler");
	// Throw the variables into below function, it will update it for you.
	if (!status_showing){
    if (analyzeAcceleration(&mCurrentType, &mCounter, &mFilter, mIsDriving, mPedometerSensitivity, acceleration, size)) {
  		// Log data for companion app
      uint32_t steps = mCounter.steps - counter.steps; // May want to log this and update UI
  /*
      char tmpStr[32];
      switch(mCurrentType) {
        case 0:
        snprintf(tmpStr, sizeof(tmpStr), "Sleep");
        break;
        case 1:
        snprintf(tmpStr, sizeof(tmpStr), "Sit");
        break;
        case 2:
        snprintf(tmpStr, sizeof(tmpStr), "Walk");
        break;
        case 3:
        snprintf(tmpStr, sizeof(tmpStr), "Jog");
        break;
      }
      app_log(APP_LOG_LEVEL_INFO, "Activity type", 0, tmpStr, window);
  
      snprintf(tmpStr, 32, "%5d (+) -> %5d", (int) steps, (int) (mCounter.steps));
      app_log(APP_LOG_LEVEL_INFO, "Steps change", 0, tmpStr, window);
  */
    }
  }
}

///
// Deint item to free up memory when leaving watchface
///
void deinit() {
	bitmap_layer_destroy(battery_layer);
	gbitmap_destroy(icon_battery_charging);
	gbitmap_destroy(icon_battery_100);
	gbitmap_destroy(icon_battery_90);
	gbitmap_destroy(icon_battery_80);
	gbitmap_destroy(icon_battery_70);
	gbitmap_destroy(icon_battery_60);
	gbitmap_destroy(icon_battery_50);
	gbitmap_destroy(icon_battery_40);
	gbitmap_destroy(icon_battery_30);
	gbitmap_destroy(icon_battery_20);
	gbitmap_destroy(icon_battery_10);
	bitmap_layer_destroy(bt_layer);
	gbitmap_destroy(icon_bt_connected);
	gbitmap_destroy(icon_bt_disconnected);
//	bitmap_layer_destroy(date_window_layer);
//	gbitmap_destroy(image_date_window);
	text_layer_destroy(date_layer);
  text_layer_destroy(steps_layer);
  text_layer_destroy(sit_layer);
  text_layer_destroy(walk_layer);
  text_layer_destroy(jog_layer);
//  text_layer_destroy(week_layer);
	layer_destroy(hands_layer);
	gpath_destroy(minute_hand);
	gpath_destroy(hour_hand);
	gbitmap_destroy(background_image);
  bitmap_layer_destroy(background_layer);
  gbitmap_destroy(icon_status_1); // from ModernCalendar
  gbitmap_destroy(icon_status_2); // from ModernCalendar
  gbitmap_destroy(icon_status_3); // from ModernCalendar
  text_layer_destroy(event_layer); // from ModernCalendar
  text_layer_destroy(event_layer2); // from ModernCalendar
  layer_destroy(event_status_layer); // from ModernCalendar

	// On11 save persistent values
	persist_write_int(0, mCounter.sleepTime);
	persist_write_int(1, mCounter.sitTime);
	persist_write_int(2, mCounter.walkTime);
	persist_write_int(3, mCounter.jogTime);
	persist_write_int(4, mCounter.timestamp);
	persist_write_int(5, mCounter.steps);

	accel_data_service_unsubscribe();

	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	accel_tap_service_unsubscribe();

  //	layer_destroy(window_layer);
	window_destroy(window);
}
///
/// Initial watchface call
///
void init() {

	// Window init
	window = window_create();
	window_stack_push(window, true /* Animated */);
	window_layer = window_get_root_layer(window);
	window_bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, GColorBlack);
	const GPoint center = grect_center_point(&window_bounds);
	
	// Background image init and draw
	if (badge) background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_BADGE);
	else background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_NOBADGE);
  //background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_NOBADGE);
	background_layer = bitmap_layer_create(window_bounds);
	bitmap_layer_set_alignment(background_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));
	bitmap_layer_set_bitmap(background_layer, background_image);
	
	// Time at init
	time_t now = time(NULL);
	current_time = localtime(&now);
	
	// steps layer
  steps_layer = text_layer_create(GRect(1, 90, 144, 20));
  text_layer_set_text_color(steps_layer, GColorWhite);
	text_layer_set_text_alignment(steps_layer, GTextAlignmentCenter);
  text_layer_set_background_color(steps_layer, GColorClear);
  text_layer_set_font(steps_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(window_layer, text_layer_get_layer(steps_layer));
	layer_set_hidden(text_layer_get_layer(steps_layer), true);
  //draw_steps_text();

  // sit layer
  sit_layer = text_layer_create(GRect(1, 110, 48, 20));
  text_layer_set_text_color(sit_layer, GColorWhite);
	text_layer_set_text_alignment(sit_layer, GTextAlignmentLeft);
  text_layer_set_background_color(sit_layer, GColorClear);
  text_layer_set_font(sit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(sit_layer));
	layer_set_hidden(text_layer_get_layer(sit_layer), true);

  // walk layer
  walk_layer = text_layer_create(GRect(48, 110, 48, 20));
  text_layer_set_text_color(walk_layer, GColorWhite);
	text_layer_set_text_alignment(walk_layer, GTextAlignmentLeft);
  text_layer_set_background_color(walk_layer, GColorClear);
  text_layer_set_font(walk_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(walk_layer));
	layer_set_hidden(text_layer_get_layer(walk_layer), true);
  
  // jog layer
  jog_layer = text_layer_create(GRect(96, 110, 48, 22));
  text_layer_set_text_color(jog_layer, GColorWhite);
	text_layer_set_text_alignment(jog_layer, GTextAlignmentLeft);
  text_layer_set_background_color(jog_layer, GColorClear);
  text_layer_set_font(jog_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(jog_layer));
	layer_set_hidden(text_layer_get_layer(jog_layer), true);

  // Status icons and status layer creation - from ModernCalendar
  icon_status_1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_1);
  icon_status_2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_2);
  icon_status_3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_3);
  GRect sframe;
  sframe.origin.x = 54;
  sframe.origin.y = 1;
  sframe.size.w = 38;
  sframe.size.h = 9;

  event_status_layer = layer_create(sframe);
  layer_set_update_proc(event_status_layer, event_status_layer_update_callback);
  layer_add_child(window_get_root_layer(window), event_status_layer);
	layer_set_hidden(event_status_layer, true);

	// Watch hands init - note that the order of layer creation determines which layer appears above which...
	hour_hand = gpath_create(&HOUR_HAND_POINTS);
 	minute_hand = gpath_create(&MINUTE_HAND_POINTS);
	gpath_move_to(hour_hand, center);
 	gpath_move_to(minute_hand, center);
	hands_layer = layer_create(window_bounds);
	layer_add_child(window_layer, hands_layer);
	layer_set_update_proc(hands_layer, &draw_hands);

  /*
  // Date Window init, draw and hide
	image_date_window = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DATE_WINDOW);
	date_window_layer = bitmap_layer_create(GRect(117,75,27,21));
	layer_add_child(window_layer, bitmap_layer_get_layer(date_window_layer));
	bitmap_layer_set_bitmap(date_window_layer, image_date_window);
	layer_set_hidden(bitmap_layer_get_layer(date_window_layer), hide_date);
  
	// Date text init, then call "draw_date"
	date_layer = text_layer_create(GRect(119, 72, 30, 30));
	text_layer_set_text_color(date_layer, GColorBlack);
	text_layer_set_text_alignment(date_layer, GTextAlignmentLeft);
	text_layer_set_background_color(date_layer, GColorClear);
	text_layer_set_font(date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
	layer_add_child(window_layer, text_layer_get_layer(date_layer));
	layer_set_hidden(text_layer_get_layer(date_layer), hide_date);
//	draw_date();
	*/
  
	// Bluetooth icon init, then call "draw_bt_icon" (doesn't call bt_connection_handler to avoid vibrate at init if bluetooth not connected)
	icon_bt_connected = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_CONNECTED);
	icon_bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_DISCONNECTED);
	bt_layer = bitmap_layer_create(GRect(2,2,28,24));
	layer_add_child(window_layer, bitmap_layer_get_layer(bt_layer));
	bt_status = bluetooth_connection_service_peek();
//	draw_bt_icon();
	
	// Pebble battery icon init, then call "battery_state_handler" with current battery state
	icon_battery_charging = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_CHARGING);
	icon_battery_100 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_100);
	icon_battery_90 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_90);
	icon_battery_80 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_80);
	icon_battery_70 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_70);
	icon_battery_60 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_60);
	icon_battery_50 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_50);
	icon_battery_40 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_40);
	icon_battery_30 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_30);
	icon_battery_20 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_20);
	icon_battery_10 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_10);
	battery_layer = bitmap_layer_create(GRect(103,2,41,24));
	bitmap_layer_set_background_color(battery_layer, GColorBlack);
	layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));
	battery_state_handler(battery_state_service_peek());
	
	// date layer (ShaBP)
  date_layer = text_layer_create(GRect(1, 68, 144, 30));
  text_layer_set_text_color(date_layer, GColorWhite);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(date_layer, GColorClear);
  text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	layer_add_child(window_layer, text_layer_get_layer(date_layer));
	layer_set_hidden(text_layer_get_layer(date_layer), true);
  //draw_date_text();

  // Small time text init (for status upon shake) - ShaBP
  small_time_layer = text_layer_create(GRect(1, 18, 144, 55));
  text_layer_set_text_color(small_time_layer, GColorWhite);
  text_layer_set_text_alignment(small_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(small_time_layer, GColorClear);
  text_layer_set_font(small_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  layer_add_child(window_layer, text_layer_get_layer(small_time_layer));
  layer_set_hidden(text_layer_get_layer(small_time_layer), true);
 
  // Event layers init - from ModernCalendar 
  event_layer = text_layer_create(GRect(1, 140, 70, 21));
  text_layer_set_text_color(event_layer, GColorWhite);
  text_layer_set_text_alignment(event_layer, GTextAlignmentLeft);
  text_layer_set_background_color(event_layer, GColorClear);
  text_layer_set_font(event_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(event_layer));
	layer_set_hidden(text_layer_get_layer(event_layer), true);

  //app_log(APP_LOG_LEVEL_INFO, "init", 615, "So far so good");
  
  event_layer2 = text_layer_create(GRect(73, 140, 71, 21));
  text_layer_set_text_color(event_layer2, GColorWhite);
  text_layer_set_text_alignment(event_layer2, GTextAlignmentLeft);
  text_layer_set_background_color(event_layer2, GColorClear);
  text_layer_set_font(event_layer2, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(event_layer2));
	layer_set_hidden(text_layer_get_layer(event_layer2), true);

//  draw_event_text();
  
	// On11 load persistent values
	mCounter.sleepTime = persist_exists(0) ? persist_read_int(0) : 0;
	mCounter.sitTime = persist_exists(1) ? persist_read_int(1) : 0;
	mCounter.walkTime = persist_exists(2) ? persist_read_int(2) : 0;
	mCounter.jogTime = persist_exists(3) ? persist_read_int(3) : 0;
	if (persist_exists(4)) {
		mCounter.timestamp = persist_read_int(4);
	} else {
		mCounter.timestamp = (uint32_t) time(NULL);
	}
	mCounter.steps = persist_exists(5) ? persist_read_int(5) : 0;
//  steps = mCounter.steps;

	// On11 setup accelerometer API
	accel_data_service_subscribe(BATCH_SIZE, &processAccelerometerData);
	accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
//  app_log(APP_LOG_LEVEL_INFO, "accel register", 556, "Registered accel handler");

	// On11 initiate low pass filter for later use
	initLowPassFilter(&mFilter);

  // Message inbox and Calendar init - from ModernCalendar
  app_message_register_inbox_received(received_message);
  app_message_open(124, 256);

  // Events subscription
	tick_timer_service_subscribe(MINUTE_UNIT, update_time);
	bluetooth_connection_service_subscribe(bt_connection_handler);
	battery_state_service_subscribe(battery_state_handler);
	accel_tap_service_subscribe(tap_handler);

  //calendar_init();

  // Show status screen on init (comment out to have battery and bluetooth status hidden at init)
	// show_status();
 		
}

///
/// Main. Initiate watchapp. Subcribe to services: Time Update, Bluetooth connection status update, Battery state update, Accelerometer tap/shake event, App message recieve.
///
int main(void) {
	init();
	app_event_loop();
	deinit();
}

