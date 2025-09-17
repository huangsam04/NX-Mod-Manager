#include "nvg_util.hpp"
#include <cstddef>
#include <cstdio>
#include <array>
#include <utility>
#include <algorithm>
#include <cstring>

namespace tj::gfx {

static constexpr std::array<std::pair<Button, const char*>, 31> buttons = {{
    {Button::POWER, "\uE0B8"},
    {Button::A, "\uE0E0"},
    {Button::B, "\uE0E1"},
    {Button::X, "\uE0E2"},
    {Button::Y, "\uE0E3"},
    {Button::L, "\uE0E4"},
    {Button::R, "\uE0E5"},
    {Button::ZL, "\uE0E6"},
    {Button::ZR, "\uE0E7"},
    {Button::SL, "\uE0E8"},
    {Button::SR, "\uE0E9"},
    {Button::UP, "\uE0EB"},
    {Button::DOWN, "\uE0EC"},
    {Button::LEFT, "\uE0ED"},
    {Button::RIGHT, "\uE0EE"},
    {Button::PLUS, "\uE0EF"},
    {Button::MINUS, "\uE0F0"},
    {Button::HOME, "\uE0F4"},
    {Button::SCREENSHOT, "\uE0F5"},
    {Button::LS, "\uE101"},
    {Button::RS, "\uE102"},
    {Button::L3, "\uE104"},
    {Button::R3, "\uE105"},
    {Button::SWITCH, "\uE121"},
    {Button::DUALCONS, "\uE122"},
    {Button::SETTINGS, "\uE130"},
    {Button::NEWS, "\uE132"},
    {Button::ESHOP, "\uE133"},
    {Button::ALBUM, "\uE134"},
    {Button::ALLAPPS, "\uE135"},
    {Button::CONTROLLER, "\uE136"},
}};

#define F(a) (a/255.f) // turn range 0-255 to 0.f-1.f range
static constexpr std::array<std::pair<Colour, NVGcolor>, 12> COLOURS = {{
    {Colour::BLACK, { F(45.f), F(45.f), F(45.f), F(255.f) }},
    {Colour::LIGHT_BLACK, { F(50.f), F(50.f), F(50.f), F(255.f) }},
    {Colour::SILVER, { F(128.f), F(128.f), F(128.f), F(255.f) }},
    {Colour::DARK_GREY, { F(70.f), F(70.f), F(70.f), F(255.f) }},
    {Colour::GREY, { F(77.f), F(77.f), F(77.f), F(255.f) }},
    {Colour::WHITE, { F(251.f), F(251.f), F(251.f), F(255.f) }},
    {Colour::CYAN, { F(0.f), F(255.f), F(200.f), F(255.f) }},
    {Colour::TEAL, { F(143.f), F(253.f), F(252.f), F(255.f) }},
    {Colour::BLUE, { F(50.f), F(155.f), F(215.f), F(255.f) }}, // 稍微调亮蓝色 (Slightly brighten blue color)
    {Colour::LIGHT_BLUE, { F(26.f), F(188.f), F(252.f), F(255.f) }},
    {Colour::YELLOW, { F(255.f), F(177.f), F(66.f), F(255.f) }},
    {Colour::RED, { F(250.f), F(90.f), F(58.f), F(255.f) }}
}};
#undef F

// https://www.youtube.com/watch?v=INn3xa4pMfg&t
template <typename Key, typename Value, std::size_t Size>
struct Map {
    std::array<std::pair<Key, Value>, Size> data;

    [[nodiscard]] constexpr Value at(const Key& key) const {
        const auto itr = std::find_if(std::begin(data), std::end(data),
            [&key](const auto& v) { return v.first == key; });
        if (itr != std::end(data)) {
            return itr->second;
        }
        __builtin_unreachable(); // it wont happen
    }
};

const char* getButton(const Button button) {
    static constexpr auto map =
        Map<Button, const char*, buttons.size()>{{buttons}};
    return map.at(button);
}

NVGcolor getColour(Colour c) {
    static constexpr auto map =
        Map<Colour, NVGcolor, COLOURS.size()>{{COLOURS}};
    return map.at(c);
}

void drawRect(NVGcontext* vg, float x, float y, float w, float h, Colour c) {
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, getColour(c));
    nvgFill(vg);
}

