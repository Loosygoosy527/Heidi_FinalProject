#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <math.h>

Adafruit_ST7735 screen(25, 27, 26); // CS, DC, RST
GFXcanvas16 frame(160, 128);
GFXcanvas16 puppyPortrait(88, 72);

const int MIC_PIN = 34;
const int MIC_THRESHOLD = 40;
const unsigned long HOLD_MS = 500;
const unsigned long MIC_GRACE_MS = 150;
const unsigned long SWIM_GRACE_MS = 300;
const unsigned long SCENE_END_MS = 3000;

const int START_X = 48;
const int HOME_X = 134;
const float SWIM_SPEED = 25.0f;
const int DOG_BREATHS = 3;
const float LAMP_BLOW_SECONDS = 3.6f;

enum Scene { FISH, DOG, LAMP };
Scene scene = FISH;

bool trackingBlow = false;
bool breathAccepted = false;
unsigned long blowStarted = 0;
unsigned long lastLoud = 0;
unsigned long lastFrame = 0;
unsigned long arrivedAt = 0;
unsigned long sceneEnteredAt = 0;

float fishX = START_X;
float swimPhase = 0.0f;
float sandAmount = 0.0f;
float sandPhase = 0.0f;
int dogBreaths = 0;
float sceneProgress = 0.0f;
float lampBlowSeconds = 0.0f;
bool puppyPortraitReady = false;

float clamp01(float value) {
  return fminf(1.0f, fmaxf(0.0f, value));
}

int readMicLevel() {
  int lowest = 4095;
  int highest = 0;
  unsigned long started = millis();

  while (millis() - started < 40) {
    int reading = analogRead(MIC_PIN);
    if (reading < lowest) lowest = reading;
    if (reading > highest) highest = reading;
  }

  return highest - lowest;
}

void drawCave() {
  const int x = HOME_X;
  const uint16_t shadow = 0x29E9;
  const uint16_t rock = 0x4A8E;
  const uint16_t ridge = 0x6BB2;
  const uint16_t opening = 0x1087;

  frame.fillRoundRect(x - 28, 86, 54, 21, 8, shadow);
  frame.fillCircle(x - 7, 81, 17, rock);
  frame.fillCircle(x + 3, 78, 20, rock);
  frame.fillCircle(x + 15, 85, 11, shadow);
  frame.fillRect(x - 22, 83, 43, 22, rock);

  frame.fillCircle(x, 88, 15, opening);
  frame.fillRect(x - 15, 88, 31, 18, opening);
  frame.fillCircle(x + 3, 90, 10, 0x21AB);
  frame.fillRect(x - 7, 90, 21, 15, 0x21AB);

  frame.drawLine(x - 19, 80, x - 14, 74, ridge);
  frame.drawLine(x - 8, 69, x + 1, 66, ridge);
  frame.drawLine(x + 1, 66, x + 9, 68, ridge);
  frame.drawLine(x + 15, 73, x + 20, 80, ridge);
  frame.drawLine(x - 22, 94, x - 17, 97, ridge);
  frame.drawPixel(x - 11, 73, 0x8C75);
  frame.drawPixel(x + 12, 72, 0x8C75);

  frame.fillRoundRect(x - 31, 101, 14, 7, 3, shadow);
  frame.fillRoundRect(x + 17, 100, 9, 8, 3, shadow);
  frame.drawFastHLine(x - 11, 104, 23, 0xA66A);
  frame.drawFastHLine(x - 6, 105, 14, 0xD58C);

  // A small coral branch marks the entrance.
  frame.drawLine(x - 25, 102, x - 26, 92, 0xCAEF);
  frame.drawLine(x - 26, 97, x - 30, 94, 0xCAEF);
  frame.drawLine(x - 26, 96, x - 22, 93, 0xCAEF);
  frame.drawPixel(x - 26, 91, 0xFBB5);
}

