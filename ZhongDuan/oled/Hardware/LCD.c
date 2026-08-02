#include "LCD.h"
#include "OLED_Font.h"

#define LCD_CS_PIN        GPIO_PIN_4
#define LCD_RD_PIN        GPIO_PIN_5
#define LCD_WR_PIN        GPIO_PIN_6
#define LCD_DC_PIN        GPIO_PIN_7
#define LCD_CTRL_PORT     GPIOC
#define LCD_BK_PIN        GPIO_PIN_2
#define LCD_BK_PORT       GPIOD

#define LCD_WIDTH         320U
#define LCD_HEIGHT        240U
#define LCD_FONT_SCALE    2U
#define LCD_LINE_HEIGHT   40U
#define LCD_TEXT_X        8U
#define LCD_MAX_CHARS     19U

#define LCD_CS_HIGH()     (LCD_CTRL_PORT->BSRR = LCD_CS_PIN)
#define LCD_CS_LOW()      (LCD_CTRL_PORT->BRR = LCD_CS_PIN)
#define LCD_RD_HIGH()     (LCD_CTRL_PORT->BSRR = LCD_RD_PIN)
#define LCD_RD_LOW()      (LCD_CTRL_PORT->BRR = LCD_RD_PIN)
#define LCD_WR_HIGH()     (LCD_CTRL_PORT->BSRR = LCD_WR_PIN)
#define LCD_WR_LOW()      (LCD_CTRL_PORT->BRR = LCD_WR_PIN)
#define LCD_DC_HIGH()     (LCD_CTRL_PORT->BSRR = LCD_DC_PIN)
#define LCD_DC_LOW()      (LCD_CTRL_PORT->BRR = LCD_DC_PIN)

static uint16_t g_lcd_id;

typedef struct {
    uint16_t codepoint;
    uint8_t bitmap[32];
} LcdChineseGlyph;