void drawRect(NVGcontext* vg, float x, float y, float w, float h, const NVGcolor& c) {
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, c);
    nvgFill(vg);
}

void drawRect(NVGcontext* vg, float x, float y, float w, float h, const NVGcolor&& c) {
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, c);
    nvgFill(vg);
}

void drawRect(NVGcontext* vg, float x, float y, float w, float h, const NVGpaint& p) {
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, p);
    nvgFill(vg);
}

void drawRect(NVGcontext* vg, float x, float y, float w, float h, const NVGpaint&& p) {
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, p);
    nvgFill(vg);
}

void drawText(NVGcontext* vg, float x, float y, float size, const char* str, const char* end, int align, Colour c) {
    nvgBeginPath(vg);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align);
    nvgFillColor(vg, getColour(c));
    nvgText(vg, x, y, str, end);
}

void drawText(NVGcontext* vg, float x, float y, float size, const char* str, const char* end, int align, const NVGcolor& c) {
    nvgBeginPath(vg);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align);
    nvgFillColor(vg, c);
    nvgText(vg, x, y, str, end);
}

void drawText(NVGcontext* vg, float x, float y, float size, const char* str, const char* end, int align, const NVGcolor&& c) {
    nvgBeginPath(vg);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align);
    nvgFillColor(vg, c);
    nvgText(vg, x, y, str, end);
}

void drawTextBox(NVGcontext* vg, float x, float y, float width, float size, const char* str, const char* end, int align, Colour c) {
    nvgBeginPath(vg);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align);
    nvgFillColor(vg, getColour(c));
    nvgTextBox(vg, x, y, width, str, end);
}

void drawTextBox(NVGcontext* vg, float x, float y, float width, float size, float lineSpacing, const char* str, const char* end, int align, Colour c) {
    nvgBeginPath(vg);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align);
    nvgFillColor(vg, getColour(c));
    nvgTextLineHeight(vg, lineSpacing);
    nvgTextBox(vg, x, y, width, str, end);
}

void drawTextBoxCentered(NVGcontext* vg, float x, float y, float width, float height, float size, float lineSpacing, const char* str, const char* end, Colour c) {
    nvgFontSize(vg, size);
    nvgFillColor(vg, getColour(c));
    nvgTextLineHeight(vg, lineSpacing);
    
    // 获取字体度量信息 (Get font metrics)
    float ascender, descender, lineh;
    nvgTextMetrics(vg, &ascender, &descender, &lineh);
    
    // 移除左右边距限制，使用整个宽度区域 (Remove left and right margin restrictions, use the entire width area)
    float textAreaWidth = width;
    float textAreaX = x;
    
    // 计算自动换行后的文本行数 (Calculate text rows after auto line wrapping)
    NVGtextRow rows[20];
    int nrows = 0;
    const char* s = str;
    int totalRows = 0;
    
    // 收集所有文本行 (Collect all text rows)
    NVGtextRow allRows[20];
    int allRowsCount = 0;
    
    while ((nrows = nvgTextBreakLines(vg, s, end, textAreaWidth, rows, 20)) && allRowsCount < 20) {
        for (int i = 0; i < nrows && allRowsCount < 20; i++) {
            allRows[allRowsCount++] = rows[i];
        }
        s = rows[nrows-1].next;
        if (s == nullptr || *s == '\0') break;
    }
    
    // 计算文本总高度 (Calculate total text height)
    float totalTextHeight = allRowsCount > 0 ? allRowsCount * lineSpacing * lineh : 0;
    
    // 计算垂直居中位置 (Calculate vertical center position)
    float centerY = y + height / 2.0f;
    // 考虑文本基线，使用ascender来正确计算起始位置 (Consider text baseline, use ascender to correctly calculate start position)
    float textStartY = centerY - totalTextHeight / 2.0f + ascender;
    
    // 设置文本对齐方式为水平居中，垂直顶部对齐 (Set text alignment to horizontal center, vertical top)
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    
    // 逐行绘制文本，每行都水平居中 (Draw text line by line, each line horizontally centered)
    float currentY = textStartY;
    float textAreaCenterX = textAreaX + textAreaWidth / 2.0f;
    
    for (int i = 0; i < allRowsCount; i++) {
        // 为每行文本单独设置水平居中位置 (Set horizontal center position for each line)
        nvgText(vg, textAreaCenterX, currentY, allRows[i].start, allRows[i].end);
        currentY += lineSpacing * lineh;
    }
}

