#ifndef FACE_CONFIG_H
#define FACE_CONFIG_H

#include <cstdint>
#include <cstring>
#include <strings.h>

// ============================================
// Structure d'un oeil (forme paramétrique)
// ============================================
struct EyeConfig {
  int16_t height;       // Hauteur en pixels
  int16_t width;        // Largeur en pixels
  int16_t offsetX;      // Décalage horizontal (asymétrie)
  int16_t offsetY;      // Décalage vertical
  int16_t radiusTop;    // Arrondi coins haut
  int16_t radiusBottom; // Arrondi coins bas
  float slopeTop;       // Inclinaison bord haut (-0.5 à 0.5)
  float slopeBottom;    // Inclinaison bord bas

  // Interpolation linéaire vers une autre config
  EyeConfig lerp(const EyeConfig& target, float t) const {
    return {
      (int16_t)(height + (target.height - height) * t),
      (int16_t)(width + (target.width - width) * t),
      (int16_t)(offsetX + (target.offsetX - offsetX) * t),
      (int16_t)(offsetY + (target.offsetY - offsetY) * t),
      (int16_t)(radiusTop + (target.radiusTop - radiusTop) * t),
      (int16_t)(radiusBottom + (target.radiusBottom - radiusBottom) * t),
      slopeTop + (target.slopeTop - slopeTop) * t,
      slopeBottom + (target.slopeBottom - slopeBottom) * t,
    };
  }
};

// ============================================
// Enum des expressions
// ============================================
enum class FaceExpression : uint8_t {
  Normal,
  Happy,
  Sad,
  Angry,
  Furious,
  Surprised,
  Disgusted,
  Fear,
  Pleading,
  Vulnerable,
  Despair,
  Guilty,
  Disappointed,
  Embarrassed,
  Horrified,
  Skeptical,
  Annoyed,
  Confused,
  Amazed,
  Excited,
  Suspicious,
  Rejected,
  Bored,
  Tired,
  Asleep,
  COUNT
};

// ============================================
// Preset : deux yeux (l'oeil droit est miroir X de l'oeil gauche)
// ============================================
struct FacePreset {
  EyeConfig left;
  EyeConfig right;
};

// Helper : miroir horizontal d'un oeil
inline EyeConfig mirror(const EyeConfig& e) {
  return { e.height, e.width, (int16_t)(-e.offsetX), e.offsetY, e.radiusTop, e.radiusBottom, -e.slopeTop, -e.slopeBottom };
}

// Helper : preset symétrique (oeil droit = miroir de l'oeil gauche)
inline FacePreset sym(const EyeConfig& e) { return { e, mirror(e) }; }

// Helper : preset asymétrique (deux configs séparées, droit est miroir de right_base)
inline FacePreset asym(const EyeConfig& left, const EyeConfig& right) { return { left, mirror(right) }; }

