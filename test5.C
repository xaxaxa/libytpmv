#include <ytpmv/framerenderer.H>
#include <stdio.h>
#include <unistd.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <math.h>

using namespace std;
using namespace ytpmv;
int main(int argc, char** argv) {
	glewExperimental = true; // Needed for core profile
	if( !glfwInit() ) {
		fprintf( stderr, "Failed to initialize GLFW\n" );
		return -1;
	}
	
	glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 
	
	
	GLFWwindow* window; // (In the accompanying source code, this variable is global for simplicity)
	window = glfwCreateWindow( 800, 500, "Tutorial 01", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window); // Initialize GLEW
	
	initGL(false);
	
	string shader =
	"vec2 mypos = vec2(param(0), param(1));\n\
	vec2 mysize = vec2(0.2,0.2);\n\
	vec2 myend = mypos+mysize;\n\
	vec2 relpos = (pos-mypos)/mysize;\n\
	if(pos.x>=mypos.x && pos.y>=mypos.y \n\
		&& pos.x<myend.x && pos.y<myend.y) \n\
		return vec4(texture2D(image, relpos).rgb, 0.8);\n\
	return vec4(0,0,0,0);\n";
	
	//texture2D(image, relpos).rgb
	
	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	
	
	FrameRenderer r(800,500);
	r.setRenderers({shader}, 16, 8);
	r.setEnabledRenderers({0});
	//r.setTime(1.2);
	
	string imgdata;
	r.setUserParams({{.7,.7}, {.35,.38}, {.57,.62}});
	r.setImages({img1, img1, img1});
	
	fprintf(stderr, "rendering\n");
	
	int frameNum = 0;
	do {
		double t = double(frameNum)/30.;
		r.setUserParams({{(sin(t*10)+1)/2,(cos(t*9)+1)/2}, {.35,.38}, {.57,.62}});
		r.draw();
		glfwSwapBuffers(window);
		glfwPollEvents();
		frameNum++;
	} while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
       glfwWindowShouldClose(window) == 0 );
	return 0;
}
