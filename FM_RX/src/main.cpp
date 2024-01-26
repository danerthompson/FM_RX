// Made by Dane Thompson

#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>
#include <ESP32Encoder.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SSD1306.h>
#include <nau8810.h>
#include <driver/ledc.h>
#include <driver/i2s.h>

// Pin definitions
#define EXT_LO_EN 17
#define PLL_LO_EN 18
#define ROT1_A 21
#define ROT1_B 35
#define ROT1_SW 36
#define ROT2_A 37
#define ROT2_B 38
#define ROT2_SW 39
#define RF_EN 40
#define GOOD_5V 41
#define TOGGLE1 42
#define LED1 47
#define LED2 48
#define BAT_ADC_EN 5
#define BAT_ADC 4
#define I2C_SDA 9
#define I2C_SCL 10
#define BUT1 11
#define I2S_BCLK 12
#define MCLK 13
#define I2S_FS 14
#define NAU_ADCOUT 15
#define NAU_DACIN 16

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDR 0x3C

#define NAU8810_ADDR 0x1A     // Datasheet says 34, but 7 bit address BS (ESP32 scanner found this)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

NAU8810 audio_codec(NAU8810_ADDR, &Wire);
int8_t volume = 30;  // max value 63
int8_t alc = 15;  // Max value 15
uint8_t audio_ctrl_state = 0;   // 0 for speaker gain, 1 for ALC gain
int8_t eq_gain[5] = {0x0C, 0x0C, 0x0C, 0x0C, 0x0C};
uint8_t audio_output = 0; // 0 for speaker, 1 for aux port

volatile uint8_t DISPLAY_FLAG = 1; // Set when display needs to update

// TwoWire i2c_bus = TwoWire(0);
volatile uint8_t freq_digit = 2;
volatile int64_t freq_step = 100000; // Adjust this to control which digit is stepped with encoder (default 100 kHz)
uint64_t pll_freq = 85600000ULL;

int64_t rot1_count = 0;
int64_t rot1_prev = 0;
int64_t rot2_count = 0;
int64_t rot2_prev = 0;

Si5351 pll;

ESP32Encoder rot1, rot2;

volatile uint8_t LO_SELECT = 1; // 1 if using PLL for LO, 0 for EXT
volatile uint8_t LO_CHANGE = 1; // 1 if LO has been changed, used to update LCD

volatile uint32_t ROT1_DEBOUNCE, ROT2_DEBOUNCE, BUT1_DEBOUNCE;
volatile uint8_t BUT1_STATE = 0; // 0 for not pressed, 1 for pressed
volatile uint8_t ROT1_STATE = 0; // 0 for not pressed, 1 for pressed
volatile uint8_t ROT2_STATE = 0; // 0 for not pressed, 1 for pressed
uint32_t DEBOUNCE_TIME = 10;


hw_timer_t *display_timer = NULL;

float batVoltage;

// INTERRUPT FUNCTIONS

// Called periodically to update LCD
void IRAM_ATTR DISPLAY_TIMER_ISR() {
  DISPLAY_FLAG = 1;
}

// Called when ROT1 switch is pressed or released (CHANGE)
void IRAM_ATTR ROT1_SW_ISR()
{
  if (!digitalRead(ROT1_SW))
  { // If button pressed
    if (!ROT1_STATE && (millis() - ROT1_DEBOUNCE >= DEBOUNCE_TIME))
    {

      ROT1_DEBOUNCE = millis();
      ROT1_STATE = 1;

      // When ROT1 button is pressed, cycle through which digit is changed with freq knob
      switch (freq_step)
      {
      case 1000000: // 1 MHz to 100 kHz
        freq_step = 100000;
        freq_digit = 2;
        break;
      case 100000: // 100 kHz to 10 kHz
        freq_step = 10000;
        freq_digit = 3;
        break;
      case 10000: // 10 kHz to 1 kHz
        freq_step = 1000;
        freq_digit = 4;
        break;
      case 1000: // 1 kHz to 1 MHz
        freq_step = 1000000;
        freq_digit = 1;
        break;
        //      default:      // Default to 100 kHz
        //        freq_step = 100000;
      }
      DISPLAY_FLAG = 1;
    }
  }
  else
  { // If button not pressed
    if (ROT1_STATE && (millis() - ROT1_DEBOUNCE >= DEBOUNCE_TIME))
    {

      ROT1_DEBOUNCE = millis();
      ROT1_STATE = 0;
    }
  }
}   // End of ROT1 ISR





