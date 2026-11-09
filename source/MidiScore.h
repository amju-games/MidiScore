// * MidiScore *
// (c) Copyright 2026 Juliet Colman

#pragma once

#include <optional>
#include <string>
#include "Event.h"
#include "Ornament.h"

namespace smf
{
class MidiFile;
class MidiEventList;
}

namespace MidiScore
{
// Flags to control the output from MidiScore.
struct Flags
{
  // debug: true for debug mode.
  bool debug = false;

  // allClefs: allow alto and tenor clefs in output if true;
  //  if false, only allow bass and treble clefs.
  bool allClefs = false;

  // grid: output in grid format
  bool grid = false;

  // Retain chords: allow chords in output, e.g. "( c4 e4 )"
  bool poly = false;

  // If specified track is polyphonic, split into separate mono tracks.
  bool saveSplitPoly = false;

  // If true, prefer staccato where possible.
  bool stac = false;

  // If true, detect and mark up ornaments.
  bool detectOrnaments = false;
};

// Output midi file as juliet shorthand notation, for reading 
//  by MakeScore.
// mf: the midi file.
// track: optional track number; if nullopt, all tracks are output.
// timeSig: NON optional time signature
// keySig: optional key signature, overrides guessing.
// quantResolution: optional, sets quantisation on and resolution level.
// bpm: optional bpm tempo.
// anacrusis: optional short first bar time value (c, m, etc).
std::string ToString(
  smf::MidiFile& mf,
  std::optional<int> track,
  std::string timeSig, 
  std::optional<int> keySig,
  std::optional<std::string> quantResolution,
  std::optional<float> bpm,
  std::optional<std::string> anacrusis,
  const Flags& flags);

// Used internally and for testing
std::string OutputEvents(const Events& events, bool grid = false);

// For tests: we want to check strings like "c t c" for note duration tests
std::string OutputNoteDurations(const Events& events);

// Output with as much info as poss
std::string OutputEventsDebug(int tpq, const Events& events);

// Generate Events from a MidiFile track.
class Quantiser;
Events GetEventsFromTrack(
  int tpq, const smf::MidiEventList& track, TimeSig ts, KeySig ks,
  const Quantiser& quantiser,
  int anacrusisTicks,
  const std::vector<OrnamentCounts>& ornaments);

// Output one event. 
// The idea is to only output duration elements when the duration
//  changes, not for every note/rest, so track the duration of the
//  current event.
std::string OutputEvent(int& prevDuration, const Event& e);

} // namespace MidiScore

