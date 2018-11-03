#include "common.H"

namespace ytpmv {
	// parse a .mod file into notes.
	// if tickMode is true, note timings "row" and "absRow" are in ticks instead of rows,
	// and bpm returned is in ticks per minute instead of rows per minute.
	// tickMode should be enabled if the song has tempo changes (e.g. swing)
	void parseMod(const uint8_t* inData, int inLen, SongInfo& outInf, vector<Instrument>& outInstruments, vector<Note>& outNotes, bool tickMode=false);
};
