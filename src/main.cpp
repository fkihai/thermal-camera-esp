#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_AMG88xx.h"
#include "TFT_eSPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#define TFT_DC 2
#define TFT_RST 4
#define TFT_CS 15

Adafruit_ILI9341 Display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define C_BLUE Display.color565(0, 0, 255)
#define C_RED Display.color565(255, 0, 0)
#define C_GREEN Display.color565(0, 255, 0)
#define C_WHITE Display.color565(255, 255, 255)
#define C_BLACK Display.color565(0, 0, 0)
#define C_LTGREY Display.color565(200, 200, 200)
#define C_DKGREY Display.color565(80, 80, 80)
#define C_GREY Display.color565(127, 127, 127)

// Added for measure Temp
boolean measure = true;
uint16_t centerTemp;
unsigned long tempTime = millis();
unsigned long batteryTime = 1;

// start with some initial colors
uint16_t MinTemp = 0;
uint16_t MaxTemp = 80;

// variables for interpolated colors
byte red, green, blue;

// variables for row/column interpolation
byte i, j, k, row, col, incr;
float intPoint, val, a, b, c, d, ii;
byte aLow, aHigh;

// size of a display "pixel"
byte BoxWidth = 3;
byte BoxHeight = 3;
int x, y;
char buf[20];

// variable to toggle the display grid
int ShowGrid = -1;
int DefaultTemp = -1;

// array for the 8 x 8 measured pixels
float pixels[64];

// array for the interpolated array
float HDTemp[80][80];

// create the camara object
Adafruit_AMG88xx ThermalSensor;

int measureBattery()
{
  int adcValue = analogRead(34);
  float volt = (float(adcValue) / 4095.0) * 3.3;
  return volt * 10;
}

void drawBattery()
{
  // Serial.print("volt: ");
  // Serial.println(measureBattery());
  int voltWidth = map(measureBattery(), 21, 25, 10, 28); // 3.50 - 4.12 volt (voltage divider 3.3v)

  // draw battery
  Display.drawRect(198, 304, 30, 10, C_WHITE);
  Display.fillRect(227, 306, 3, 6, C_WHITE);
  Display.fillRect(199, 305, 28, 8, C_BLACK);

  if (voltWidth < 10)
    voltWidth = 10;
  if (voltWidth > 28)
    voltWidth = 28;

  if (voltWidth > 12)
    Display.fillRect(199, 305, voltWidth, 8, C_GREEN);
  else
    Display.fillRect(199, 305, voltWidth, 8, C_RED);
}

uint16_t GetColor(float val)
{

  red = constrain(255.0 / (c - b) * val - ((b * 255.0) / (c - b)), 0, 255);

  if ((val > MinTemp) & (val < a))
  {
    green = constrain(255.0 / (a - MinTemp) * val - (255.0 * MinTemp) / (a - MinTemp), 0, 255);
  }
  else if ((val >= a) & (val <= c))
  {
    green = 255;
  }
  else if (val > c)
  {
    green = constrain(255.0 / (c - d) * val - (d * 255.0) / (c - d), 0, 255);
  }
  else if ((val > d) | (val < a))
  {
    green = 0;
  }

  if (val <= b)
  {
    blue = constrain(255.0 / (a - b) * val - (255.0 * b) / (a - b), 0, 255);
  }
  else if ((val > b) & (val <= d))
  {
    blue = 0;
  }
  else if (val > d)
  {
    blue = constrain(240.0 / (MaxTemp - d) * val - (d * 240.0) / (MaxTemp - d), 0, 240);
  }

  return Display.color565(red, green, blue);
}

