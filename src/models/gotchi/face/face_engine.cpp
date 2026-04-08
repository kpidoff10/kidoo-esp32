#include "face_engine.h"
#include "face_renderer.h"
#include <cstdlib>
#include <cmath>
#include <esp_random.h>

namespace {

// ============================================
// État courant (interpolé chaque frame)
// ============================================
EyeConfig s_currentLeft;
EyeConfig s_currentRight;
float s_currentLookX = 0;
float s_currentLookY = 0;
float s_mouthState = 0;

// ============================================
// Cible (preset + regard)
// ============================================
EyeConfig s_targetLeft;
EyeConfig s_targetRight;
float s_targetLookX = 0;
float s_targetLookY = 0;

FaceExpression s_currentExpr = FaceExpression::Normal;

// ============================================
// Transition expression
// ============================================
constexpr uint32_t TRANSITION_DURATION_MS = 300;
uint32_t s_transitionElapsed = 0;
bool s_transitioning = false;
EyeConfig s_transStartLeft;
EyeConfig s_transStartRight;

// ============================================
// Blink
// ============================================
enum class BlinkPhase { None, Closing, Closed, Opening };
BlinkPhase s_blinkPhase = BlinkPhase::None;
uint32_t s_blinkTimer = 0;
constexpr uint32_t BLINK_CLOSE_MS = 80;
constexpr uint32_t BLINK_HOLD_MS  = 50;
constexpr uint32_t BLINK_OPEN_MS  = 80;
int16_t s_blinkSavedHeightL = 0;
int16_t s_blinkSavedHeightR = 0;
float s_blinkSavedSlopeTopL = 0;
float s_blinkSavedSlopeTopR = 0;
float s_blinkSavedSlopeBotL = 0;
float s_blinkSavedSlopeBotR = 0;

// ============================================
// Auto behavior (blink + idle look)
// ============================================
bool s_autoMode = true;
uint32_t s_nextBlinkIn = 0;
uint32_t s_nextLookIn = 0;

// ============================================
// Gestes (nod/shake)
// ============================================
enum class GestureType { None, Nod, Shake };
GestureType s_gestureType = GestureType::None;
float s_gestureAngle = 0;      // Angle courant du sinus
float s_gestureSpeed = 0;      // Vitesse angulaire (rad/s)
float s_gestureAmplitude = 0;  // Amplitude du mouvement
int s_gestureCycles = 0;       // Nombre de cycles restants
int s_gestureTotalCycles = 0;  // Nombre de cycles total

uint32_t randomRange(uint32_t minMs, uint32_t maxMs) {
  return minMs + (rand() % (maxMs - minMs + 1));
}

void scheduleNextBlink() {
  s_nextBlinkIn = randomRange(2000, 6000);
}

void scheduleNextLook() {
  s_nextLookIn = randomRange(1500, 4000);
}

float flerp(float a, float b, float t) {
  return a + (b - a) * t;
}

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// ============================================
// Mise à jour transition expression
// ============================================
void updateTransition(uint32_t dtMs) {
  if (!s_transitioning) return;

  s_transitionElapsed += dtMs;
  float t = (float)s_transitionElapsed / TRANSITION_DURATION_MS;
  if (t >= 1.0f) {
    t = 1.0f;
    s_transitioning = false;
  }

  s_currentLeft = s_transStartLeft.lerp(s_targetLeft, t);
  s_currentRight = s_transStartRight.lerp(s_targetRight, t);
}

// ============================================
// Mise à jour blink
// ============================================
void updateBlink(uint32_t dtMs) {
  if (s_blinkPhase == BlinkPhase::None) return;

  s_blinkTimer += dtMs;

  switch (s_blinkPhase) {
    case BlinkPhase::Closing: {
      float t = clampf((float)s_blinkTimer / BLINK_CLOSE_MS, 0, 1);
      s_currentLeft.height = (int16_t)(s_blinkSavedHeightL * (1.0f - t));
      s_currentRight.height = (int16_t)(s_blinkSavedHeightR * (1.0f - t));
      // Réduire le slope proportionnellement
      s_currentLeft.slopeTop = s_blinkSavedSlopeTopL * (1.0f - t);
      s_currentLeft.slopeBottom = s_blinkSavedSlopeBotL * (1.0f - t);
      s_currentRight.slopeTop = s_blinkSavedSlopeTopR * (1.0f - t);
      s_currentRight.slopeBottom = s_blinkSavedSlopeBotR * (1.0f - t);
      if (s_blinkTimer >= BLINK_CLOSE_MS) {
        s_blinkPhase = BlinkPhase::Closed;
        s_blinkTimer = 0;
      }
      break;
    }
    case BlinkPhase::Closed: {
      s_currentLeft.height = 1;
      s_currentRight.height = 1;
      s_currentLeft.slopeTop = s_currentLeft.slopeBottom = 0;
      s_currentRight.slopeTop = s_currentRight.slopeBottom = 0;
      if (s_blinkTimer >= BLINK_HOLD_MS) {
        s_blinkPhase = BlinkPhase::Opening;
        s_blinkTimer = 0;
      }
      break;
    }
    case BlinkPhase::Opening: {
      float t = clampf((float)s_blinkTimer / BLINK_OPEN_MS, 0, 1);
      s_currentLeft.height = (int16_t)(s_blinkSavedHeightL * t);
      s_currentRight.height = (int16_t)(s_blinkSavedHeightR * t);
      s_currentLeft.slopeTop = s_blinkSavedSlopeTopL * t;
      s_currentLeft.slopeBottom = s_blinkSavedSlopeBotL * t;
      s_currentRight.slopeTop = s_blinkSavedSlopeTopR * t;
      s_currentRight.slopeBottom = s_blinkSavedSlopeBotR * t;
      if (s_blinkTimer >= BLINK_OPEN_MS) {
        s_blinkPhase = BlinkPhase::None;
        s_currentLeft.height = s_blinkSavedHeightL;
        s_currentRight.height = s_blinkSavedHeightR;
        s_currentLeft.slopeTop = s_blinkSavedSlopeTopL;
        s_currentLeft.slopeBottom = s_blinkSavedSlopeBotL;
        s_currentRight.slopeTop = s_blinkSavedSlopeTopR;
        s_currentRight.slopeBottom = s_blinkSavedSlopeBotR;
      }
      break;
    }
    default: break;
  }
}

// ============================================
// Mise à jour regard (smooth vers target)
// ============================================
void updateLook(uint32_t dtMs) {
  // Ease-out : rapide au début, ralentit vers la cible
  float dist = fabsf(s_targetLookX - s_currentLookX) + fabsf(s_targetLookY - s_currentLookY);
  float speed = 0.06f + dist * 0.12f;
  if (speed > 0.25f) speed = 0.25f;
  s_currentLookX = flerp(s_currentLookX, s_targetLookX, speed);
  s_currentLookY = flerp(s_currentLookY, s_targetLookY, speed);
}

// ============================================
// Mise à jour geste (nod/shake)
// ============================================
void updateGesture(uint32_t dtMs) {
  if (s_gestureType == GestureType::None) return;

  float sec = dtMs / 1000.0f;
  s_gestureAngle += s_gestureSpeed * sec;

  // Mouvement sinusoïdal
  float val = sinf(s_gestureAngle) * s_gestureAmplitude;

  // Décroissance de l'amplitude au fil des cycles (plus naturel)
  float progress = (float)(s_gestureTotalCycles - s_gestureCycles) / s_gestureTotalCycles;
  float decay = 1.0f - progress * 0.4f; // Réduit de 40% vers la fin
  val *= decay;

  if (s_gestureType == GestureType::Nod) {
    s_currentLookY = val;
    s_currentLookX = flerp(s_currentLookX, 0, 0.1f); // Recentre X
  } else {
    s_currentLookX = val;
    s_currentLookY = flerp(s_currentLookY, 0, 0.1f); // Recentre Y
  }

  // Compter les demi-cycles (passage par zéro)
  static float s_prevVal = 0;
  if ((s_prevVal >= 0 && val < 0) || (s_prevVal <= 0 && val > 0)) {
    s_gestureCycles--;
    if (s_gestureCycles <= 0) {
      s_gestureType = GestureType::None;
      s_currentLookX = 0;
      s_currentLookY = 0;
    }
  }
  s_prevVal = val;
}

// ============================================
// Auto behavior
// ============================================
void updateAuto(uint32_t dtMs) {
  if (!s_autoMode) return;

  // Auto blink
  if (s_nextBlinkIn <= dtMs) {
    FaceEngine::blink();
    scheduleNextBlink();
  } else {
    s_nextBlinkIn -= dtMs;
  }

  // Auto look (micro-movements)
  if (s_nextLookIn <= dtMs) {
    s_targetLookX = ((float)(rand() % 200) - 100) / 600.0f; // ±0.17
    s_targetLookY = ((float)(rand() % 200) - 100) / 800.0f; // ±0.12
    scheduleNextLook();
  } else {
    s_nextLookIn -= dtMs;
  }
}

} // namespace