void drawOcean(unsigned long now) {
  frame.fillScreen(0x01AD);
  frame.fillRect(0, 0, 160, 32, 0x0437);
  frame.fillRect(0, 32, 160, 25, 0x0334);
  frame.fillRect(0, 57, 160, 22, 0x0271);
  frame.fillRect(0, 106, 160, 22, 0xB489);

  for (int i = 0; i < 14; i++) {
    frame.drawPixel(5 + i * 11, 112 + (i * 7) % 12, 0x83A6);
  }

  for (int i = 0; i < 6; i++) {
    int y = 102 - (now / (30 + i * 5) + i * 17) % 74;
    int x = 14 + i * 25 + roundf(2 * sinf(now * 0.003f + i));
    int radius = 1 + i % 3;
    frame.drawCircle(x, y, radius, 0x7DFF);
    if (radius == 3) frame.drawPixel(x - 1, y - 1, ST77XX_WHITE);
  }

  for (int i = 0; i < 4; i++) {
    int x = 9 + i * 9;
    int sway = roundf(3 * sinf(now * 0.003f + i));
    frame.drawLine(x, 108, x + sway, 96, 0x3D88);
    frame.drawLine(x + sway, 96, x + sway + 2, 83 + i % 3 * 5, 0x3D88);
  }

  int grainCount = roundf(18 * sandAmount);
  for (int i = 0; i < grainCount; i++) {
    float age = fmodf(sandPhase + i * 0.173f, 1.0f);
    int x = roundf(fishX) - 12 - roundf(44 * age) - (i * 7) % 13;
    int lift = roundf(22 * sinf(age * 3.14159f) * sandAmount);
    int y = 110 + (i * 7) % 9 - lift;
    uint16_t color = i % 2 ? 0xD5EC : 0xFF32;

    if (i % 4 == 0) {
      frame.fillCircle(x, y, 1, color);
    } else {
      frame.drawPixel(x, y, color);
    }
  }

  drawCave();
}

void drawFish(bool home, unsigned long now) {
  const uint16_t body = 0xFE06;
  const uint16_t fin = 0xFCA0;
  const uint16_t belly = 0xFFE9;
  int x = roundf(fishX);

  if (home) {
    unsigned long restingMs = now - arrivedAt;
    float tuck = clamp01(restingMs / 600.0f);
    int y = 89 + roundf(4 * tuck);
    int radius = 10 - roundf(2 * tuck);
    int tailX = x - 19 + roundf(9 * tuck);

    frame.fillTriangle(x - 6, y, tailX, y - 4, tailX, y + 4, fin);
    frame.fillCircle(x, y, radius, body);
    frame.fillCircle(x + 1, y + 2, radius - 4, belly);
    frame.drawFastHLine(x + 2, y - 2, 5, ST77XX_BLACK);

    if (restingMs >= 600) {
      int rise = ((restingMs - 600) / 80) % 16;
      frame.setTextColor(0xAF9E);
      frame.setCursor(HOME_X + 7, 71 - rise);
      frame.print("z");
    }
    return;
  }

  float progress = (fishX - START_X) / (HOME_X - START_X);
  int y = 72 + roundf(17 * progress + 2 * sinf(swimPhase) * (1 - progress));
  int tail = roundf(4 * sinf(swimPhase * 2));

  frame.fillTriangle(x - 7, y, x - 19, y - 5 + tail,
                     x - 19, y + 5 + tail, fin);
  frame.fillTriangle(x - 8, y - 7, x - 6, y - 14, x - 1, y - 8, body);
  frame.fillTriangle(x, y - 8, x + 4, y - 14, x + 8, y - 6, body);
  frame.fillTriangle(x - 7, y + 6, x - 6, y + 13, x, y + 8, body);
  frame.fillCircle(x, y, 10, body);
  frame.fillCircle(x + 1, y + 3, 6, belly);
  frame.fillTriangle(x - 3, y + 2, x - 9, y + 5 + tail,
                     x - 4, y + 6, fin);
  frame.fillCircle(x + 5, y - 2, 3, ST77XX_WHITE);
  frame.fillCircle(x + 6, y - 2, 1, ST77XX_BLACK);
}

