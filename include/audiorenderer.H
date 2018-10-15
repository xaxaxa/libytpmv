#include "common.H"
#include <functional>

using namespace std;
namespace ytpmv {
	void renderAudio(const vector<AudioSegment>& segments, int srate, function<void(float* data, int len)> writeData);
};
