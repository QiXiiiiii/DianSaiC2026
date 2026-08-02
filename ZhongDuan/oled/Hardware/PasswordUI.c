#include "PasswordUI.h"
#include "LCD.h"
#include "Touch.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"
#include <stdio.h>
#include <string.h>

#define PASSWORD_FLASH_PAGE       0x0803F800UL
#define PASSWORD_FLASH_MAGIC      0x50494E31UL
#define PASSWORD_FLASH_CHECK_XOR  0xA55A3CC3UL
#define PASSWORD_DEFAULT          1234U
#define PASSWORD_DIGITS           4U
#define MAIN_BUTTON_RAW_Y_MIN     2800U
#define MAIN_BUTTON_SCREEN_X_SPLIT 220U
#define PASSWORD_RAW_X_MIN        500U
#define PASSWORD_RAW_X_MAX        2200U
#define ZIGBEE_RAW_X_MIN          2400U
#define ZIGBEE_RAW_X_MAX          3900U
#define TOUCH_RELEASE_STABLE_MS   150U
#define KEYPAD_TOUCH_LOCKOUT_MS   1000U
#define KEYPAD_RAW_X_MIN          350U
#define KEYPAD_RAW_X_BOUNDARY_1   1350U
#define KEYPAD_RAW_X_BOUNDARY_2   2550U
#define KEYPAD_RAW_X_MAX          3700U
#define KEYPAD_RAW_Y_MIN          650U
#define KEYPAD_RAW_Y_BOUNDARY_1   1320U
#define KEYPAD_RAW_Y_BOUNDARY_2   1940U
#define KEYPAD_RAW_Y_BOUNDARY_3   2520U
#define KEYPAD_RAW_Y_MAX          3250U

typedef enum {
    PASSWORD_PAGE_MAIN = 0,
    PASSWORD_PAGE_ENTER,
    PASSWORD_PAGE_NEW_FIRST,
    PASSWORD_PAGE_NEW_CONFIRM
} PasswordPage;

static PasswordPage g_page = PASSWORD_PAGE_MAIN;
static uint16_t g_password = PASSWORD_DEFAULT;
static uint16_t g_entry;
static uint16_t g_first_new_password;
static uint8_t g_entry_digits;
static uint8_t g_touch_latched;
static uint8_t g_main_touch_latched;
static uint8_t g_touch_feedback;
static uint8_t g_press_pending;
static uint8_t g_release_pending;
static uint32_t g_press_started_ms;
static uint32_t g_release_started_ms;
static uint32_t g_keypad_lockout_until_ms;

volatile uint16_t g_password_debug_x;
volatile uint16_t g_password_debug_y;
volatile uint16_t g_password_debug_raw_x;
volatile uint16_t g_password_debug_raw_y;
volatile int8_t g_password_debug_digit = -1;
volatile uint32_t g_password_debug_count;

static uint8_t PasswordStorageValid(void)
{
    const uint32_t *storage = (const uint32_t *)PASSWORD_FLASH_PAGE;
    return ((storage[0] == PASSWORD_FLASH_MAGIC) &&
            (storage[1] <= 9999U) &&
            (storage[2] == (storage[0] ^ storage[1] ^
                            PASSWORD_FLASH_CHECK_XOR))) ? 1U : 0U;
}

static uint8_t PasswordStorageSave(uint16_t password)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = PASSWORD_FLASH_PAGE;
    erase.NbPages = 1U;

    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    if (status == HAL_OK) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   PASSWORD_FLASH_PAGE,
                                   PASSWORD_FLASH_MAGIC);
    }
    if (status == HAL_OK) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   PASSWORD_FLASH_PAGE + 4U, password);
    }
    if (status == HAL_OK) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   PASSWORD_FLASH_PAGE + 8U,
                                   PASSWORD_FLASH_MAGIC ^ password ^
                                   PASSWORD_FLASH_CHECK_XOR);
    }
    HAL_FLASH_Lock();
    return (status == HAL_OK) ? 1U : 0U;
}

