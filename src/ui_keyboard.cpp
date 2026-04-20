#include "ui_keyboard.h"

#include <TFT_eSPI.h>

#include "display_hal.h"
#include "ui_keyboard_layout.h"

static UiKeyboardState *gKeyboardState = nullptr;
static TFT_eSPI_Button gKeyButtons[45];
static int gKeyCenterX[45];
static int gKeyCenterY[45];
static bool gKeyboardButtonsInitialized = false;

// Keyboard placement
static constexpr int KEY_W = 20;
static constexpr int KEY_H = 26;
static constexpr int KEY_GAP = 3;

static constexpr int ROW1_X = 6;
static constexpr int ROW1_Y = 102;

static constexpr int ROW2_X = 6;
static constexpr int ROW2_Y = 132;

static constexpr int ROW3_X = 6;
static constexpr int ROW3_Y = 162;

static constexpr int ROW4_X = 30;
static constexpr int ROW4_Y = 192;

static constexpr int CAPS_CX = 35;
static constexpr int CAPS_CY = 240;
static constexpr int CAPS_W = 58;
static constexpr int CAPS_H = 26;

static constexpr int SPACE_CX = 120;
static constexpr int SPACE_CY = 240;
static constexpr int SPACE_W = 76;
static constexpr int SPACE_H = 26;

static constexpr int DEL_CX = 205;
static constexpr int DEL_CY = 240;
static constexpr int DEL_W = 48;
static constexpr int DEL_H = 26;

static constexpr int MODE_CX = 45;
static constexpr int MODE_CY = 272;
static constexpr int MODE_W = 60;
static constexpr int MODE_H = 26;

static constexpr int OK_CX = 175;
static constexpr int OK_CY = 272;
static constexpr int OK_W = 90;
static constexpr int OK_H = 26;

static String getKeyLabel(int index)
{
    if (!gKeyboardState)
        return "";

    if (index >= 0 && index < row1TextCount)
    {
        return gKeyboardState->symbolMode ? String(row1Sym[index]) : String(row1Text[index]);
    }

    if (index >= 10 && index < 20)
    {
        const int i = index - 10;
        return gKeyboardState->symbolMode ? String(row2Sym[i]) : String(row2Text[i]);
    }

    if (index >= 20 && index < 30)
    {
        const int i = index - 20;
        return gKeyboardState->symbolMode ? String(row3Sym[i]) : String(row3Text[i]);
    }

    if (index >= 30 && index < 37)
    {
        const int i = index - 30;
        return gKeyboardState->symbolMode ? String(row4Sym[i]) : String(row4Text[i]);
    }

    if (index == CAPS_INDEX)
        return "CAPS";

    if (index == SPACE_INDEX)
        return "SPACE";

    if (index == DEL_INDEX)
        return "DEL";

    if (index == OK_INDEX)
        return "OK";

    if (index == MODE_INDEX)
        return gKeyboardState->symbolMode ? "ABC" : "FN";

    return "";
}

static String applyCaseToValue(const String &value)
{
    if (!gKeyboardState)
        return value;

    if (gKeyboardState->symbolMode)
        return value;

    if (value.length() != 1)
        return value;

    char c = value[0];

    if (c >= 'A' && c <= 'Z')
    {
        if (!gKeyboardState->capsLock)
        {
            c = char(c + ('a' - 'A'));
        }
        return String(c);
    }

    if (c >= 'a' && c <= 'z')
    {
        if (gKeyboardState->capsLock)
        {
            c = char(c - ('a' - 'A'));
        }
        return String(c);
    }

    return value;
}

static void initOneButton(int index, TFT_eSPI &tft, int cx, int cy, int w, int h)
{
    gKeyCenterX[index] = cx;
    gKeyCenterY[index] = cy;

    gKeyButtons[index].initButton(
        &tft,
        cx,
        cy,
        w,
        h,
        KEY_BORDER_COLOR,
        KEY_FILL_COLOR,
        KEY_TEXT_COLOR,
        (char *)"",
        1);
}

