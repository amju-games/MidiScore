#include <iostream>
#include "KeySig.h"
#include "Ornament.h"
#include "Pitch.h"

namespace MidiScore
{
int GetOrnamentNoteMaxTicks(int tpq, TimeVal quant)
{
  int m = CalcTpqMultipleForTimeVal(tpq, quant);
  // TODO check this.. if quant is qq, any note less than qqq 
  //  is a grace note? Maybe we want m * 2 / 3.
  return m / 2; // Half
//  return m * 2 / 3; // Two-thirds?
//  return m * 3 / 4; // Three quarters?!
}

int OrnamentCounts::total() const
{
  return static_cast<int>(allOrnaments.size());
    //grace_notes.size() + trills.size() + mordents.size() + turns.size());
}

static std::string Stringify(const OrnamentVec& p, const std::string& s)
{
  auto n = p.size();
  if (n == 0) return "";
  return std::to_string(n) + " " + s + ((n > 1) ? "s" : "") + " ";
}

std::string OrnamentCounts::ToString() const
{
  std::string res;

  //res += Stringify(allOrnaments, "ornament");

  res += Stringify(graceNotes, "grace note");
  res += Stringify(graceRuns, "grace run");
  res += Stringify(trills, "trill");
  res += Stringify(mordents, "mordent");
  res += Stringify(turns, "turn");

  return res;
}

std::vector<OrnamentCounts> OrnamentAnalyzer::GetOrnamentationPerBar(
    smf::MidiFile& midifile,
    int trackIndex,
    int totalBars,
    int ticksPerBar,
    // Maximum duration for a note to be considered part of an ornament,
    //  in ticks.
    int maxOrnamentNoteDurationTicks)
{
  // Calc max gap between notes in ticks:  a fraction of 
  //  maxOrnamentNoteDurationTicks.
  const int maxGap = maxOrnamentNoteDurationTicks / 4;
//  std::cout << "Max gap: " << maxGap << "\n";

    std::vector<OrnamentCounts> barReports(totalBars);

    if (trackIndex < 0 || trackIndex >= midifile.getTrackCount()) 
    {
        std::cout << "Track index " << trackIndex << " out of range, no ornaments found.\n";
        return barReports;
    }

    smf::MidiEventList& track = midifile[trackIndex];
    std::vector<Note> notes = extractNotes(track);

    size_t i = 0;
    while (i < notes.size()) 
    {
        //double dur = notes[i].end_time - notes[i].start_time;
        int durTicks = notes[i].end_tick - notes[i].start_tick;

        if (durTicks > maxOrnamentNoteDurationTicks) {
            //std::cout << "ORN: note " << i << " longer than orn length, skipping.\n";
            i++;
            continue;
        }

        // Group consecutive short notes
        size_t seqEnd = i;
        while (seqEnd + 1 < notes.size()) 
        {
            //double nextDur = notes[seqEnd + 1].end_time - notes[seqEnd + 1].start_time;
            int nextDurTicks = notes[seqEnd + 1].end_tick - notes[seqEnd + 1].start_tick;
            // Extended max duration: after the first short note, 
            //  subsequent notes in the ornament can be longer. 
            int extMaxDuration = maxOrnamentNoteDurationTicks; // * 3 / 2;
            //double gap = notes[seqEnd + 1].start_time - notes[seqEnd].end_time;
            int gap = notes[seqEnd + 1].start_tick - notes[seqEnd].end_tick;

            if (nextDurTicks < extMaxDuration && gap < maxGap) 
            {
                seqEnd++;
            } else {
                break;
            }
        }

        OrnamentType type = classifySequence(notes, i, seqEnd, maxOrnamentNoteDurationTicks, maxGap);

        // Classifying weeds out quick notes that are not grace notes
        //  (staccato notes).
        if (type != OrnamentType::None) {
            // round() here didn't improve the situation.
            int barIndex = static_cast<int>(std::floor(
                  static_cast<float>(notes[i].start_tick) / 
                  static_cast<float>(ticksPerBar)));
            barIndex = std::clamp(barIndex, 0, totalBars - 1);
            assert(barIndex >= 0 && barIndex < totalBars);

            int start = notes[i].start_tick;
            int end = notes[seqEnd].end_tick;
            PitchVec pitches;
            for (int j = i; j <= seqEnd; ++j) pitches.push_back(notes[j].pitch);
/*
            std::cout << "Found an ornament: bar: " << barIndex + 1
              << " start: "
              << start << " end: " << end 
              << " pitches: ";
            for (int p : pitches) 
              std::cout << PitchToString(p, KeySig::KS_SHARP_0) << " ";
            std::cout << "type: " << static_cast<int>(type) << "\n";
*/
            recordOrnament(barReports[barIndex], type, start, end,
                pitches);
            
            i = seqEnd + 1; // Skip past identified ornament
        } else {
            i++;
        }
    }

    return barReports;
}

std::string Ornament::ToString(KeySig ks) const
{
  std::string res =  "<qq_grace> "; // TODO 

  for (int p : m_pitches) 
    res += PitchToString(p, ks) + " ";

  return res;
}

OrnamentType OrnamentAnalyzer::classifySequence(
    const std::vector<Note>& notes,
    size_t start,
    size_t end,
    int maxGraceDurationTicks,
    int maxGap)
{
    size_t count = end - start + 1;

    // 1. Single rapid note before a main note -> Grace Note
    if (count == 1) {
        if (start + 1 < notes.size()) {
            int pitchDiff = std::abs(notes[start].pitch - notes[start + 1].pitch);
            //double timeToNext = notes[start + 1].start_time - notes[start].start_time;
            int timeToNextTicks = notes[start + 1].start_tick - notes[start].start_tick;
            if (pitchDiff <= 2 && timeToNextTicks <= maxGap) {
                return OrnamentType::GraceNote;
            }
        }
        return OrnamentType::None;
    }

    // Calculate consecutive interval steps between adjacent notes
    std::vector<int> steps;
    for (size_t k = start; k < end; ++k) {
        steps.push_back(notes[k + 1].pitch - notes[k].pitch);
    }

    auto validStep = [](int s) { return std::abs(s) >= 1 && std::abs(s) <= 2; };

    // 2. Three notes (2 steps) -> Mordent [+step, -step] or [-step, +step]
    if (count == 3) {
        if (steps[0] == -steps[1] && validStep(steps[0])) {
            return OrnamentType::Mordent;
        }
    }
    // 3. Four notes (3 steps) -> Turn (Standard or Inverted)
    if (count == 4 && validStep(steps[0]) && validStep(steps[1]) && validStep(steps[2])) {
        bool standardTurn = (steps[0] < 0 && steps[1] < 0 && steps[2] > 0);
        bool invertedTurn = (steps[0] > 0 && steps[1] > 0 && steps[2] < 0);

        if (standardTurn || invertedTurn) {
            return OrnamentType::Turn;
        }
    }

    // 4. Five notes (4 steps) -> 5-Note Turn starting on principal note
    if (count == 5 && validStep(steps[0]) && validStep(steps[1]) && validStep(steps[2]) && validStep(steps[3])) {
        bool fiveNoteTurn = (steps[0] > 0 && steps[1] < 0 && steps[2] < 0 && steps[3] > 0);
        if (fiveNoteTurn) {
            return OrnamentType::Turn;
        }
    }

    // 5. Four or more alternating notes -> Trill
    if (count >= 4) {
        bool isAlternating = true;
        for (size_t k = 0; k < steps.size(); ++k) {
            if (!validStep(steps[k])) {
                isAlternating = false;
                break;
            }
            if (k > 0 && ((steps[k] > 0) == (steps[k - 1] > 0))) {
                isAlternating = false; // Same direction twice = not alternating
                break;
            }
        }
        if (isAlternating) {
            return OrnamentType::Trill;
        }
    }

    // Not one of the above, but some kind of run of grace notes.
    return OrnamentType::GraceRun;
}

void OrnamentAnalyzer::recordOrnament(
    OrnamentCounts& counts,
    OrnamentType type, int start, int end,
    PitchVec pitches)
{
    if (type == OrnamentType::None) return;

    counts.allOrnaments.emplace_back(Ornament(start, end, pitches, 
      type));

    // Split by type for testing
    switch (type) {
        case OrnamentType::GraceNote:
          counts.graceNotes.emplace_back(Ornament(start, end, pitches, 
              OrnamentType::GraceNote));
          break;

        case OrnamentType::GraceRun:
          counts.graceRuns.emplace_back(Ornament(start, end, pitches, 
              OrnamentType::GraceRun));
          break;

        case OrnamentType::Trill:
          counts.trills.emplace_back(Ornament(start, end, pitches,
              OrnamentType::Trill));
          break;

        case OrnamentType::Mordent:
          counts.mordents.emplace_back(Ornament(start, end, pitches,
              OrnamentType::Mordent));
          break;

        case OrnamentType::Turn:
          counts.turns.emplace_back(Ornament(start, end, pitches,
              OrnamentType::Turn));
          break;

        default: break;
    }
}

std::vector<OrnamentAnalyzer::Note> 
OrnamentAnalyzer::extractNotes(smf::MidiEventList& track)
{
    std::vector<Note> notes;
    track.linkNotePairs();

    for (int i = 0; i < track.size(); ++i) {
        smf::MidiEvent& ev = track[i];
        if (ev.isNoteOn() && ev.isLinked()) {
            smf::MidiEvent* offEv = ev.getLinkedEvent();
            notes.push_back({
                ev.getKeyNumber(),
                ev.getVelocity(),
                ev.getChannel(),
                ev.tick,
                offEv->tick,
                ev.seconds,
                offEv->seconds
            });
        }
    }
    return notes;
}
}

