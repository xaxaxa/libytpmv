#include "common.H"

using namespace std;
namespace ytpmv {
	// load audio from a file supported by gstreamer
	// systemSRate should be set to the system sample rate
	// and is used to adjust the pitch and tempo corrections
	AudioSource* loadAudio(const char* file, int systemSRate);
	
	// load video from a file supported by gstreamer
	// deprecated in favor of MemoryVideoSource
	ImageArraySource* loadVideo(const char* file);
	
	// load video using gstreamer, and output frames (24bpp) into fd
	// DO NOT USE. See comment in common.C.
	void loadVideoToFD(const char* file, int fd);
	
	// encode a video file using raw audio (44100hz, stereo, S16LE)
	// and raw video from file descriptors, and write to outFD
	void encodeVideo(int audioFD, int videoFD, int w, int h, double fps, int srate, int outFD);
	
	// a video source that loads the entire video into textures at prepare() time.
	class MemoryVideoSource: public VideoSource {
	public:
		string file;
		vector<uint32_t> frames;
		double fps = 30.;
		
		MemoryVideoSource(string file): file(file) {}
		virtual void prepare() override;
		virtual int32_t getFrame(double timeSeconds) override;
		virtual void releaseFrame(uint32_t texture) override;
		virtual ~MemoryVideoSource() override;
	};
	
	// a video source that dynamically decodes a video file as needed; 
	// recommended only for sources that are long, referenced a few times,
	// and never overlaps.
	// currently only supports playing once
	class DynamicVideoSource: public VideoSource {
	public:
		string file;
		double fps = 30.;
		int w,h;
		
		// private
		int texture = 0;
		int videoPipe[2];
		double lastFrameTime = 0.;
		
		DynamicVideoSource(string file, int w, int h): file(file), w(w), h(h) {
			videoPipe[0] = videoPipe[1] = -1;
		}
		virtual void prepare() override;
		virtual int32_t getFrame(double timeSeconds) override;
		virtual void releaseFrame(uint32_t texture) override;
		virtual ~DynamicVideoSource() override;
	};
}
