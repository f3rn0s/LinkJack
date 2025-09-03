#include <Arduino.h>
#include <factory_gui.h>
#include "lvgl.h"
#include "lv_conf.h"
#include "pin_config.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include "OneButton.h"
#include <power.h>

#include <SPI.h>
#include <Ethernet.h>

static void timer_task(lv_timer_t *t);
static lv_obj_t *disp;

OneButton button1(PIN_BUTTON_1, true);
OneButton button2(PIN_BUTTON_2, true);

#define MSG_NEW_LINKSTATUS      1
#define MSG_NEW_DHCPSTATUS      2
#define MSG_NEW_IPADDRESS       3
#define MSG_NEW_GATEWAY         4
#define MSG_NEW_DNSSERVER  5
#define MSG_NEW_INTERNETSTATUS  6

static void update_text_subscriber_cb(lv_event_t *e) {
  lv_obj_t *label = lv_event_get_target(e);
  lv_msg_t *m = lv_event_get_msg(e);

  const char* msg = (const char*) lv_msg_get_payload(m);

  lv_label_set_text(label, msg);
}

void ui_begin() {
  disp = lv_tileview_create(lv_scr_act());
  lv_obj_align(disp, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_size(disp, LV_PCT(100), LV_PCT(100));
  lv_obj_set_scrollbar_mode(disp, LV_SCROLLBAR_MODE_OFF);
  //lv_obj_set_style_bg_color(disp, lv_color_black(), LV_PART_MAIN);

  lv_obj_t *tv1 = lv_tileview_add_tile(disp, 0, 0, LV_DIR_HOR);
  lv_obj_t *loading_label = lv_label_create(tv1);
  lv_label_set_text(loading_label, "Loading...");
  lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *tv2 = lv_tileview_add_tile(disp, 0, 1, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(tv2, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *port_label = lv_label_create(tv2);
  lv_label_set_recolor(port_label, true);

  lv_obj_t *dhcp_label = lv_label_create(tv2);
  lv_label_set_recolor(dhcp_label, true);

  lv_obj_t *ipad_label = lv_label_create(tv2);
  lv_label_set_recolor(ipad_label, true);

  lv_obj_t *gate_label = lv_label_create(tv2);
  lv_label_set_recolor(gate_label, true);

  lv_obj_t *dns_label = lv_label_create(tv2);
  lv_label_set_recolor(dns_label, true);

  lv_obj_t *intn_label = lv_label_create(tv2);
  lv_label_set_recolor(intn_label, true);

  lv_obj_add_event_cb(port_label, update_text_subscriber_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_obj_add_event_cb(dhcp_label, update_text_subscriber_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_obj_add_event_cb(ipad_label, update_text_subscriber_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_obj_add_event_cb(gate_label, update_text_subscriber_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_obj_add_event_cb(dns_label, update_text_subscriber_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_obj_add_event_cb(intn_label, update_text_subscriber_cb, LV_EVENT_MSG_RECEIVED, NULL);

  lv_msg_subsribe_obj(MSG_NEW_LINKSTATUS, port_label, NULL); // #ff0000 no#";
  lv_msg_subsribe_obj(MSG_NEW_DHCPSTATUS, dhcp_label, NULL); // #ff0000 no#";
  lv_msg_subsribe_obj(MSG_NEW_IPADDRESS,  ipad_label, NULL); // #ff0000 no#";
  lv_msg_subsribe_obj(MSG_NEW_GATEWAY,  gate_label, NULL); // #ff0000 no#";
  lv_msg_subsribe_obj(MSG_NEW_DNSSERVER,  dns_label, NULL); // #ff0000 no#";
  lv_msg_subsribe_obj(MSG_NEW_INTERNETSTATUS, intn_label, NULL); // #ff0000 no#";

  static lv_style_t style;

  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_text_line_space(&style, 20);
  lv_style_set_pad_all(&style, 5);

  lv_obj_add_style(port_label, &style, 0);
  lv_obj_add_style(dhcp_label, &style, 0);
  lv_obj_add_style(ipad_label, &style, 0);
  lv_obj_add_style(gate_label, &style, 0);
  lv_obj_add_style(dns_label, &style, 0);
  lv_obj_add_style(intn_label, &style, 0);
  //lv_label_set_text(debug_label, text.c_str());
  //lv_obj_align(debug_label, LV_ALIGN_LEFT_MID, 0, 0);


  lv_obj_align(port_label, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_align_to(dhcp_label, port_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
  lv_obj_align_to(ipad_label, dhcp_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
  lv_obj_align_to(gate_label, ipad_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
  lv_obj_align_to(dns_label, gate_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
  lv_obj_align_to(intn_label, dns_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

  lv_obj_set_tile_id(disp, 0, 0, LV_ANIM_OFF);

  //lv_timer_t *timer = lv_timer_create(timer_task, 500, seg_text);
}

esp_lcd_panel_io_handle_t io_handle = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_color_t *lv_disp_buf;
static bool is_initialized_lvgl = false;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
  if (is_initialized_lvgl) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
  }
  return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
  int offsetx1 = area->x1;
  int offsetx2 = area->x2;
  int offsety1 = area->y1;
  int offsety2 = area->y2;
  // copy a buffer's content to a specific area of the display
  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

void displayInit() {
  pinMode(PIN_LCD_RD, OUTPUT);
  digitalWrite(PIN_LCD_RD, HIGH);
  esp_lcd_i80_bus_handle_t i80_bus = NULL;
  esp_lcd_i80_bus_config_t bus_config = {
      .dc_gpio_num = PIN_LCD_DC,
      .wr_gpio_num = PIN_LCD_WR,
      .clk_src = LCD_CLK_SRC_PLL160M,
      .data_gpio_nums =
          {
              PIN_LCD_D0,
              PIN_LCD_D1,
              PIN_LCD_D2,
              PIN_LCD_D3,
              PIN_LCD_D4,
              PIN_LCD_D5,
              PIN_LCD_D6,
              PIN_LCD_D7,
          },
      .bus_width = 8,
      .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
  };
  esp_lcd_new_i80_bus(&bus_config, &i80_bus);

  esp_lcd_panel_io_i80_config_t io_config = {
      .cs_gpio_num = PIN_LCD_CS,
      .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = 20,
      .on_color_trans_done = notify_lvgl_flush_ready,
      .user_ctx = &disp_drv,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .dc_levels =
          {
              .dc_idle_level = 0,
              .dc_cmd_level = 0,
              .dc_dummy_level = 0,
              .dc_data_level = 1,
          },
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = PIN_LCD_RES,
      .color_space = ESP_LCD_COLOR_SPACE_RGB,
      .bits_per_pixel = 16,
  };
  esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
  esp_lcd_panel_reset(panel_handle);
  esp_lcd_panel_init(panel_handle);
  esp_lcd_panel_invert_color(panel_handle, true);

  esp_lcd_panel_swap_xy(panel_handle, true);
  esp_lcd_panel_mirror(panel_handle, true, false);
  // the gap is LCD panel specific, even panels with the same driver IC, can
  // have different gap value
  esp_lcd_panel_set_gap(panel_handle, 0, 35);

  /* Lighten the screen with gradient */
  ledcSetup(0, 10000, 8);
  ledcAttachPin(PIN_LCD_BL, 0);
  for (uint8_t i = 0; i < 0xFF; i++) {
    ledcWrite(0, i);
    delay(2);
  }

  lv_init();
  lv_disp_buf = (lv_color_t *)heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, NULL, LVGL_LCD_BUF_SIZE);
  /*Initialize the display*/
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
  lv_disp_drv_register(&disp_drv);

  is_initialized_lvgl = true;
}

#define USE_TWO_ETH_PORTS 0
 
#define ETH_PHY_TYPE        ETH_PHY_W5500
#define ETH_PHY_ADDR        1
#define ETH_PHY_CS          10
#define ETH_PHY_IRQ         -1 // -1 if you won't wire
#define ETH_PHY_RST         -1 // -1 if you won't wire
 
// SPI pins
#define ETH_SPI_SCK         12
#define ETH_SPI_MISO        13
#define ETH_SPI_MOSI        11

bool linkStatus;
bool dhcpStatus;

byte mac[] = { 0x00, 0x06, 0x4F, 0x0D, 0xAC, 0xA9 };

void checkDHCP() {
    lv_obj_set_tile_id(disp, 0, 0, LV_ANIM_ON);
    
    LV_DELAY(100);

    dhcpStatus = Ethernet.begin(mac, (unsigned long) 15000, (unsigned long) 15000);
    //dhcpStatus = Ethernet.begin(mac);
    linkStatus = Ethernet.linkStatus() == LinkON;

    String link_msg = "Port: ";
    link_msg += linkStatus ? "#00FF00 Online #" : "#FF0000 Offline #";

    String dhcp_msg = "DHCP: ";
    dhcp_msg += dhcpStatus ? "#00FF00 Yes #" : "#FF0000 No #";

    String ipaddress_msg = "IP: ";
    ipaddress_msg += dhcpStatus ? Ethernet.localIP().toString() : "N/A";

    String gateway_msg = "GW: ";
    gateway_msg += dhcpStatus ? Ethernet.gatewayIP().toString() : "N/A";

    String dns_msg = "DNS: ";
    dns_msg += dhcpStatus ? Ethernet.dnsServerIP().toString() : "N/A";

    String internet_msg = "Internet: ";

    if (dhcpStatus) {
      EthernetClient client;

      char server[] = "www.google.com";

      if (client.connect(server, 80)) {
        client.stop();
        internet_msg += "#00FF00 Yes/Proxy #";
      } else {
        internet_msg += "#FF0000 No #";
      }
    } else {
      internet_msg += "#FF0000 No #";
    }

    lv_msg_send(MSG_NEW_LINKSTATUS, link_msg.c_str());
    lv_msg_send(MSG_NEW_DHCPSTATUS, dhcp_msg.c_str());
    lv_msg_send(MSG_NEW_IPADDRESS, ipaddress_msg.c_str());
    lv_msg_send(MSG_NEW_GATEWAY, gateway_msg.c_str());
    lv_msg_send(MSG_NEW_DNSSERVER, dns_msg.c_str());
    lv_msg_send(MSG_NEW_INTERNETSTATUS, internet_msg.c_str());

    lv_obj_set_tile_id(disp, 0, 1, LV_ANIM_ON);
}

void setup() {
  Serial.begin(115200);

  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  //delay(2000); // Give time for USB to attach

  displayInit();

  button1.attachClick([]() { checkDHCP(); });
  button2.attachClick([]() { shutdown(); });

  LV_DELAY(100);

  ui_begin();

  checkDHCP();



  //Serial.println("Initializing Ethernet...");
  //Serial.flush();

  // To use
  //dhcpStatus = Ethernet.begin(mac);
  //linkStatus = Ethernet.linkStatus() == LinkON;
}

void loop() {
  lv_timer_handler();
  button1.tick();
  button2.tick();
  delay(3);
}