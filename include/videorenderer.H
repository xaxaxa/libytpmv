#include "common.H"
#include <functional>

using namespace std;
namespace ytpmv {
	// you must call ytpmv::initGL() or initialize GLEW yourself prior to calling this function
	void renderVideo(const vector<VideoSegment>& segments, int fps, int w, int h, function<void(uint8_t* data)> writeFrame);
	
	// for realtime rendering; this allows skipping frames
	class VideoRendererState;
	class VideoRendererTimeDriven {
	public:
		VideoRendererState* st;
		
		VideoRendererTimeDriven(const vector<VideoSegment>& segments, int w, int h, double fps, double systemFPS);
		~VideoRendererTimeDriven();
		
		void drawFrame();
		void advanceTo(int frame);
	};
};
