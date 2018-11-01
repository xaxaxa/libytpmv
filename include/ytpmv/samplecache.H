#include "common.H"
#include <math.h>
#include <map>

using namespace std;
namespace ytpmv {
	class SampleCache {
	public:
		static const int pitchPrecision = 8; // number of steps per semitone
		struct Key {
			const float* data;
			int32_t pitch; // fraction of a semitone
		};
		
		map<Key, basic_string<float> > entries;
		map<double, int32_t> pitchCache;
		
		SampleCache();
		
		// calculate internal pitch value from linear pitch
		int32_t calculatePitch(double pitch);
		
		// return a pitch shifted sample; results are cached
		basic_string<float>& getPitchShiftedSample(const float* sampleData, int sampleLen, double pitch);
	};
	
	bool operator<(const SampleCache::Key& k1, const SampleCache::Key& k2);
}