void drawCloud(int x, int y) {
  frame.fillRoundRect(x, y + 5, 35, 9, 4, 0xD75F);
  frame.fillCircle(x + 8, y + 4, 6, ST77XX_WHITE);
  frame.fillCircle(x + 17, y, 9, ST77XX_WHITE);
  frame.fillCircle(x + 26, y + 4, 6, ST77XX_WHITE);
  frame.fillRoundRect(x + 2, y + 4, 31, 7, 3, ST77XX_WHITE);
}

void drawMeadow(unsigned long now) {
  frame.fillScreen(0x9E7E);
  frame.fillRect(0, 43, 160, 33, 0xBF3F);

  int drift = roundf(3.0f * sinf((now % 12000UL) * 0.000524f));
  drawCloud(10 + drift, 38);
  drawCloud(113 - drift, 31);

  frame.fillCircle(4, 111, 53, 0x9E4D);
  frame.fillCircle(146, 112, 53, 0xA68E);

  for (int x = 0; x < 160; x++) {
    int edge = 83 + roundf(4.0f * sinf(x * 0.049f));
    frame.drawFastVLine(x, edge, 128 - edge, 0x6D8C);
  }

  for (int i = 0; i < 25; i++) {
    int x = 4 + (i * 37) % 152;
    int y = 91 + (i * 13) % 35;
    uint16_t blade = i % 2 ? 0x4569 : 0xA6D0;
    frame.drawLine(x, y, x - 2, y - 3, blade);
    frame.drawLine(x, y, x + 1, y - 4 - i % 2, blade);
  }

  for (int i = 0; i < 6; i++) {
    int x = i < 3 ? 10 + 11 * i : 124 + 11 * (i - 3);
    int y = 102 + (i * 7) % 18;
    frame.drawFastVLine(x, y, 4, 0x4569);
    frame.fillCircle(x, y - 1, 2, i % 2 ? 0xFDF3 : 0xFFF6);
    frame.drawPixel(x, y - 1, 0xFDE2);
  }
}

void drawFlowerPatch(int left, int top, int columns, int rows,
                     int stage, int direction) {
  const uint16_t colors[] = {0xFB91, 0xFDF3, 0xFD48, 0xFBD8};
  float wave = clamp01(sceneProgress * DOG_BREATHS - stage);
  if (wave >= 1.0f) return;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < columns; col++) {
      int seed = row * 13 + col * 7 + left;
      float stagger = (seed % 8) * 0.025f;
      float flight = clamp01((wave - stagger) / 0.8f);
      if (flight >= 1.0f) continue;

      int x = left + col * 7 + (row % 2);
      int y = top + row * 7 + (seed % 3 - 1);
      x += direction * roundf((126 + seed % 25) * flight);
      y -= roundf((15 + seed % 27) * flight);
      y += roundf(6 * flight * sinf(flight * 8.0f + seed));

      uint16_t color = colors[seed % 4];
      if (flight < 0.12f) {
        frame.fillCircle(x, y, 6, color);
        frame.fillCircle(x - 3, y, 3, color);
        frame.fillCircle(x + 3, y, 3, color);
        frame.fillCircle(x, y - 4, 3, color);
        frame.fillCircle(x, y + 4, 3, color);
        frame.fillCircle(x, y, 2, 0xFFB1);
      } else {
        int flutter = roundf(3 * sinf(flight * 16.0f + seed));
        frame.fillCircle(x, y, 3, color);
        frame.fillCircle(x - direction * 5, y + flutter, 2, color);
        frame.drawPixel(x - direction * 9, y - 3, 0xFFB1);
      }
    }
  }
}

