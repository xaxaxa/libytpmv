#include "include/audiorenderer.H"
#include <algorithm>
#include <map>

using namespace std;
namespace ytpmv {
	struct NoteEvent {
		double t;
		int segmentIndex;
		bool off;
	};
	bool operator<(const NoteEvent& a, const NoteEvent& b) {
		return a.t < b.t;
	}

	struct ActiveNote {
		double speed;
		double amplitude;
		int startTimeSamples;
		
		int waveformLength;
		const double* waveform;
	};
	void renderRegion(ActiveNote* notes, int noteCount, int curTimeSamples,
						int durationSamples, int srate, double* outBuf) {
		double f = 50./srate;
		for(int i=0;i<durationSamples;i++) {
			int t = i + curTimeSamples;
			double curSample = 0;
			for(int j=0;j<noteCount;j++) {
				ActiveNote n = notes[j];
				int relTime = t-n.startTimeSamples;
				int sampleTime = relTime*n.speed;
				if(sampleTime<0) continue;
				if(sampleTime > n.waveformLength) continue;
				//sampleTime = sampleTime % n.waveformLength;
				
				double sample = n.waveform[sampleTime];
				//double sample = sin(2*M_PI*f*n.frequencyNormalized*relTime);
				curSample += sample*n.amplitude;
			}
			outBuf[i] = curSample;
		}
	}

	void renderAudio(const vector<AudioSegment>& segments, int srate, function<void(double* data, int len)> writeData) {
		vector<NoteEvent> events;
		for(int i=0;i<(int)segments.size();i++) {
			NoteEvent evt;
			evt.t = segments[i].startSeconds;
			evt.off = false;
			evt.segmentIndex = i;
			events.push_back(evt);
			
			evt.t = segments[i].endSeconds;
			evt.off = true;
			events.push_back(evt);
		}
		// sort based on event time
		sort(events.begin(), events.end());
		
		basic_string<double> buf;
		int evts = (int)events.size();
		
		
		map<int, int> notesActive; // map from note index to start time in samples
		for(int i=0;i<evts-1;i++) {
			NoteEvent evt = events[i];
			int curTimeSamples = events[i].t*srate;
			
			// enable/disable sound
			if(!evt.off) { // note on
				notesActive[evt.segmentIndex] = curTimeSamples;
				const AudioSegment& s = segments.at(evt.segmentIndex);
				fprintf(stderr, "note on: %5d:  pitch %5.2f  vol %3.1f  dur %3.2fs\n", evt.segmentIndex, s.pitch,s.amplitude, s.durationSeconds());
			} else {
				notesActive.erase(evt.segmentIndex);
				fprintf(stderr, "note off:%5d\n", evt.segmentIndex);
			}
			
			// calculate time until next event
			int durationSamples = (events[i+1].t - evt.t)*srate;
			if(durationSamples < 3) continue;
			
			// render this region
			int noteCount = notesActive.size();
			ActiveNote tmp[noteCount];
			int j=0;
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const AudioSegment& s = segments.at((*it).first);
				tmp[j].speed = s.pitch;
				tmp[j].startTimeSamples = s.startSeconds*srate;
				tmp[j].waveform = s.sampleData;
				tmp[j].waveformLength = s.sampleLength;
				tmp[j].amplitude = s.amplitude;
				j++;
			}
			
			int bufIndex = buf.length();
			buf.resize(buf.length() + durationSamples);
			renderRegion(tmp, notesActive.size(), curTimeSamples, durationSamples, srate,
						((double*)buf.data()) + bufIndex);
			
			if(buf.size() > 8192) {
				writeData((double*)buf.data(),buf.length());
				buf.resize(0);
			}
		}
		writeData((double*)buf.data(),buf.length());
	}
}
