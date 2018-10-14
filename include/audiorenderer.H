#include "common.H"
#include <functional>

using namespace std;
namespace ytpmv {
	void renderAudio(const vector<AudioSegment>& segments, int srate, function<void(double* data, int len)> writeData);
};