void drawPuppyFace(Adafruit_GFX &canvas, int x, int y, int tuft,
                   bool tongueOut) {
  const uint16_t fur = 0xED69;
  const uint16_t darkFur = 0xB386;
  const uint16_t ears = 0x9A85;
  const uint16_t cream = 0xFF34;
  const uint16_t ink = 0x4144;

  canvas.fillRoundRect(x + 7, y + 21, 20, 34, 9, ears);
  canvas.fillRoundRect(x + 61, y + 21, 20, 34, 9, ears);
  canvas.fillRoundRect(x + 12, y + 27, 8, 22, 4, darkFur);
  canvas.fillRoundRect(x + 68, y + 27, 8, 22, 4, darkFur);
  canvas.fillRoundRect(x + 17, y + 11, 54, 46, 19, fur);

  canvas.fillTriangle(x + 33, y + 16, x + 40 + tuft, y + 6,
                      x + 47, y + 16, fur);
  canvas.fillTriangle(x + 42, y + 16, x + 52 + tuft, y + 7,
                      x + 56, y + 16, fur);

  canvas.fillCircle(x + 29, y + 43, 3, 0xFBB1);
  canvas.fillCircle(x + 59, y + 43, 3, 0xFBB1);
  canvas.fillRoundRect(x + 27, y + 40, 34, 16, 7, cream);
  canvas.fillRoundRect(x + 39, y + 41, 10, 6, 3, ink);

  canvas.fillCircle(x + 32, y + 34, 3, ink);
  canvas.fillCircle(x + 56, y + 34, 3, ink);
  canvas.drawPixel(x + 31, y + 33, ST77XX_WHITE);
  canvas.drawPixel(x + 55, y + 33, ST77XX_WHITE);

  if (tongueOut) {
    canvas.fillRoundRect(x + 39, y + 49, 11, 7, 3, ink);
    canvas.fillRoundRect(x + 41, y + 52, 8, 10, 3, 0xFB31);
    canvas.drawFastVLine(x + 45, y + 53, 5, 0xD28C);
  } else {
    canvas.drawFastHLine(x + 41, y + 50, 7, ink);
  }
}

void drawTiltedPuppyFace() {
  uint16_t *pixels = puppyPortrait.getBuffer();
  if (pixels == nullptr) {
    drawPuppyFace(frame, 36, 28, 0, true);
    return;
  }

  // Build the tilted sprite once, then reuse it for every ending frame.
  if (!puppyPortraitReady) {
    constexpr int width = 88;
    constexpr int height = 72;
    constexpr int pivotX = 44;
    constexpr int pivotY = 35;
    constexpr float angle = 0.25f;

    uint16_t *framePixels = frame.getBuffer();
    if (framePixels == nullptr) return;

    // Use the frame as temporary drawing space before the first meadow frame.
    frame.fillRect(0, 0, width, height, 0);
    drawPuppyFace(frame, 0, 0, 0, true);
    float cosine = cosf(angle);
    float sine = sinf(angle);

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int dx = x - pivotX;
        int dy = y - pivotY;
        int sourceX = roundf(pivotX + cosine * dx + sine * dy);
        int sourceY = roundf(pivotY - sine * dx + cosine * dy);

        uint16_t color = 0;
        if (sourceX >= 0 && sourceX < width &&
            sourceY >= 0 && sourceY < height) {
          color = framePixels[sourceY * 160 + sourceX];
        }
        pixels[y * width + x] = color;
      }
    }

    puppyPortraitReady = true;
  }

  for (int y = 0; y < 72; y++) {
    for (int x = 0; x < 88; x++) {
      uint16_t color = pixels[y * 88 + x];
      if (color != 0) {
        frame.drawPixel(36 + x, 28 + y, color);
      }
    }
  }
}