// 支持自定义对齐方式的drawTextBoxCentered函数重载 (Overloaded drawTextBoxCentered function with custom alignment support)
void drawTextBoxCentered(NVGcontext* vg, float x, float y, float width, float height, float size, float lineSpacing, const char* str, const char* end, int align, Colour c) {
    nvgFontSize(vg, size);
    nvgFillColor(vg, getColour(c));
    nvgTextLineHeight(vg, lineSpacing);
    
    // 获取字体度量信息 (Get font metrics)
    float ascender, descender, lineh;
    nvgTextMetrics(vg, &ascender, &descender, &lineh);
    
    // 移除左右边距限制，使用整个宽度区域 (Remove left and right margin restrictions, use the entire width area)
    float textAreaWidth = width;
    float textAreaX = x;
    
    // 计算自动换行后的文本行数 (Calculate text rows after auto line wrapping)
    NVGtextRow rows[20];
    int nrows = 0;
    const char* s = str;
    
    // 收集所有文本行 (Collect all text rows)
    NVGtextRow allRows[20];
    int allRowsCount = 0;
    
    while ((nrows = nvgTextBreakLines(vg, s, end, textAreaWidth, rows, 20)) && allRowsCount < 20) {
        for (int i = 0; i < nrows && allRowsCount < 20; i++) {
            allRows[allRowsCount++] = rows[i];
        }
        s = rows[nrows-1].next;
        if (s == nullptr || *s == '\0') break;
    }
    
    // 计算文本总高度 (Calculate total text height)
    float totalTextHeight = allRowsCount > 0 ? allRowsCount * lineSpacing * lineh : 0;
    
    // 根据对齐方式计算起始位置 (Calculate starting position based on alignment)
    float textStartY;
    float textX;
    
    // 垂直对齐处理 (Vertical alignment handling)
    if (align & NVG_ALIGN_TOP) {
        textStartY = y + ascender;  // 顶部对齐，考虑文本基线 (Top alignment, consider text baseline)
    } else if (align & NVG_ALIGN_BOTTOM) {
        textStartY = y + height - totalTextHeight + ascender;  // 底部对齐 (Bottom alignment)
    } else if (align & NVG_ALIGN_MIDDLE) {
        // 垂直居中对齐，使用NanoVG的MIDDLE对齐方式 (Vertical center alignment using NanoVG's MIDDLE alignment)
        float centerY = y + height / 2.0f;
        textStartY = centerY;
    } else {
        // 默认垂直居中 (Default vertical center)
        float centerY = y + height / 2.0f;
        textStartY = centerY - totalTextHeight / 2.0f + ascender;
    }
    
    // 水平对齐处理 (Horizontal alignment handling)
    if (align & NVG_ALIGN_LEFT) {
        textX = textAreaX;  // 左对齐 (Left alignment)
    } else if (align & NVG_ALIGN_RIGHT) {
        textX = textAreaX + textAreaWidth;  // 右对齐 (Right alignment)
    } else {
        // 默认水平居中 (Default horizontal center)
        textX = textAreaX + textAreaWidth / 2.0f;
    }
    
    // 设置文本对齐方式 (Set text alignment)
    nvgTextAlign(vg, align);
    
    // 逐行绘制文本 (Draw text line by line)
    float currentY = textStartY;
    
    if (align & NVG_ALIGN_MIDDLE) {
        // 对于MIDDLE对齐，需要调整多行文本的起始位置 (For MIDDLE alignment, adjust starting position for multi-line text)
        float totalTextHeight = allRowsCount * lineSpacing * lineh;
        currentY = textStartY - totalTextHeight / 2.0f + lineSpacing * lineh / 2.0f;
    }
    
    for (int i = 0; i < allRowsCount; i++) {
        nvgText(vg, textX, currentY, allRows[i].start, allRows[i].end);
        currentY += lineSpacing * lineh;
    }
}

