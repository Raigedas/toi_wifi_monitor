
#include "melody_player.h"


MelodyPlayer::MelodyPlayer(byte _pin): pin(_pin) {
  reset();
}

bool MelodyPlayer::play(int _priority, MelodyPart _melody[], unsigned int _partCount) {
  bool r = (melody != _melody);
  if (!r) {
    return r;
  }
  if (melody != NULL && priority > _priority) {
    return false;
  }
  reset();
  priority = _priority;
  melody = _melody;
  partCount = _partCount;
  startPlayPart();
  return r;
}

bool MelodyPlayer::stop() {
  bool r = (melody != NULL);
  reset();
  melody = NULL;
  return r;
}

bool MelodyPlayer::stop(MelodyPart _melody[]) {
  if (melody == _melody) {
    stop();
  }
  return false;
}

void MelodyPlayer::handle() {
  if (melody == NULL) {
    return;
  }
  uint32_t now = millis();
  uint32_t timePassed = (now >= startedAt)
    ? now -startedAt
    : startedAt -now; // uint32_t overflow case

  MelodyPart part = melody[partIndex];
  if (timePassed > part.soundPeriod + part.silencePeriod) {
    partIndex++;
    partIndex %= partCount;
    startPlayPart();
  } else {
    if (timePassed > part.soundPeriod) {
      playTone(0);
    } else {
      if (part.startFrequency != part.endFrequency && part.endFrequency != -1) {
        playTone(map(timePassed, 0,  part.soundPeriod, part.startFrequency, part.endFrequency));
      }
    }
  }
}

void MelodyPlayer::startPlayPart() {
  playTone(melody[partIndex].startFrequency);
  startedAt = millis();
}

void MelodyPlayer::playTone(int frequency) {
  if (frequency != currentTone) {
    tone(pin, frequency);
    currentTone = frequency;
  }
}

void MelodyPlayer::reset() {
  partIndex = 0;
  startedAt = millis();
  priority = 0;
  currentTone = 0;
  noTone(pin);
}