/* Embedded 16x16 glyphs used by the door-lock UI (row-major, MSB first). */
static const LcdChineseGlyph g_chinese_glyphs[] = {
    {0x667AU, {0x18U,0x00U,0x10U,0x00U,0x3FU,0xBCU,0x24U,0x24U,0x7FU,0xA4U,0x0CU,0x24U,0x1BU,0x3CU,0x31U,0x00U,0x2FU,0xF8U,0x08U,0x18U,0x0FU,0xF8U,0x08U,0x18U,0x08U,0x18U,0x0FU,0xF8U,0x08U,0x18U,0x00U,0x00U}},
    {0x80FDU, {0x08U,0x40U,0x18U,0x44U,0x36U,0x58U,0x23U,0x60U,0x7FU,0xC2U,0x00U,0x7EU,0x3FU,0x00U,0x23U,0x40U,0x3FU,0x44U,0x23U,0x78U,0x23U,0x60U,0x3FU,0x40U,0x23U,0x42U,0x27U,0x7EU,0x20U,0x00U,0x00U,0x00U}},
    {0x95E8U, {0x10U,0x00U,0x19U,0xFCU,0x08U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x04U,0x20U,0x1CU,0x00U,0x00U,0x00U,0x00U}},
    {0x9501U, {0x10U,0x20U,0x11U,0x24U,0x3DU,0xACU,0x20U,0xA8U,0x60U,0x20U,0x7DU,0xFCU,0x11U,0x84U,0x11U,0xA4U,0x7DU,0xA4U,0x11U,0xA4U,0x11U,0xA4U,0x14U,0x60U,0x18U,0x4CU,0x13U,0x86U,0x01U,0x00U,0x00U,0x00U}},
    {0x8DDDU, {0x3EU,0xFEU,0x26U,0x80U,0x26U,0x80U,0x3EU,0x80U,0x08U,0xFCU,0x08U,0x84U,0x28U,0x84U,0x2EU,0x84U,0x28U,0xFCU,0x28U,0x80U,0x2AU,0x80U,0x7EU,0x80U,0x40U,0xFEU,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U}},
    {0x79BBU, {0x01U,0x00U,0x7FU,0xFEU,0x00U,0x20U,0x17U,0x60U,0x11U,0xC4U,0x16U,0x34U,0x1FU,0xFCU,0x01U,0x00U,0x01U,0x00U,0x3FU,0xFEU,0x26U,0x26U,0x2CU,0xF6U,0x27U,0x16U,0x20U,0x0EU,0x20U,0x08U,0x00U,0x00U}},
    {0x89D2U, {0x06U,0x00U,0x04U,0x00U,0x0FU,0xF0U,0x18U,0x60U,0x7FU,0xFCU,0x10U,0x84U,0x10U,0x84U,0x1FU,0xFCU,0x10U,0x84U,0x10U,0x84U,0x1FU,0xFCU,0x10U,0x84U,0x20U,0x84U,0x60U,0x9CU,0x00U,0x00U,0x00U,0x00U}},
    {0x5EA6U, {0x00U,0x80U,0x00U,0x80U,0x3FU,0xFCU,0x22U,0x10U,0x22U,0x10U,0x2FU,0xFCU,0x22U,0x10U,0x23U,0xF0U,0x20U,0x00U,0x2FU,0xF8U,0x23U,0x10U,0x21U,0xA0U,0x60U,0xC0U,0x47U,0x3EU,0x08U,0x02U,0x00U,0x00U}},
    {0x8FCEU, {0x21U,0x80U,0x27U,0x3CU,0x26U,0x24U,0x26U,0x24U,0x06U,0x24U,0x76U,0x24U,0x36U,0x24U,0x36U,0x24U,0x36U,0xA4U,0x37U,0x2CU,0x34U,0x20U,0x30U,0x20U,0x3CU,0x00U,0x47U,0xFEU,0x40U,0x00U,0x00U,0x00U}},
    {0x5BBEU, {0x01U,0x80U,0x01U,0x80U,0x7FU,0xFEU,0x60U,0x16U,0x7FU,0xF8U,0x18U,0x00U,0x18U,0x00U,0x1FU,0xFCU,0x18U,0x20U,0x18U,0x20U,0x7FU,0xFEU,0x0CU,0x20U,0x18U,0x18U,0x70U,0x0CU,0x00U,0x00U,0x00U,0x00U}},
    {0x533AU, {0x3FU,0xFEU,0x20U,0x00U,0x20U,0x18U,0x26U,0x10U,0x21U,0xB0U,0x20U,0xE0U,0x20U,0xE0U,0x20U,0xB0U,0x21U,0x18U,0x26U,0x0CU,0x24U,0x00U,0x20U,0x00U,0x3FU,0xFEU,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U}},
    {0x89E6U, {0x18U,0x10U,0x10U,0x10U,0x3EU,0x10U,0x64U,0xFCU,0x7EU,0x94U,0x2AU,0x94U,0x2AU,0x94U,0x3EU,0x94U,0x2AU,0xFCU,0x3EU,0x90U,0x2AU,0x14U,0x2AU,0x12U,0x6AU,0x7EU,0x46U,0xC2U,0x00U,0x00U,0x00U,0x00U}},
    {0x6478U, {0x10U,0x90U,0x10U,0x90U,0x13U,0xFEU,0x10U,0x90U,0x7DU,0xFCU,0x11U,0x04U,0x11U,0xFCU,0x19U,0x04U,0x71U,0xFCU,0x10U,0x20U,0x13U,0xFEU,0x10U,0x50U,0x10U,0x98U,0x77U,0x06U,0x00U,0x00U,0x00U,0x00U}},
    {0x5C31U, {0x18U,0x20U,0x0CU,0x28U,0x7FU,0xA4U,0x00U,0x26U,0x00U,0x20U,0x3FU,0xFEU,0x23U,0x20U,0x23U,0x38U,0x3FU,0x38U,0x2AU,0x38U,0x2BU,0x78U,0x69U,0x58U,0x48U,0xDAU,0x19U,0x9EU,0x10U,0x00U,0x00U,0x00U}},
    {0x7EEAU, {0x10U,0x40U,0x10U,0x44U,0x31U,0xF4U,0x26U,0x48U,0x64U,0x58U,0x7BU,0xFFU,0x08U,0x20U,0x11U,0xFCU,0x21U,0x84U,0x7FU,0x84U,0x03U,0xFCU,0x05U,0x84U,0x7DU,0xFCU,0x41U,0xFCU,0x01U,0x80U,0x00U,0x00U}}
};

