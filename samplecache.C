#include <ytpmv/samplecache.H>
#include <soundtouch/SoundTouch.h>

namespace ytpmv {
	SampleCache::SampleCache() {}
	int32_t SampleCache::calculatePitch(double pitch) {
		// look in cache first
		auto it = pitchCache.find(pitch);
		if(it != pitchCache.end()) return (*it).second;
		
		// calculate and return semitones * pitchPrecision, rounded to integer
		int32_t res = int32_t(round(log2(pitch)*12.*double(pitchPrecision)));
		pitchCache[pitch] = res;
		return res;
	}
	basic_string<float>& SampleCache::getPitchShiftedSample(const float* sampleData, int sampleLen, double pitch) {
		// look in cache first
		Key key = {sampleData, calculatePitch(pitch)};
		auto it = entries.find(key);
		if(it != entries.end()) return (*it).second;
		
		// perform pitch shifting
		soundtouch::SoundTouch st;
		st.setChannels(CHANNELS);
		st.setSampleRate(44100);
		st.setPitch(pitch);
		st.putSamples(sampleData,sampleLen/CHANNELS);
		
		// make sure we reference the string object that is actually in the map structure
		basic_string<float>& ret = entries[key];
		while(1) {
			int bs = 4096;
			int pos = (int)ret.size();
			ret.resize(pos+bs*CHANNELS);
			int r = (int)st.receiveSamples(const_cast<float*>(ret.data()+pos), bs);
			if(r < bs) {
				ret.resize(pos+r*CHANNELS);
				break;
			}
		}
		return ret;
	}
	bool operator<(const SampleCache::Key& k1, const SampleCache::Key& k2) {
		if(k1.data < k2.data) return true;
		if(k1.data > k2.data) return false;
		return k1.pitch < k2.pitch;
	}
}
