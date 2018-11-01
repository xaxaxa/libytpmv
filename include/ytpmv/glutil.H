
#ifndef LIBYTPMV_GLUTIL_H
#define LIBYTPMV_GLUTIL_H

class GLFWwindow;

namespace ytpmv {
	// initialize glew
	void initGL(bool createContext=true);
	// create a window & context, then initialize glew
	GLFWwindow* initGLWindowed(int w, int h);
	
	
	uint32_t createTexture();
	void deleteTexture(uint32_t texture);
	
	// image format is rgb (24bpp).
	// stride is w*3 rounded up to nearest multiple of 4
	void setTextureImage(uint32_t texture, const void* image, int w, int h);
}

#endif