static void initKeyboardButtons()
{
    if (gKeyboardButtonsInitialized)
        return;

    TFT_eSPI &tft = display();

    int x = ROW1_X;
    for (int i = 0; i < 10; i++)
    {
        initOneButton(i, tft, x + KEY_W / 2, ROW1_Y + KEY_H / 2, KEY_W, KEY_H);
        x += KEY_W + KEY_GAP;
    }

    x = ROW2_X;
    for (int i = 10; i < 20; i++)
    {
        initOneButton(i, tft, x + KEY_W / 2, ROW2_Y + KEY_H / 2, KEY_W, KEY_H);
        x += KEY_W + KEY_GAP;
    }

    x = ROW3_X;
    for (int i = 20; i < 30; i++)
    {
        initOneButton(i, tft, x + KEY_W / 2, ROW3_Y + KEY_H / 2, KEY_W, KEY_H);
        x += KEY_W + KEY_GAP;
    }

    x = ROW4_X;
    for (int i = 30; i < 37; i++)
    {
        initOneButton(i, tft, x + KEY_W / 2, ROW4_Y + KEY_H / 2, KEY_W, KEY_H);
        x += KEY_W + KEY_GAP;
    }

    initOneButton(CAPS_INDEX, tft, CAPS_CX, CAPS_CY, CAPS_W, CAPS_H);
    initOneButton(SPACE_INDEX, tft, SPACE_CX, SPACE_CY, SPACE_W, SPACE_H);
    initOneButton(DEL_INDEX, tft, DEL_CX, DEL_CY, DEL_W, DEL_H);
    initOneButton(MODE_INDEX, tft, MODE_CX, MODE_CY, MODE_W, MODE_H);
    initOneButton(OK_INDEX, tft, OK_CX, OK_CY, OK_W, OK_H);

    gKeyboardButtonsInitialized = true;
}

static void drawKeyLabel(int index, const String &label, bool inverted)
{
    TFT_eSPI &tft = display();

    const uint16_t bg = inverted ? KEY_TEXT_COLOR : KEY_FILL_COLOR;
    const uint16_t fg = inverted ? KEY_FILL_COLOR : KEY_TEXT_COLOR;

    const int cx = gKeyCenterX[index];
    const int cy = gKeyCenterY[index];

    screenLock();
    tft.setTextColor(fg, bg);
    tft.setTextSize(1);
    tft.setCursor(cx - int(label.length()) * 3, cy - 4);
    tft.print(label);
    screenUnlock();
}

static void drawOneKey(int index, bool pressed)
{
    if (!gKeyboardState)
        return;

    const String label = getKeyLabel(index);

    bool selected = false;

    if (index == CAPS_INDEX)
    {
        selected = gKeyboardState->capsLock;
    }
    else if (index == MODE_INDEX)
    {
        selected = gKeyboardState->symbolMode;
    }

    const bool inverted = pressed || selected;

    screenLock();
    gKeyButtons[index].drawButton(inverted);
    screenUnlock();

    drawKeyLabel(index, label, inverted);
}

static void appendTextValue(const String &value)
{
    if (!gKeyboardState || !gKeyboardState->text)
        return;

    if (value.length() == 0)
        return;

    if (gKeyboardState->text->length() >= gKeyboardState->maxLength)
        return;

    *(gKeyboardState->text) += applyCaseToValue(value);
}

static void deleteLastCharacter()
{
    if (!gKeyboardState || !gKeyboardState->text)
        return;

    if (gKeyboardState->text->length() == 0)
        return;

    gKeyboardState->text->remove(gKeyboardState->text->length() - 1);
}

static UiKeyboardAction handleKeyIndex(int index)
{
    if (!gKeyboardState)
        return UI_KEYBOARD_ACTION_NONE;

    if (index >= 0 && index <= 36)
    {
        appendTextValue(getKeyLabel(index));
        return UI_KEYBOARD_ACTION_CHANGED;
    }

    if (index == CAPS_INDEX)
    {
        gKeyboardState->capsLock = !gKeyboardState->capsLock;
        return UI_KEYBOARD_ACTION_CHANGED;
    }

    if (index == SPACE_INDEX)
    {
        appendTextValue(" ");
        return UI_KEYBOARD_ACTION_CHANGED;
    }

    if (index == DEL_INDEX)
    {
        deleteLastCharacter();
        return UI_KEYBOARD_ACTION_CHANGED;
    }

    if (index == MODE_INDEX)
    {
        gKeyboardState->symbolMode = !gKeyboardState->symbolMode;
        return UI_KEYBOARD_ACTION_CHANGED;
    }

    if (index == OK_INDEX)
    {
        return UI_KEYBOARD_ACTION_OK;
    }

    return UI_KEYBOARD_ACTION_NONE;
}

void uiKeyboardInit(UiKeyboardState *state)
{
    gKeyboardState = state;
    initKeyboardButtons();
}

void uiKeyboardDraw()
{
    if (!gKeyboardState)
        return;

    for (int i = 0; i < 37; i++)
    {
        drawOneKey(i, false);
    }

    drawOneKey(CAPS_INDEX, false);
    drawOneKey(SPACE_INDEX, false);
    drawOneKey(DEL_INDEX, false);
    drawOneKey(MODE_INDEX, false);
    drawOneKey(OK_INDEX, false);
}

UiKeyboardAction uiKeyboardHandleTouch(uint16_t x, uint16_t y)
{
    if (!gKeyboardState)
        return UI_KEYBOARD_ACTION_NONE;

    for (int i = 0; i < 45; i++)
    {
        if (!gKeyButtons[i].contains(x, y))
            continue;

        drawOneKey(i, true);
        waitTouchRelease();
        drawOneKey(i, false);

        return handleKeyIndex(i);
    }

    return UI_KEYBOARD_ACTION_NONE;
}