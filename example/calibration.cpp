#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>

// ===================== PINS =====================
// Adjust these if your wiring is different.
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ===================== DISPLAY =====================
static TFT_eSPI tft;
static XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 320;

// ===================== CURRENT TOUCH TRANSFORM =====================
// These values are only used for preview.
// We will replace them after collecting raw calibration points.
static bool swapXY  = true;
static bool invertX = true;
static bool invertY = false;

static int TOUCH_MIN_X = 25;
static int TOUCH_MAX_X = 222;
static int TOUCH_MIN_Y = 31;
static int TOUCH_MAX_Y = 281;

// ==================================================

bool readTouchRawAveraged(int &rawX, int &rawY)
{
    const int samples = 8;
    long sumX = 0;
    long sumY = 0;
    int valid = 0;

    for (int i = 0; i < samples; i++)
    {
        TouchPoint p = ts.getTouch();

        if (!(p.x == 0 && p.y == 0))
        {
            sumX += p.x;
            sumY += p.y;
            valid++;
        }

        delay(2);
    }

    if (valid < 3)
        return false;

    rawX = sumX / valid;
    rawY = sumY / valid;
    return true;
}

bool mapTouchToScreen(int rawInputX, int rawInputY, int &screenX, int &screenY)
{
    int rawX = rawInputX;
    int rawY = rawInputY;

    if (swapXY)
    {
        int tmp = rawX;
        rawX = rawY;
        rawY = tmp;
    }

    long sx = map(rawX, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_W - 1);
    long sy = map(rawY, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_H - 1);

    if (invertX)
        sx = (SCREEN_W - 1) - sx;

    if (invertY)
        sy = (SCREEN_H - 1) - sy;

    sx = constrain(sx, 0, SCREEN_W - 1);
    sy = constrain(sy, 0, SCREEN_H - 1);

    screenX = (int)sx;
    screenY = (int)sy;
    return true;
}

void waitTouchReleaseRaw()
{
    unsigned long start = millis();

    while (millis() - start < 300)
    {
        int rawX, rawY;
        if (!readTouchRawAveraged(rawX, rawY))
            break;
        delay(10);
    }
}

void drawStaticGuide()
{
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 10);
    tft.print("TOUCH CALIBRATION");

    tft.setTextSize(1);
    tft.setCursor(10, 40);
    tft.print("Touch corners + center");

    tft.drawCircle(20, 20, 6, TFT_YELLOW);
    tft.drawCircle(SCREEN_W - 21, 20, 6, TFT_YELLOW);
    tft.drawCircle(20, SCREEN_H - 21, 6, TFT_YELLOW);
    tft.drawCircle(SCREEN_W - 21, SCREEN_H - 21, 6, TFT_YELLOW);
    tft.drawCircle(SCREEN_W / 2, SCREEN_H / 2, 6, TFT_YELLOW);

    tft.setCursor(8, 55);
    tft.print("TL");

    tft.setCursor(SCREEN_W - 22, 55);
    tft.print("TR");

    tft.setCursor(8, SCREEN_H - 35);
    tft.print("BL");

    tft.setCursor(SCREEN_W - 22, SCREEN_H - 35);
    tft.print("BR");

    tft.setCursor((SCREEN_W / 2) - 8, (SCREEN_H / 2) + 14);
    tft.print("C");
}

void drawTouchPreview(int x, int y)
{
    drawStaticGuide();

    tft.drawLine(x - 10, y, x + 10, y, TFT_RED);
    tft.drawLine(x, y - 10, x, y + 10, TFT_RED);
    tft.drawCircle(x, y, 3, TFT_RED);
}

void printInstructions()
{
    Serial.println();
    Serial.println("==== TOUCH CALIBRATION MODE ====");
    Serial.println("Touch these 5 points and write down RAW values:");
    Serial.println("1) Top-left");
    Serial.println("2) Top-right");
    Serial.println("3) Bottom-left");
    Serial.println("4) Bottom-right");
    Serial.println("5) Center");
    Serial.println();
    Serial.println("For each touch, note:");
    Serial.println("RAW: x=?, y=?");
    Serial.println();
    Serial.println("You can also observe the red cross preview.");
    Serial.println("================================");
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    ts.begin();

    drawStaticGuide();
    printInstructions();
}

void loop()
{
    int rawX, rawY;

    if (readTouchRawAveraged(rawX, rawY))
    {
        int screenX = 0;
        int screenY = 0;
        mapTouchToScreen(rawX, rawY, screenX, screenY);

        drawTouchPreview(screenX, screenY);

        Serial.printf(
            "RAW touch -> x=%d y=%d | mapped -> x=%d y=%d\n",
            rawX, rawY, screenX, screenY);

        waitTouchReleaseRaw();
    }

    delay(20);
}