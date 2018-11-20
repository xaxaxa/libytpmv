#include "common.H"
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

namespace ytpmv {
	extern string defaultVertexShader;
	extern string defaultFragmentShader;
	
	// given only fragment shader body, generate the full shader code
	string FrameRenderer2_generateCode(string code, int maxParams);
	
	class FrameRenderer2 {
	public:
		FrameRenderer2(int w, int h);
		
		// called once for entire video
		// shaders: interleaved array of (vertexShader0, fragmentShader0, ...)
		// maxConcurrent: the maximum number of renderer invocations in each frame
		// maxParams: the size of the user parameters array for each renderer invocation
		// the functions have the signature:
		// vec4 render(sampler2d img, vec2 pos) {...}
		// img is the current frame of the input video; pos is the on-screen
		// pixel position from 0.0 to 1.0
		void setRenderers(vector<string> shaders, int maxConcurrent=128, int maxParams=16);
		
		// called every time a segment starts or ends
		
		// sets the list of renderers that are called (invocations)
		void setEnabledRenderers(vector<int> enabledRenderers);
		
		void setInstanceCount(vector<int> instanceCount);
		
		// set user params; params[x][y] is the yth parameter of invocation x
		void setUserParams(vector<vector<float> > params);
		
		// called every frame
		void setImages(const vector<const Image*>& images);
		void setImage(int invocation, const Image& img);
		void setImage(int invocation, int texture);
		
		void setTime(float secondsAbs, const vector<float>& secondsRel);
		
		// layout of the vertex array is:
		// V0var0_0, V0var0_1, V0var0_2, V0var1_0, V0var1_1, V1var0_0, ...
		// varSizes specifies the number of elements for each variable and is null-terminated
		void setVertexes(int invocation, const vector<float>& vertexArray, const int* varSizes);
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
		int oversample = 8;
		unsigned int fbo;
		unsigned int rbo[2];
		unsigned int outp_fbo = 0, outp_rbo = 0;
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
		vector<int> instanceCount;
		vector<vector<float> > userParams;
		float secondsAbs;
		vector<float> secondsRel;
		vector<const vector<float>* > vertexes;
		vector<const int*> vertexVariableSizes;
	};
	typedef FrameRenderer2 FrameRenderer;
}
