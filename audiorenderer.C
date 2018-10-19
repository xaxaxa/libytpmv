#include "include/audiorenderer.H"
#include "include/samplecache.H"
#include <algorithm>
#include <map>

using namespace std;
namespace ytpmv {
	struct NoteEvent {
		double t;
		int segmentIndex;
		bool off;
	};
	static bool operator<(const NoteEvent& a, const NoteEvent& b) {
		return a.t < b.t;
	}

	struct ActiveNote {
		double speed;
		double amplitude[CHANNELS];
		int startTimeSamples;
		int endTimeSamples;
		
		int waveformLength;
		const float* waveform;
	};
	void renderRegion(ActiveNote* notes, int noteCount, int curTimeSamples,
						int durationSamples, int srate, float* outBuf) {
		double f = 50./srate;
		for(int i=0;i<durationSamples;i++) {
			int t = i + curTimeSamples;
			float curSample[CHANNELS] = {};
			
			for(int j=0;j<noteCount;j++) {
				ActiveNote n = notes[j];
				int relTime = t-n.startTimeSamples;
				int relTimeLeft = n.endTimeSamples-t;
				int sampleTime = relTime*n.speed;
				if(sampleTime<0) continue;
				if(sampleTime >= n.waveformLength/CHANNELS) continue;
				//sampleTime = sampleTime % n.waveformLength;
				
				for(int k=0; k<CHANNELS; k++) {
					float sample = n.waveform[sampleTime*CHANNELS+k];
					
					int fadeSamples=20;
					double scale = 1./(double)fadeSamples;
					if(relTime < fadeSamples) sample *= relTime*scale;
					if(relTimeLeft < fadeSamples) sample *= relTimeLeft*scale;
					//double sample = sin(2*M_PI*f*n.frequencyNormalized*relTime);
					curSample[k] += sample*n.amplitude[k];
				}
			}
			for(int k=0; k<CHANNELS; k++)
				outBuf[i*CHANNELS+k] = curSample[k];
		}
	}

	void renderAudio(const vector<AudioSegment>& segments, int srate, function<void(float* data, int len)> writeData) {
		SampleCache cache;
		
		// pre-populate sample cache with all used samples and pitches
		for(const AudioSegment& s: segments) {
			double relativePitch = s.pitch/s.tempo;
			cache.getPitchShiftedSample(s.sampleData, s.sampleLength, relativePitch);
		}
		fprintf(stderr, "sample cache populated; %d samples\n", (int)cache.entries.size());
		
		// convert note list into note event list
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
		
		basic_string<float> buf;
		int evts = (int)events.size();
		
		// go through all note events and render the regions between events
		
		map<int, int> notesActive; // map from note index to start time in samples
		for(int i=0;i<evts-1;i++) {
			NoteEvent evt = events[i];
			int curTimeSamples = events[i].t*srate;
			
			// enable/disable sound
			if(!evt.off) { // note on
				notesActive[evt.segmentIndex] = curTimeSamples;
				const AudioSegment& s = segments.at(evt.segmentIndex);
				fprintf(stderr, "note on: %5d:  pitch %5.2f  vol %3.1f  dur %3.2fs\n", evt.segmentIndex, s.pitch,s.amplitude[0], s.durationSeconds());
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
				
				// relative pitch is the amount we have to pitch shift the waveform
				double relativePitch = s.pitch/s.tempo;
				if(relativePitch != 1.) {
					// pitch correction needed
					basic_string<float>& waveform = cache.getPitchShiftedSample(s.sampleData, s.sampleLength, relativePitch);
					tmp[j].waveform = waveform.data();
					tmp[j].waveformLength = waveform.length();
				} else {
					tmp[j].waveform = s.sampleData;
					tmp[j].waveformLength = s.sampleLength;
				}
				tmp[j].speed = s.tempo;
				tmp[j].startTimeSamples = s.startSeconds*srate;
				tmp[j].endTimeSamples = s.endSeconds*srate;
				for(int k=0; k<CHANNELS; k++)
					tmp[j].amplitude[k] = s.amplitude[k];
				j++;
			}
			
			int bufIndex = buf.length();
			buf.resize(buf.length() + durationSamples*CHANNELS);
			renderRegion(tmp, notesActive.size(), curTimeSamples, durationSamples, srate,
						((float*)buf.data()) + bufIndex);
			
			if(buf.size() > 8192) {
				writeData((float*)buf.data(),buf.length());
				buf.resize(0);
			}
		}
		writeData((float*)buf.data(),buf.length());
	}
}
