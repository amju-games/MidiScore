# MidiScore
This is a standalone command-line tool, that converts midi files into a shorthand text format, called **Juliet** format. This is step 1 of a toolchain for getting songs into my game, **"Piano Fest"**.

## Acknowledgements
 * MidiFile - Reads MIDI files. https://github.com/craigsapp/midifile
 * Catch2 - For the testing framework. https://github.com/catchorg/catch2

## To build and run
Running the example:
```
make clean
make
./midiscore files/twinkle.txt
```
What this generates*:
```
stave clef-t 4/4 key-s-0 <c> c5 c5 g5 g5 | a5 a5 <m> g5 | <c> f5 f5 e5 e5 | d5 d5 <m> c5 | 
stave clef-b 4/4 key-s-0 <c> c3 c4 e4 c4 | f4 c4 e4 c4 | d4 b3 c4 a3 | f3 g3 <m> c3 | 
```
* It also outputs a load of comments ATM

To build and run tests:
```
cd test
make clean
make
make run
```

## Command-line options
```
midiscore <midifile> --info [--track n] [--timesig ts]
```
Displays info about the midi file or just one specified track. The time signature is optional but can help with calculating the number of bars etc. Timesig guessing is not implemented "yet".
```
midiscore <midifile> --timesig ts [options] 
```
Outputs the midi content in Juliet format, according to the given time signature and other options. Here's the current list, or run midiscore with no input file to see the list.

`--track <n>` - Output notation for track n only; default is all tracks, as separate staves. NB track numbers are zero-based.

`--timesig <t>` - Set time signature to t, a string. Format for t is `3/4`, `4/4`, etc.

`--keysig <k>` - Set key signature to k, an int. Positive k is number of sharps; negative k is number of flats.

`--quant <q>` - Quantise starts and durations, to resolution _q_. q is one of: `qqq`, `qq`, `q`, `c`, `m`, `sb`, `sb2`, `sb4`

`--debug` - Verbose output for debugging.

`--bpm <f>` - Set Beats Per Minute tempo: passed through to the output. _f_ is a number, can be fractional.

`--ana <a>` - Anacrusis: set length of incomplete first bar. _a_ is one of: `q`, `c`, `m`, etc.

`--allclefs` - Output alto and tenor clefs as well as bass and treble.

`--grid` - Output in grid format - or, at least, in a per-bar format, making it easier to check against a good score, and make edits.

`--poly` - Retain chords; otherwise, we split polyphonic tracks into separate mono tracks, and output the highest pitched track only.

`--splitpoly` - Save new version of midi file with polyphonic tracks split into separate mono tracks.

`--stac` - Prefer staccato notation where possible, (up to crotchets).

## "Juliet" output format
### Pitches
step/octave/alter format, like MusicXML. So e.g. `c4` is middle C. `e4-` is e flat above middle C. `a4+` is A# (Bb) above middle C. We use the prevailing key sig to decide between equivalent pitches like A#/Bb.
### Chords
Parens around the notes in the chord. E.g. `( c4 e4 g4 c5 )`
### Time values
Time values apply to all subsequent pitches and rests until the next time value, (that's why they are in pointy brackets).

`<sb>` - semibreve (whole note) 

`<m>` - minim (half note) 

`<c>` - crotchet (quarter note) 

`q` - quaver 

`qq` - semiquaver, etc. 

A dot at the end means 'half as long again'. E.g. `<c.>` - dotted crotchet, worth three quavers. 
### Rests
`r` or `R` for a whole bar rest.
### Staccato
Dot after the pitch. E.g. `<c> c4.` is a staccato crotchet. (Dots after time values mean 'half as long again'.)
### Ties
`t` between the tied notes.
### Key signatures
`key-f-2` means key sig of 2 flats. `key-s-3` means key sig of 3 sharps.
### Time signatures
`2/4`, `3/4`, `4/4`, etc
### Clefs
`clef-t`, `clef-b`, `clef-alto`, and `clef-tenor`.
### Bar Lines
`|`
### Staves
`stave` means start a new stave.