void drawDog(unsigned long now, bool petalsMoving) {
  const uint16_t fur = 0xED69;
  const uint16_t darkFur = 0xB386;
  const uint16_t cream = 0xFF34;
  const uint16_t ink = 0x4144;
  bool happy = sceneProgress >= 1.0f;

  drawMeadow(now);
  frame.fillRoundRect(43, 113, 80, 9, 4, 0x4CC9);

  int tailY = 100 + roundf((happy ? 6 : 3) *
                          sinf((now % 6000UL) * 0.0105f));
  frame.fillTriangle(100, 101, 123, tailY - 4, 109, 111, darkFur);
  frame.fillCircle(123, tailY, 5, darkFur);
  frame.fillCircle(125, tailY - 1, 3, cream);

  frame.fillCircle(61, 103, 14, darkFur);
  frame.fillCircle(99, 103, 14, darkFur);
  frame.fillRoundRect(57, 80, 46, 34, 14, fur);
  frame.fillRoundRect(69, 84, 22, 24, 8, cream);
  frame.fillRoundRect(62, 94, 14, 23, 5, fur);
  frame.fillRoundRect(84, 94, 14, 23, 5, fur);
  frame.fillRoundRect(60, 108, 17, 10, 4, cream);
  frame.fillRoundRect(83, 108, 17, 10, 4, cream);
  frame.drawFastVLine(65, 114, 3, darkFur);
  frame.drawFastVLine(71, 114, 3, darkFur);
  frame.drawFastVLine(89, 114, 3, darkFur);
  frame.drawFastVLine(95, 114, 3, darkFur);

  frame.fillRoundRect(60, 83, 40, 8, 3, 0x3D7D);
  frame.fillCircle(80, 92, 4, 0xFDE2);
  frame.drawPixel(79, 91, ST77XX_WHITE);

  if (happy) {
    drawTiltedPuppyFace();
  } else {
    int tuft = petalsMoving ? roundf(4 * sinf(now * 0.021f)) : 0;
    drawPuppyFace(frame, 36, 28, tuft, false);
  }

  drawFlowerPatch(43, 48, 3, 6, 0, -1);
  drawFlowerPatch(102, 48, 3, 6, 0, 1);
  drawFlowerPatch(54, 36, 8, 5, 1, 1);
  drawFlowerPatch(55, 70, 8, 3, 2, 1);

  for (int i = 0; i < DOG_BREATHS; i++) {
    int x = 68 + 12 * i;
    if (i < dogBreaths) {
      frame.fillCircle(x, 123, 2, ink);
    } else {
      frame.drawCircle(x, 123, 2, ink);
    }
  }
}

uint16_t blend565(uint16_t from, uint16_t to, float amount) {
  int red = roundf((from >> 11) * (1 - amount) + (to >> 11) * amount);
  int green = roundf(((from >> 5) & 63) * (1 - amount) +
                     ((to >> 5) & 63) * amount);
  int blue = roundf((from & 31) * (1 - amount) +
                    (to & 31) * amount);
  return (red << 11) | (green << 5) | blue;
}

void drawCenteredText(const char *text, int y) {
  int width = 0;
  for (const char *letter = text; *letter; letter++) width += 6;
  frame.setCursor((160 - width) / 2, y);
  frame.print(text);
}

void drawSparkle(int x, int y, int size, uint16_t color) {
  frame.fillTriangle(x, y - size, x - 2, y, x + 2, y, color);
  frame.fillTriangle(x, y + size, x - 2, y, x + 2, y, color);
  frame.drawFastHLine(x - size, y, 2 * size + 1, color);
}

void drawLampReminder(unsigned long now) {
  const uint16_t card = 0x222D;
  const uint16_t gold = 0xFEDD;
  const uint16_t mint = 0xB755;

  frame.fillRoundRect(13, 9, 134, 41, 10, card);
  frame.drawRoundRect(13, 9, 134, 41, 10, gold);

  frame.setTextColor(ST77XX_WHITE);
  drawCenteredText("Remember to take", 17);
  frame.setTextColor(gold);
  drawCenteredText("deep breaths :)", 31);

  int drift = roundf(2 * sinf((now - arrivedAt) * 0.004f));
  drawSparkle(29, 64 + drift, 5, gold);
  drawSparkle(131, 64 - drift, 5, gold);
  drawSparkle(47, 76 - drift, 3, ST77XX_WHITE);
  drawSparkle(113, 80 + drift, 3, ST77XX_WHITE);

  for (int side = 0; side < 2; side++) {
    int x = side == 0 ? 28 : 132;
    int inward = side == 0 ? 1 : -1;
    frame.drawLine(x, 108, x + 4 * inward, 88, mint);
    frame.fillTriangle(x + 2 * inward, 99, x - 7 * inward, 92,
                       x - 4 * inward, 100, mint);
    frame.fillTriangle(x + 3 * inward, 95, x + 12 * inward, 88,
                       x + 10 * inward, 96, mint);
    frame.fillCircle(x + 4 * inward, 87, 2, gold);
  }
}

