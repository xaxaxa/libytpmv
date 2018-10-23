#include <stdio.h>
#include <ytpmv/modparser.H>
#include <ytpmv/common.H>
#include <math.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include <algorithm>
#include <map>

using namespace std;
using namespace ytpmv;


struct NoteEvent {
	Time t;
	enum {
		T_ON,
		T_OFF
	} type;
	int noteIndex;
};
bool operator<(const NoteEvent& a, const NoteEvent& b) {
	return a.t < b.t;
}

struct ActiveNote {
	double frequencyNormalized;
	double amplitude;
	int startTimeSamples;
	
	int waveformLength;
	const float* waveform;
};
void renderRegion(ActiveNote* notes, int noteCount, int curTimeSamples,
					int durationSamples, int srate, int16_t* outBuf) {
	double f = 50./srate;
	for(int i=0;i<durationSamples;i++) {
		int t = i + curTimeSamples;
		float curSample = 0;
		for(int j=0;j<noteCount;j++) {
			ActiveNote n = notes[j];
			int relTime = t-n.startTimeSamples;
			int sampleTime = relTime*n.frequencyNormalized;
			if(sampleTime<0) continue;
			if(sampleTime >= n.waveformLength) continue;
			//sampleTime = sampleTime % n.waveformLength;
			
			float sample = n.waveform[sampleTime];
			//double sample = sin(2*M_PI*f*n.frequencyNormalized*relTime);
			curSample += sample*n.amplitude;
		}
		float tmp = curSample*256*32;
		if(tmp > 32767) tmp = 32767;
		if(tmp < -32767) tmp = -32767;
		outBuf[i] = tmp;
	}
}

void renderSong(const SongInfo& inf, const vector<Instrument>& instr, const vector<Note>& notes, int srate) {
	vector<NoteEvent> events;
	for(int i=0;i<(int)notes.size();i++) {
		NoteEvent evt;
		evt.t = notes[i].start;
		evt.type = NoteEvent::T_ON;
		evt.noteIndex = i;
		events.push_back(evt);
		
		evt.t = notes[i].end;
		evt.type = NoteEvent::T_OFF;
		events.push_back(evt);
	}
	// sort based on event time
	sort(events.begin(), events.end());
	
	basic_string<int16_t> buf;
	int evts = (int)events.size();
	double rowSamples = inf.rowDurationSeconds()*srate;
	
	int curTimeSamples = events[0].t.absRow*rowSamples;
	map<int, int> notesActive; // map from note index to start time in samples
	for(int i=0;i<evts;i++) {
		NoteEvent evt = events[i];
		
		// enable/disable sound
		if(evt.type == NoteEvent::T_ON) {
			notesActive[evt.noteIndex] = curTimeSamples;
			const Note& n = notes.at(evt.noteIndex);
			fprintf(stderr, "note on: %5d:  pitch %5.1f  instr %2d  vol %3.0f dB  dur %3.0f rows\n", evt.noteIndex, n.pitchSemitones,n.instrument,n.amplitudeDB, n.durationRows());
		} else {
			notesActive.erase(evt.noteIndex);
			fprintf(stderr, "note off:%5d\n", evt.noteIndex);
		}
		
		// calculate time until next event
		int durationSamples;
		if(i == evts-1)
			durationSamples = rowSamples*16;
		else {
			durationSamples = (events[i+1].t - evt.t)*rowSamples;
		}
		if(durationSamples < 3) continue;
		
		// render this region
		int noteCount = notesActive.size();
		ActiveNote tmp[noteCount];
		int j=0;
		for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
			const Note& n = notes.at((*it).first);
			const Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
			tmp[j].frequencyNormalized = pow(2,n.pitchSemitones/12.);
			tmp[j].startTimeSamples = n.start.absRow*rowSamples;
			tmp[j].waveform = ins.sampleData.data();
			tmp[j].waveformLength = ins.sampleData.length();
			tmp[j].amplitude = pow(10,n.amplitudeDB/20.);
			j++;
		}
		
		int bufIndex = buf.length();
		buf.resize(buf.length() + durationSamples);
		renderRegion(tmp, notesActive.size(), curTimeSamples, durationSamples, srate,
					(int16_t*)buf.data() + bufIndex);
		curTimeSamples += durationSamples;
		if(buf.size() > 8192) {
			write(1,buf.data(),buf.length()*2);
			buf.resize(0);
		}
	}
	write(1,buf.data(),buf.length()*2);
}

int main(int argc, char** argv) {
	if(argc < 2) {
		fprintf(stderr, "usage: %s file.mod\n", argv[0]);
		return 1;
	}
	string buf = get_file_contents(argv[1]);
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones += ins.tuningSemitones;
		//printf("%2d %5d - %2d %5d: %.2f %3d\n", n.start.seq, n.start.row, n.end.seq, n.end.row, n.pitchSemitones, n.instrument);
	}
	renderSong(inf,instr,notes,44100);
	return 0;
}