// Called when ROT2 switch is pressed or released (CHANGE)
void IRAM_ATTR ROT2_SW_ISR()
{
  if (!digitalRead(ROT2_SW))
  { // If button pressed
    if (!ROT2_STATE && (millis() - ROT2_DEBOUNCE >= DEBOUNCE_TIME))
    {

      ROT2_DEBOUNCE = millis();
      ROT2_STATE = 1;


      audio_ctrl_state++;
      if (audio_ctrl_state > 7) {
        audio_ctrl_state = 0;     
      }


      DISPLAY_FLAG = 1;
    }
  }
  else
  { // If button not pressed
    if (ROT2_STATE && (millis() - ROT2_DEBOUNCE >= DEBOUNCE_TIME))
    {

      ROT2_DEBOUNCE = millis();
      ROT2_STATE = 0;
    }
  }
}   // End of ROT2 ISR




// Called when BUT1 switch changes
void IRAM_ATTR BUTTON_ISR()
{
  if (!digitalRead(BUT1))
  { // If button pressed
    if (!BUT1_STATE && (millis() - BUT1_DEBOUNCE >= DEBOUNCE_TIME))
    {

      BUT1_DEBOUNCE = millis();
      BUT1_STATE = 1;

      // Toggle which source the LO signal comes from (PLL or external)
      LO_SELECT = !LO_SELECT;
      LO_CHANGE = 1;
      digitalWrite(EXT_LO_EN, LO_SELECT);
      digitalWrite(PLL_LO_EN, !LO_SELECT);
      digitalWrite(LED1, LO_SELECT); // LED1 turns on when PLL is used as LO

      DISPLAY_FLAG = 1;
      
    }
  }
  else
  { // If button not pressed
    if (BUT1_STATE && (millis() - BUT1_DEBOUNCE >= DEBOUNCE_TIME))
    {

      BUT1_DEBOUNCE = millis();
      BUT1_STATE = 0;
    }
  }
}   // End of BUT1 ISR

void setup()
{

  setCpuFrequencyMhz(80); // Lower power draw

  // Pin initializations
  pinMode(EXT_LO_EN, OUTPUT);
  pinMode(PLL_LO_EN, OUTPUT);
  pinMode(ROT1_A, INPUT);
  pinMode(ROT1_B, INPUT);
  pinMode(ROT1_SW, INPUT);
  pinMode(ROT2_A, INPUT);
  pinMode(ROT2_B, INPUT);
  pinMode(ROT2_SW, INPUT);
  pinMode(RF_EN, OUTPUT);
  pinMode(GOOD_5V, INPUT);
  pinMode(TOGGLE1, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(BAT_ADC_EN, OUTPUT);
  pinMode(BAT_ADC, INPUT);
  pinMode(BUT1, INPUT);
  pinMode(MCLK, OUTPUT);
  pinMode(I2S_BCLK, OUTPUT);
  pinMode(I2S_FS, OUTPUT);

  // Set LO input
  // digitalWrite( EXT_LO_EN, digitalRead(TOGGLE1) );
  // digitalWrite( PLL_LO_EN, !digitalRead(TOGGLE1) );

  attachInterrupt(ROT1_SW, ROT1_SW_ISR, CHANGE);
  attachInterrupt(ROT2_SW, ROT2_SW_ISR, CHANGE);
  attachInterrupt(BUT1, BUTTON_ISR, CHANGE);

  // Initialize timer0 with 80 prescale (80 MHz / 80 = 1 MHz)
  display_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(display_timer, DISPLAY_TIMER_ISR, true);
  timerAlarmWrite(display_timer, 100000, true);   // Call ISR every 100,000 counts (10 times / second)
  timerAlarmEnable(display_timer);

  digitalWrite(EXT_LO_EN, LO_SELECT);
  digitalWrite(PLL_LO_EN, !LO_SELECT);
  digitalWrite(LED1, LO_SELECT); // LED1 turns on when PLL is used as LO

  // digitalWrite( LED1, HIGH);
  digitalWrite(RF_EN, digitalRead(TOGGLE1)); // Enable 5V RF rail

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);

  delay(1000);
  Serial.println("Hello world");

  Wire.setPins(I2C_SDA, I2C_SCL);

  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 48000,
    .bits_per_sample = i2s_bits_per_sample_t(24),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 8,
    .use_apll = true,
    .mclk_multiple = I2S_MCLK_MULTIPLE_384
  };
 
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);


    // Set I2S pin configuration
  const i2s_pin_config_t pin_config = {
    .mck_io_num = MCLK,
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_FS,
    .data_out_num = -1,
    .data_in_num = NAU_ADCOUT
  };
 
  i2s_set_pin(I2S_NUM_0, &pin_config);

  i2s_start(I2S_NUM_0);
  
  

  if (audio_codec.begin()) {
    Serial.println("Failed to intialize audio codec");
    while(1) 
    {
    }
  }
  Serial.println(audio_codec.setPLL(5000000));
  audio_codec.setSpeakerVolume(volume);
  audio_codec.setALCGain(alc);
  audio_codec.setOutput(audio_output);
  Serial.println(audio_codec.readRegister(NAU_ALC2_CTRL_ADDR), HEX);
  Serial.println(audio_codec.readRegister(NAU_POWER2_ADDR), HEX);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR))
  {
    Serial.println("Failed to initialize OLED");
    while (1)
    {
    };
  }
  Serial.println("Hello world");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);

  if (!pll.init(SI5351_CRYSTAL_LOAD_10PF, 0, 0))
  { // Add 3.9 pF caps on either side of oscillator to make load capacitance 12 (10 + 4/2)
    Serial.println("Failed to initialize Si5351");
    while (1)
    {
    };
  }

  pll.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  pll.set_correction(-215*100, SI5351_PLL_INPUT_XO);           // Set this to the difference in frequency from CLK2 and 100 MHz (in 0.01 Hz increments)
  pll.set_freq(pll_freq * 100, SI5351_CLK0);

  
  //pll.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  //pll.set_freq(10000000ULL*100, SI5351_CLK2);             // Output desired 100 MHz on CLK2 for PLL calibration
  //pll.update_status();
  Serial.println("Hello world");

  rot1.attachSingleEdge(ROT1_A, ROT1_B);
  rot2.attachSingleEdge(ROT2_A, ROT2_B);

  rot1.clearCount();
  rot2.clearCount();
}

