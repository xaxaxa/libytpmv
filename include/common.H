#ifndef LIBYTPMV_COMMON_H
#define LIBYTPMV_COMMON_H
#include <string.h>
#include <math.h>
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
	
	// song info read from mod or midi file
	struct SongInfo {
		string name;
		double bpm; // rows per minute
		double rowDurationSeconds() const {
			return (60./bpm);
		}
	};
	// represents one note from mod/midi file
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
	// represents an instrument or sample from mod file
	struct Instrument {
		int id;
		string name;
		double tuningSemitones; // add this value to all notes played with this instrument
		int amplitudeDB; // in dB; 0 is default amplitude
		basic_string<double> sampleData; // values should be normalized to [-1.0, 1.0]
	};
	
	// a note or segment in the audio timeline; passed into AudioRenderer
	class AudioSegment {
	public:
		double startSeconds, endSeconds;
		double tempo; // linear tempo; 1 => original tempo
		double pitch; // linear pitch; 1 => original pitch, 2 => 12 semitones up, etc
		double amplitude;
		const double* sampleData;
		int sampleLength;
		
		AudioSegment() {}
		AudioSegment(const Note& n, const Instrument& ins, double bpm) {
			startSeconds = n.start.absRow*60/bpm;
			endSeconds = n.end.absRow*60/bpm;
			pitch = pow(2,(n.pitchSemitones + ins.tuningSemitones)/12.);
			tempo = pitch;
			amplitude = pow(10,n.amplitudeDB/20.);
			sampleData = ins.sampleData.data();
			sampleLength = ins.sampleData.length();
		}
		double durationSeconds() const {
			return endSeconds - startSeconds;
		}
	};
}
#endif
