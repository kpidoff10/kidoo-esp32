#ifndef BEHAVIOR_NEEDS_H
#define BEHAVIOR_NEEDS_H

#include "behavior_stats.h"

enum class Need : uint8_t {
  None,
  Sick,       // health < 30
  Exhausted,  // energy < 15
  Hungry,     // hunger < 25
  Dirty,      // hygiene < 20
  Unhappy,    // happiness < 20
  Lonely,     // boredom > 75 && happiness < 40
  Bored,      // boredom > 60
  Excited,    // excitement > 60 && happiness > 50
};

inline Need getMostUrgentNeed(const BehaviorStats& s) {
  if (s.health < 30)                          return Need::Sick;
  if (s.energy < 15)                          return Need::Exhausted;
  if (s.hunger < 25)                          return Need::Hungry;
  if (s.hygiene < 20)                         return Need::Dirty;
  if (s.happiness < 20)                       return Need::Unhappy;
  if (s.boredom > 75 && s.happiness < 40)     return Need::Lonely;
  if (s.boredom > 60)                         return Need::Bored;
  if (s.excitement > 60 && s.happiness > 50)  return Need::Excited;
  return Need::None;
}

inline const char* needToString(Need n) {
  switch (n) {
    case Need::Sick:      return "sick";
    case Need::Exhausted: return "exhausted";
    case Need::Hungry:    return "hungry";
    case Need::Dirty:     return "dirty";
    case Need::Unhappy:   return "unhappy";
    case Need::Lonely:    return "lonely";
    case Need::Bored:     return "bored";
    case Need::Excited:   return "excited";
    default:              return "none";
  }
}

#endif
