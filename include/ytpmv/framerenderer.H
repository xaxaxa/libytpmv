#include "common.H"
#include <string>
#include <vector>
using namespace std;

namespace ytpmv {
	class FrameRenderer {
	public:
		FrameRenderer(int w, int h);
		
		// called once for entire video
		// code: array of glsl code containing function body only
		// maxConcurrent: the maximum number of renderer invocations in each frame
		// maxParams: the size of the user parameters array for each renderer invocation
		// the functions have the signature:
		// vec4 render(sampler2d img, vec2 pos) {...}
		// img is the current frame of the input video; pos is the on-screen
		// pixel position from 0.0 to 1.0
		void setRenderers(vector<string> code, int maxConcurrent=16, int maxParams=16);
		
		// called every time a segment starts or ends
		
		// sets the list of renderers that are called; values are the index into
		// the code array when setRenderers was called
		void setEnabledRenderers(vector<int> enabledRenderers);
		
		// sets all user parameters; params must be an array of size maxConcurrent*maxParams
		// params[n*maxParams + i] is the user parameter i for invocation n (the index into 
		// the enabledRenderers array when setEnabledRenderers was called)
		void setUserParams(float* params);
		
		void setUserParams(vector<vector<float> > params);
		
		// called every frame
		void setImages(vector<Image> images);
		void setTime(float secondsAbs, const float* secondsRel);
		string render();
		
		// call this before draw() to render to screen
		void setRenderToScreen();
		// call this to return to drawing to internal framebuffer (needed for render())
		void setRenderToInternal();
		
		// mat should be an array of 6 containing:
		// xx, xy,
		// yx, yy,
		// bx, by
		void setTransform(float* mat);
	
	public:
		void draw();
		
		uint32_t programID = -1;
		int maxConcurrent = 0;
		int maxParams = 0;
		int w,h;
		unsigned int fbo,rbo;
		uint32_t vertexbuffer;
		vector<uint32_t> textures;
	};
}
