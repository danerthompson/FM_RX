#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>
#include <ESP32Encoder.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SSD1306.h>
#include <nau8810.h>

// Pin definitions
#define EXT_LO_EN   17
#define PLL_LO_EN   18
#define ROT1_A      21
#define ROT1_B      35
#define ROT1_SW     36
#define ROT2_A      37
#define ROT2_B      38
#define ROT2_SW     39
#define RF_EN       40
#define GOOD_5V     41
#define TOGGLE1     42
#define LED1        47
#define LED2        48
#define BAT_ADC_EN  5
#define BAT_ADC     4
#define I2C_SDA     9
#define I2C_SCL     10
#define BUT1        11
#define I2S_BCLK    12
#define I2S_MCLK    13
#define I2S_FS      14
#define NAU_ADCOUT  15
#define NAU_DACIN   16

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDR   0x78

#define NAU8810_ADDR  0x34

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

NAU8810 audio_codec(NAU8810_ADDR, &Wire);



//TwoWire i2c_bus = TwoWire(0);
volatile int64_t freq_step = 1000000LL;   // Adjust this to control which digit is stepped with encoder (default 1 MHz)
uint64_t pll_freq = 96300000ULL;

int64_t rot1_count = 0;
int64_t rot1_prev = 0;
int64_t rot2_count = 0;
int64_t rot2_prev = 0;

Si5351 pll;

ESP32Encoder rot1, rot2;

// INTERRUPT FUNCTIONS

// Called when ROT1 switch is pressed (falling edge)
void IRAM_ATTR ROT1_SW_ISR() {
  // When ROT1 button is pressed, cycle through which digit is changed with freq knob
  switch (freq_step) {
    case 1000000: // 1 MHz to 100 kHz
      freq_step = 100000;
      break;
    case 100000:  // 100 kHz to 10 kHz
      freq_step = 10000;
      break;
    case 10000:   // 10 kHz to 1 kHz
      freq_step = 1000;
      break;
    case 1000:    // 1 kHz to 1 MHz
      freq_step = 1000000;
      break;
    default:      // Default to 100 kHz
      freq_step = 100000;
  }
}

// Called when TOGGLE1 switch changes
void IRAM_ATTR TOGGLE1_ISR() {
  // Toggle which source the LO signal comes from (PLL or external)
  digitalWrite( EXT_LO_EN, digitalRead(TOGGLE1) );
  digitalWrite( PLL_LO_EN, !digitalRead(TOGGLE1) );
  digitalWrite( LED1, !digitalRead(TOGGLE1) );  // LED1 turns on when PLL is used as LO
}

void setup() {

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

  

  // Set LO input
  digitalWrite( EXT_LO_EN, digitalRead(TOGGLE1) );
  digitalWrite( PLL_LO_EN, !digitalRead(TOGGLE1) );

  attachInterrupt(ROT1_SW, ROT1_SW_ISR, FALLING);
  attachInterrupt(TOGGLE1, TOGGLE1_ISR, CHANGE);


  digitalWrite(RF_EN, HIGH);  // Enable 5V RF rail

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);

  Wire.setPins(I2C_SDA, I2C_SCL);

  if (!audio_codec.begin()) {
    Serial.println("Failed to intialize audio codec");
    //while(1) {};
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println("Failed to initialize OLED");
    while(1) {};
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);

  if (!pll.init(SI5351_CRYSTAL_LOAD_10PF, 25000000, 0)) {  // Add 3.9 pF caps on either side of oscillator to make load capacitance 12 (10 + 4/2)
    Serial.println("Failed to initialize Si5351");
    while(1) {};
  }

  pll.set_freq(pll_freq, SI5351_CLK0);
  pll.update_status();

  // Print frequency to screen
  display.setCursor(0,0);
  if (pll_freq <= 100000000) {
    display.print(F(" "));
    display.print((float)pll_freq/1000000.0, 3);
    display.print(F(" MHz"));
  }
  else {
    display.print((float)pll_freq/1000000.0, 3);
    display.print(F(" MHz"));
  }
  display.display();

  rot1.attachHalfQuad(ROT1_A, ROT1_B);
  rot2.attachHalfQuad(ROT2_A, ROT2_B);

  rot1.clearCount();
  rot2.clearCount();

}

void loop() {

  rot1_count = rot1.getCount();   // Controls PLL frequency
  rot2_count = rot2.getCount();   // Controls volume?

  if (rot1_count != rot1_prev) {
    pll_freq = (rot1_count - rot1_prev) * freq_step + pll_freq;
    pll.set_freq(pll_freq, SI5351_CLK0);
    Serial.println(pll_freq);
    //rot1_prev = rot1_count;
    rot1_prev = 0;
    rot1.clearCount();

    // Print frequency to screen
    display.setCursor(0,0);
    if (pll_freq <= 100000000) {
      display.print(F(" "));
      display.print((float)pll_freq/1000000.0, 3);
      display.print(F(" MHz"));
    }
    else {
      display.print((float)pll_freq/1000000.0, 3);
      display.print(F(" MHz"));
    }
    display.display();
  }
}

