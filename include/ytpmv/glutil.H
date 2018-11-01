
#ifndef LIBYTPMV_GLUTIL_H
#define LIBYTPMV_GLUTIL_H

class GLFWwindow;

namespace ytpmv {
	// initialize glew
	void initGL(bool createContext=true);
	// create a window & context, then initialize glew
	GLFWwindow* initGLWindowed(int w, int h);
}

#endif