void loop()
{

  rot1_count = rot1.getCount(); // Controls PLL frequency
  rot2_count = rot2.getCount(); // Controls volume?

  if (rot1_count != rot1_prev || rot2_count != rot2_prev || DISPLAY_FLAG)
  {
    // Change PLL settings
    pll_freq = (rot1_count - rot1_prev) * freq_step + pll_freq;
    pll.set_freq(pll_freq * 100, SI5351_CLK0);
    //pll.set_freq(pll_freq * 100, SI5351_CLK1);
    pll.update_status();

    if (LO_CHANGE) {
      pll.output_enable(SI5351_CLK0, LO_SELECT);
      LO_CHANGE = 0;
    }

    //Serial.println(pll_freq);
    // rot1_prev = rot1_count;
    rot1_prev = 0;
    rot1.clearCount();


    // Update audio codec
    if (rot2_count != rot2_prev) {
      switch (audio_ctrl_state) {
        // Volume control
        case 0:
          volume += (rot2_count-rot2_prev);
          if (volume > 63) {
            volume = 63;
          }
          else if (volume < 0) {
            volume = 0;
          }
          audio_codec.setSpeakerVolume(volume);
          //Serial.println((rot2_count - rot2_prev) + volume);
          
        
        break;
        // ACL control
        case 1:
          alc += rot2_count - rot2_prev;
          if (alc > 15) {
            alc = 15;
            audio_codec.writeToRegister(NAU_DAC_CTRL_ADDR, NAU_DAC_CTRL_CMD);
          }
          else if (alc < 0) {
            alc = 0;
            audio_codec.writeToRegister(NAU_DAC_CTRL_ADDR, 0x0000);
          }
          audio_codec.setALCGain(alc);
          break;

        // Output control
        case 2:
          if (audio_output == 0) {
            audio_output = 1;
          }
          else {
            audio_output = 0;
          }
          audio_codec.setOutput(audio_output);
          break;

        // EQ1 control
        case 3:
          eq_gain[0] -= rot2_count - rot2_prev;
          if (eq_gain[0] > 0x18) {
            eq_gain[0] = 0x18;
          }
          else if (eq_gain[0] < 0) {
            eq_gain[0] = 0;
          }
          audio_codec.setEQGain(1, eq_gain[0]);
          break;

        // EQ2 control
        case 4:
          eq_gain[1] -= rot2_count - rot2_prev;
          if (eq_gain[1] > 0x18) {
            eq_gain[1] = 0x18;
          }
          else if (eq_gain[1] < 0) {
            eq_gain[1] = 0;
          }
          audio_codec.setEQGain(2, eq_gain[1]);
          break;

        // EQ3 control
        case 5:
          eq_gain[2] -= rot2_count - rot2_prev;
          if (eq_gain[2] > 0x18) {
            eq_gain[2] = 0x18;
          }
          else if (eq_gain[2] < 0) {
            eq_gain[2] = 0;
          }
          audio_codec.setEQGain(3, eq_gain[2]);
          break;

        // EQ4 control
        case 6:
          eq_gain[3] -= rot2_count - rot2_prev;
          if (eq_gain[3] > 0x18) {
            eq_gain[3] = 0x18;
          }
          else if (eq_gain[3] < 0) {
            eq_gain[3] = 0;
          }
          audio_codec.setEQGain(4, eq_gain[3]);
          break;

        // EQ5 control
        case 7:
          eq_gain[4] -= rot2_count - rot2_prev;
          if (eq_gain[4] > 0x18) {
            eq_gain[4] = 0x18;
          }
          else if (eq_gain[4] < 0) {
            eq_gain[4] = 0;
          }
          audio_codec.setEQGain(5, eq_gain[4]);
          break;
      }
      rot2_prev = 0;
      rot2.clearCount();
    }



    // Read input voltage
    digitalWrite(BAT_ADC_EN, HIGH);
    delay(1);
    batVoltage = 2.0 * 3.3 * (float)analogRead(BAT_ADC) / 4095.0;
    digitalWrite(BAT_ADC_EN, LOW);


    display.setTextColor(SSD1306_WHITE);


    // Print frequency to screen
    display.clearDisplay();
    display.setCursor(0, 0);
    if (pll_freq + 10700000 < 100000000)
    {
      display.print(F(" "));
      display.print((float)pll_freq / 1000000.0 + 10.7, 3);
      display.print(F(" MHz"));
    }
    else
    {
      display.print((float)pll_freq / 1000000.0 + 10.7, 3);
      display.print(F(" MHz"));
    }
    display.setCursor(0, 10);
    display.print(F(" "));
    // To consider decimal point
    if (freq_digit > 1)
    {
      display.print(F(" "));
    }
    for (uint8_t i = 0; i < freq_digit; i++)
    {
      display.print(F(" "));
    }
    display.print(F("^"));
    
    // Print input voltage to LCD
    display.setCursor(0, 16);
    display.print("Bat: ");
    display.print(batVoltage, 2);

    // Print volume to LCD
    display.setCursor(0,26);
    if (audio_ctrl_state == 0) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    display.print(F("Vol:"));
    if (audio_ctrl_state == 0) {
      display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    }
    display.print(F(" "));
    display.print(volume);

    // Print ALC level to LCD
    display.setCursor(0,36);
    if (audio_ctrl_state == 1) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    display.print(F("ALC:"));
    if (audio_ctrl_state == 1) {
      display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    }
    display.print(F(" "));
    display.print(alc);


    // Print output selection to LCD
    display.setCursor(0,46);
    if (audio_ctrl_state == 2) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    display.print(F("Out:"));
    if (audio_ctrl_state == 2) {
      display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    }
    display.print(F(" "));
    if (audio_output == 0) {
      display.print(F("SPK"));
    }
    else {
      display.print(F("AUX"));
    }

    
    // Print EQ levels to LCD
    for (uint8_t i = 1; i <= 5; i++) {
      display.setCursor(80,6+10*i);
      if (audio_ctrl_state == i+2) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      }
      display.print(F("EQ"));
      display.print(i);
      display.print(F(":"));
      if (audio_ctrl_state == i+2) {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
      }
      display.print(F(" "));
      display.print(12-eq_gain[i-1]);
    }


    

    // Print LO selection
    display.setCursor(100,0);
    if (LO_SELECT) {
      display.print(F("PLL"));
    }
    else {
      display.print(F("EXT"));
    }

    

    display.display();
    DISPLAY_FLAG = 0;
  }
}
