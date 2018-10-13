#ifndef LIBYTPMV_COMMON_H
#define LIBYTPMV_COMMON_H
#include <string.h>
#include <vector>
#include <string>
using namespace std;

namespace ytpmv {
	struct Time {
		// number of patterns full before this note
		int seq;
		// row index of this note in current pattern
		int row;
		// total rows since beginning of song
		int absRow;
		// from 0 to 1; timing offset in the specified row
		double rowOffset;
	};
	inline bool operator<(const Time& t1, const Time& t2) {
		if(t1.absRow < t2.absRow) return true;
		if(t1.absRow > t2.absRow) return false;
		return t1.rowOffset < t2.rowOffset;
	}
	inline double operator-(const Time& t1, const Time& t2) {
		return (t1.absRow - t2.absRow) + (t1.rowOffset - t2.rowOffset);
	}
	struct SongInfo {
		string name;
		double bpm; // rows per minute
		double rowDurationSeconds() const {
			return (60./bpm);
		}
	};
	class Note {
	public:
		Time start, end;
		int channel;
		int instrument;
		double pitchSemitones; // semitones relative to the sample note (a pitch of 0 means play the sample at original speed)
		double amplitudeDB; // in dB; 0 is default amplitude
		
		// returns duration in rows
		double durationRows() const {
			return (end.absRow - start.absRow) + (end.rowOffset - start.rowOffset);
		}
	};
	struct Instrument {
		int id;
		string name;
		double tuningSemitones; // add this value to all notes played with this instrument
		int amplitudeDB; // in dB; 0 is default amplitude
		basic_string<double> sampleData; // values should be normalized to [-1.0, 1.0]
	};
}
#endif
