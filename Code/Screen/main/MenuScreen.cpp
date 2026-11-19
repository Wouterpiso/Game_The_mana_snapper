/**
 * @file MenuScreen.cpp
 * @brief Implements all UI pages and touch hit-testing.
 */
#include "MenuScreen.hpp"

#include "cJSON.h"
#include "jpeg_decoder.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr int kButtonX = 105;
constexpr int kButtonWidth = 110;
constexpr int kButtonHeight = 48;
constexpr int kButtonYs[] = {18, 96, 174};
constexpr int kVisualButtonXs[] = {32, 136, 240};

const uint8_t* glyph(char character) {
  static const uint8_t patterns[][5] = {
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
      {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
      {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
      {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
      {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
      {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
      {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
      {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
      {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
      {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
      {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
      {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
      {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
      {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
      {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
      {0x46, 0x49, 0x49, 0x49, 0x31}, // S
      {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
      {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
      {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
      {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
      {0x63, 0x14, 0x08, 0x14, 0x63}, // X
      {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
      {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
  };
  if (character >= 'A' && character <= 'Z') {
    return patterns[character - 'A'];
  }
  static const uint8_t digits[][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
      {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
      {0x62, 0x51, 0x49, 0x49, 0x46}, // 2
      {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
      {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
      {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
      {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
      {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
      {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
  };
  if (character >= '0' && character <= '9') {
    return digits[character - '0'];
  }
  static const uint8_t slash[] = {0x20, 0x10, 0x08, 0x04, 0x02};
  if (character == '/') {
    return slash;
  }
  // Punctuation, so oracle text and names stay readable.
  static const struct {
    char character;
    uint8_t pattern[5];
  } punctuation[] = {
      {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},  {'"', {0x00, 0x07, 0x00, 0x07, 0x00}},
      {'%', {0x23, 0x13, 0x08, 0x64, 0x62}},  {'&', {0x36, 0x49, 0x55, 0x22, 0x50}},
      {'\'', {0x00, 0x05, 0x03, 0x00, 0x00}}, {'(', {0x00, 0x1C, 0x22, 0x41, 0x00}},
      {')', {0x00, 0x41, 0x22, 0x1C, 0x00}},  {'*', {0x14, 0x08, 0x3E, 0x08, 0x14}},
      {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},  {',', {0x00, 0x50, 0x30, 0x00, 0x00}},
      {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},  {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
      {':', {0x00, 0x36, 0x36, 0x00, 0x00}},  {';', {0x00, 0x56, 0x36, 0x00, 0x00}},
      {'?', {0x02, 0x01, 0x51, 0x09, 0x06}},
  };
  for (const auto& entry : punctuation) {
    if (entry.character == character) {
      return entry.pattern;
    }
  }
  return nullptr;
}

// The panel is used in portrait: the reader sees the 320x240 back buffer
// turned a quarter. All page content below is laid out in "reader" coordinates
// (u = right, 0..239; v = down, 0..319) and mapped onto the buffer here, the
// same way drawRotatedText() does it.
constexpr int kReaderWidth = St7789Display::HEIGHT; // 240
constexpr int kReaderHeight = St7789Display::WIDTH; // 320
inline int bufferX(int /*u*/, int v) {
  return v;
}
inline int bufferY(int u, int /*v*/) {
  return kReaderWidth - 1 - u;
}

// Collection page layout (reader coordinates).
constexpr int kCollectionFirstRowV = 58;
constexpr int kCollectionRowHeight = 24;
constexpr int kCollectionMaxRows = 8;
constexpr int kNavV = 284;
constexpr int kNavHeight = 26;
constexpr int kNavWidth = 48;
constexpr int kNavPreviousU = 24;
constexpr int kNavPageU = 96;
constexpr int kNavNextU = 168;
constexpr int kNavTouchPadding = 8;
// IMPORT/EXPORT button in the top-left corner of the collection page (the
// mirror of EXIT), and the two buttons of the page it opens.
constexpr int kTransferButtonU = 8;
constexpr int kTransferButtonV = 8;
constexpr int kTransferButtonWidth = 30;
constexpr int kTransferButtonHeight = 38;
constexpr int kTransferMenuButtonU = 40;
constexpr int kTransferMenuButtonWidth = 160;
constexpr int kTransferMenuButtonHeight = 52;
constexpr int kTransferMenuButtonVs[2] = {96, 176};

// Width in pixels that readerText() needs for `text` (same advance rules).
int textWidth(const char* text, int scale) {
  int width = 0;
  for (const char* character = text; *character != '\0'; ++character) {
    width += (*character == ' ') ? 4 * scale : 6 * scale;
  }
  return width;
}

// Copies `|`-separated field `index` of the master's set list payload.
bool setField(const std::string& payload, int index, char* out, size_t outSize) {
  const char* cursor = payload.c_str();
  for (int field = 0; field < index; ++field) {
    cursor = std::strchr(cursor, '|');
    if (cursor == nullptr) {
      out[0] = '\0';
      return false;
    }
    ++cursor;
  }
  const char* end = std::strchr(cursor, '|');
  size_t length = end == nullptr ? std::strlen(cursor) : static_cast<size_t>(end - cursor);
  if (length >= outSize) {
    length = outSize - 1;
  }
  std::memcpy(out, cursor, length);
  out[length] = '\0';
  return true;
}

// Touch coordinates -> reader coordinates. The touch driver scales its two raw
// axes to 0..319 (x) and 0..239 (y), but x runs along the buffer height and y
// along the buffer width, so each one is stretched back to the buffer size.
void touchToReader(int touchX, int touchY, int& u, int& v) {
  const int bufferXPos = touchY * St7789Display::WIDTH / St7789Display::HEIGHT;
  const int bufferYPos = touchX * St7789Display::HEIGHT / St7789Display::WIDTH;
  u = kReaderWidth - 1 - bufferYPos;
  v = bufferXPos;
}

bool insideReaderRect(int touchX, int touchY, int u, int v, int width, int height, int padding) {
  int touchU = 0;
  int touchV = 0;
  touchToReader(touchX, touchY, touchU, touchV);
  return touchU >= u - padding && touchU <= u + width + padding && touchV >= v - padding &&
         touchV <= v + height + padding;
}

uint16_t colorPipValue(St7789Display& display, char color) {
  switch (color) {
  case 'W':
    return display.rgb565(255, 255, 255);
  case 'U':
    return display.rgb565(30, 100, 255);
  case 'B':
    return display.rgb565(0, 0, 0);
  case 'R':
    return display.rgb565(230, 30, 30);
  case 'G':
    return display.rgb565(30, 180, 60);
  default:
    return display.rgb565(128, 128, 128);
  }
}

// Turns card text into something the small font can draw: mana braces are
// dropped, accents are removed and typographic punctuation becomes ASCII.
std::string cleanText(const char* text) {
  // Latin-1 letters U+00C0..U+00FF without their accents.
  static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
  std::string out;
  if (text == nullptr) {
    return out;
  }
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  while (*p != 0) {
    const unsigned char c = *p;
    if (c == '{' || c == '}') {
      ++p;
      continue;
    }
    if (c < 0x80) {
      out += static_cast<char>(c);
      ++p;
      continue;
    }
    const int length = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    int available = 1;
    while (available < length && p[available] != 0 && (p[available] & 0xC0) == 0x80) {
      ++available;
    }
    if (length == 1 || available < length) {
      ++p;
      continue;
    }
    uint32_t codePoint = 0;
    if (length == 2) {
      codePoint = ((c & 0x1F) << 6) | (p[1] & 0x3F);
    } else if (length == 3) {
      codePoint = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    p += length;
    if (length == 2 && codePoint >= 0xC0 && codePoint <= 0xFF) {
      out += latin1[codePoint - 0xC0];
      continue;
    }
    switch (codePoint) {
    case 0x2018:
    case 0x2019:
      out += '\'';
      break;
    case 0x201C:
    case 0x201D:
      out += '"';
      break;
    case 0x2013:
    case 0x2014:
    case 0x2212:
      out += '-';
      break;
    case 0x2022:
      out += '*';
      break;
    case 0x2026:
      out += "...";
      break;
    default:
      break;
    }
  }
  return out;
}

// Word-wraps `text` (honouring '\n') into lines that fit `maxWidth` pixels.
std::vector<std::string> wrapLines(const std::string& text, int maxWidth) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    const std::string paragraph = text.substr(start, end - start);
    std::string line;
    size_t position = 0;
    while (position < paragraph.size()) {
      size_t space = paragraph.find(' ', position);
      if (space == std::string::npos) {
        space = paragraph.size();
      }
      std::string word = paragraph.substr(position, space - position);
      position = space + 1;
      if (word.empty()) {
        continue;
      }
      while (textWidth(word.c_str(), 1) > maxWidth) {
        size_t fit = word.size();
        while (fit > 1 && textWidth(word.substr(0, fit).c_str(), 1) > maxWidth) {
          --fit;
        }
        if (!line.empty()) {
          lines.push_back(line);
          line.clear();
        }
        lines.push_back(word.substr(0, fit));
        word = word.substr(fit);
      }
      const std::string candidate = line.empty() ? word : line + " " + word;
      if (textWidth(candidate.c_str(), 1) <= maxWidth) {
        line = candidate;
      } else {
        lines.push_back(line);
        line = word;
      }
    }
    if (!line.empty()) {
      lines.push_back(line);
    }
    if (end == text.size()) {
      break;
    }
    start = end + 1;
  }
  return lines;
}

// Detail screens (reader coordinates).
constexpr int kDetailButtonU = 40;
constexpr int kDetailButtonWidth = 160;
constexpr int kDetailButtonHeight = 48;
constexpr int kDetailButtonCount = 4;
constexpr int kDetailButtonVs[kDetailButtonCount] = {66, 124, 182, 240};

// Remove confirmation (reader coordinates).
constexpr int kConfirmButtonWidth = 104;
constexpr int kConfirmButtonHeight = 48;
constexpr int kConfirmYesV = 252;
constexpr int kConfirmYesU = 10;
constexpr int kConfirmNoU = 126;
constexpr int kOneCopyU = 20;
constexpr int kOneCopyV = 190;
constexpr int kOneCopyWidth = 200;
constexpr int kOneCopyHeight = 48;
constexpr int kImageTop = 8;
constexpr int kImageMaxWidth = 200;
constexpr int kImageMaxHeight = 210;
constexpr int kBodyFirstV = 58;
constexpr int kBodyLastV = 316;

// Camera page (reader coordinates).
constexpr int kShutterU = 40;
constexpr int kShutterV = 264;
constexpr int kShutterWidth = 160;
constexpr int kShutterHeight = 44;
constexpr int kCaptureBarV = 250;
constexpr int kCaptureBarHeight = kReaderHeight - kCaptureBarV;
constexpr int kCaptureTouchPadding = 6;

// ADD CARD menu (reader coordinates).
constexpr int kAddMenuButtonU = 40;
constexpr int kAddMenuButtonWidth = 160;
constexpr int kAddMenuButtonHeight = 52;
constexpr int kAddMenuButtonVs[] = {74, 140, 206};

// Name keyboard (reader coordinates).
constexpr int kKeyCols = 7;
constexpr int kKeyWidth = 30;
constexpr int kKeyHeight = 30;
constexpr int kKeyGap = 3;
constexpr int kKeyFirstU = 6;
constexpr int kKeyFirstV = 108;
constexpr int kKeyRowStep = 34;
constexpr int kEnterU = 6;
constexpr int kEnterV = 280;
constexpr int kEnterWidth = 228;
constexpr int kEnterHeight = 30;
constexpr int kKeyboardTextV = 70;
constexpr int kKeyboardTextMaxChars = 19;

// SCAN DEMO photo (reader coordinates).
constexpr int kDemoBarV = 250;
constexpr int kDemoBarHeight = kReaderHeight - kDemoBarV;
constexpr int kDemoBackU = 24;
constexpr int kDemoBackV = 292;
constexpr int kDemoBackWidth = 192;
constexpr int kDemoBackHeight = 22;

struct DecodedImage {
  uint16_t* pixels = nullptr;
  int width = 0;
  int height = 0;
};

// Decodes a baseline JPEG to RGB565, halving its size until it fits maxWidth x maxHeight.
bool decodeJpeg(const uint8_t* data, size_t length, DecodedImage& image,
                int maxWidth = kImageMaxWidth, int maxHeight = kImageMaxHeight) {
  if (data == nullptr || length == 0) {
    return false;
  }
  esp_jpeg_image_cfg_t config = {};
  config.indata = const_cast<uint8_t*>(data);
  config.indata_size = static_cast<uint32_t>(length);
  config.out_format = JPEG_IMAGE_FORMAT_RGB565;

  // esp_jpeg_get_image_info() reports the unscaled frame size, so the scale
  // has to be picked by dividing the dimensions ourselves.
  static const esp_jpeg_image_scale_t scales[] = {JPEG_IMAGE_SCALE_0, JPEG_IMAGE_SCALE_1_2,
                                                  JPEG_IMAGE_SCALE_1_4, JPEG_IMAGE_SCALE_1_8};
  static const int divisors[] = {1, 2, 4, 8};

  esp_jpeg_image_output_t info = {};
  bool fits = false;
  for (size_t index = 0; index < 4; ++index) {
    config.out_scale = scales[index];
    if (esp_jpeg_get_image_info(&config, &info) != ESP_OK) {
      return false;
    }
    if (static_cast<int>(info.width) / divisors[index] <= maxWidth &&
        static_cast<int>(info.height) / divisors[index] <= maxHeight) {
      fits = true;
      break;
    }
  }
  if (!fits) {
    return false;
  }

  config.outbuf = static_cast<uint8_t*>(std::malloc(info.output_len));
  if (config.outbuf == nullptr) {
    return false;
  }
  config.outbuf_size = static_cast<uint32_t>(info.output_len);
  // After decoding, info holds the scaled dimensions.
  if (esp_jpeg_decode(&config, &info) != ESP_OK) {
    std::free(config.outbuf);
    return false;
  }
  image.pixels = reinterpret_cast<uint16_t*>(config.outbuf);
  image.width = info.width;
  image.height = info.height;
  return true;
}

uint16_t blendRgb565(uint16_t a, uint16_t b, int weight) {
  const int aRed = (a >> 11) & 0x1F;
  const int aGreen = (a >> 5) & 0x3F;
  const int aBlue = a & 0x1F;
  const int bRed = (b >> 11) & 0x1F;
  const int bGreen = (b >> 5) & 0x3F;
  const int bBlue = b & 0x1F;
  const int red = aRed + (((bRed - aRed) * weight) >> 8);
  const int green = aGreen + (((bGreen - aGreen) * weight) >> 8);
  const int blue = aBlue + (((bBlue - aBlue) * weight) >> 8);
  return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

const char* yesNo(cJSON* value) {
  return cJSON_IsTrue(value) ? "YES" : "NO";
}
} // namespace

void MenuScreen::drawRotatedText(St7789Display& display, const char* text, int centerX, int centerY,
                                 uint16_t color) const {
  constexpr int glyphWidth = 5;
  constexpr int glyphHeight = 7;
  constexpr int spacing = 1;
  const int length = static_cast<int>(std::strlen(text));
  const int sourceWidth = length * (glyphWidth + spacing) - spacing;
  const int left = centerX - glyphHeight / 2;
  const int top = centerY - sourceWidth / 2;

  for (int characterIndex = 0; characterIndex < length; ++characterIndex) {
    const uint8_t* pattern = glyph(text[characterIndex]);
    if (pattern == nullptr) {
      continue;
    }
    const int sourceLeft = characterIndex * (glyphWidth + spacing);
    for (int sourceX = 0; sourceX < glyphWidth; ++sourceX) {
      for (int sourceY = 0; sourceY < glyphHeight; ++sourceY) {
        if ((pattern[sourceX] & (1 << sourceY)) != 0) {
          display.drawPixel(left + sourceY, top + sourceWidth - 1 - sourceLeft - sourceX, color);
        }
      }
    }
  }
}

void MenuScreen::drawHome(St7789Display& display) const {
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(display.rgb565(0, 0, 0));
  const char* labels[] = {"ADD CARD", "COLLECTION", "DECKS"};
  for (int index = 0; index < 3; ++index) {
    const int visualX = kVisualButtonXs[index];
    const int visualY = (St7789Display::HEIGHT - kButtonWidth) / 2;
    display.drawRectangle(visualX, visualY, kButtonHeight, kButtonWidth, white, false);
    drawRotatedText(display, labels[index], visualX + kButtonHeight / 2, visualY + kButtonWidth / 2,
                    white);
  }
  display.present();
}

void MenuScreen::drawPage(St7789Display& display, const char* title) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  display.drawRectangle(8, 8, 38, 30, white, true);
  drawRotatedText(display, "EXIT", 27, 23, black);
  drawRotatedText(display, title, St7789Display::WIDTH / 2, St7789Display::HEIGHT / 2, white);
  display.present();
}

void MenuScreen::drawCollectionLoading(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  display.drawRectangle(8, 8, 38, 30, white, true);
  drawRotatedText(display, "EXIT", 27, 23, black);
  drawRotatedText(display, "COLLECTION", 32, 160, white);
  drawRotatedText(display, "LOADING", 160, 120, white);
  drawTransferButton(display);
  display.present();
}
void MenuScreen::drawAddMenu(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "ADD CARD", 32, 160, white);

  static const char* labels[] = {"TYPE NAME", "SCAN DEMO", "OCR DEMO"};
  for (int index = 0; index < 3; ++index) {
    readerRect(display, kAddMenuButtonU, kAddMenuButtonVs[index], kAddMenuButtonWidth,
               kAddMenuButtonHeight, white, false);
    const int labelWidth = textWidth(labels[index], 2);
    readerText(display, labels[index], kAddMenuButtonU + (kAddMenuButtonWidth - labelWidth) / 2,
               kAddMenuButtonVs[index] + (kAddMenuButtonHeight - 7 * 2) / 2 + 1, white, 2);
  }
  display.present();
}

void MenuScreen::drawKeyboard(St7789Display& display, const char* text,
                              const char* enterLabel) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t gray = display.rgb565(90, 90, 90);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "ADD CARD", 32, 160, white);

  readerText(display, "CARD NAME", 8, 56, white, 1);
  // Only the last characters stay visible once the name gets long.
  const int length = static_cast<int>(std::strlen(text));
  const char* shown = text + (length > kKeyboardTextMaxChars ? length - kKeyboardTextMaxChars : 0);
  readerText(display, shown, 8, kKeyboardTextV, white, 2);
  readerLine(display, 8, 92, kReaderWidth - 8, 92, gray);

  static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < kKeyCols; ++column) {
      const int u = kKeyFirstU + column * (kKeyWidth + kKeyGap);
      const int v = kKeyFirstV + row * kKeyRowStep;
      readerRect(display, u, v, kKeyWidth, kKeyHeight, gray, false);
      if (row == 3 && column >= 5) {
        const char* label = column == 5 ? "SPC" : "DEL";
        const int labelWidth = textWidth(label, 1);
        readerText(display, label, u + (kKeyWidth - labelWidth) / 2, v + (kKeyHeight - 7) / 2 + 1,
                   white, 1);
        continue;
      }
      const int letterIndex = row < 3 ? row * kKeyCols + column : 21 + column;
      char label[2] = {letters[letterIndex], '\0'};
      readerText(display, label, u + (kKeyWidth - textWidth(label, 2)) / 2,
                 v + (kKeyHeight - 7 * 2) / 2 + 1, white, 2);
    }
  }

  const bool sending = std::strcmp(enterLabel, "ENTER") != 0;
  readerRect(display, kEnterU, kEnterV, kEnterWidth, kEnterHeight, sending ? gray : white, true);
  const int labelWidth = textWidth(enterLabel, 2);
  readerText(display, enterLabel, kEnterU + (kEnterWidth - labelWidth) / 2,
             kEnterV + (kEnterHeight - 7 * 2) / 2 + 1, sending ? white : black, 2);
  display.present();
}

void MenuScreen::drawCameraWaiting(St7789Display& display, const char* message) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "ADD CARD", 32, 160, white);
  drawRotatedText(display, "CAMERA", 130, 120, white);
  drawRotatedText(display, message, 200, 120, white);
  display.present();
}

bool MenuScreen::drawImage(St7789Display& display, const uint8_t* jpeg, size_t jpegLength) const {
  // The best decode is a half-size one (160x120, 38 KB), but DRAM is tight
  // next to the back buffer, so fall back to a quarter/eighth (80x60).
  DecodedImage image;
  if (!decodeJpeg(jpeg, jpegLength, image, 160, 120) &&
      !decodeJpeg(jpeg, jpegLength, image, 80, 60)) {
    return false;
  }

  display.fillScreen(display.rgb565(0, 0, 0));
  int drawWidth = kReaderWidth;
  int drawHeight = drawWidth * image.height / image.width;
  if (drawHeight > kReaderHeight) {
    drawHeight = kReaderHeight;
    drawWidth = drawHeight * image.width / image.height;
  }
  const int imageLeft = (kReaderWidth - drawWidth) / 2;
  const int imageTop = (kReaderHeight - drawHeight) / 2;
  const int lastRow = image.height - 1;
  const int lastColumn = image.width - 1;
  for (int row = 0; row < drawHeight; ++row) {
    // Sample positions use 8.8 fixed point, measured at the pixel centres.
    const int rowFixed = std::max(
        0, std::min(lastRow << 8, ((2 * row + 1) * image.height * 128) / drawHeight - 128));
    const int sourceRow = rowFixed >> 8;
    const int nextRow = std::min(sourceRow + 1, lastRow);
    const int rowWeight = rowFixed & 0xFF;
    for (int column = 0; column < drawWidth; ++column) {
      const int columnFixed = std::max(
          0, std::min(lastColumn << 8, ((2 * column + 1) * image.width * 128) / drawWidth - 128));
      const int sourceColumn = columnFixed >> 8;
      const int nextColumn = std::min(sourceColumn + 1, lastColumn);
      const int columnWeight = columnFixed & 0xFF;

      const uint16_t* upperRow = image.pixels + sourceRow * image.width;
      const uint16_t* lowerRow = image.pixels + nextRow * image.width;
      const uint16_t upper =
          blendRgb565(upperRow[sourceColumn], upperRow[nextColumn], columnWeight);
      const uint16_t lower =
          blendRgb565(lowerRow[sourceColumn], lowerRow[nextColumn], columnWeight);
      const int u = imageLeft + column;
      const int v = imageTop + row;
      display.drawPixel(bufferX(u, v), bufferY(u, v), blendRgb565(upper, lower, rowWeight));
    }
  }
  std::free(image.pixels);
  return true;
}

bool MenuScreen::drawCameraView(St7789Display& display, const uint8_t* jpeg,
                                size_t jpegLength) const {
  if (!drawImage(display, jpeg, jpegLength)) {
    return false;
  }

  drawExitButton(display);
  // SNAP button under the picture.
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  readerRect(display, kShutterU, kShutterV, kShutterWidth, kShutterHeight, white, true);
  const int labelWidth = textWidth("SNAP", 2);
  readerText(display, "SNAP", kShutterU + (kShutterWidth - labelWidth) / 2,
             kShutterV + (kShutterHeight - 7 * 2) / 2 + 1, black, 2);
  display.present();
  return true;
}

bool MenuScreen::drawDemoPhoto(St7789Display& display, const uint8_t* jpeg,
                               size_t jpegLength) const {
  // When the photo cannot be decoded the picture already on screen (the last
  // preview frame) stays; the bar below is all that changes.
  const bool drawn = drawImage(display, jpeg, jpegLength);
  if (drawn) {
    drawExitButton(display);
  }
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  readerRect(display, 0, kDemoBarV, kReaderWidth, kDemoBarHeight, black, true);
  static const char* lines[] = {"THIS IS A DEMO,", "NEEDS A BETTER MASTER", "MICRO CONTROLLER TO",
                                "PROPERLY WORK"};
  for (int index = 0; index < 4; ++index) {
    const int lineWidth = textWidth(lines[index], 1);
    readerText(display, lines[index], (kReaderWidth - lineWidth) / 2, kDemoBarV + 4 + index * 10,
               white, 1);
  }
  readerRect(display, kDemoBackU, kDemoBackV, kDemoBackWidth, kDemoBackHeight, white, false);
  const int labelWidth = textWidth("BACK", 1);
  readerText(display, "BACK", kDemoBackU + (kDemoBackWidth - labelWidth) / 2,
             kDemoBackV + (kDemoBackHeight - 7) / 2 + 1, white, 1);
  display.present();
  return drawn;
}

void MenuScreen::drawCaptureSending(St7789Display& display, const char* label) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  readerRect(display, 0, kCaptureBarV, kReaderWidth, kCaptureBarHeight, black, true);
  const int textW = textWidth(label, 2);
  readerText(display, label, (kReaderWidth - textW) / 2,
             kCaptureBarV + (kCaptureBarHeight - 7 * 2) / 2 + 1, white, 2);
  display.present();
}

void MenuScreen::drawOcrLoading(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "OCR DEMO", 32, 160, white);
  const char* label = "READING TEST IMAGE";
  readerText(display, label, (kReaderWidth - textWidth(label, 2)) / 2, 150, white, 2);
  display.present();
}

void MenuScreen::drawOcrResult(St7789Display& display, int testImage, const char* text) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t gray = display.rgb565(150, 150, 150);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "OCR DEMO", 32, 160, white);

  char testLabel[16] = {};
  std::snprintf(testLabel, sizeof(testLabel), "TEST %d", testImage);
  readerText(display, testLabel, (kReaderWidth - textWidth(testLabel, 2)) / 2, 54, gray, 2);
  const char* header = "RECOGNISED";
  readerText(display, header, (kReaderWidth - textWidth(header, 1)) / 2, 92, gray, 1);

  std::vector<std::string> lines = wrapLines(cleanText(text), 110);
  if (lines.empty()) {
    lines.push_back("NOTHING");
  }
  const int maxLines = 7;
  for (int index = 0; index < static_cast<int>(lines.size()) && index < maxLines; ++index) {
    readerText(display, lines[index].c_str(),
               (kReaderWidth - textWidth(lines[index].c_str(), 2)) / 2, 112 + index * 22, white, 2);
  }
  display.present();
}

void MenuScreen::drawOcrFailed(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t gray = display.rgb565(150, 150, 150);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "OCR DEMO", 32, 160, white);
  const char* title = "OCR FAILED";
  readerText(display, title, (kReaderWidth - textWidth(title, 2)) / 2, 130, white, 2);
  const char* hint = "CHECK THE OCR SERVER";
  readerText(display, hint, (kReaderWidth - textWidth(hint, 1)) / 2, 165, gray, 1);
  display.present();
}

void MenuScreen::drawAddSets(St7789Display& display, const char* payload) {
  addSetsPayload_ = payload == nullptr ? "" : payload;
  addSetsCount_ = 0;
  if (!addSetsPayload_.empty()) {
    int fields = 1;
    for (char character : addSetsPayload_) {
      if (character == '|') {
        ++fields;
      }
    }
    addSetsCount_ = (fields - 1) / 3;
  }
  drawAddSetsPage(display, 0);
}

// The card's sets, 8 rows per page with previous/next like the collection.
int MenuScreen::drawAddSetsPage(St7789Display& display, int page) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t gray = display.rgb565(150, 150, 150);
  const uint16_t separator = display.rgb565(60, 60, 60);
  display.fillScreen(black);
  drawExitButton(display);
  drawRotatedText(display, "SETS", 32, 160, white);

  // The card's name above the list.
  char cardName[64] = {};
  setField(addSetsPayload_, 0, cardName, sizeof(cardName));
  std::string title = cleanText(cardName);
  const int titleLimit = kReaderHeight - 16;
  while (textWidth(title.c_str(), 1) > titleLimit && !title.empty()) {
    title.pop_back();
  }
  readerText(display, title.c_str(), 44, 8, gray, 1);

  const int totalPages = std::max(1, (addSetsCount_ + kCollectionMaxRows - 1) / kCollectionMaxRows);
  const int safePage = std::max(0, std::min(page, totalPages - 1));
  const int firstSet = safePage * kCollectionMaxRows;
  const int setCount = std::min(addSetsCount_ - firstSet, kCollectionMaxRows);
  for (int index = 0; index < setCount; ++index) {
    const int printing = firstSet + index;
    char setCode[16] = {};
    char number[16] = {};
    char setName[64] = {};
    setField(addSetsPayload_, 1 + printing * 3, setCode, sizeof(setCode));
    setField(addSetsPayload_, 2 + printing * 3, number, sizeof(number));
    setField(addSetsPayload_, 3 + printing * 3, setName, sizeof(setName));
    for (char* character = setCode; *character != '\0'; ++character) {
      *character = static_cast<char>(std::toupper(static_cast<unsigned char>(*character)));
    }

    const int rowV = kCollectionFirstRowV + index * kCollectionRowHeight;

    // "<SET> #<number>" on the right.
    char right[40] = {};
    std::snprintf(right, sizeof(right), "%s #%s", setCode, number);
    const int rightWidth = textWidth(right, 1);
    const int rightU = kReaderWidth - 6 - rightWidth;
    readerText(display, right, rightU, rowV + 9, gray, 1);

    // Set name on the left: big text when it fits, small text otherwise.
    std::string name = cleanText(setName);
    const int nameLimit = rightU - 14;
    int scale = textWidth(name.c_str(), 2) <= nameLimit ? 2 : 1;
    while (textWidth(name.c_str(), scale) > nameLimit && !name.empty()) {
      name.pop_back();
    }
    readerText(display, name.c_str(), 6, rowV + (scale == 2 ? 0 : 4), white, scale);

    readerLine(display, 6, rowV + 19, kReaderWidth - 6, rowV + 19, separator);
  }

  if (setCount == 0) {
    const int width = textWidth("NO SETS", 2);
    readerText(display, "NO SETS", (kReaderWidth - width) / 2, 150, white, 2);
  }

  // Previous / page indicator / next.
  const uint16_t buttonColor = display.rgb565(80, 80, 80);
  readerRect(display, kNavPreviousU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
  readerLine(display, kNavPreviousU + 28, kNavV + 7, kNavPreviousU + 20, kNavV + 13, white);
  readerLine(display, kNavPreviousU + 20, kNavV + 13, kNavPreviousU + 28, kNavV + 19, white);

  readerRect(display, kNavPageU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
  char pageText[24] = {};
  std::snprintf(pageText, sizeof(pageText), "%d/%d", safePage + 1, totalPages);
  const int pageScale = textWidth(pageText, 2) <= kNavWidth - 6 ? 2 : 1;
  const int pageTextWidth = textWidth(pageText, pageScale);
  readerText(display, pageText, kNavPageU + (kNavWidth - pageTextWidth) / 2,
             kNavV + (kNavHeight - 7 * pageScale) / 2 + 1, white, pageScale);

  readerRect(display, kNavNextU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
  readerLine(display, kNavNextU + 20, kNavV + 7, kNavNextU + 28, kNavV + 13, white);
  readerLine(display, kNavNextU + 28, kNavV + 13, kNavNextU + 20, kNavV + 19, white);

  display.present();
  return safePage;
}

void MenuScreen::drawCollection(St7789Display& display, const char* json) {
  collectionJson_ = json == nullptr ? "" : json;
  cJSON* cards = cJSON_Parse(collectionJson_.c_str());
  collectionCount_ = cJSON_IsArray(cards) ? cJSON_GetArraySize(cards) : 0;
  cJSON_Delete(cards);
  drawCollectionPage(display, 0);
}

void MenuScreen::readerRect(St7789Display& display, int u, int v, int width, int height,
                            uint16_t color, bool fill) const {
  // A reader rectangle (width x height) is a (height x width) buffer rectangle.
  display.drawRectangle(bufferX(u, v), bufferY(u + width, v), height, width, color, fill);
}

void MenuScreen::readerLine(St7789Display& display, int u0, int v0, int u1, int v1,
                            uint16_t color) const {
  display.drawLine(bufferX(u0, v0), bufferY(u0, v0), bufferX(u1, v1), bufferY(u1, v1), color);
}

void MenuScreen::readerText(St7789Display& display, const char* text, int u, int v, uint16_t color,
                            int scale) const {
  int currentU = u;
  for (const char* character = text; *character != '\0'; ++character) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(*character)));
    const uint8_t* pattern = glyph(upper);
    if (pattern != nullptr) {
      for (int column = 0; column < 5; ++column) {
        for (int row = 0; row < 7; ++row) {
          if ((pattern[column] & (1 << row)) == 0) {
            continue;
          }
          for (int offsetU = 0; offsetU < scale; ++offsetU) {
            for (int offsetV = 0; offsetV < scale; ++offsetV) {
              const int pixelU = currentU + column * scale + offsetU;
              const int pixelV = v + row * scale + offsetV;
              display.drawPixel(bufferX(pixelU, pixelV), bufferY(pixelU, pixelV), color);
            }
          }
        }
      }
    }
    currentU += (*character == ' ') ? 4 * scale : 6 * scale;
  }
}

void MenuScreen::drawExitButton(St7789Display& display, const char* label) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.drawRectangle(8, 8, 38, 30, white, true);
  drawRotatedText(display, label, 27, 23, black);
}

// Mirrors drawExitButton() into the opposite (top-left) corner.
void MenuScreen::drawTransferButton(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  readerRect(display, kTransferButtonU, kTransferButtonV, kTransferButtonWidth,
             kTransferButtonHeight, white, true);
  const int labelU = kTransferButtonU + (kTransferButtonWidth - 17) / 2;
  readerText(display, "IMP", labelU, kTransferButtonV + 6, black, 1);
  readerText(display, "EXP", labelU, kTransferButtonV + 21, black, 1);
}

void MenuScreen::drawTransferMenu(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display);
  const char* title = "IMP/EXP";
  readerText(display, title, (kReaderWidth - textWidth(title, 2)) / 2, 14, white, 2);

  static const char* labels[] = {"IMPORT", "EXPORT"};
  for (int index = 0; index < 2; ++index) {
    readerRect(display, kTransferMenuButtonU, kTransferMenuButtonVs[index],
               kTransferMenuButtonWidth, kTransferMenuButtonHeight, white, false);
    const int labelWidth = textWidth(labels[index], 2);
    readerText(
        display, labels[index], kTransferMenuButtonU + (kTransferMenuButtonWidth - labelWidth) / 2,
        kTransferMenuButtonVs[index] + (kTransferMenuButtonHeight - 7 * 2) / 2 + 1, white, 2);
  }
  display.present();
}

int MenuScreen::drawCollectionPage(St7789Display& display, int page) const {
  cJSON* cards = cJSON_Parse(collectionJson_.c_str());
  if (cards == nullptr || !cJSON_IsArray(cards)) {
    cJSON_Delete(cards);
    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    display.fillScreen(black);
    drawExitButton(display);
    drawRotatedText(display, "COLLECTION", 32, 160, white);
    drawRotatedText(display, "DATA ERROR", 160, 120, white);
    drawTransferButton(display);
    display.present();
    return 0;
  }

  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t separator = display.rgb565(60, 60, 60);
  const uint16_t outline = display.rgb565(140, 140, 140);
  display.fillScreen(black);
  drawExitButton(display);
  drawTransferButton(display);
  drawRotatedText(display, "COLLECTION", 32, 160, white);

  constexpr int margin = 6;
  constexpr int pipWidth = 8;
  constexpr int pipHeight = 14;
  constexpr int pipSpacing = 3;
  constexpr int maxPips = 5;
  const int totalCards = cJSON_GetArraySize(cards);
  const int totalPages = std::max(1, (totalCards + kCollectionMaxRows - 1) / kCollectionMaxRows);
  const int safePage = std::max(0, std::min(page, totalPages - 1));
  const int firstCard = safePage * kCollectionMaxRows;
  const int cardCount = std::min(totalCards - firstCard, kCollectionMaxRows);
  for (int index = 0; index < cardCount; ++index) {
    cJSON* card = cJSON_GetArrayItem(cards, firstCard + index);
    cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
    cJSON* colors = cJSON_GetObjectItemCaseSensitive(card, "colors");
    cJSON* amount = cJSON_GetObjectItemCaseSensitive(card, "amount");
    if (!cJSON_IsString(name) || !cJSON_IsArray(colors)) {
      continue;
    }

    const int rowV = kCollectionFirstRowV + index * kCollectionRowHeight;

    // Colour pips on the right, one per colour (white/blue/black/red/green).
    const int pipCount = std::min(cJSON_GetArraySize(colors), maxPips);
    const int pipsWidth = pipCount * pipWidth + std::max(0, pipCount - 1) * pipSpacing;
    const int pipsStartU = kReaderWidth - margin - pipsWidth;
    int drawnPips = 0;
    for (int colorIndex = 0; colorIndex < pipCount; ++colorIndex) {
      cJSON* color = cJSON_GetArrayItem(colors, colorIndex);
      if (!cJSON_IsString(color) || color->valuestring[0] == '\0') {
        continue;
      }
      const int pipU = pipsStartU + drawnPips * (pipWidth + pipSpacing);
      readerRect(display, pipU, rowV, pipWidth, pipHeight,
                 colorPipValue(display, color->valuestring[0]), true);
      // Outline so a black pip stays visible on the black background.
      readerRect(display, pipU, rowV, pipWidth, pipHeight, outline, false);
      ++drawnPips;
    }

    // How many are owned, right-aligned next to the pips.
    char amountText[16] = {};
    std::snprintf(amountText, sizeof(amountText), "X%d",
                  cJSON_IsNumber(amount) ? amount->valueint : 1);
    const int amountWidth = textWidth(amountText, 2);
    const int amountU = pipsStartU - margin - amountWidth;
    readerText(display, amountText, amountU, rowV, white, 2);

    // Name on the left: big text when it fits, small text otherwise.
    const int nameWidthLimit = amountU - margin - margin;
    char displayName[48] = {};
    std::strncpy(displayName, cleanText(name->valuestring).c_str(), sizeof(displayName) - 1);
    int scale = textWidth(displayName, 2) <= nameWidthLimit ? 2 : 1;
    while (textWidth(displayName, scale) > nameWidthLimit && std::strlen(displayName) > 0) {
      displayName[std::strlen(displayName) - 1] = '\0';
    }
    const int textV = rowV + (scale == 2 ? 0 : 4);
    readerText(display, displayName, margin, textV, white, scale);

    readerLine(display, margin, rowV + 19, kReaderWidth - margin, rowV + 19, separator);
  }

  if (cardCount == 0) {
    const int width = textWidth("NO CARDS", 2);
    readerText(display, "NO CARDS", (kReaderWidth - width) / 2, 150, white, 2);
  }

  // Previous / page indicator / next.
  const uint16_t buttonColor = display.rgb565(80, 80, 80);
  readerRect(display, kNavPreviousU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
  readerLine(display, kNavPreviousU + 28, kNavV + 7, kNavPreviousU + 20, kNavV + 13, white);
  readerLine(display, kNavPreviousU + 20, kNavV + 13, kNavPreviousU + 28, kNavV + 19, white);

  readerRect(display, kNavPageU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
  char pageText[24] = {};
  std::snprintf(pageText, sizeof(pageText), "%d/%d", safePage + 1, totalPages);
  const int pageScale = textWidth(pageText, 2) <= kNavWidth - 6 ? 2 : 1;
  const int pageTextWidth = textWidth(pageText, pageScale);
  readerText(display, pageText, kNavPageU + (kNavWidth - pageTextWidth) / 2,
             kNavV + (kNavHeight - 7 * pageScale) / 2 + 1, white, pageScale);

  readerRect(display, kNavNextU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
  readerLine(display, kNavNextU + 20, kNavV + 7, kNavNextU + 28, kNavV + 13, white);
  readerLine(display, kNavNextU + 28, kNavV + 13, kNavNextU + 20, kNavV + 19, white);

  display.present();
  cJSON_Delete(cards);
  return safePage;
}

int MenuScreen::hitTest(int x, int y) const {
  constexpr int touchPaddingX = 10;
  constexpr int touchPaddingY = 10;
  for (int index = 0; index < 3; ++index) {
    if (x >= kButtonX - touchPaddingX && x < kButtonX + kButtonWidth + touchPaddingX &&
        y >= kButtonYs[index] - touchPaddingY &&
        y < kButtonYs[index] + kButtonHeight + touchPaddingY) {
      return index;
    }
  }
  return -1;
}

bool MenuScreen::hitExit(int x, int y) const {
  return x >= 8 && x < 46 && y >= 8 && y < 38;
}

int MenuScreen::hitAddMenu(int x, int y) const {
  for (int index = 0; index < 3; ++index) {
    if (insideReaderRect(x, y, kAddMenuButtonU, kAddMenuButtonVs[index], kAddMenuButtonWidth,
                         kAddMenuButtonHeight, 8)) {
      return index;
    }
  }
  return -1;
}

char MenuScreen::hitKeyboardKey(int x, int y) const {
  int u = 0;
  int v = 0;
  touchToReader(x, y, u, v);
  if (u >= kEnterU && u <= kEnterU + kEnterWidth && v >= kEnterV && v <= kEnterV + kEnterHeight) {
    return '\n';
  }
  if (v < kKeyFirstV || v >= kKeyFirstV + 4 * kKeyRowStep) {
    return 0;
  }
  const int row = (v - kKeyFirstV) / kKeyRowStep;
  const int column = (u - kKeyFirstU) / (kKeyWidth + kKeyGap);
  if (column < 0 || column >= kKeyCols) {
    return 0;
  }
  const int keyU = kKeyFirstU + column * (kKeyWidth + kKeyGap);
  const int keyV = kKeyFirstV + row * kKeyRowStep;
  if (u < keyU || u > keyU + kKeyWidth || v < keyV || v > keyV + kKeyHeight) {
    return 0;
  }
  static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (row < 3) {
    return letters[row * kKeyCols + column];
  }
  if (column < 5) {
    return letters[21 + column];
  }
  return column == 5 ? ' ' : '\b';
}

bool MenuScreen::hitDemoBack(int x, int y) const {
  return insideReaderRect(x, y, kDemoBackU, kDemoBackV, kDemoBackWidth, kDemoBackHeight, 6);
}

bool MenuScreen::hitCameraShutter(int x, int y) const {
  return insideReaderRect(x, y, kShutterU, kShutterV, kShutterWidth, kShutterHeight,
                          kCaptureTouchPadding);
}

bool MenuScreen::hitCollectionPrevious(int x, int y) const {
  return insideReaderRect(x, y, kNavPreviousU, kNavV, kNavWidth, kNavHeight, kNavTouchPadding);
}

bool MenuScreen::hitCollectionNext(int x, int y) const {
  return insideReaderRect(x, y, kNavNextU, kNavV, kNavWidth, kNavHeight, kNavTouchPadding);
}

bool MenuScreen::hitTransfer(int x, int y) const {
  return insideReaderRect(x, y, kTransferButtonU, kTransferButtonV, kTransferButtonWidth,
                          kTransferButtonHeight, 8);
}

int MenuScreen::hitTransferMenu(int x, int y) const {
  for (int index = 0; index < 2; ++index) {
    if (insideReaderRect(x, y, kTransferMenuButtonU, kTransferMenuButtonVs[index],
                         kTransferMenuButtonWidth, kTransferMenuButtonHeight, 8)) {
      return index;
    }
  }
  return -1;
}

void MenuScreen::setCardInfo(const char* json) {
  cardInfoJson_ = json == nullptr ? "" : json;
}

int MenuScreen::hitCollectionRow(int x, int y, int page) const {
  int u = 0;
  int v = 0;
  touchToReader(x, y, u, v);
  const int offset = v - (kCollectionFirstRowV - 2);
  if (offset < 0) {
    return -1;
  }
  const int row = offset / kCollectionRowHeight;
  const int index = page * kCollectionMaxRows + row;
  if (row >= kCollectionMaxRows || index >= collectionCount_) {
    return -1;
  }
  return index;
}

int MenuScreen::hitAddSetRow(int x, int y, int page) const {
  int u = 0;
  int v = 0;
  touchToReader(x, y, u, v);
  const int offset = v - (kCollectionFirstRowV - 2);
  if (offset < 0) {
    return -1;
  }
  const int row = offset / kCollectionRowHeight;
  const int index = page * kCollectionMaxRows + row;
  if (row >= kCollectionMaxRows || index >= addSetsCount_) {
    return -1;
  }
  return index;
}

int MenuScreen::hitRemoveConfirm(int x, int y) const {
  if (insideReaderRect(x, y, kConfirmYesU, kConfirmYesV, kConfirmButtonWidth, kConfirmButtonHeight,
                       4)) {
    return 0;
  }
  if (insideReaderRect(x, y, kConfirmNoU, kConfirmYesV, kConfirmButtonWidth, kConfirmButtonHeight,
                       4)) {
    return 1;
  }
  if (insideReaderRect(x, y, kOneCopyU, kOneCopyV, kOneCopyWidth, kOneCopyHeight, 4)) {
    return 2;
  }
  return -1;
}

int MenuScreen::hitDetailButton(int x, int y) const {
  for (int index = 0; index < kDetailButtonCount; ++index) {
    if (insideReaderRect(x, y, kDetailButtonU, kDetailButtonVs[index], kDetailButtonWidth,
                         kDetailButtonHeight, 4)) {
      return index;
    }
  }
  return -1;
}

void MenuScreen::drawDetailTitle(St7789Display& display, const char* title) const {
  const uint16_t white = display.rgb565(255, 255, 255);
  constexpr int left = 6;
  constexpr int limit = 188;
  std::string text = cleanText(title);
  const int scale = textWidth(text.c_str(), 2) <= limit ? 2 : 1;
  while (textWidth(text.c_str(), scale) > limit && !text.empty()) {
    text.pop_back();
  }
  readerText(display, text.c_str(), left, scale == 2 ? 16 : 20, white, scale);
}

void MenuScreen::drawDetailLoading(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display, "BACK");
  drawRotatedText(display, "LOADING", 160, 120, white);
  display.present();
}

void MenuScreen::drawDetailMenu(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display, "BACK");

  cJSON* info = cJSON_Parse(cardInfoJson_.c_str());
  cJSON* name = cJSON_GetObjectItemCaseSensitive(info, "name");
  cJSON* amount = cJSON_GetObjectItemCaseSensitive(info, "amount");
  cJSON* manaCost = cJSON_GetObjectItemCaseSensitive(info, "mana_cost");
  drawDetailTitle(display, cJSON_IsString(name) ? name->valuestring : "CARD");

  // Under the name: how many are owned, then the mana cost.
  char amountText[32] = {};
  std::snprintf(amountText, sizeof(amountText), "AMOUNT: %d",
                cJSON_IsNumber(amount) ? amount->valueint : 1);
  const uint16_t gray = display.rgb565(170, 170, 170);
  readerText(display, amountText, 6, 36, gray, 1);

  std::string mana = cJSON_IsString(manaCost) ? cleanText(manaCost->valuestring) : "";
  if (mana.empty()) {
    mana = "-";
  }
  char manaText[64] = {};
  std::snprintf(manaText, sizeof(manaText), "MANA COST: %s", mana.c_str());
  readerText(display, manaText, 6, 47, gray, 1);
  cJSON_Delete(info);

  static const char* const labels[kDetailButtonCount] = {"CARD", "LEGALITIES", "OTHER", "REMOVE"};
  const uint16_t red = display.rgb565(230, 30, 30);
  for (int index = 0; index < kDetailButtonCount; ++index) {
    // The REMOVE button is red so it is not mistaken for a view button.
    const uint16_t color = index == kDetailButtonCount - 1 ? red : white;
    readerRect(display, kDetailButtonU, kDetailButtonVs[index], kDetailButtonWidth,
               kDetailButtonHeight, color, false);
    const int width = textWidth(labels[index], 2);
    readerText(display, labels[index], kDetailButtonU + (kDetailButtonWidth - width) / 2,
               kDetailButtonVs[index] + (kDetailButtonHeight - 14) / 2, color, 2);
  }
  display.present();
}

void MenuScreen::drawRemoveConfirm(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t red = display.rgb565(230, 30, 30);
  const uint16_t green = display.rgb565(30, 180, 60);
  display.fillScreen(black);

  const int titleWidth = textWidth("REMOVE CARD?", 2);
  readerText(display, "REMOVE CARD?", (kReaderWidth - titleWidth) / 2, 40, red, 2);

  // The card's name, wrapped over up to three lines and centred.
  cJSON* info = cJSON_Parse(cardInfoJson_.c_str());
  cJSON* name = cJSON_GetObjectItemCaseSensitive(info, "name");
  const std::string cardName = cJSON_IsString(name) ? cleanText(name->valuestring) : "";
  cJSON_Delete(info);
  std::vector<std::string> lines = wrapLines(cardName, kReaderWidth - 24);
  if (lines.size() > 3) {
    lines.resize(3);
    lines.back() += "...";
  }
  for (size_t index = 0; index < lines.size(); ++index) {
    const int width = textWidth(lines[index].c_str(), 1);
    readerText(display, lines[index].c_str(), (kReaderWidth - width) / 2,
               100 + static_cast<int>(index) * 14, white, 1);
  }
  const int noteWidth = textWidth("THIS CANNOT BE UNDONE", 1);
  readerText(display, "THIS CANNOT BE UNDONE", (kReaderWidth - noteWidth) / 2, 160,
             display.rgb565(150, 150, 150), 1);

  // One button per amount: take one copy, or the whole card (YES), or NO.
  readerRect(display, kOneCopyU, kOneCopyV, kOneCopyWidth, kOneCopyHeight, white, false);
  const int oneCopyWidth = textWidth("REMOVE ONE COPY", 2);
  readerText(display, "REMOVE ONE COPY", kOneCopyU + (kOneCopyWidth - oneCopyWidth) / 2,
             kOneCopyV + (kOneCopyHeight - 14) / 2, white, 2);

  struct Choice {
    const char* label;
    int u;
    uint16_t color;
  };
  const Choice choices[] = {{"YES", kConfirmYesU, red}, {"NO", kConfirmNoU, green}};
  for (const Choice& choice : choices) {
    readerRect(display, choice.u, kConfirmYesV, kConfirmButtonWidth, kConfirmButtonHeight,
               choice.color, false);
    const int width = textWidth(choice.label, 3);
    readerText(display, choice.label, choice.u + (kConfirmButtonWidth - width) / 2,
               kConfirmYesV + (kConfirmButtonHeight - 21) / 2, choice.color, 3);
  }
  display.present();
}

bool MenuScreen::drawCardView(St7789Display& display, const uint8_t* jpeg,
                              size_t jpegLength) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t gray = display.rgb565(120, 120, 120);
  display.fillScreen(black);
  drawExitButton(display, "BACK");

  // The card picture, centred at the top.
  int imageBottom = kImageTop + 204;
  DecodedImage image;
  const bool decoded = decodeJpeg(jpeg, jpegLength, image);
  if (decoded) {
    const int left = (kReaderWidth - image.width) / 2;
    for (int row = 0; row < image.height; ++row) {
      for (int column = 0; column < image.width; ++column) {
        const int u = left + column;
        const int v = kImageTop + row;
        display.drawPixel(bufferX(u, v), bufferY(u, v), image.pixels[row * image.width + column]);
      }
    }
    imageBottom = kImageTop + image.height;
    std::free(image.pixels);
  } else {
    readerRect(display, (kReaderWidth - 146) / 2, kImageTop, 146, 204, gray, false);
    const int width = textWidth("NO IMAGE", 2);
    readerText(display, "NO IMAGE", (kReaderWidth - width) / 2, kImageTop + 95, gray, 2);
  }

  // The oracle text under the card.
  cJSON* info = cJSON_Parse(cardInfoJson_.c_str());
  cJSON* oracle = cJSON_GetObjectItemCaseSensitive(info, "oracle_text");
  std::string text = cJSON_IsString(oracle) ? cleanText(oracle->valuestring) : "";
  cJSON_Delete(info);
  if (text.empty()) {
    text = "NO TEXT";
  }
  constexpr int left = 6;
  constexpr int lineHeight = 10;
  const int firstV = imageBottom + 8;
  const int maxLines = std::max(0, (kBodyLastV + 3 - firstV) / lineHeight);
  std::vector<std::string> lines = wrapLines(text, kReaderWidth - 2 * left);
  if (static_cast<int>(lines.size()) > maxLines && maxLines > 0) {
    lines.resize(maxLines);
    std::string& last = lines.back();
    while (!last.empty() && textWidth((last + "...").c_str(), 1) > kReaderWidth - 2 * left) {
      last.pop_back();
    }
    last += "...";
  }
  for (size_t index = 0; index < lines.size() && static_cast<int>(index) < maxLines; ++index) {
    readerText(display, lines[index].c_str(), left, firstV + static_cast<int>(index) * lineHeight,
               white, 1);
  }
  display.present();
  return decoded;
}

void MenuScreen::drawLegalities(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  display.fillScreen(black);
  drawExitButton(display, "BACK");
  drawDetailTitle(display, "LEGALITIES");

  cJSON* info = cJSON_Parse(cardInfoJson_.c_str());
  cJSON* legalities = cJSON_GetObjectItemCaseSensitive(info, "legalities");
  if (!cJSON_IsObject(legalities)) {
    readerText(display, "NO DATA", 6, kBodyFirstV, white, 2);
    cJSON_Delete(info);
    display.present();
    return;
  }

  constexpr int rowsPerColumn = 11;
  constexpr int rowHeight = 22;
  constexpr int boxSize = 10;
  constexpr int columnWidth = 118;
  const uint16_t green = display.rgb565(30, 180, 60);
  const uint16_t amber = display.rgb565(230, 190, 30);
  const uint16_t red = display.rgb565(230, 30, 30);

  int index = 0;
  cJSON* format = nullptr;
  cJSON_ArrayForEach(format, legalities) {
    if (index >= 2 * rowsPerColumn) {
      break;
    }
    const int u = 6 + (index / rowsPerColumn) * columnWidth;
    const int v = kBodyFirstV + (index % rowsPerColumn) * rowHeight;
    const char* status = cJSON_IsString(format) ? format->valuestring : "";
    const uint16_t color = std::strcmp(status, "legal") == 0        ? green
                           : std::strcmp(status, "restricted") == 0 ? amber
                                                                    : red;
    readerRect(display, u, v, boxSize, boxSize, color, true);
    readerText(display, format->string != nullptr ? format->string : "", u + boxSize + 6, v + 2,
               white, 1);
    ++index;
  }
  cJSON_Delete(info);
  display.present();
}

void MenuScreen::drawOther(St7789Display& display) const {
  const uint16_t black = display.rgb565(0, 0, 0);
  const uint16_t white = display.rgb565(255, 255, 255);
  const uint16_t gray = display.rgb565(150, 150, 150);
  display.fillScreen(black);
  drawExitButton(display, "BACK");
  drawDetailTitle(display, "OTHER");

  cJSON* info = cJSON_Parse(cardInfoJson_.c_str());
  if (!cJSON_IsObject(info)) {
    readerText(display, "NO DATA", 6, kBodyFirstV, white, 2);
    cJSON_Delete(info);
    display.present();
    return;
  }

  constexpr int left = 6;
  constexpr int lineHeight = 11;
  const int width = kReaderWidth - 2 * left;
  int v = kBodyFirstV;

  // "LABEL: value" on one line when it fits, otherwise the value is wrapped below.
  auto field = [&](const char* label, const std::string& value) {
    if (v + 7 > kBodyLastV) {
      return;
    }
    const std::string prefix = std::string(label) + ": ";
    readerText(display, prefix.c_str(), left, v, gray, 1);
    if (textWidth(prefix.c_str(), 1) + textWidth(value.c_str(), 1) <= width) {
      readerText(display, value.c_str(), left + textWidth(prefix.c_str(), 1), v, white, 1);
      v += lineHeight;
      return;
    }
    v += lineHeight;
    std::vector<std::string> lines = wrapLines(value, width);
    const int maxLines = std::max(0, (kBodyLastV + 4 - v) / lineHeight);
    if (static_cast<int>(lines.size()) > maxLines && maxLines > 0) {
      lines.resize(maxLines);
      std::string& last = lines.back();
      while (!last.empty() && textWidth((last + "...").c_str(), 1) > width) {
        last.pop_back();
      }
      last += "...";
    }
    for (size_t index = 0; index < lines.size() && static_cast<int>(index) < maxLines; ++index) {
      readerText(display, lines[index].c_str(), left, v, white, 1);
      v += lineHeight;
    }
  };
  auto text = [&](const char* key) -> std::string {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(info, key);
    return cJSON_IsString(item) ? cleanText(item->valuestring) : std::string();
  };
  auto number = [&](const char* key) -> std::string {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(info, key);
    if (!cJSON_IsNumber(item)) {
      return "";
    }
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%g", item->valuedouble);
    return buffer;
  };
  auto list = [&](const char* key, const char* separator) -> std::string {
    std::string joined;
    cJSON* values = cJSON_GetObjectItemCaseSensitive(info, key);
    cJSON* value = nullptr;
    cJSON_ArrayForEach(value, values) {
      if (cJSON_IsString(value)) {
        joined += (joined.empty() ? "" : separator) + cleanText(value->valuestring);
      }
    }
    return joined;
  };
  auto orDash = [](const std::string& value) { return value.empty() ? std::string("-") : value; };

  field("RESERVED", yesNo(cJSON_GetObjectItemCaseSensitive(info, "reserved")));
  field("GAME CHANGER", yesNo(cJSON_GetObjectItemCaseSensitive(info, "game_changer")));
  field("AMOUNT", orDash(number("amount")));
  field("SET", orDash(text("set")));
  field("RARITY", orDash(text("rarity")));
  field("CMC", orDash(number("cmc")));
  field("MANA COST", orDash(text("mana_cost")));
  field("TYPE", orDash(text("type_line")));
  field("COLOR IDENTITY", orDash(list("color_identity", " ")));
  field("KEYWORDS", orDash(list("keywords", ", ")));
  const std::string flavor = text("flavor_text");
  if (!flavor.empty()) {
    field("FLAVOR", flavor);
  }
  cJSON_Delete(info);
  display.present();
}
