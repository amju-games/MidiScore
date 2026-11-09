#include <cassert>
#include <cmath>
#include "Quantiser.h"

namespace MidiScore
{
Quantiser& MakeQuantiser(TimeVal res)
{
  static Quantiser* q = new MonoQuantiser(res);
  return *q;
}

void MonoQuantiser::QuantiseStartTime(int tpq, Event& e) const 
{
  // If -1, unquantised start time was not set when
  //  event was created.
  assert(e.m_unquantisedStart > -1);

  int mult = CalcTpqMultipleForTimeVal(tpq, m_res);

  // Get closest multiple of mult to the unquantised start time.
  e.m_start = mult * static_cast<int>(std::round(
    static_cast<float>(e.m_unquantisedStart) /
    static_cast<float>(mult)));
}

void MonoQuantiser::QuantiseDuration(int tpq, Event& e) const 
{
  assert(e.m_unquantisedDuration > -1);

  int mult = CalcTpqMultipleForTimeVal(tpq, m_res);

  // Here we ensure that duration is at least the quant resolution --
  //  otherwise the note would disappear, and surely we don't want 
  //  that, right???
  e.m_duration = mult * static_cast<int>(std::max(1.f,  // duration is at least resolution
    std::round(
      static_cast<float>(e.m_unquantisedDuration) / 
      static_cast<float>(mult))));
 
  // DON'T set time val! First we need to consider splitting the
  //  note into two or more tied notes.
  //e.SetTimeVal(tpq); 
}

void MonoQuantiser::SecondPass(Events& events) const
{
  // What to do if we want a mono track but there are overlaps?
  // None of the options below are super appealing.

  // Make sure there are no overlapping events
#ifdef SHUNT_FORWARD
  // Option 1. 
  // If a note overlaps with previous note, shunt it forward to
  //  the end of the prev note; update its end time, but don't 
  //  change its duration. This tends to increase the length of 
  //  the song, as all notes can ripple forward until there is 
  //  a sufficiently long rest to absorb.
  int numEvents = events.size();
  for (int i = 1; i < numEvents; ++i)
  {
    Event& prev = events[i-1];
    Event& e = events[i];
    if (e.IsNote() && prev.IsNote() && e.m_start < prev.m_end)
    {
      e.m_start = prev.m_end;
      e.m_end = e.m_start + e.m_duration;
    }
  }
#endif

#ifdef CHOP_BUT_NO_ZERO_LENGTH_NOTES
  // Option 2.
  // If note overlaps with previous, move it forward to the end
  //  of the prev note. Don't update its end time, and shorten
  //  its duration.
  // This can cause some notes to have zero length. 
  // So, if the new duration is zero, then increase its duration
  //  and update end time.
  int numEvents = events.size();
  for (int i = 1; i < numEvents; ++i)
  {
    Event& prev = events[i-1];
    Event& e = events[i];
    if (e.IsNote() && prev.IsNote() && e.m_start < prev.m_end)
    {
      e.m_start = prev.m_end;
      auto newDuration = e.m_end - e.m_start;
      if (newDuration == 0)
      {
        // Don't chop note to zero duration, so increase its end time.
        e.m_end = e.m_start + e.m_duration;
      }
      else
      {
        // New duration is fine.
        e.m_duration = newDuration;
      }
    }
  }
#endif

#ifdef CHOP_NOTES_TO_ZERO
  // Option 3. 
  // Chop notes to zero if necessary to avoid overlap.
  int numEvents = events.size();
  for (int i = 1; i < numEvents; ++i)
  {
    Event& prev = events[i-1];
    Event& e = events[i];
    if (e.IsNote() && prev.IsNote() && e.m_start < prev.m_end)
    {
      e.m_start = prev.m_end;
      e.m_duration = e.m_end - e.m_start;
    }
  }
  // Remove zero-duration notes
  events.erase(
    std::remove_if(events.begin(), events.end(),
    [](const Event& e) 
    { 
      return e.IsNote() && e.m_duration == 0; 
    }),
    events.end());
#endif
}
}