static void LCD_DataOutput(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_All;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void LCD_DataInput(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_All;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void LCD_WriteCommand(uint16_t command)
{
    LCD_CS_LOW();
    LCD_DC_LOW();
    LCD_RD_HIGH();
    GPIOB->ODR = command;
    LCD_WR_LOW();
    __NOP();
    LCD_WR_HIGH();
    LCD_CS_HIGH();
}

static void LCD_WriteData(uint16_t data)
{
    LCD_CS_LOW();
    LCD_DC_HIGH();
    LCD_RD_HIGH();
    GPIOB->ODR = data;
    LCD_WR_LOW();
    __NOP();
    LCD_WR_HIGH();
    LCD_CS_HIGH();
}

static uint16_t LCD_ReadData(void)
{
    uint16_t data;
    LCD_DataInput();
    LCD_DC_HIGH();
    LCD_WR_HIGH();
    LCD_CS_LOW();
    LCD_RD_LOW();
    __NOP();
    __NOP();
    data = (uint16_t)GPIOB->IDR;
    LCD_RD_HIGH();
    LCD_CS_HIGH();
    LCD_DataOutput();
    return data;
}

static void LCD_WriteRegister(uint16_t command,
                              const uint8_t *data, uint8_t length)
{
    uint8_t i;
    LCD_WriteCommand(command);
    for (i = 0U; i < length; i++) {
        LCD_WriteData(data[i]);
    }
}

static uint16_t LCD_ReadId(void)
{
    uint16_t id;

    LCD_WriteCommand(0x04U);
    (void)LCD_ReadData();
    (void)LCD_ReadData();
    id = (uint16_t)(LCD_ReadData() << 8);
    id |= (uint16_t)(LCD_ReadData() & 0x00FFU);
    if (id == 0x8552U) {
        return id;
    }

    LCD_WriteCommand(0xD3U);
    (void)LCD_ReadData();
    (void)LCD_ReadData();
    id = (uint16_t)(LCD_ReadData() << 8);
    id |= (uint16_t)(LCD_ReadData() & 0x00FFU);
    return (id == 0x9341U) ? id : 0U;
}

static void LCD_InitIli9341(void)
{
    static const uint8_t cf[] = {0x00U, 0x81U, 0x30U};
    static const uint8_t ed[] = {0x64U, 0x03U, 0x12U, 0x81U};
    static const uint8_t e8[] = {0x85U, 0x10U, 0x78U};
    static const uint8_t cb[] = {0x39U, 0x2CU, 0x00U, 0x34U, 0x02U};
    static const uint8_t ea[] = {0x00U, 0x00U};
    static const uint8_t b1[] = {0x00U, 0x1BU};
    static const uint8_t b6[] = {0x0AU, 0xA2U};
    static const uint8_t c5[] = {0x45U, 0x45U};
    static const uint8_t gamma_pos[] = {
        0x0FU, 0x26U, 0x24U, 0x0BU, 0x0EU, 0x09U, 0x54U, 0xA8U,
        0x46U, 0x0CU, 0x17U, 0x09U, 0x0FU, 0x07U, 0x00U
    };
    static const uint8_t gamma_neg[] = {
        0x00U, 0x19U, 0x1BU, 0x04U, 0x10U, 0x07U, 0x2AU, 0x47U,
        0x39U, 0x03U, 0x06U, 0x06U, 0x30U, 0x38U, 0x0FU
    };
    uint8_t value;

    LCD_WriteRegister(0xCFU, cf, sizeof(cf));
    LCD_WriteRegister(0xEDU, ed, sizeof(ed));
    LCD_WriteRegister(0xE8U, e8, sizeof(e8));
    LCD_WriteRegister(0xCBU, cb, sizeof(cb));
    value = 0x20U; LCD_WriteRegister(0xF7U, &value, 1U);
    LCD_WriteRegister(0xEAU, ea, sizeof(ea));
    LCD_WriteRegister(0xB1U, b1, sizeof(b1));
    LCD_WriteRegister(0xB6U, b6, sizeof(b6));
    value = 0x35U; LCD_WriteRegister(0xC0U, &value, 1U);
    value = 0x11U; LCD_WriteRegister(0xC1U, &value, 1U);
    LCD_WriteRegister(0xC5U, c5, sizeof(c5));
    value = 0xA2U; LCD_WriteRegister(0xC7U, &value, 1U);
    value = 0x00U; LCD_WriteRegister(0xF2U, &value, 1U);
    value = 0x01U; LCD_WriteRegister(0x26U, &value, 1U);
    LCD_WriteRegister(0xE0U, gamma_pos, sizeof(gamma_pos));
    LCD_WriteRegister(0xE1U, gamma_neg, sizeof(gamma_neg));
    /* Wildfire scan mode 5: landscape, rotated 180 degrees from mode 3. */
    value = 0xA8U; LCD_WriteRegister(0x36U, &value, 1U);
    value = 0x55U; LCD_WriteRegister(0x3AU, &value, 1U);
    LCD_WriteCommand(0x11U);
    HAL_Delay(120U);
    LCD_WriteCommand(0x29U);
}

static void LCD_InitSt7789(void)
{
    static const uint8_t cf[] = {0x00U, 0xC1U, 0x30U};
    static const uint8_t ed[] = {0x64U, 0x03U, 0x12U, 0x81U};
    static const uint8_t e8[] = {0x85U, 0x10U, 0x78U};
    static const uint8_t cb[] = {0x39U, 0x2CU, 0x00U, 0x34U, 0x02U};
    static const uint8_t ea[] = {0x00U, 0x00U};
    static const uint8_t b1[] = {0x00U, 0x17U};
    static const uint8_t b6[] = {0x0AU, 0xA2U};
    static const uint8_t c5[] = {0x2DU, 0x33U};
    static const uint8_t f6[] = {0x01U, 0x30U};
    static const uint8_t gamma_pos[] = {
        0xD0U, 0x00U, 0x02U, 0x07U, 0x0BU, 0x1AU, 0x31U,
        0x54U, 0x40U, 0x29U, 0x12U, 0x12U, 0x12U, 0x17U
    };
    static const uint8_t gamma_neg[] = {
        0xD0U, 0x00U, 0x02U, 0x07U, 0x05U, 0x25U, 0x2DU,
        0x44U, 0x45U, 0x1CU, 0x18U, 0x16U, 0x1CU, 0x1DU
    };
    uint8_t value;

    LCD_WriteRegister(0xCFU, cf, sizeof(cf));
    LCD_WriteRegister(0xEDU, ed, sizeof(ed));
    LCD_WriteRegister(0xE8U, e8, sizeof(e8));
    LCD_WriteRegister(0xCBU, cb, sizeof(cb));
    value = 0x20U; LCD_WriteRegister(0xF7U, &value, 1U);
    LCD_WriteRegister(0xEAU, ea, sizeof(ea));
    value = 0x21U; LCD_WriteRegister(0xC0U, &value, 1U);
    value = 0x11U; LCD_WriteRegister(0xC1U, &value, 1U);
    LCD_WriteRegister(0xC5U, c5, sizeof(c5));
    /* Wildfire scan mode 5: landscape, rotated 180 degrees from mode 3. */
    value = 0xA0U; LCD_WriteRegister(0x36U, &value, 1U);
    value = 0x55U; LCD_WriteRegister(0x3AU, &value, 1U);
    LCD_WriteRegister(0xB1U, b1, sizeof(b1));
    LCD_WriteRegister(0xB6U, b6, sizeof(b6));
    LCD_WriteRegister(0xF6U, f6, sizeof(f6));
    value = 0x00U; LCD_WriteRegister(0xF2U, &value, 1U);
    value = 0x01U; LCD_WriteRegister(0x26U, &value, 1U);
    LCD_WriteRegister(0xE0U, gamma_pos, sizeof(gamma_pos));
    LCD_WriteRegister(0xE1U, gamma_neg, sizeof(gamma_neg));
    LCD_WriteCommand(0x11U);
    HAL_Delay(120U);
    LCD_WriteCommand(0x29U);
}

static void LCD_SetWindow(uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height)
{
    uint8_t data[4];
    uint16_t end;

    end = (uint16_t)(x + width - 1U);
    data[0] = (uint8_t)(x >> 8);
    data[1] = (uint8_t)x;
    data[2] = (uint8_t)(end >> 8);
    data[3] = (uint8_t)end;
    LCD_WriteRegister(0x2AU, data, sizeof(data));

    end = (uint16_t)(y + height - 1U);
    data[0] = (uint8_t)(y >> 8);
    data[1] = (uint8_t)y;
    data[2] = (uint8_t)(end >> 8);
    data[3] = (uint8_t)end;
    LCD_WriteRegister(0x2BU, data, sizeof(data));
    LCD_WriteCommand(0x2CU);
}

static void LCD_BeginPixelWrite(void)
{
    LCD_CS_LOW();
    LCD_DC_HIGH();
    LCD_RD_HIGH();
}

static void LCD_StreamPixel(uint16_t color)
{
    GPIOB->ODR = color;
    LCD_WR_LOW();
    __NOP();
    LCD_WR_HIGH();
}

static void LCD_EndPixelWrite(void)
{
    LCD_CS_HIGH();
}

static void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width,
                         uint16_t height, uint16_t color)
{
    uint32_t count = (uint32_t)width * height;
    LCD_SetWindow(x, y, width, height);
    LCD_BeginPixelWrite();
    while (count-- != 0U) {
        LCD_StreamPixel(color);
    }
    LCD_EndPixelWrite();
}

static void LCD_DrawChar(uint16_t x, uint16_t y, char character,
                         uint16_t foreground, uint16_t background)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;
    uint8_t sy;
    uint8_t sx;

    if ((character < ' ') || (character > '~')) {
        character = '?';
    }
    glyph = OLED_F8x16[character - ' '];
    LCD_SetWindow(x, y, 8U * LCD_FONT_SCALE, 16U * LCD_FONT_SCALE);
    LCD_BeginPixelWrite();
    for (row = 0U; row < 16U; row++) {
        for (sy = 0U; sy < LCD_FONT_SCALE; sy++) {
            for (col = 0U; col < 8U; col++) {
                uint8_t byte = glyph[col + ((row >= 8U) ? 8U : 0U)];
                uint16_t color = ((byte & (1U << (row & 7U))) != 0U) ?
                                 foreground : background;
                for (sx = 0U; sx < LCD_FONT_SCALE; sx++) {
                    LCD_StreamPixel(color);
                }
            }
        }
    }
    LCD_EndPixelWrite();
}