void drawTextArgs(NVGcontext* vg, float x, float y, float size, int align, Colour c, const char* str, ...) {
    std::va_list v;
    va_start(v, str);
    char buffer[0x100];
    vsnprintf(buffer, sizeof(buffer), str, v);
    drawText(vg, x, y, size, buffer, nullptr, align, c);
    va_end(v);
}

void drawButton(NVGcontext* vg, float x, float y, float size, Button button) {
    drawText(vg, x, y, size, getButton(button), nullptr, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, Colour::WHITE);
}

// 绘制包含按钮标记的文本（如[PLUS]）
void drawTextWithButtonMarker(NVGcontext* vg, float x, float y, float size, const char* str, const char* end, int align, Colour c) {
    if (!str) return;
    
    // 查找[PLUS]标记
    const char* marker_start = nullptr;
    const char* marker_end = nullptr;
    const char* current = str;
    
    // 寻找[PLUS]标记
    while (current && (end == nullptr || current < end)) {
        if (*current == '[') {
            const char* bracket_end = current + 1;
            while (bracket_end && (end == nullptr || bracket_end < end) && *bracket_end != ']' && *bracket_end != '\0') {
                bracket_end++;
            }
            if (*bracket_end == ']') {
                // 检查是否是PLUS标记
                if ((bracket_end - current - 1) == 4 && 
                    current[1] == 'P' && current[2] == 'L' && current[3] == 'U' && current[4] == 'S') {
                    marker_start = current;
                    marker_end = bracket_end + 1;
                    break;
                }
            }
        }
        current++;
        if (*current == '\0') break;
    }
    
    // 如果没有找到[PLUS]标记，直接绘制原文本
    if (!marker_start) {
        drawText(vg, x, y, size, str, end, align, c);
        return;
    }
    
    // 计算各部分的宽度
    nvgFontSize(vg, size);
    
    // 第一部分文本（[PLUS]之前）
    float part1_width = 0;
    if (marker_start > str) {
        part1_width = nvgTextBounds(vg, 0, 0, str, marker_start, nullptr);
    }
    
    // 按钮部分
    const char* plus_char = getButton(Button::PLUS);
    float button_width = nvgTextBounds(vg, 0, 0, plus_char, nullptr, nullptr);
    
    // 第二部分文本（[PLUS]之后）
    float part2_width = 0;
    if (marker_end && (end == nullptr || marker_end < end) && *marker_end != '\0') {
        part2_width = nvgTextBounds(vg, 0, 0, marker_end, end, nullptr);
    }
    
    // 计算总宽度和起始位置（图标两边各加5.0f间距）
    float icon_spacing = 5.0f;
    float total_width = part1_width + (part1_width > 0 ? icon_spacing : 0) + button_width + (part2_width > 0 ? icon_spacing : 0) + part2_width;
    float current_x = x;
    
    // 根据对齐方式调整起始位置
    if (align & NVG_ALIGN_CENTER) {
        current_x -= total_width / 2.0f;
    } else if (align & NVG_ALIGN_RIGHT) {
        current_x -= total_width;
    }
    
    // 设置垂直对齐
    int vertical_align = align & (NVG_ALIGN_TOP | NVG_ALIGN_MIDDLE | NVG_ALIGN_BOTTOM | NVG_ALIGN_BASELINE);
    if (vertical_align == 0) vertical_align = NVG_ALIGN_BASELINE;
    
    nvgFillColor(vg, getColour(c));
    
    // 绘制第一部分文本
    if (part1_width > 0) {
        nvgTextAlign(vg, NVG_ALIGN_LEFT | vertical_align);
        nvgText(vg, current_x, y, str, marker_start);
        current_x += part1_width + icon_spacing; // 添加图标前间距
    }
    
    // 绘制按钮
    nvgTextAlign(vg, NVG_ALIGN_LEFT | vertical_align);
    nvgText(vg, current_x, y, plus_char, nullptr);
    current_x += button_width;
    
    // 绘制第二部分文本
    if (part2_width > 0) {
        current_x += icon_spacing; // 添加图标后间距
        nvgTextAlign(vg, NVG_ALIGN_LEFT | vertical_align);
        nvgText(vg, current_x, y, marker_end, end);
    }
}

