#include "common.H"

using namespace std;
namespace ytpmv {
	// load audio from a file supported by gstreamer
	// systemSRate should be set to the system sample rate
	// and is used to adjust the pitch and tempo corrections
	AudioSource loadAudio(const char* file, int systemSRate);
	
	VideoSource loadVideo(const char* file, double systemFPS);
}
