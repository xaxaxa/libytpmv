#include "common.H"

using namespace std;
namespace ytpmv {
	// load audio from a file supported by gstreamer
	// systemSRate should be set to the system sample rate
	// and is used to adjust the pitch and tempo corrections
	AudioSource loadAudio(const char* file, int systemSRate);
	
	// load video from a file supported by gstreamer
	VideoSource loadVideo(const char* file, double systemFPS);
	
	// encode a video file using raw audio (44100hz, stereo, S16LE)
	// and raw video from file descriptors, and write to outFD
	void encodeVideo(int audioFD, int videoFD, int w, int h, double fps, int srate, int outFD);
}