void drawRoundedRect(NVGcontext* vg, float x, float y, float w, float h, float r, int red, int green, int blue) {
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, nvgRGB(red, green, blue));
    nvgFill(vg);
}

// 绘制可变圆角矩形实现 (Draw rounded rectangle with varying corners implementation)
void drawRoundedRectVarying(NVGcontext* vg, float x, float y, float w, float h, float radiusTopLeft, float radiusTopRight, float radiusBottomRight, float radiusBottomLeft, int red, int green, int blue) {
    nvgBeginPath(vg);
    nvgRoundedRectVarying(vg, x, y, w, h, radiusTopLeft, radiusTopRight, radiusBottomRight, radiusBottomLeft);
    nvgFillColor(vg, nvgRGB(red, green, blue));
    nvgFill(vg);
}

// 解析颜色标记字符串，返回对应的Colour枚举 (Parse color markup string and return corresponding Colour enum)
static Colour parseColorFromString(const char* color_str, size_t len) {
    if (len == 5 && strncmp(color_str, "BLACK", 5) == 0) return Colour::BLACK;
    if (len == 11 && strncmp(color_str, "LIGHT_BLACK", 11) == 0) return Colour::LIGHT_BLACK;
    if (len == 6 && strncmp(color_str, "SILVER", 6) == 0) return Colour::SILVER;
    if (len == 9 && strncmp(color_str, "DARK_GREY", 9) == 0) return Colour::DARK_GREY;
    if (len == 4 && strncmp(color_str, "GREY", 4) == 0) return Colour::GREY;
    if (len == 5 && strncmp(color_str, "WHITE", 5) == 0) return Colour::WHITE;
    if (len == 4 && strncmp(color_str, "CYAN", 4) == 0) return Colour::CYAN;
    if (len == 4 && strncmp(color_str, "TEAL", 4) == 0) return Colour::TEAL;
    if (len == 4 && strncmp(color_str, "BLUE", 4) == 0) return Colour::BLUE;
    if (len == 10 && strncmp(color_str, "LIGHT_BLUE", 10) == 0) return Colour::LIGHT_BLUE;
    if (len == 6 && strncmp(color_str, "YELLOW", 6) == 0) return Colour::YELLOW;
    if (len == 3 && strncmp(color_str, "RED", 3) == 0) return Colour::RED;
    return Colour::WHITE; // 默认颜色 (Default color)
}

// 查找字符在字符串中的位置 (Find character position in string)
static const char* findChar(const char* str, const char* end, char c) {
    while (str < end && *str != c) {
        str++;
    }
    return (str < end) ? str : nullptr;
}

