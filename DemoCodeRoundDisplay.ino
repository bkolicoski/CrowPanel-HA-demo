#define LGFX_USE_V1
#include "Arduino.h"
#include <lvgl.h>
//#include "demos/lv_demos.h"
#include <LovyanGFX.hpp>
#include "CST816D.h"

#define LV_BUILD_EXAMPLES 0

// I2C pins
#define I2C_SDA 4
#define I2C_SCL 5
// Touch panel interrupt and reset pins
#define TP_INT 0
#define TP_RST -1

// I2C address for the I/O extender
#define PI4IO_I2C_ADDR 0x43

// Buffer size definition
#define buf_size 120

// LGFX class definition for display device interface
class LGFX : public lgfx::LGFX_Device
{
  // Internal components of the LGFX class
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
  public:
    LGFX(void)
    {
      {
        // Configuration for the SPI bus and panel
        auto cfg = _bus_instance.config();
        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = 80000000;
        cfg.freq_read = 20000000;
        cfg.spi_3wire = true;
        cfg.use_lock = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk = 6;
        cfg.pin_mosi = 7;
        cfg.pin_miso = -1;
        cfg.pin_dc = 2;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }
      {
        auto cfg = _panel_instance.config();
        cfg.pin_cs = 10;
        cfg.pin_rst = -1;
        cfg.pin_busy = -1;
        cfg.memory_width = 240;
        cfg.memory_height = 240;
        cfg.panel_width = 240;
        cfg.panel_height = 240;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        cfg.offset_rotation = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = false;
        cfg.invert = true;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;
        _panel_instance.config(cfg);
      }
      setPanel(&_panel_instance);
    }
};

// Global instances for the display and touch screen
LGFX tft;
CST816D touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

/*Change to your screen resolution*/
static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][screenWidth * buf_size];

#if LV_USE_LOG != 0
// Custom print function for LVGL debugging
void my_print(lv_log_level_t level, const char *file, uint32_t line, const char *fn_name, const char *dsc)
{
  Serial.printf("%s(%s)@%d->%s\r\n", file, fn_name, line, dsc);
  Serial.flush();
}
#endif

// Function to handle flushing of display buffer to the screen
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  if (tft.getStartCount() == 0)
  {
    tft.endWrite();
  }

  tft.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (lgfx::swap565_t *)&color_p->full);

  lv_disp_flush_ready(disp); /* tell lvgl that flushing is done */
}

// Function to read touchpad input
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  bool touched;
  uint8_t gesture;
  uint16_t touchX, touchY;
  touched = touch.getTouch(&touchX, &touchY, &gesture);
  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;
    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

// Function to initialize I/O extender
void init_IO_extender() {
  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x01); // test register
  Wire.endTransmission();
  Wire.requestFrom(PI4IO_I2C_ADDR, 1);
  uint8_t rxdata = Wire.read();
  Serial.print("Device ID: ");
  Serial.println(rxdata, HEX);

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x03); // IO direction register
  Wire.write((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4)); // set pins 0, 1, 2 as outputs
  Wire.endTransmission();

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x07); // Output Hi-Z register
  Wire.write(~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4))); // set pins 0, 1, 2 low
  Wire.endTransmission();
}

// Function to set I/O pin state on the extender
void set_pin_io(uint8_t pin_number, bool value) {

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x05); // test register
  Wire.endTransmission();
  Wire.requestFrom(PI4IO_I2C_ADDR, 1);
  uint8_t rxdata = Wire.read();
  Serial.print("Before the change: ");
  Serial.println(rxdata, HEX);

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x05); // Output register

  if (!value)
    Wire.write((~(1 << pin_number)) & rxdata); // set pin low
  else
    Wire.write((1 << pin_number) | rxdata); // set pin high
  Wire.endTransmission();

  Wire.beginTransmission(PI4IO_I2C_ADDR);
  Wire.write(0x05); // test register
  Wire.endTransmission();
  Wire.requestFrom(PI4IO_I2C_ADDR, 1);
  rxdata = Wire.read();
  Serial.print("after the change: ");
  Serial.println(rxdata, HEX);
}

static lv_obj_t * meter;
static lv_obj_t * humidity_label;

static void set_value(void * indic, int32_t v)
{
    lv_meter_set_indicator_value(meter, (lv_meter_indicator_t *)indic, v);
}

/**
 * A simple meter
 */
void lv_example_meter_1(void)
{
    meter = lv_meter_create(lv_scr_act());
    lv_obj_center(meter);
    lv_obj_set_size(meter, 230, 230);

    /*Add a scale first*/
    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 10, 4, 15, lv_color_black(), 10);

    lv_meter_set_scale_range(meter, scale, -20, 60, 270, 135);

    lv_meter_indicator_t * indic;

    /*Add a blue arc for cold temperatures*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, indic, -20);
    lv_meter_set_indicator_end_value(meter, indic, 10);

    /*Make the tick lines blue for cold temperatures*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE),
                                     false, 0);
    lv_meter_set_indicator_start_value(meter, indic, -20);
    lv_meter_set_indicator_end_value(meter, indic, 10);

    /*Add a red arc for hot temperatures*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, 40);
    lv_meter_set_indicator_end_value(meter, indic, 60);

    /*Make the tick lines red for hot temperatures*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false,
                                     0);
    lv_meter_set_indicator_start_value(meter, indic, 40);
    lv_meter_set_indicator_end_value(meter, indic, 60);

    /*Add a needle line indicator*/
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    /*Create an animation to set the value*/
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_value);
    lv_anim_set_var(&a, indic);
    lv_anim_set_values(&a, -20, 60);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    // Create a label for displaying humidity
    humidity_label = lv_label_create(lv_scr_act());
    lv_label_set_text(humidity_label, "0.00%");
    lv_obj_center(humidity_label);
}

void setup()
{
  // Initialization of serial communication, I2C, display, touch screen, and LVGL
  Serial.begin(115200); /* prepare for possible serial debug */
  Serial.println("I am LVGL_Arduino");
  Wire.begin(4, 5);
  init_IO_extender();
  delay(100);
  set_pin_io(3, true);
  set_pin_io(4, true);
  set_pin_io(2, true);
  //turn off vibration motor
  set_pin_io(0, false);
  tft.init();
  tft.initDMA();
  tft.startWrite();
  tft.setColor(0, 0, 0);

  tft.fillScreen(TFT_BLACK);
  delay(200);
  touch.begin();
  lv_init();
#if LV_USE_LOG != 0
  //lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], screenWidth * buf_size);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);


#if 0
  /* Create simple label */
  lv_obj_t *label = lv_label_create( lv_scr_act() );
  lv_label_set_text( label, "Hello Arduino! (V8.0.X)" );
  lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );
#else
  // uncomment one of these demos
  //lv_demo_widgets(); // OK
  lv_example_meter_1();
#endif
  Serial.println("Setup done");
}

void set_humidity_label(float humidity) {
    // Create a string to store the formatted text
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f%%", humidity);
    lv_label_set_text(humidity_label, buf); // Set the text of the label
    lv_obj_center(humidity_label); // Re-center the label if needed
}


void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