void DrawLegend()
{

  j = 0;

  float inc = (MaxTemp - MinTemp) / 220.0;

  for (ii = MinTemp; ii < MaxTemp; ii += inc)
  {
    Display.drawFastVLine(10 + j++, 255, 30, GetColor(ii));
  }

// FAHRENHEIT
#ifdef IMPERIAL
  MinTemp = MinTemp * 1.8 + 32;
  MaxTemp = MaxTemp * 1.8 + 32;
#endif

  int xpos;
  if (MaxTemp > 99)
  {
    xpos = 184;
  }
  else
  {
    xpos = 196;
  };

  Display.setTextSize(2);
  Display.setCursor(0, 235);
  Display.setTextColor(C_WHITE, C_BLACK);
  sprintf(buf, "%2d", MinTemp);
  Display.print(buf);

  Display.setTextSize(2);
  Display.setCursor(xpos, 235);
  Display.setTextColor(C_WHITE, C_BLACK);
  sprintf(buf, " %2d", MaxTemp);
  Display.print(buf);
}

void InterpolateRows()
{

  // interpolate the 8 rows (interpolate the 70 column points between the 8 sensor pixels first)
  for (row = 0; row < 8; row++)
  {
    for (col = 0; col < 70; col++)
    {
      // get the first array point, then the next
      // also need to bump by 8 for the subsequent rows
      aLow = col / 10 + (row * 8);
      aHigh = (col / 10) + 1 + (row * 8);
      // get the amount to interpolate for each of the 10 columns
      // here were doing simple linear interpolation mainly to keep performace high and
      // display is 5-6-5 color palet so fancy interpolation will get lost in low color depth
      intPoint = ((pixels[aHigh] - pixels[aLow]) / 10.0);
      // determine how much to bump each column (basically 0-9)
      incr = col % 10;
      // find the interpolated value
      val = (intPoint * incr) + pixels[aLow];
      // store in the 70 x 70 array
      // since display is pointing away, reverse row to transpose row data
      HDTemp[(7 - row) * 10][col] = val;
    }
  }
}

// interplation function to interpolate 70 columns from the interpolated rows
void InterpolateCols()
{

  // then interpolate the 70 rows between the 8 sensor points
  for (col = 0; col < 70; col++)
  {
    for (row = 0; row < 70; row++)
    {
      // get the first array point, then the next
      // also need to bump by 8 for the subsequent cols
      aLow = (row / 10) * 10;
      aHigh = aLow + 10;
      // get the amount to interpolate for each of the 10 columns
      // here were doing simple linear interpolation mainly to keep performace high and
      // display is 5-6-5 color palet so fancy interpolation will get lost in low color depth
      intPoint = ((HDTemp[aHigh][col] - HDTemp[aLow][col]) / 10.0);
      // determine how much to bump each column (basically 0-9)
      incr = row % 10;
      // find the interpolated value
      val = (intPoint * incr) + HDTemp[aLow][col];
      // store in the 70 x 70 array
      HDTemp[row][col] = val;
    }
  }
}

void drawMeasurement()
{
  // Mark center measurement
  Display.drawCircle(120, 120, 3, ILI9341_WHITE);

  // Measure and print center temperature
  centerTemp = pixels[27];
  if (centerTemp < 0)
    centerTemp = 0;
  if (centerTemp > 80)
    centerTemp = 80;

    // FAHRENHEIT
#ifdef IMPERIAL
  centerTemp = centerTemp * 1.8 + 32;
#endif

  Display.setCursor(10, 300);
  Display.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  Display.setTextSize(2);
  sprintf(buf, "%s:%02d", "Temp", centerTemp);
  Display.print(buf);
}

void DisplayGradient()
{

  // rip through 70 rows
  for (row = 0; row < 70; row++)
  {

    // fast way to draw a non-flicker grid--just make every 10 pixels 2x2 as opposed to 3x3
    // drawing lines after the grid will just flicker too much
    if (ShowGrid < 0)
    {
      BoxWidth = 3;
    }
    else
    {
      if ((row % 10 == 9))
      {
        BoxWidth = 2;
      }
      else
      {
        BoxWidth = 3;
      }
    }
    // then rip through each 70 cols
    for (col = 0; col < 70; col++)
    {

      // fast way to draw a non-flicker grid--just make every 10 pixels 2x2 as opposed to 3x3
      if (ShowGrid < 0)
      {
        BoxHeight = 3;
      }
      else
      {
        if ((col % 10 == 9))
        {
          BoxHeight = 2;
        }
        else
        {
          BoxHeight = 3;
        }
      }
      // finally we can draw each the 70 x 70 points, note the call to get interpolated color
      Display.fillRect((row * 3) + 15, (col * 3) + 15, BoxWidth, BoxHeight, GetColor(HDTemp[row][col]));

      if (row == 36 && col == 36)
      {
        drawMeasurement(); // Draw after center pixels to reduce flickering
      }
    }
  }
}