// ============================================
// 25 presets d'émotions — inspirés du reference sheet
// Format: { H, W, oX, oY, rT, rB, slopeTop, slopeBottom }
// ============================================
namespace FacePresets {

// --- 6 Basic ---
//                                            H     W   oX   oY  rT   rB    sT     sB
constexpr EyeConfig _normal          = { 110, 120,   0,   0,  25,  25,  0.0f,  0.0f };
constexpr EyeConfig _happy           = {  28, 120,   0,   0,  28,   0,  0.0f,  0.0f };
constexpr EyeConfig _sadL            = { 100, 110,  -5,   0,   6,  30, -0.6f,  0.0f };
constexpr EyeConfig _angryL          = {  70, 120,  -8,   0,   4,  35,  0.55f, 0.0f };
constexpr EyeConfig _surprised       = { 140, 130,   0,   0,  20,  20,  0.0f,  0.0f };
constexpr EyeConfig _disgustedL      = {  55, 120, -10,   0,   4,  30,  0.3f,  0.15f };
constexpr EyeConfig _fearL           = { 100, 100,  -6,   0,  10,  10, -0.25f, 0.0f };
constexpr EyeConfig _fearR           = {  80, 100,   6,   0,  10,  10, -0.15f, 0.0f };

// --- Sub-faces of Sadness ---
constexpr EyeConfig _pleading        = { 120, 120,   0,  10,  20,  20, -0.2f,  0.0f }; // grands yeux bas
constexpr EyeConfig _vulnerableL     = {  90, 100,   0,   5,  10,  25, -0.4f,  0.0f };
constexpr EyeConfig _vulnerableR     = {  70, 100,   0,   5,  10,  25, -0.25f, 0.0f };
constexpr EyeConfig _despairL        = {  50, 110,  -5,   0,   4,  20, -0.5f,  0.0f };
constexpr EyeConfig _despairR        = {  40, 100,   5,   5,   4,  20, -0.35f, 0.0f };
constexpr EyeConfig _guiltyL         = {  90, 100,   0,  15,  20,  20, -0.15f, 0.0f }; // yeux bas
constexpr EyeConfig _disappointedL   = {  50, 110,   0,   0,   4,  25, -0.35f, 0.0f };
constexpr EyeConfig _embarrassedL    = {  45, 100,   5,   5,   5,  20, -0.2f,  0.0f };

// --- Sub-faces of Disgust & Anger ---
constexpr EyeConfig _horrified       = { 130, 120,   0,   0,  15,  15,  0.0f,  0.0f };
constexpr EyeConfig _skepticalL      = { 110, 120,   0,   0,  25,  25,  0.0f,  0.0f }; // un oeil normal
constexpr EyeConfig _skepticalR      = {  55, 120,   5,  -5,   4,  25,  0.35f, 0.0f }; // l'autre plissé
constexpr EyeConfig _annoyedL        = {  50, 120, -10,   0,   4,  30,  0.35f, 0.0f };
constexpr EyeConfig _annoyedR        = {  40, 110,  10,   0,   4,  25,  0.25f, 0.0f };
constexpr EyeConfig _furiousL        = {  80, 120, -10,   0,   4,  20,  0.65f, 0.0f };
constexpr EyeConfig _suspiciousL     = {  30, 120,   0,   0,   4,   4,  0.15f, 0.0f };
constexpr EyeConfig _rejectedL       = {  70, 110,  -8,  10,   5,  25,  0.3f,  0.0f };

// --- Sub-faces of Surprise ---
constexpr EyeConfig _confusedL       = { 120, 110,   0,   0,  20,  20, -0.15f, 0.0f };
constexpr EyeConfig _confusedR       = {  80, 100,   5,  -8,  15,  15,  0.1f,  0.0f };
constexpr EyeConfig _amazed          = { 130, 120,   0,   0,  15,  15,  0.0f,  0.0f };
constexpr EyeConfig _excitedL        = { 110, 100,  -5,   0,  15,  15,  0.0f,  0.0f };
constexpr EyeConfig _excitedR        = { 120, 110,   5,  -5,  15,  15,  0.0f,  0.0f };

// --- Bad expressions ---
constexpr EyeConfig _bored           = { 100, 120,   0,   0,  15,  15,  0.0f,  0.0f };
constexpr EyeConfig _tired           = {  30, 120,   0,   0,   6,   6, -0.2f, -0.2f };
constexpr EyeConfig _asleep          = {   8, 100,   0,   0,   0,   0,  0.0f,  0.0f };

inline FacePreset getPreset(FaceExpression expr) {
  switch (expr) {
    case FaceExpression::Normal:       return sym(_normal);
    case FaceExpression::Happy:        return sym(_happy);
    case FaceExpression::Sad:          return sym(_sadL);
    case FaceExpression::Angry:        return sym(_angryL);
    case FaceExpression::Furious:      return sym(_furiousL);
    case FaceExpression::Surprised:    return sym(_surprised);
    case FaceExpression::Disgusted:    return sym(_disgustedL);
    case FaceExpression::Fear:         return asym(_fearL, _fearR);
    case FaceExpression::Pleading:     return sym(_pleading);
    case FaceExpression::Vulnerable:   return asym(_vulnerableL, _vulnerableR);
    case FaceExpression::Despair:      return asym(_despairL, _despairR);
    case FaceExpression::Guilty:       return sym(_guiltyL);
    case FaceExpression::Disappointed: return sym(_disappointedL);
    case FaceExpression::Embarrassed:  return sym(_embarrassedL);
    case FaceExpression::Horrified:    return sym(_horrified);
    case FaceExpression::Skeptical:    return asym(_skepticalL, _skepticalR);
    case FaceExpression::Annoyed:      return asym(_annoyedL, _annoyedR);
    case FaceExpression::Confused:     return asym(_confusedL, _confusedR);
    case FaceExpression::Amazed:       return sym(_amazed);
    case FaceExpression::Excited:      return asym(_excitedL, _excitedR);
    case FaceExpression::Suspicious:   return sym(_suspiciousL);
    case FaceExpression::Rejected:     return sym(_rejectedL);
    case FaceExpression::Bored:        return sym(_bored);
    case FaceExpression::Tired:        return sym(_tired);
    case FaceExpression::Asleep:       return sym(_asleep);
    default:                           return sym(_normal);
  }
}

inline FaceExpression parseExpression(const char* name) {
  if (strcasecmp(name, "normal") == 0)       return FaceExpression::Normal;
  if (strcasecmp(name, "happy") == 0)        return FaceExpression::Happy;
  if (strcasecmp(name, "sad") == 0)          return FaceExpression::Sad;
  if (strcasecmp(name, "angry") == 0)        return FaceExpression::Angry;
  if (strcasecmp(name, "furious") == 0)      return FaceExpression::Furious;
  if (strcasecmp(name, "surprised") == 0)    return FaceExpression::Surprised;
  if (strcasecmp(name, "disgusted") == 0)    return FaceExpression::Disgusted;
  if (strcasecmp(name, "fear") == 0)         return FaceExpression::Fear;
  if (strcasecmp(name, "pleading") == 0)     return FaceExpression::Pleading;
  if (strcasecmp(name, "vulnerable") == 0)   return FaceExpression::Vulnerable;
  if (strcasecmp(name, "despair") == 0)      return FaceExpression::Despair;
  if (strcasecmp(name, "guilty") == 0)       return FaceExpression::Guilty;
  if (strcasecmp(name, "disappointed") == 0) return FaceExpression::Disappointed;
  if (strcasecmp(name, "embarrassed") == 0)  return FaceExpression::Embarrassed;
  if (strcasecmp(name, "horrified") == 0)    return FaceExpression::Horrified;
  if (strcasecmp(name, "skeptical") == 0)    return FaceExpression::Skeptical;
  if (strcasecmp(name, "annoyed") == 0)      return FaceExpression::Annoyed;
  if (strcasecmp(name, "confused") == 0)     return FaceExpression::Confused;
  if (strcasecmp(name, "amazed") == 0)       return FaceExpression::Amazed;
  if (strcasecmp(name, "excited") == 0)      return FaceExpression::Excited;
  if (strcasecmp(name, "suspicious") == 0)   return FaceExpression::Suspicious;
  if (strcasecmp(name, "rejected") == 0)     return FaceExpression::Rejected;
  if (strcasecmp(name, "bored") == 0)        return FaceExpression::Bored;
  if (strcasecmp(name, "tired") == 0)        return FaceExpression::Tired;
  if (strcasecmp(name, "asleep") == 0)       return FaceExpression::Asleep;
  return FaceExpression::Normal;
}

} // namespace FacePresets

#endif
