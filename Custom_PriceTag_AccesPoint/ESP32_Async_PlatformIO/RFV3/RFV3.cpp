#include <Arduino.h>
#include <SPI.h>
#include <U8x8lib.h>
#include "RFV3.h"
#include "main_variables.h"
#include "cc1101_spi.h"
#include "cc1101.h"
#include "class.h"
#include "compression.h"
#include "interval_timer.h"
#include "settings.h"
#include "web.h"
#include "utils.h"
#include "mode_sync.h"
#include "mode_full_sync.h"
#include "mode_idle.h"
#include "mode_trans.h"
#include "mode_wu.h"
#include "mode_wu_reset.h"
#include "mode_wu_activation.h"
#include "mode_wun_activation.h"
#include "mode_activation.h"

class ModePlaceholder : public mode_class
{
};
ModePlaceholder modePlaceholder;

mode_class *currentMode = &modePlaceholder;
mode_class *tempMode = &modeIdle;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/15, /* data=*/4, /* reset=*/16);

volatile int interrupt_counter = 0;
int no_count_counter = 0;

volatile int int_fired = 0;
void IRAM_ATTR GDO2_interrupt()
{
  interrupt_counter++;
  int_fired++;
}

void init_interrupt()
{
  pinMode(GDO2, INPUT);
  attachInterrupt(GDO2, GDO2_interrupt, FALLING);
}

void init_oled()
{
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, "initializing...");
}

void log(String message)
{
  Serial.print(millis());
  Serial.println(" : " + message);
}

void setup()
{
  Serial.begin(500000);
  Serial.setDebugOutput(true);
  SPIFFS.begin(true);
  init_oled();
  init_spi();
  uint8_t radio_status;
  while ((radio_status = init_radio()))
  {
    if (radio_status == 1)
      Serial.println("Radio not working!!! ERROR");
    if (radio_status == 2)
      Serial.println("GPIO2 Interrupt input not working");
    delay(1000);
  }
  read_boot_settings();
  init_interrupt();
  init_timer();
  init_web();

  u8x8.clearLine(2);
  u8x8.setCursor(0, 2);
  u8x8.print(WiFi.localIP());
  u8x8.setCursor(0, 4);
  u8x8.print("Uptime:");
}

void loop()
{
  if (!WiFi.isConnected())
  {
    u8x8.clearLine(2);
    u8x8.setCursor(0, 2);
    u8x8.print("disconnected");
    if (WiFi.reconnect())
    {
      u8x8.clearLine(2);
      u8x8.setCursor(0, 2);
      u8x8.print(WiFi.localIP());
    };
  }

  if (int_fired)
  {
    int_fired--;
    currentMode->interrupt();
  }
  if (currentMode != tempMode)
  {
    currentMode->post();
    currentMode = tempMode;
    log("Mode changed to " + currentMode->get_name());
    u8x8.clearLine(0);
    u8x8.setCursor(0, 0);
    u8x8.print("Mode: ");
    u8x8.print(currentMode->get_name());
    u8x8.setCursor(0, 5);
    u8x8.print(millis() / 1000);
    currentMode->pre();
  }

  currentMode->main();
  if (check_new_interval())
  {
    log("Count: " + String(interrupt_counter));
    if (interrupt_counter == 0)
    {
      if (no_count_counter++ > 5)
      {
        no_count_counter = 0;
        log("no interrupts anymore, something is broken, trying to fix it now");
        if (get_trans_mode())
        {
          set_trans_mode(0);
          restore_current_settings();
        }
        set_last_activation_status(0);
        if (currentMode == &modeIdle)
          set_mode_full_sync();
        else if (currentMode == &modeFullSync || currentMode == &modeWu || currentMode == &modeWuAct || currentMode == &modeWunAct || currentMode == &modeWuReset || currentMode == &modeActivation)
        {
          currentMode->pre();
        }
        else
          set_mode_idle();
      }
    }
    else
    {
      no_count_counter = 0;
    }
    interrupt_counter = 0;
    currentMode->new_interval();
  }
}

void set_mode_idle()
{
  tempMode = &modeIdle;
}

void set_mode_sync()
{
  tempMode = &modeSync;
}

void set_mode_full_sync()
{
  tempMode = &modeFullSync;
}

void set_mode_trans()
{
  tempMode = &modeTrans;
}

void set_mode_wu()
{
  tempMode = &modeWu;
}

void set_mode_wu_reset()
{
  tempMode = &modeWuReset;
}

void set_mode_wu_activation()
{
  tempMode = &modeWuAct;
}

void set_mode_wun_activation()
{
  tempMode = &modeWunAct;
}

void set_mode_activation()
{
  tempMode = &modeActivation;
}

String get_mode_string()
{
  return currentMode->get_name();
}