void Getabcd()
{

  a = MinTemp + (MaxTemp - MinTemp) * 0.2121;
  b = MinTemp + (MaxTemp - MinTemp) * 0.3182;
  c = MinTemp + (MaxTemp - MinTemp) * 0.4242;
  d = MinTemp + (MaxTemp - MinTemp) * 0.8182;
}

void SetTempScale()
{

  if (false)
  { // DefaultTemp < 0) {
    MinTemp = 0;
    MaxTemp = 80;
    Getabcd();
    DrawLegend();
  }
  else
  {
    MinTemp = 255;
    MaxTemp = 0;

    for (i = 0; i < 64; i++)
    {

      // MinTemp = min(MinTemp, (uint16_t)pixels[i]);
      // MaxTemp = max(MaxTemp, (uint16_t)pixels[i]);
      if ((uint16_t)pixels[i] < MinTemp)
      {
        MinTemp = (uint16_t)pixels[i];
      }
      if ((uint16_t)pixels[i] > MaxTemp)
      {
        MaxTemp = (uint16_t)pixels[i];
      }
    }

    MaxTemp = MaxTemp + 5.0;
    MinTemp = MinTemp + 3.0;
    Getabcd();
    DrawLegend();
  }
}

void setup()
{

  Serial.begin(9600);
  Display.begin();

  Display.fillScreen(C_BLACK);
  Display.setRotation(2);

  Display.setTextSize(2);
  Display.setCursor(62, 61);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Thermal");

  Display.setCursor(60, 60);
  Display.setTextColor(C_BLUE);
  Display.print("Thermal");

  Display.setCursor(92, 101);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Camera");

  Display.setCursor(90, 100);
  Display.setTextColor(C_RED);
  Display.print("Camera");

  // let sensor boot up
  bool status = ThermalSensor.begin();
  delay(100);

  // check status and display results
  if (!status)
  {
    while (1)
    {
      Display.setCursor(20, 180);
      Display.setTextColor(C_RED, C_BLACK);
      Display.print("Sensor: FAIL");
      delay(500);
      Display.setCursor(20, 180);
      Display.setTextColor(C_BLACK, C_BLACK);
      Display.print("Sensor: FAIL");
      delay(500);
    }
  }
  else
  {
    Display.setCursor(20, 180);
    Display.setTextColor(C_GREEN, C_BLACK);
    Display.print("Sensor: FOUND");
  }

  // read the camera for initial testing
  ThermalSensor.readPixels(pixels);

  // check status and display results
  if (pixels[0] < 0)
  {
    while (1)
    {
      // Display.setFont(DroidSans_20);
      Display.setCursor(20, 210);
      Display.setTextColor(C_RED, C_BLACK);
      Display.print("Readings: FAIL");
      delay(500);
      // Display.setFont(DroidSans_20);
      Display.setCursor(20, 210);
      Display.setTextColor(C_BLACK, C_BLACK);
      Display.print("Readings: FAIL");
      delay(500);
    }
  }
  else
  {
    // Display.setFont(DroidSans_20);
    Display.setCursor(20, 210);
    Display.setTextColor(C_GREEN, C_BLACK);
    Display.print("Readings: OK");
    delay(2000);
  }

  Display.fillScreen(C_BLACK);
  Getabcd();
  drawBattery();
  DrawLegend();
  Display.fillRect(10, 10, 220, 220, C_WHITE);
}

void loop()
{
  ThermalSensor.readPixels(pixels);
  InterpolateRows();
  InterpolateCols();
  DisplayGradient();

  // Update battery everx 20s
  if (batteryTime < millis())
  {
    drawBattery();
    batteryTime = millis() + 5000;
  }
}