namespace FaceEngine {

void init() {
  FaceRenderer::init();

  FacePreset preset = FacePresets::getPreset(FaceExpression::Normal);
  s_currentLeft = preset.left;
  s_currentRight = preset.right;
  s_targetLeft = preset.left;
  s_targetRight = preset.right;
  s_currentExpr = FaceExpression::Normal;

  srand(esp_random());
  scheduleNextBlink();
  scheduleNextLook();

  FaceRenderer::render(s_currentLeft, s_currentRight, 0, 0);
}

void update(uint32_t dtMs) {
  updateTransition(dtMs);
  updateBlink(dtMs);
  if (s_gestureType != GestureType::None) {
    updateGesture(dtMs); // Le geste override le look
  } else {
    updateLook(dtMs);
  }
  updateAuto(dtMs);

  FaceRenderer::render(s_currentLeft, s_currentRight, s_currentLookX, s_currentLookY, s_mouthState);
}

void setExpression(FaceExpression expr) {
  s_currentExpr = expr;
  FacePreset preset = FacePresets::getPreset(expr);

  s_transStartLeft = s_currentLeft;
  s_transStartRight = s_currentRight;
  s_targetLeft = preset.left;
  s_targetRight = preset.right;
  s_transitionElapsed = 0;
  s_transitioning = true;
}

void lookAt(float x, float y) {
  s_targetLookX = clampf(x, -1.0f, 1.0f);
  s_targetLookY = clampf(y, -1.0f, 1.0f);
}

void blink() {
  if (s_blinkPhase != BlinkPhase::None) return; // Déjà en cours

  s_blinkSavedHeightL = s_currentLeft.height > 0 ? s_currentLeft.height : s_targetLeft.height;
  s_blinkSavedHeightR = s_currentRight.height > 0 ? s_currentRight.height : s_targetRight.height;
  s_blinkSavedSlopeTopL = s_currentLeft.slopeTop;
  s_blinkSavedSlopeTopR = s_currentRight.slopeTop;
  s_blinkSavedSlopeBotL = s_currentLeft.slopeBottom;
  s_blinkSavedSlopeBotR = s_currentRight.slopeBottom;
  s_blinkPhase = BlinkPhase::Closing;
  s_blinkTimer = 0;
}

void setAutoMode(bool enabled) {
  s_autoMode = enabled;
  if (enabled) {
    scheduleNextBlink();
    scheduleNextLook();
  }
}

void setMouthState(float state) {
  s_mouthState = state;
}

void nod(GestureSpeed speed) {
  s_gestureType = GestureType::Nod;
  s_gestureAngle = 0;
  switch (speed) {
    case GestureSpeed::Slow:   s_gestureSpeed = 8.0f;  s_gestureAmplitude = 0.5f; s_gestureCycles = 4; break;
    case GestureSpeed::Normal: s_gestureSpeed = 12.0f;  s_gestureAmplitude = 0.6f; s_gestureCycles = 5; break;
    case GestureSpeed::Fast:   s_gestureSpeed = 18.0f;  s_gestureAmplitude = 0.4f; s_gestureCycles = 7; break;
  }
  s_gestureTotalCycles = s_gestureCycles;
}

void shake(GestureSpeed speed) {
  s_gestureType = GestureType::Shake;
  s_gestureAngle = 0;
  switch (speed) {
    case GestureSpeed::Slow:   s_gestureSpeed = 8.0f;  s_gestureAmplitude = 0.6f; s_gestureCycles = 4; break;
    case GestureSpeed::Normal: s_gestureSpeed = 14.0f;  s_gestureAmplitude = 0.7f; s_gestureCycles = 6; break;
    case GestureSpeed::Fast:   s_gestureSpeed = 20.0f;  s_gestureAmplitude = 0.5f; s_gestureCycles = 8; break;
  }
  s_gestureTotalCycles = s_gestureCycles;
}

bool isGesturePlaying() {
  return s_gestureType != GestureType::None;
}

FaceExpression getCurrentExpression() {
  return s_currentExpr;
}

} // namespace FaceEngine
