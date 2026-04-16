#ifndef GOTCHI_SPEAKER_TEST_H
#define GOTCHI_SPEAKER_TEST_H

#include <cstdint>

namespace GotchiSpeakerTest {
  bool playTone(uint16_t freqHz = 440, uint16_t durationMs = 500, uint8_t volume = 70);
  bool playMelody();
  bool playSound(const uint8_t* pcmData, uint32_t pcmLen);
  // Non-bloquant: lance le son dans une FreeRTOS task
  void playSoundAsync(const uint8_t* pcmData, uint32_t pcmLen);
  void playEatingSound();
  bool scanES8311();
  bool dumpRegisters();
}

#endif