void drawLamp(unsigned long now) {
  const uint16_t wall = 0x190F;
  const uint16_t warmGlow = 0xFE46;
  float light = sceneProgress;
  frame.fillScreen(wall);

  if (light > 0.0f) {
    int radius = 15 + roundf(87 * light);

    for (int layer = 0; layer < 6; layer++) {
      int rx = radius - layer * radius / 12;
      int ry = 5 + rx * 3 / 4;
      int cy = 67 - roundf(34 * light);
      uint16_t glow = blend565(wall, warmGlow, light * (layer + 1) / 6.0f);

      for (int y = 0; y < 107; y++) {
        float dy = (y - cy) / float(ry);
        if (fabsf(dy) >= 1.0f) continue;

        int halfWidth = roundf(rx * sqrtf(1.0f - dy * dy));
        frame.drawFastHLine(80 - halfWidth, y, 2 * halfWidth + 1, glow);
      }
    }
  }

  frame.fillRect(0, 110, 160, 18, 0x10A8);
  frame.drawFastHLine(0, 109, 160, 0x4235);
  frame.fillRoundRect(46, 101, 68, 27, 4, 0x31D9);
  frame.fillRoundRect(52, 105, 56, 14, 3, 0x427D);
  frame.drawFastHLine(72, 112, 16, 0x212F);
  frame.fillRoundRect(67, 96, 26, 5, 2, 0x841B);
  frame.fillRect(78, 73, 4, 24, 0x841B);
  frame.drawFastVLine(92, 74, 9, 0x841B);
  frame.fillCircle(92, 83, 2, 0xA55F);

  uint16_t shade = blend565(0x4238, 0xFA86, light);
  frame.fillRect(71, 55, 18, 20, shade);
  frame.fillTriangle(71, 55, 62, 75, 71, 75, shade);
  frame.fillTriangle(88, 55, 98, 75, 88, 75, shade);
  frame.drawFastHLine(62, 75, 37, blend565(0x529C, 0xFECB, light));

  if (light < 0.4f) {
    const int starX[] = {24, 49, 112, 135};
    const int starY[] = {54, 76, 51, 70};

    for (int i = 0; i < 4; i++) {
      float twinkle = 0.75f + 0.25f * sinf((now % 8000) * 0.002f + i);
      uint16_t color = blend565(wall, 0xE71F, twinkle * (1 - light / 0.4f));
      int size = 1 + i % 2;
      frame.drawFastHLine(starX[i] - size, starY[i], 2 * size + 1, color);
      frame.drawFastVLine(starX[i], starY[i] - size, 2 * size + 1, color);
    }
  }

  if (light >= 1.0f) {
    drawLampReminder(now);
  } else {
    const char *cue = "Exhale...";

    if (breathAccepted) {
      cue = "Let the light grow";
    } else if (now - sceneEnteredAt < 1400 ||
               (light > 0.0f && !trackingBlow && now - lastLoud < 1400)) {
      cue = "Breathe in";
    }

    frame.fillRoundRect(22, 12, 116, 17, 6, wall);
    frame.setTextColor(ST77XX_WHITE);
    drawCenteredText(cue, 17);
    frame.drawRoundRect(55, 35, 50, 5, 2, 0x841B);

    int barWidth = roundf(46 * light);
    if (barWidth > 0) {
      frame.drawFastHLine(57, 37, barWidth, 0xFEDD);
    }
  }
}