static void PasswordEntryReset(void)
{
    g_entry = 0U;
    g_entry_digits = 0U;
}

static void PasswordShowHeader(const char *title)
{
    char line[20];
    uint8_t position;

    snprintf(line, sizeof(line), "%-10s ", title);
    position = (uint8_t)strlen(line);
    if (g_entry_digits != 0U) {
        (void)snprintf(&line[position], sizeof(line) - position,
                       "%0*u", (int)g_entry_digits,
                       (unsigned int)g_entry);
    } else {
        line[position] = '\0';
    }
    LCD_ShowLine(0, line, LCD_COLOR_WHITE, LCD_COLOR_NAVY);
}

static void PasswordShowKeypad(const char *title, uint8_t can_change)
{
    PasswordShowHeader(title);
    LCD_ShowLine(1, " [1]    [2]    [3]", LCD_COLOR_WHITE, LCD_COLOR_DARKGREY);
    LCD_ShowLine(2, " [4]    [5]    [6]", LCD_COLOR_WHITE, LCD_COLOR_DARKGREY);
    LCD_ShowLine(3, " [7]    [8]    [9]", LCD_COLOR_WHITE, LCD_COLOR_DARKGREY);
    LCD_ShowLine(4, " [CLR]  [0]    [OK]", LCD_COLOR_YELLOW, LCD_COLOR_BLUE);
    LCD_ShowLine(5, can_change ? " [BACK]  [CHANGE]" : " [BACK]",
                 LCD_COLOR_CYAN, LCD_COLOR_BLACK);
}

static void PasswordShowMessage(const char *message, uint16_t color)
{
    LCD_ShowLine(0, message, color, LCD_COLOR_NAVY);
}

void PasswordUI_ShowMainButtons(uint8_t password_unlocked)
{
    LCD_ShowLine(5,
                 password_unlocked ? "[PASSWORD:ON] [ZIG]" :
                                     "[PASSWORD]    [ZIG]",
                 password_unlocked ? LCD_COLOR_GREEN : LCD_COLOR_CYAN,
                 LCD_COLOR_BLACK);
}

void PasswordUI_Init(uint8_t password_unlocked)
{
    if (PasswordStorageValid()) {
        g_password = (uint16_t)(*((const uint32_t *)(PASSWORD_FLASH_PAGE + 4U)));
    } else {
        g_password = PASSWORD_DEFAULT;
    }
    g_page = PASSWORD_PAGE_MAIN;
    PasswordEntryReset();
    PasswordUI_ShowMainButtons(password_unlocked);
}

uint8_t PasswordUI_IsActive(void)
{
    return (g_page == PASSWORD_PAGE_MAIN) ? 0U : 1U;
}

static int8_t PasswordRawColumn(uint16_t raw_x)
{
    if ((raw_x < KEYPAD_RAW_X_MIN) || (raw_x > KEYPAD_RAW_X_MAX)) {
        return -1;
    }
    if (raw_x < KEYPAD_RAW_X_BOUNDARY_1) {
        return 0;
    }
    if (raw_x < KEYPAD_RAW_X_BOUNDARY_2) {
        return 1;
    }
    return 2;
}

static int8_t PasswordDigitAtRaw(uint16_t raw_x, uint16_t raw_y)
{
    int8_t column = PasswordRawColumn(raw_x);
    uint8_t row;

    if ((column < 0) || (raw_y < KEYPAD_RAW_Y_MIN) ||
        (raw_y > KEYPAD_RAW_Y_MAX)) {
        return -1;
    }
    if (raw_y < KEYPAD_RAW_Y_BOUNDARY_1) {
        row = 0U;
    } else if (raw_y < KEYPAD_RAW_Y_BOUNDARY_2) {
        row = 1U;
    } else if (raw_y < KEYPAD_RAW_Y_BOUNDARY_3) {
        row = 2U;
    } else {
        row = 3U;
    }
    if (row < 3U) {
        return (int8_t)(row * 3U + (uint8_t)column + 1U);
    }
    if (column == 1) {
        return 0;
    }
    return -1;
}

