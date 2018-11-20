#include <ytpmv/audiorenderer.H>
#include <ytpmv/samplecache.H>
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

	struct ActiveNoteKeyFrame {
		// absolute time
		int timeSamples;
		double amplitude[CHANNELS];
	};
	struct ActiveNote {
		double speed;
		ActiveNoteKeyFrame *kfRight; // the keyframes right of the current point
		ActiveNoteKeyFrame* kfEnd;
		int startTimeSamples, endTimeSamples;
		
		int waveformLength;
		const float* waveform;
	};
	void renderRegion(ActiveNote* notes, int noteCount, int curTimeSamples,
						int durationSamples, int srate, float* outBuf) {
		for(int i=0;i<durationSamples;i++) {
			int t = i + curTimeSamples;
			float curSample[CHANNELS] = {};
			
			for(int j=0;j<noteCount;j++) {
				ActiveNote& n = notes[j];
				
				if(n.kfRight >= n.kfEnd) continue;
				while(t >= n.kfRight->timeSamples) {
					n.kfRight++;
					if(n.kfRight >= n.kfEnd) goto cont;
				}
				{
					ActiveNoteKeyFrame* kfLeft = n.kfRight-1;
					
					int relTime = t-n.startTimeSamples;
					int relTimeLeft = n.endTimeSamples-t;
					int kfLength = n.kfRight->timeSamples - kfLeft->timeSamples;
					double kfTimeNormalized = double(t - kfLeft->timeSamples)/double(kfLength);
					
					int sampleTime = relTime*n.speed;
					if(sampleTime<0) continue;
					if(sampleTime >= n.waveformLength/CHANNELS) continue;
					//sampleTime = sampleTime % n.waveformLength;
					
					for(int k=0; k<CHANNELS; k++) {
						float sample = n.waveform[sampleTime*CHANNELS+k];
						double amplitude = kfLeft->amplitude[k] * (1.-kfTimeNormalized)
										+ n.kfRight->amplitude[k] * kfTimeNormalized;
						
						
						int fadeSamples=20;
						double scale = 1./(double)fadeSamples;
						if(relTime < fadeSamples) sample *= relTime*scale;
						if(relTimeLeft < fadeSamples) sample *= relTimeLeft*scale;
						
						curSample[k] += sample*amplitude;
					}
				}
			cont: ;
			}
			for(int k=0; k<CHANNELS; k++)
				outBuf[i*CHANNELS+k] = curSample[k];
		}
	}

	void renderAudio(const vector<AudioSegment>& segments, int srate, function<void(float* data, int len)> writeData) {
		SampleCache cache;
		
		// pre-populate sample cache with all used samples and pitches
		PRNT(0, "populating pitch shift cache...\n");
		for(const AudioSegment& s: segments) {
			double relativePitch = s.pitch/s.tempo;
			if(relativePitch != 1.0)
				cache.getPitchShiftedSample(s.sampleData, s.sampleLength, relativePitch);
		}
		PRNT(0, "sample cache populated; %d samples\n", (int)cache.entries.size());
		
		// convert note list into note event list
		vector<NoteEvent> events;
		for(int i=0;i<(int)segments.size();i++) {
			if(segments[i].startSeconds >= segments[i].endSeconds) continue;
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
			int nextTimeSamples = events[i+1].t*srate;
			
			// enable/disable sound
			if(!evt.off) { // note on
				notesActive[evt.segmentIndex] = curTimeSamples;
				const AudioSegment& s = segments.at(evt.segmentIndex);
				PRNT(1, "note on: %5d:  pitch %5.2f  vol %3.1f  dur %3.2fs\n", evt.segmentIndex, s.pitch,s.amplitude[0], s.durationSeconds());
			} else {
				notesActive.erase(evt.segmentIndex);
				PRNT(1, "note off:%5d\n", evt.segmentIndex);
			}
			
			// calculate time until next event
			int durationSamples = nextTimeSamples-curTimeSamples;
			if(durationSamples < 3) continue;
			
			// render this region
			int noteCount = notesActive.size();
			ActiveNote tmp[noteCount];
			vector<vector<ActiveNoteKeyFrame> > keyframes;
			keyframes.resize(noteCount);
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
				if(tmp[j].endTimeSamples > tmp[j].startTimeSamples + tmp[j].waveformLength)
					tmp[j].endTimeSamples = tmp[j].startTimeSamples + tmp[j].waveformLength;
				
				// add initial keyframe
				ActiveNoteKeyFrame kf1;
				kf1.timeSamples = tmp[j].startTimeSamples;
				for(int k=0; k<CHANNELS; k++)
					kf1.amplitude[k] = s.amplitude[k];
				keyframes[j].push_back(kf1);
				
				// add note keyframes
				for(const AudioKeyFrame& kf: s.keyframes) {
					kf1.timeSamples = tmp[j].startTimeSamples + (int)round(kf.relTimeSeconds*srate);
					for(int k=0; k<CHANNELS; k++)
						kf1.amplitude[k] = s.amplitude[k]*kf.amplitude[k];
					keyframes[j].push_back(kf1);
				}
				
				// add end key frame
				kf1.timeSamples = tmp[j].endTimeSamples;
				keyframes[j].push_back(kf1);
				
				tmp[j].kfRight = keyframes[j].data() + 1;
				tmp[j].kfEnd = keyframes[j].data() + keyframes[j].size();
				j++;
			}
			
			int bufIndex = buf.length();
			buf.resize(buf.length() + durationSamples*CHANNELS);
			renderRegion(tmp, notesActive.size(), curTimeSamples, durationSamples, srate,
						((float*)buf.data()) + bufIndex);
			
			if(buf.size() > 1024*32) {
				writeData((float*)buf.data(),buf.length());
				buf.resize(0);
			}
		}
		writeData((float*)buf.data(),buf.length());
	}
}