void drawScene(bool swimming, unsigned long now) {
  if (scene == LAMP) {
    drawLamp(now);
  } else {
    const char *message;

    if (scene == FISH) {
      bool home = fishX >= HOME_X;
      drawOcean(now);
      drawFish(home, now);

      if (home) {
        message = "Safe in the cave!";
      } else if (swimming) {
        message = "Swimming...";
      } else if (fishX > START_X) {
        message = "Take a deep breath         and exhale...";
      } else {
        message = "Exhale to swim";
      }
    } else {
      bool petalsMoving = sceneProgress < dogBreaths / float(DOG_BREATHS);
      drawDog(now, petalsMoving);

      if (sceneProgress >= 1.0f) {
        message = "Hello, little pup!";
      } else if (petalsMoving) {
        message = "There go the petals!";
      } else {
        message = "Take a deep breathe        and exhale...";
      }
    }

    frame.setTextColor(scene == DOG ? 0x2128 : ST77XX_WHITE);
    frame.setCursor(8, 10);
    frame.print(message);
  }

  screen.drawRGBBitmap(0, 0, frame.getBuffer(), 160, 128);
}

void setup() {
  Serial.begin(115200);
  SPI.begin(18, -1, 23, 25);
  screen.initR(INITR_BLACKTAB);
  screen.setRotation(1);

  // Cache the artwork before the mic and animation clocks start.
  drawTiltedPuppyFace();

  lastFrame = millis();
  drawScene(false, lastFrame);
}

void loop() {
  int level = readMicLevel();
  unsigned long now = millis();
  Serial.println(level);

  bool newBreath = false;
  unsigned long allowedGap = breathAccepted ? SWIM_GRACE_MS : MIC_GRACE_MS;

  if (trackingBlow && now - lastLoud >= allowedGap) {
    trackingBlow = false;
    breathAccepted = false;
  }

  if (level > MIC_THRESHOLD) {
    if (!trackingBlow) {
      blowStarted = now;
      trackingBlow = true;
    }

    lastLoud = now;

    if (now - blowStarted >= HOLD_MS) {
      newBreath = !breathAccepted;
      breathAccepted = true;
    }
  }

  bool sceneFinished = scene == FISH
                         ? fishX >= HOME_X
                         : sceneProgress >= 1.0f;

  if (sceneFinished && now - arrivedAt >= SCENE_END_MS) {
    if (scene == FISH) {
      scene = DOG;
    } else if (scene == DOG) {
      scene = LAMP;
    } else {
      scene = FISH;
    }

    sceneEnteredAt = now;
    fishX = START_X;
    swimPhase = 0.0f;
    sandAmount = 0.0f;
    sandPhase = 0.0f;
    dogBreaths = 0;
    sceneProgress = 0.0f;
    lampBlowSeconds = 0.0f;
    trackingBlow = false;
    breathAccepted = false;
    newBreath = false;
  }

  float seconds = (now - lastFrame) / 1000.0f;
  lastFrame = now;
  bool swimming = scene == FISH && breathAccepted && fishX < HOME_X;

  if (scene == FISH) {
    if (swimming) {
      fishX += SWIM_SPEED * seconds;
      swimPhase += 9.0f * seconds;

      if (fishX >= HOME_X) {
        fishX = HOME_X;
        arrivedAt = now;
      }
    }

    sandAmount = clamp01(sandAmount + (swimming ? 3.0f : -1.7f) * seconds);
    sandPhase = fmodf(sandPhase + seconds * (swimming ? 1.2f : 0.55f), 1.0f);

  } else if (sceneProgress < 1.0f) {
    if (scene == DOG) {
      if (newBreath && dogBreaths < DOG_BREATHS) {
        dogBreaths++;
      }

      float target = dogBreaths / float(DOG_BREATHS);
      sceneProgress = fminf(target, sceneProgress + 0.32f * seconds);

    } else if (breathAccepted && level > MIC_THRESHOLD) {
      float blowTime = newBreath ? (now - blowStarted) / 1000.0f : seconds;
      lampBlowSeconds += blowTime;
      sceneProgress = clamp01(lampBlowSeconds / LAMP_BLOW_SECONDS);
    }

    if (sceneProgress >= 1.0f) {
      arrivedAt = now;
    }
  }

  drawScene(swimming, now);
}