#include "emotion_manager.h"
#include "../../../model_config.h"

#ifdef HAS_LCD
#include "../../../common/managers/lcd/lcd_manager.h"
#endif

Emotion EmotionManager::_current = Emotion::Happy;

bool EmotionManager::init() {
  _current = Emotion::Happy;
  return true;
}

void EmotionManager::setEmotion(Emotion e) {
  if (e >= Emotion::Count) return;
  _current = e;
  drawFace();
}

Emotion EmotionManager::getEmotion() {
  return _current;
}

void EmotionManager::drawFace() {
  drawFace(_current);
}

void EmotionManager::drawFace(Emotion e) {
#ifdef HAS_LCD
  if (!LCDManager::isAvailable()) return;
  LCDManager::fillScreen(LCDManager::COLOR_BLACK);
  switch (e) {
    case Emotion::Happy:   drawHappy();   break;
    case Emotion::Sad:     drawSad();     break;
    case Emotion::Hungry:  drawHungry();  break;
    case Emotion::Sleepy:  drawSleepy();  break;
    case Emotion::Sick:    drawSick();    break;
    case Emotion::Angry:   drawAngry();   break;
    case Emotion::Neutral: drawNeutral(); break;
    default:               drawHappy();   break;
  }
#else
  (void)e;
#endif
}

void EmotionManager::drawEyes(int16_t cx, int16_t cy, int16_t eyeRadius, uint16_t color) {
#ifdef HAS_LCD
  int16_t dx = 42;
  LCDManager::fillCircle(cx - dx, cy - 42, eyeRadius, color);
  LCDManager::fillCircle(cx + dx, cy - 42, eyeRadius, color);
#else
  (void)cx; (void)cy; (void)eyeRadius; (void)color;
#endif
}

void EmotionManager::drawSmile(int16_t cx, int16_t mouthY, int16_t radius, uint16_t color) {
#ifdef HAS_LCD
  // Sourire = demi-cercle du bas : on dessine un cercle puis on masque le haut
  LCDManager::fillCircle(cx, mouthY, radius, color);
  LCDManager::fillRect(cx - radius - 1, mouthY - radius - 1, (radius + 1) * 2, radius + 1, LCDManager::COLOR_BLACK);
#else
  (void)cx; (void)mouthY; (void)radius; (void)color;
#endif
}

void EmotionManager::drawFrown(int16_t cx, int16_t mouthY, int16_t radius, uint16_t color) {
#ifdef HAS_LCD
  // Triste = demi-cercle du haut
  LCDManager::fillCircle(cx, mouthY, radius, color);
  LCDManager::fillRect(cx - radius - 1, mouthY, (radius + 1) * 2, radius + 1, LCDManager::COLOR_BLACK);
#else
  (void)cx; (void)mouthY; (void)radius; (void)color;
#endif
}

void EmotionManager::drawLineMouth(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
#ifdef HAS_LCD
  LCDManager::drawLine(x0, y0, x1, y1, color);
#else
  (void)x0; (void)y0; (void)x1; (void)y1; (void)color;
#endif
}

void EmotionManager::drawHappy() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  drawEyes(cx, cy, 14, faceColor);
  drawSmile(cx, cy + 52, 28, faceColor);
#endif
}

void EmotionManager::drawSad() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  drawEyes(cx, cy, 14, faceColor);
  drawFrown(cx, cy + 38, 28, faceColor);
#endif
}

void EmotionManager::drawHungry() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  drawEyes(cx, cy, 14, faceColor);
  // Bouche ouverte (cercle)
  LCDManager::fillCircle(cx, cy + 52, 18, faceColor);
  LCDManager::fillCircle(cx, cy + 52, 12, LCDManager::COLOR_BLACK);
#endif
}

void EmotionManager::drawSleepy() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  int16_t dx = 42;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  // Yeux en demi-lune (fermés) : traits
  LCDManager::drawLine(cx - dx - 18, cy - 42, cx - dx + 18, cy - 42, faceColor);
  LCDManager::drawLine(cx + dx - 18, cy - 42, cx + dx + 18, cy - 42, faceColor);
  // Petit sourire
  drawSmile(cx, cy + 52, 20, faceColor);
#endif
}

void EmotionManager::drawSick() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  // Yeux un peu plus petits, bouche neutre / légère tristesse
  drawEyes(cx, cy, 10, faceColor);
  drawLineMouth(cx - 22, cy + 55, cx + 22, cy + 55, faceColor);
#endif
}

void EmotionManager::drawAngry() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  int16_t dx = 42;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  // Sourcils en V au-dessus des yeux
  LCDManager::drawLine(cx - dx - 12, cy - 58, cx - dx + 12, cy - 48, faceColor);
  LCDManager::drawLine(cx + dx - 12, cy - 48, cx + dx + 12, cy - 58, faceColor);
  drawEyes(cx, cy, 12, faceColor);
  // Bouche en ligne descendante (colère)
  LCDManager::drawLine(cx - 20, cy + 48, cx + 20, cy + 58, faceColor);
#endif
}

void EmotionManager::drawNeutral() {
#ifdef HAS_LCD
  int16_t w = LCDManager::width();
  int16_t h = LCDManager::height();
  int16_t cx = w / 2;
  int16_t cy = h / 2;
  uint16_t faceColor = LCDManager::COLOR_WHITE;
  drawEyes(cx, cy, 14, faceColor);
  drawLineMouth(cx - 25, cy + 52, cx + 25, cy + 52, faceColor);
#endif
}
