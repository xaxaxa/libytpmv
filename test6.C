#include <ytpmv/videorenderer.H>
#include <ytpmv/framerenderer.H>
#include <stdio.h>
#include <unistd.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <math.h>

using namespace std;
using namespace ytpmv;

// returns time in microseconds
uint64_t getTimeMicros() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return uint64_t(ts.tv_sec)*1000000 + uint64_t(ts.tv_nsec)/1000;
}
int main(int argc, char** argv) {
	int w=800, h=500;
	GLFWwindow* window = initGLWindowed(w,h);
	
	string shader =
	"vec2 mypos = vec2(param(0), param(1));\n\
	vec2 mysize = vec2(0.2+secondsRel/10,0.2+secondsRel/10);\n\
	vec2 myend = mypos+mysize;\n\
	vec2 relpos = (pos-mypos)/mysize;\n\
	if(pos.x>=mypos.x && pos.y>=mypos.y \n\
		&& pos.x<myend.x && pos.y<myend.y) \n\
		return vec4(texture2D(image, relpos).rgb, 0.5);\n\
	return vec4(0,0,0,0);\n";
	
	
	//texture2D(image, relpos).rgb
	
	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	
	vector<VideoSegment> segments;
	for(int i=0;i<5;i++) {
		VideoSegment s;
		s.startSeconds = i*2;
		s.endSeconds = i*2+1;
		s.speed = 1.;
		s.source = &img1;
		s.sourceFrames = 1;
		s.shader = shader;
		s.shaderParams = {0.5, 0.5};
		segments.push_back(s);
	}
	
	/*renderVideo(segments, 30, w, h, [&](uint8_t*) {
		glfwSwapBuffers(window);
		glfwPollEvents();
		if((glfwGetKey(window, GLFW_KEY_ESCAPE ) == GLFW_PRESS) ||
			glfwWindowShouldClose(window)) exit(0);
	});*/
	double fps = 30;
	VideoRendererTimeDriven r(segments, w, h, fps);
	uint64_t startTimeMicros = getTimeMicros();
	do {
		uint64_t t = getTimeMicros() - startTimeMicros;
		int frame = double(t)/1e6*fps;
		r.advanceTo(frame);
		fprintf(stderr, "drawing frame %d\n", frame);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		r.drawFrame();
		
		glfwSwapBuffers(window);
		glfwPollEvents();
	} while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
       glfwWindowShouldClose(window) == 0 );
	return 0;
}
