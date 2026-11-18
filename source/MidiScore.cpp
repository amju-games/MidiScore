// * MidiScore *
// (c) Copyright 2026 Juliet Colman

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <MidiFile.h>
#include "Anacrusis.h"
#include "Clef.h"
#include "Event.h"
#include "Grid.h"
#include "Info.h"
#include "KeySig.h"
#include "MidiScore.h"
#include "NumBars.h"
#include "Ornament.h"
#include "Polyphony.h"
#include "Quantiser.h"
#include "Staccato.h"
#include "TimeSig.h"

namespace MidiScore
{
static Event MakeOrnamentEvent(const Ornament& o)
{
  Event e;
  e.m_type = EventType::ORNAMENT;
  e.m_ornament = o;
  e.m_start = o.m_start;
  e.m_end = e.m_start;
  return e;
}

// Splice the given ornaments into the given event sequence.
static void AddOrnaments(
  Events& events,
  std::vector<OrnamentCounts> ornaments)
{
  // Ornaments are split by bar
  int barNum = 1;

  // Good news, events and ornaments are both sorted by start time.
  // So once we find an event, we can search from there to find the
  //  next one.
  Events::iterator searchBegin = events.begin();

  for (const auto& barOrnaments : ornaments)
  {
    for (const Ornament& o : barOrnaments.allOrnaments)
    {
      // Find spot to insert ornament o
      Events::iterator it = std::upper_bound(
        searchBegin, events.end(),
        o.m_start, 
        [](int start, const Event& e) 
        { 
          return start < e.m_start; 
        });

      std::cout << "// Insert " 
        << o.ToString(KeySig::KS_SHARP_0) 
        << " (time " << o.m_start
        << ") at event: " << it->ToString()
        << " in bar " << barNum
        << "\n";

      searchBegin = events.insert(it, MakeOrnamentEvent(o));
    }
    ++barNum;
  }
}

static bool MidiNoteIsInOrnament(const smf::MidiEvent& mev,
  const std::vector<OrnamentCounts>& ornaments)
{
  if (!mev.isNoteOn()) return false;

  for (const OrnamentCounts& oc : ornaments)
  {
    const OrnamentVec& allOrns = oc.allOrnaments;
    for (const Ornament& o : allOrns)
    {
      if (mev.tick >= o.m_start && mev.tick <= o.m_end)
      {
        // Tick for this event is within the time range of
        //  ornament o.
        return true;
      }
    }
  }
  return false;
}

static void AddEventToVec(
  int tpq, const smf::MidiEvent& mev, Events& events, TimeSig ts, 
  KeySig ks,
  const Quantiser& quantiser, int anacrusisTicks)
{
  if (mev.isNoteOn())
  {
    auto numBytes = mev.size();
    if (numBytes > 1)
    {   
      // Add note event to vec of events
      Event e;
      e.m_unquantisedStart = mev.tick;
      e.m_unquantisedDuration = mev.getTickDuration();

      quantiser.QuantiseStartTime(tpq, e);

      // Set duration to the closest multiple of tpqs in the quant resolution.
      // But don't set the timeval to the closest timeval. That would
      //  obliterate crucial timing info -- we might need to split the
      //  note to capture its length.
      quantiser.QuantiseDuration(tpq, e);

      e.m_end = e.m_start + e.m_duration;

      e.m_pitch = static_cast<int>(mev[1]);
      e.m_keySig = ks;

      if (numBytes > 2)
      {
        e.m_dynamics.SetVelocity(static_cast<int>(mev[2]));
      }

      // TimeVal is NOT set yet! In this function we decide whether or
      //  not to split the note, and then assign TimeVals based on that.
      const bool NO_SPLIT_ON_BEATS = false;
      AppendNoteEventToEvents(tpq, e, events, ts, NO_SPLIT_ON_BEATS, anacrusisTicks);
    }
  }
}

std::string OutputEvent(int& prevDuration, const Event& e)
{
  // If this event is a note, check the duration - only output if it
  //  has changed.
  if (e.IsNote() && e.m_duration == prevDuration)
  {
    // Duration the same -- just need to output pitch
    return e.NoteToStringNoDuration(); // also dynamics etc
  }

  // If a rest, and the duration has not changed, just output
  //  immediate rest token.
  if (e.IsRest() && e.m_duration == prevDuration)
  {
    return (e.m_isWholeBar ? "R" : "r"); // immediate rest token
  }

  if (e.m_duration > 0 || prevDuration < 0)
  {
    prevDuration = e.m_duration;
  }

  if (e.IsOrnament())
  {
    prevDuration = -1; 
  }

  return e.ToString();
}

std::string OutputTrack(
  int tpq, Events& events, TimeSig ts, KeySig ks, int numBars, 
  bool yesDynamics, bool yesTimeSetEvents, int anacrusisTicks, 
  const Flags& flags,
  const std::vector<OrnamentCounts>& ornaments)
{
  if (events.empty())
  {
    return "";
  }

//std::cout << "Raw events: " << OutputEvents(events) << "\n";

  if (flags.stac)
  {
    StaccatoPass(events); // use staccato notation where possible.
  }

  // Prepend time sig and key sig. TODO Later on consider looking for
  //  key sig changes, as we do for clefs.
  const int atStart = 0;
  events.insert(events.begin(), MakeKeySigEvent(ks, atStart)); 
  events.insert(events.begin(), MakeTimeSigEvent(ts, atStart)); 

  if (yesDynamics)
    InsertDynamics(events);

  // Reset last dynamics string output (on prev track)
  Dynamics::SetLastDynamicsString();

  // If polyphonic tracks are required, mark up chords in the track.
  if (flags.poly)
    InsertChordMarkers(events);

//std::cout << "With chord markers: " << OutputEvents(events) << "\n";

  InsertBarLines(tpq, ts, events, numBars, anacrusisTicks);
//std::cout << "With bar lines: " << OutputEvents(events) << "\n";

  // Get initial clef event and possibly changes; interleave these events.
  ClefChanges allClefChanges;
  GuessClef(events, tpq, anacrusisTicks, ts, allClefChanges, !flags.allClefs);
  InsertClefs(events, allClefChanges);

  // Fill 'gaps' between note events with rests
  InsertRests(tpq, events, ts);
//std::cout << "With rests: " << OutputEvents(events) << "\n";

  if (yesTimeSetEvents)
    InsertTimeSetEvents(tpq, events);
//std::cout << "With time sets: " << OutputEvents(events) << "\n";

  // Splice ornaments back into events
  AddOrnaments(events, ornaments);

  if (flags.debug)
  {
    return OutputEventsDebug(tpq, events);
  }

  return OutputEvents(events, flags.grid);
}

std::string OutputEventsDebug(int tpq, const Events& events)
{
  std::string res = "\n";

  // Traverse events. Output time val when it changes.
  int prevDuration = -1;

  // Zero-based bar numbers to match MakeScore
  res += "// BAR: 0\n";
  int barNum = 1; // next bar num 

  // So we can output times as crotchets from last bar line
  int lastBarStart = 0;

  for (int i = 0; i < events.size(); i++)
  {
    const Event& e = events[i];

    if (e.IsBarLine())
    {
      // Bar line events are the end of bars, but we output as if it's
      //  the start of the new bar - unless this is the final event.
      if (i == events.size() - 1)
      {
        res += "// Final bar line. ";
      }
      else 
      {
        res += "// BAR: " + std::to_string(barNum++);
      }
      res += " Start: " + std::to_string(e.m_start) + 
        " (" + std::to_string(e.m_start / tpq) + " crotchets):\n";

      lastBarStart = e.m_start;
    }
    else if (e.IsNote() || e.IsRest())
    {
      auto startFromBar = (e.m_start - lastBarStart) / tpq;
      std::stringstream ss;
      ss << startFromBar;
      res += "// (Start: " + ss.str() + " c in bar)";
      res += "  (Duration: " + e.DurationString() + "):\n";
    }
  
    // Output the event, even if just bar line... because
    //  MakeScore can read this debug format :)
    res += OutputEvent(prevDuration, e);
   
    res += "\n";
  }
  return res;
}

// For tests
std::string OutputNoteDurations(const Events& events)
{
  std::string res;

  for (int i = 0; i < events.size(); i++)
  {
    const auto& e = events[i];

    if (e.IsNote())
      res += e.DurationString();
    else
      res += e.ToString();

    if (i < (events.size() - 1))
      res += " ";
  }
  return res;
}

std::string OutputEvents(const Events& events, bool grid)
{
  if (grid)
  {
    return Gridify(events);
  }

  std::string res;

  // Traverse events. Output time val when it changes.
  int prevDuration = -1;
  for (int i = 0; i < events.size(); i++)
  {
    res += OutputEvent(prevDuration, events[i]);
    res += " ";
  }

  return res;
}

void GuessTimeSigAndKeySig(int tpq, const Events& events, TimeSig& ts, KeySig& ks)
{
  ts = GuessTimeSig(tpq, events); 

  const bool preferFlatKey = true; // TODO command line param?
  ks = GuessKeySig(events, preferFlatKey); 
}

Events GetEventsFromTrack(
  int tpq, const smf::MidiEventList& track, TimeSig ts, KeySig ks, 
  const Quantiser& quantiser,
  int anacrusisTicks,
  const std::vector<OrnamentCounts>& ornaments)
{
  Events events;
  int numStripped = 0;

  for (int event = 0; event < track.size(); event++) 
  {
    const auto& mev = track[event];

    // Don't add notes that are part of an ornament
    if (MidiNoteIsInOrnament(mev, ornaments))
    {
      ++numStripped;
    }
    else
    {
      AddEventToVec(tpq, mev, events, ts, ks, quantiser, anacrusisTicks);
    }
  }   
  
//  std::cout << "Stripped " << numStripped 
//    << " out of " << track.size() << " events.\n";

  if (events.empty()) return events;

  // Sort by ascending start time, to minimise number of time jumps 
  std::sort(events.begin(), events.end(), 
    [](const Event& e1, const Event& e2) 
    {   
      return e1.m_start < e2.m_start; 
    }   
  ); 

  // Second quantisation pass over events, e.g. to chop durations so 
  //  there are no overlaps, if required
  quantiser.SecondPass(events);

  return events;
}

static std::vector<int> MakeOneTrackMono(
  smf::MidiFile& midifile, 
  int track,
  TimeSig ts)
{ 
  // Chop polyphonic track into multiple mono tracks.
  auto splitResult = SplitPolyTrack(midifile, track, ts);
  if (splitResult.empty())
  {
    std::cout << "// No polyphony found for track " << track << ".\n";
  }
  else
  {
    std::cout << "// Split poly track " << track 
      << ", creating " 
      << splitResult.size() 
      << " new tracks. Total num tracks in midi is now: " << midifile.getTrackCount()
      << "\n";
  }
  return splitResult;
}

// Return number of new tracks created.
static int MakeTrackMono(
  smf::MidiFile& midifile, 
  std::optional<int>& track,
  TimeSig ts)
{ 
  if (track)
  {
    auto res = MakeOneTrackMono(midifile, *track, ts);
    *track = res[0];
    std::cout << "// New track to output: " << *track << "\n";
    return res.size(); 
  }
  else
  {
    int res = 0;
    for (int t = 0; t < midifile.getNumTracks(); ++t)
    {
      auto newTracks = MakeOneTrackMono(midifile, t, ts);
      res += newTracks.size();
    }
    return res;
  }
}

static std::vector<OrnamentCounts> FindOrnaments(
  smf::MidiFile& midifile, 
  int track,
  TimeSig ts,
  int ornamentNoteMaxDurationTicks,
  int ornamentMaxGapTicks)
{
  int numBars = NumBars(midifile, track, ts);
  if (numBars == 0) 
  {
    std::cout << "ERROR: Num bars is zero?!\n";
    return {};
  } 
 
  int ticksPerBar = TicksPerBar(midifile, ts);

  auto reports = OrnamentAnalyzer::GetOrnamentationPerBar(
    midifile, track, numBars, ticksPerBar, ornamentNoteMaxDurationTicks,
    ornamentMaxGapTicks);

  return reports;
}

std::string ToString(
  smf::MidiFile& midifile, 
  std::optional<int> track,
  std::string timeSig, 
  std::optional<int> keySig,
  std::optional<std::string> quant, 
  std::optional<float> bpm,
  std::optional<std::string> anacrusis,
  const Flags& flags)
{
  TimeSig ts = GetTimeSigFromString(timeSig);

  TimeVal quantRes = TimeVal::NONE;
  if (quant)
  {
    quantRes = GetTimeValFromString(*quant);
    if (quantRes == TimeVal::NONE)
    {
      std::cout << "Unrecognised quant resolution! Use one of sb/m/c/q/qq/qqq.\n";
      return {};
    }

    std::cout << "// Quantising to: " << TimeValString(quantRes) << "\n";
  }

  // Unless --poly is specified, split poly tracks into multiple
  //  mono tracks. I.e. one track with a three note chord gets
  //  converted to three separate tracks. 
  if (!flags.poly)
  {
    int numNewTracks = MakeTrackMono(midifile, track, ts);

    // Breaks MakeScore - just for dev/debug.
//  std::cout << "INFO FOR NEW TRACKS:\n";
//  std::cout << InfoString(midifile, std::nullopt, anacrusis, timeSig, allClefs, quantRes);

    midifile.removeEmpties();                                     
    midifile.doTimeAnalysis();                 
    midifile.linkNotePairs();                

    // If this flag is specified, save the file with the extra mono
    //  tracks.
    if (flags.saveSplitPoly && numNewTracks > 0)
    {
      // Save midifile with the new mono tracks
      const std::string filename = std::string(midifile.getFilename()) +
        ".split_poly.mid";
      midifile.write(filename);
      std::cout << "// ** Saved file with split poly->mono tracks: " 
        << filename << "\n";
    }
  }

  const int tpq = midifile.getTicksPerQuarterNote();
  std::cout << "// TPQ: " << tpq << "\n";

  // Find ornamentation, with info so we can flag ornament notes.
  std::vector<OrnamentCounts> ornaments;
  if (track)
  {
    // Do this before quantising.
    // If no track, do all tracks TODO
    // Doesn't strip them, but detects them.

    int ornamentDurationTicks = 
      CalcTpqMultipleForTimeVal(tpq, quantRes);
    // TODO how valid is this
    int ornamentMaxGapTicks = ornamentDurationTicks / 2;

    std::cout << "// Ornamentation duration: " 
      << ornamentDurationTicks << " ticks "
      << "(1/"
      << (tpq / ornamentDurationTicks) << " crotchet)\n";

    ornaments = FindOrnaments(midifile, *track, ts, 
      ornamentDurationTicks, ornamentMaxGapTicks);
/*
    std::cout << "Ornamentation:\n";
    for (int i = 0; i < ornaments.size(); ++i)
    {
      if (ornaments[i].total() > 0)
      {
        std::cout 
          << "\tBar " 
          << i + 1 
          << ": " 
          << ornaments[i].ToString() 
          << "\n";
      }
    }
*/
    int numOrnaments = 0;
    for (const auto& b : ornaments)
    {
      numOrnaments += b.total();
    }
    std::cout << "// Found ornaments: " << numOrnaments << "\n";
  }

  std::string res;


  // TODO Strategy pattern: create different quantiser type depending on 
  //  command line params, in MakeQuantiser. 
  // E.g. if no quant res set, create a null quantiser that does nothing.
  // We will start off with a quantiser impl that chops durations so there
  //  are no chords, then add a different impl later when required.
  const Quantiser& quantiser = MakeQuantiser(quantRes); 

  int anacrusisTicks = GetAnacrusisTicks(anacrusis, tpq); 
  // number of tpq ticks in first, incomplete bar

  // First pass: Join all tracks 
  midifile.joinTracks(); 
  Events allEvents = GetEventsFromTrack(
    tpq, midifile[0], ts, KeySig::KS_SHARP_0, quantiser, anacrusisTicks,
    ornaments);

  if (allEvents.empty())
  {
    std::cout << "No events!\n";
    return "";
  }

  KeySig ks;
  TimeSig tsGuess;
  // Guess Key sig (and possibly in future Time sig) from ALL events.
  // (We don't use the guessed time sig for now.)
  GuessTimeSigAndKeySig(tpq, allEvents, tsGuess, ks);

  // Override guess if value given
  if (keySig)
  {
    ks = IntToKeySig(*keySig);
  }

  int numBars = static_cast<int>(
    std::ceil(
    static_cast<float>(allEvents.back().m_end) / 
    static_cast<float>(tpq) / 
    static_cast<float>(BeatsInBar(ts))));

  if (anacrusis) 
  {
    std::cout << "// Anacrusis: " << *anacrusis << "\n";
  }

  std::cout << "// Num bars: " << numBars << "\n";

  // Give a rough page width using the number of bars
  const float ROUGH_PAGE_WIDTH_MULT = 3.f;
  res += "page-w " + 
    std::to_string(ROUGH_PAGE_WIDTH_MULT  * numBars) + "\n";

  if (bpm)
  {
    res += "bpm " + (std::stringstream() << *bpm).str() + "\n";
  }

  // TODO do other first-pass things on all the events
  // E.g. create dynamics markers

  // 2nd pass: Un-join tracks and output each track 
  //  as a separate stave  
  midifile.splitTracks();

  // TODO These should go in flags
  const bool noDynamics = false;
  const bool noTimeSets = false;

  // If track is specified, just output that one track.
  // Otherwise, output all non-empty tracks.
  int numTracks = midifile.getNumTracks();
  int start = 0;
  int end = numTracks;
  if (track)
  {
    start = *track;
    end = start + 1;
  }

  int stave = 0;
  for (int t = start; t < end; t++)
  {
    Events events = GetEventsFromTrack(
      tpq, midifile[t], ts, ks, quantiser,
      anacrusisTicks,
      ornaments);

    // TODO Fix Multi-track output and ornaments

    if (events.empty()) continue;

    res += "// ** STAVE " + std::to_string(stave++) + " **\n";
    res += "stave " + 
      OutputTrack(tpq, events, ts, ks, numBars, noDynamics, noTimeSets,
        anacrusisTicks, flags, ornaments) + "\n";
  }

  res += "\n";
  return res;
}
}

