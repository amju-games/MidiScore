#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "MidiFile.h"
#include "TimeVal.h"

namespace MidiScore
{
enum class KeySig;

enum class OrnamentType {
    None,
    GraceNote,
    GraceRun, // general sequence that is not one of the below.
    Mordent,
    Turn,
    Trill
};

using PitchVec = std::vector<int>;

// One ornament
struct Ornament
{
  Ornament() = default;
  Ornament(int start, int end, const PitchVec& pitches, OrnamentType t):
    m_start(start), m_end(end), m_pitches(pitches), m_type(t) {}

  // Generate output string: pitches use key sig to resolve 
  //  enharmonic equivalents.
  std::string ToString(KeySig ks) const;

  int m_start = 0; // start time of the ornament in the piece, in ticks
  int m_end = 0; // end time of ornament, in ticks
  PitchVec m_pitches; // midi pitches of notes in ornament
  OrnamentType m_type; // type of ornament if we can identify it 
};

using OrnamentVec = std::vector<Ornament>;

struct OrnamentCounts 
{
  std::string ToString() const;

  // All the ornaments together; they are distinguished by type.
  OrnamentVec allOrnaments;

  // For testing, all ornaments split by type
  // TODO Get rid of these
  OrnamentVec graceNotes;
  OrnamentVec graceRuns;
  OrnamentVec trills;
  OrnamentVec mordents;
  OrnamentVec turns;

  // Get total number of ornaments
  int total() const;
};

// Helper function: get the max number of ticks for a note to be
//  considered part of an ornament.
int GetOrnamentNoteMaxTicks(int tpq, TimeVal quant);

class OrnamentAnalyzer 
{
public:
    // Find ornaments in each bar for the given track.
    //
    // maxOrnNoteDurTicks: 
    // Maximum duration for a note to be considered part of an ornament,
    //  in ticks.
    // maxGapTicks:
    // Max gap between notes to be considered part of an ornament.
    // Outputs a report of the positions of each ornament found.
    static std::vector<OrnamentCounts> GetOrnamentationPerBar(
        smf::MidiFile& midifile,
        int trackIndex,
        int totalBars,
        int ticksPerBar,
        int maxOrnNoteDurTicks,
        int maxGapTicks);

    struct Note {
        int pitch;
        int velocity;
        int channel;
        int start_tick;
        int end_tick;
        double start_time;
        double end_time;
    };

private:
    // Try to identify type of ornament; returns type, which can be
    //  None if classification fails.
    static OrnamentType classifySequence(
        const std::vector<Note>& notes,
        size_t start,
        size_t end,
        int maxGraceDurationTicks,
        int maxGap);

    // Add the given info about one Ornament to the given report.
    static void recordOrnament(
        OrnamentCounts& counts, 
        OrnamentType type, int start, int end,
        PitchVec pitches);

    // Get midi event list into a Vector of Note structure.
    static std::vector<Note> extractNotes(smf::MidiEventList& track);
};
}