// 查找子字符串在字符串中的位置 (Find substring position in string)
static const char* findSubstring(const char* str, const char* end, const char* pattern, size_t pattern_len) {
    if (pattern_len == 0) return str;
    const char* search_end = end - pattern_len + 1;
    
    for (const char* pos = str; pos < search_end; pos++) {
        if (strncmp(pos, pattern, pattern_len) == 0) {
            return pos;
        }
    }
    return nullptr;
}

// 支持颜色标记的文本绘制函数实现 (Implementation of text drawing function with color markup support)
// 格式：[color=COLOR]text[/color] 其中COLOR是颜色名称，如TEAL、RED等
void drawTextBoxWithColorMarkup(NVGcontext* vg, float x, float y, float width, float size, float lineSpacing, const char* str, const char* end, int align, tj::gfx::Colour default_color) {
    if (!str) return;
    
    nvgFontSize(vg, size);
    nvgTextAlign(vg, align);
    nvgTextLineHeight(vg, lineSpacing);
    
    const char* text_end = end ? end : str + strlen(str);
    float current_x = x;
    float current_y = y;
    
    const char* pos = str;
    while (pos < text_end) {
        // 查找颜色标记的开始 [color=
        const char* color_tag_start = findSubstring(pos, text_end, "[color=", 7);
        
        if (!color_tag_start) {
            // 没有更多标记，绘制剩余文本 (No more markup, draw remaining text)
            nvgFillColor(vg, getColour(default_color));
            nvgText(vg, current_x, current_y, pos, text_end);
            break;
        }
        
        // 绘制标记前的普通文本 (Draw normal text before markup)
        if (color_tag_start > pos) {
            nvgFillColor(vg, getColour(default_color));
            float bounds[4];
            nvgTextBounds(vg, current_x, current_y, pos, color_tag_start, bounds);
            nvgText(vg, current_x, current_y, pos, color_tag_start);
            current_x += bounds[2] - bounds[0] + 10.0f; // 添加1像素间距补偿 (Add 1 pixel spacing compensation)
        }
        
        // 查找颜色名称的结束位置 ]
        const char* color_name_start = color_tag_start + 7; // 跳过 "[color="
        const char* color_name_end = findChar(color_name_start, text_end, ']');
        if (!color_name_end) {
            // 标记格式错误，绘制剩余文本 (Markup format error, draw remaining text)
            nvgFillColor(vg, getColour(default_color));
            nvgText(vg, current_x, current_y, color_tag_start, text_end);
            break;
        }
        
        // 查找结束标记 [/color]
        const char* end_tag = findSubstring(color_name_end + 1, text_end, "[/color]", 8);
        if (!end_tag) {
            // 没有结束标记，绘制剩余文本 (No end tag, draw remaining text)
            nvgFillColor(vg, getColour(default_color));
            nvgText(vg, current_x, current_y, color_tag_start, text_end);
            break;
        }
        
        // 提取颜色名称和文本内容 (Extract color name and text content)
        size_t color_name_len = color_name_end - color_name_start;
        const char* content_start = color_name_end + 1; // 跳过 ']'
        size_t content_len = end_tag - content_start;
        
        // 解析颜色并绘制文本 (Parse color and draw text)
        Colour text_color = parseColorFromString(color_name_start, color_name_len);
        nvgFillColor(vg, getColour(text_color));
        
        float bounds[4];
        nvgTextBounds(vg, current_x, current_y, content_start, content_start + content_len, bounds);
        nvgText(vg, current_x, current_y, content_start, content_start + content_len);
        current_x += bounds[2] - bounds[0] + 10.0f; // 添加1像素间距补偿 (Add 1 pixel spacing compensation)
        
        // 移动到结束标记之后 (Move to after end tag)
        pos = end_tag + 8; // 跳过 "[/color]"
    }
}

} // namespace tj::gfx
