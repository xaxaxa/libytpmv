#include "common.H"

namespace ytpmv {
	// parse a .mod file into notes
	void parseMod(const uint8_t* inData, int inLen, SongInfo& outInf, vector<Instrument>& outInstruments, vector<Note>& outNotes);
};