static const uint8_t *LCD_FindChineseGlyph(uint16_t codepoint)
{
    uint8_t i;
    for (i = 0U;
         i < (uint8_t)(sizeof(g_chinese_glyphs) / sizeof(g_chinese_glyphs[0]));
         i++) {
        if (g_chinese_glyphs[i].codepoint == codepoint) {
            return g_chinese_glyphs[i].bitmap;
        }
    }
    return NULL;
}

static void LCD_DrawChinese(uint16_t x, uint16_t y, const uint8_t *glyph,
                            uint16_t foreground, uint16_t background)
{
    uint8_t row;
    uint8_t column;
    uint8_t sy;
    uint8_t sx;

    LCD_SetWindow(x, y, 32U, 32U);
    LCD_BeginPixelWrite();
    for (row = 0U; row < 16U; row++) {
        uint16_t bits = (uint16_t)((uint16_t)glyph[row * 2U] << 8) |
                        glyph[row * 2U + 1U];
        for (sy = 0U; sy < 2U; sy++) {
            for (column = 0U; column < 16U; column++) {
                uint16_t color = ((bits & (uint16_t)(0x8000U >> column)) != 0U) ?
                                 foreground : background;
                for (sx = 0U; sx < 2U; sx++) {
                    LCD_StreamPixel(color);
                }
            }
        }
    }
    LCD_EndPixelWrite();
}

