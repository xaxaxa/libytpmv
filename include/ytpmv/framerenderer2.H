#include "common.H"
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;
class GLFWwindow;

namespace ytpmv {
	// initialize glew
	void initGL(bool createContext=true);
	// create a window & context, then initialize glew
	GLFWwindow* initGLWindowed(int w, int h);
	
	class FrameRenderer2 {
	public:
		FrameRenderer2(int w, int h);
		
		// called once for entire video
		// code: array of glsl code containing function body only
		// maxConcurrent: the maximum number of renderer invocations in each frame
		// maxParams: the size of the user parameters array for each renderer invocation
		// the functions have the signature:
		// vec4 render(sampler2d img, vec2 pos) {...}
		// img is the current frame of the input video; pos is the on-screen
		// pixel position from 0.0 to 1.0
		void setRenderers(vector<string> code, int maxConcurrent=128, int maxParams=16);
		
		// called every time a segment starts or ends
		
		// sets the list of renderers that are called (invocations); values are the index into
		// the code array when setRenderers was called
		void setEnabledRenderers(vector<int> enabledRenderers);
		
		// set user params; params[x][y] is the yth parameter of invocation x
		void setUserParams(vector<vector<float> > params);
		
		// called every frame
		void setImages(const vector<const Image*>& images);
		void setTime(float secondsAbs, const float* secondsRel);
		void setVertexes(vector<float> vertexArray);
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
		
		vector<uint32_t> programID;
		int maxParams = 0;
		int maxConcurrent = 0;
		int w,h;
		unsigned int fbo,rbo;
		uint32_t vertexbuffer;
		
		// system maximum texture units
		int textureUnits;
		
		// if true, we will keep images in texture units and use up to the maximum
		// number of texture units
		bool persistTextureUnits = false;
		unordered_map<const void*, uint32_t> textures;
		unordered_map<const void*, int> textureUnitMap;
		vector<const void*> textureUnitContents;
		int nextTextureUnit = 0;
		
		vector<int> enabledRenderers;
		vector<int> images;
		vector<vector<float> > userParams;
		float secondsAbs;
		vector<float> secondsRel;
	};
	typedef FrameRenderer2 FrameRenderer;
}