static void PasswordAppendDigit(uint8_t digit)
{
    if (g_entry_digits < PASSWORD_DIGITS) {
        g_entry = (uint16_t)(g_entry * 10U + digit);
        g_entry_digits++;
    }
}

static void PasswordReturnToMain(uint8_t password_unlocked)
{
    g_page = PASSWORD_PAGE_MAIN;
    PasswordEntryReset();
    LCD_Clear(LCD_COLOR_BLACK);
    LCD_ShowLine(0, "DIGITAL DOORLOCK", LCD_COLOR_WHITE, LCD_COLOR_NAVY);
    PasswordUI_ShowMainButtons(password_unlocked);
}

PasswordUiEvent PasswordUI_Task(uint8_t password_unlocked)
{
    uint16_t x;
    uint16_t y;
    uint16_t raw_x;
    uint16_t raw_y;
    int8_t digit;
    uint8_t column;
    uint32_t now = HAL_GetTick();

    /* Main-page buttons use their own original raw-coordinate touch path. */
    if (g_page == PASSWORD_PAGE_MAIN) {
        if (!Touch_IsPressed()) {
            g_main_touch_latched = 0U;
            g_touch_latched = 0U;
            g_release_pending = 0U;
            if (g_touch_feedback) {
                PasswordUI_ShowMainButtons(password_unlocked);
                g_touch_feedback = 0U;
            }
            return PASSWORD_UI_EVENT_NONE;
        }
        if (g_main_touch_latched) {
            return PASSWORD_UI_EVENT_NONE;
        }
        HAL_Delay(60U);
        if (!Touch_Read(&x, &y)) {
            return PASSWORD_UI_EVENT_NONE;
        }
        g_main_touch_latched = 1U;
        g_touch_latched = 1U;
        if (y < 200U) {
            return PASSWORD_UI_EVENT_NONE;
        }
        if (x < MAIN_BUTTON_SCREEN_X_SPLIT) {
            g_page = PASSWORD_PAGE_ENTER;
            PasswordEntryReset();
            PasswordShowKeypad("PASSWORD", password_unlocked);
            return PASSWORD_UI_EVENT_NONE;
        }
        return PASSWORD_UI_EVENT_ZIGBEE;
    }

    if (!Touch_IsPressed()) {
        g_press_pending = 0U;
        if (g_touch_latched) {
            if (!g_release_pending) {
                g_release_pending = 1U;
                g_release_started_ms = now;
            }
            if ((uint32_t)(now - g_release_started_ms) < TOUCH_RELEASE_STABLE_MS) {
                return PASSWORD_UI_EVENT_NONE;
            }
            g_touch_latched = 0U;
            g_release_pending = 0U;
        }
        if (g_touch_feedback && (g_page == PASSWORD_PAGE_MAIN)) {
            PasswordUI_ShowMainButtons(password_unlocked);
            g_touch_feedback = 0U;
        }
        return PASSWORD_UI_EVENT_NONE;
    }
    g_release_pending = 0U;
    if (g_touch_latched) {
        return PASSWORD_UI_EVENT_NONE;
    }
    if ((g_page != PASSWORD_PAGE_MAIN) &&
        ((int32_t)(now - g_keypad_lockout_until_ms) < 0)) {
        /* Ignore IRQ glitches and repeated samples from the same physical tap. */
        g_touch_latched = 1U;
        return PASSWORD_UI_EVENT_NONE;
    }
    if (!g_press_pending) {
        g_press_pending = 1U;
        g_press_started_ms = now;
        return PASSWORD_UI_EVENT_NONE;
    }
    if ((uint32_t)(now - g_press_started_ms) < 120U) {
        return PASSWORD_UI_EVENT_NONE;
    }
    g_press_pending = 0U;
    if (!Touch_Read(&x, &y)) {
        LCD_ShowLine(5, "TOUCH READ FAILED",
                     LCD_COLOR_RED, LCD_COLOR_BLACK);
        g_touch_latched = 1U;
        g_touch_feedback = 1U;
        return PASSWORD_UI_EVENT_NONE;
    }
    g_touch_latched = 1U;

    Touch_GetLastRaw(&raw_x, &raw_y);
    digit = PasswordDigitAtRaw(raw_x, raw_y);
    g_password_debug_raw_x = raw_x;
    g_password_debug_raw_y = raw_y;
    g_password_debug_x = x;
    g_password_debug_y = y;
    g_password_debug_digit = digit;
    g_password_debug_count++;
    g_keypad_lockout_until_ms = now + KEYPAD_TOUCH_LOCKOUT_MS;
    if (digit >= 0) {
        PasswordAppendDigit((uint8_t)digit);
        PasswordShowKeypad((g_page == PASSWORD_PAGE_ENTER) ? "PASSWORD" :
                           ((g_page == PASSWORD_PAGE_NEW_FIRST) ? "NEW PIN" :
                                                                 "AGAIN"),
                           (g_page == PASSWORD_PAGE_ENTER) && password_unlocked);
        return PASSWORD_UI_EVENT_NONE;
    }

    {
        int8_t raw_column = PasswordRawColumn(raw_x);
        column = (raw_column < 0) ? 3U : (uint8_t)raw_column;
    }
    if ((raw_y >= KEYPAD_RAW_Y_BOUNDARY_3) &&
        (raw_y <= KEYPAD_RAW_Y_MAX)) {
        if (column == 0U) {
            PasswordEntryReset();
            PasswordShowKeypad((g_page == PASSWORD_PAGE_ENTER) ? "PASSWORD" :
                               ((g_page == PASSWORD_PAGE_NEW_FIRST) ? "NEW PIN" :
                                                                     "AGAIN"),
                               (g_page == PASSWORD_PAGE_ENTER) && password_unlocked);
        } else if (column == 2U) {
            if (g_entry_digits != PASSWORD_DIGITS) {
                PasswordShowMessage("ENTER 4 DIGITS", LCD_COLOR_YELLOW);
            } else if (g_page == PASSWORD_PAGE_ENTER) {
                if (g_entry == g_password) {
                    PasswordReturnToMain((uint8_t)!password_unlocked);
                    return PASSWORD_UI_EVENT_TOGGLE_LOCK;
                }
                PasswordEntryReset();
                PasswordShowMessage("WRONG PASSWORD", LCD_COLOR_RED);
            } else if (g_page == PASSWORD_PAGE_NEW_FIRST) {
                g_first_new_password = g_entry;
                g_page = PASSWORD_PAGE_NEW_CONFIRM;
                PasswordEntryReset();
                PasswordShowKeypad("AGAIN", 0U);
            } else if (g_entry != g_first_new_password) {
                g_page = PASSWORD_PAGE_NEW_FIRST;
                PasswordEntryReset();
                PasswordShowMessage("PIN NOT SAME", LCD_COLOR_RED);
            } else if (PasswordStorageSave(g_entry)) {
                g_password = g_entry;
                PasswordReturnToMain(password_unlocked);
            } else {
                PasswordEntryReset();
                PasswordShowMessage("FLASH SAVE FAILED", LCD_COLOR_RED);
            }
        }
        return PASSWORD_UI_EVENT_NONE;
    }

    if (y >= 200U) {
        if (x < 160U) {
            PasswordReturnToMain(password_unlocked);
        } else if ((g_page == PASSWORD_PAGE_ENTER) && password_unlocked) {
            g_page = PASSWORD_PAGE_NEW_FIRST;
            PasswordEntryReset();
            PasswordShowKeypad("NEW PIN", 0U);
        }
    }
    return PASSWORD_UI_EVENT_NONE;
}