static uint16_t LCD_DecodeUtf8(const uint8_t **text)
{
    const uint8_t *p = *text;
    uint16_t codepoint;

    if (p[0] < 0x80U) {
        *text = p + 1;
        return p[0];
    }
    if (((p[0] & 0xF0U) == 0xE0U) &&
        ((p[1] & 0xC0U) == 0x80U) && ((p[2] & 0xC0U) == 0x80U)) {
        codepoint = (uint16_t)(((uint16_t)(p[0] & 0x0FU) << 12) |
                              ((uint16_t)(p[1] & 0x3FU) << 6) |
                              (uint16_t)(p[2] & 0x3FU));
        *text = p + 3;
        return codepoint;
    }
    *text = p + 1;
    return (uint16_t)'?';
}

uint16_t LCD_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    LCD_DataOutput();
    GPIOB->ODR = 0xFFFFU;

    gpio.Pin = LCD_CS_PIN | LCD_RD_PIN | LCD_WR_PIN | LCD_DC_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_CTRL_PORT, &gpio);
    LCD_CS_HIGH();
    LCD_RD_HIGH();
    LCD_WR_HIGH();
    LCD_DC_HIGH();

    gpio.Pin = LCD_BK_PIN;
    HAL_GPIO_Init(LCD_BK_PORT, &gpio);
    HAL_GPIO_WritePin(LCD_BK_PORT, LCD_BK_PIN, GPIO_PIN_SET);

    HAL_Delay(50U);
    g_lcd_id = LCD_ReadId();
    if (g_lcd_id == 0x8552U) {
        LCD_InitSt7789();
    } else {
        LCD_InitIli9341();
        if (g_lcd_id == 0U) {
            g_lcd_id = 0x9341U;
        }
    }
    HAL_GPIO_WritePin(LCD_BK_PORT, LCD_BK_PIN, GPIO_PIN_RESET);
    LCD_Clear(LCD_COLOR_BLACK);
    return g_lcd_id;
}

