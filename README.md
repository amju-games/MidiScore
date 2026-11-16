# MidiScore
This is a standalone command-line tool, that converts midi files into a shorthand text format, called **Juliet** format. This is step 1 of a toolchain for getting songs into my game, **"Piano Fest"**.

## Acknowledgements
MidiFile - Reads MIDI files. https://github.com/craigsapp/midifile
Catch - Test framework.

## To build and run example:
```
make clean
make
./midiscore files/twinkle.txt
```

To build and run tests:
```
cd test
make clean
make
make run
```

## How to run MidiScore
```
midiscore midifile --info [--track n] 
midiscore midifile [--
```

## "Juliet" output format
### Pitches
step/octave/alter format, like MusicXML. So e.g. `c4` is middle C. `e4-` is e flat above middle C.  
### Chords
Parens around the notes in the chord. E.g. `( c4 e4 g4 c5 )`
### Time values
Time values apply to all subsequent pitches and rests until the next time value, (that's why they are in pointy brackets). 
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
