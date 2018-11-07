#include "common.H"
#include <functional>

using namespace std;
namespace ytpmv {
	// you must call ytpmv::initGL() or initialize GLEW yourself prior to calling this function;
	// data is in RGBA32 format
	void renderVideo(const vector<VideoSegment>& segments, double fps, int w, int h, function<void(uint8_t* data)> writeFrame);
	
	void renderVideo2(const vector<VideoSegment>& segments, double fps, double startSeconds, int w, int h, function<void(uint8_t* data)> writeFrame);
	
	// for realtime rendering; this allows skipping frames
	class VideoRendererState;
	class VideoRendererTimeDriven {
	public:
		VideoRendererState* st;
		
		VideoRendererTimeDriven(const vector<VideoSegment>& segments, int w, int h, double fps, double systemFPS);
		~VideoRendererTimeDriven();
		
		// draw current frame
		void drawFrame();
		
		// returns true if we are still within bounds of the video
		bool advanceTo(int frame);
		
		int concurrentSegments();
	};
};