void LCD_Clear(uint16_t color)
{
    LCD_FillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD_ShowLine(uint8_t line, const char *text,
                  uint16_t foreground, uint16_t background)
{
    uint16_t y;
    uint8_t i;

    if ((line >= 6U) || (text == NULL)) {
        return;
    }
    y = (uint16_t)line * LCD_LINE_HEIGHT;
    LCD_FillRect(0U, y, LCD_WIDTH, LCD_LINE_HEIGHT, background);
    for (i = 0U; (i < LCD_MAX_CHARS) && (text[i] != '\0'); i++) {
        LCD_DrawChar((uint16_t)(LCD_TEXT_X + i * 16U),
                     (uint16_t)(y + 4U), text[i], foreground, background);
    }
}

void LCD_ShowUtf8Line(uint8_t line, const char *text,
                      uint16_t foreground, uint16_t background)
{
    const uint8_t *cursor = (const uint8_t *)text;
    uint16_t x = LCD_TEXT_X;
    uint16_t y;

    if ((line >= 6U) || (text == NULL)) {
        return;
    }
    y = (uint16_t)line * LCD_LINE_HEIGHT;
    LCD_FillRect(0U, y, LCD_WIDTH, LCD_LINE_HEIGHT, background);

    while (*cursor != 0U) {
        uint16_t codepoint = LCD_DecodeUtf8(&cursor);
        if (codepoint < 0x80U) {
            if ((x + 16U) > LCD_WIDTH) {
                break;
            }
            LCD_DrawChar(x, (uint16_t)(y + 4U), (char)codepoint,
                         foreground, background);
            x = (uint16_t)(x + 16U);
        } else {
            const uint8_t *glyph = LCD_FindChineseGlyph(codepoint);
            if ((x + 32U) > LCD_WIDTH) {
                break;
            }
            if (glyph != NULL) {
                LCD_DrawChinese(x, (uint16_t)(y + 4U), glyph,
                                foreground, background);
                x = (uint16_t)(x + 32U);
            } else {
                LCD_DrawChar(x, (uint16_t)(y + 4U), '?',
                             foreground, background);
                x = (uint16_t)(x + 16U);
            }
        }
    }
}
