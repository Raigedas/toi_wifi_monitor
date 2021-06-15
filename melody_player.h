
#ifndef MELODY_PLAYER_H
#define MELODY_PLAYER_H

#include <Arduino.h>


typedef struct {
  int startFrequency;
  int endFrequency;
  unsigned int soundPeriod;
  unsigned int silencePeriod;
} MelodyPart;


class MelodyPlayer
{
public:
  MelodyPlayer(byte _pin);
  bool play(int _priority, MelodyPart _melody[], unsigned int _partCount);
  bool stop();
  bool stop(MelodyPart _melody[]);
  void handle();

protected:
  void reset();
  void startPlayPart();
  void playTone(int frequency);
  const byte pin;
  int currentTone;
  int priority;
  MelodyPart *melody;
  unsigned int partCount;
  unsigned int partIndex;
  uint32_t startedAt;
};

#endif 
