#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
//#include <GLES3/gl31.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <ytpmv/glcontext.H>

using namespace std;

namespace ytpmv {
	
	// libglew-dev libglfw3-dev libglm-dev  	libgles2-mesa-dev

	void _init_opengl__() {
		bool res;
	 
		int32_t fd = open ("/dev/dri/renderD128", O_RDWR);
		assert (fd > 0);

		struct gbm_device *gbm = gbm_create_device (fd);
		assert (gbm != NULL);

		/* setup EGL from the GBM device */
		EGLDisplay egl_dpy = eglGetPlatformDisplay (EGL_PLATFORM_GBM_MESA, gbm, NULL);
		assert (egl_dpy != NULL);

		res = eglInitialize (egl_dpy, NULL, NULL);
		assert (res);

		const char *egl_extension_st = eglQueryString (egl_dpy, EGL_EXTENSIONS);
		assert (strstr (egl_extension_st, "EGL_KHR_create_context") != NULL);
		assert (strstr (egl_extension_st, "EGL_KHR_surfaceless_context") != NULL);

		static const EGLint config_attribs[] = {
		  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
		  EGL_NONE
		};
		EGLConfig cfg;
		EGLint count;

		res = eglChooseConfig (egl_dpy, config_attribs, &cfg, 1, &count);
		assert (res);

		res = eglBindAPI (EGL_OPENGL_API);
		assert (res);

		static const EGLint attribs[] = {
		  EGL_CONTEXT_CLIENT_VERSION, 3,
		  EGL_NONE
		};
		EGLContext core_ctx = eglCreateContext (egl_dpy,
											   cfg,
											   EGL_NO_CONTEXT,
											   attribs);
		assert (core_ctx != EGL_NO_CONTEXT);

		res = eglMakeCurrent (egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, core_ctx);
		assert (res);
	}

	void _init_opengl__2() {
		
		static const EGLint configAttribs[] = {
			  EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
			  EGL_BLUE_SIZE, 8,
			  EGL_GREEN_SIZE, 8,
			  EGL_RED_SIZE, 8,
			  EGL_DEPTH_SIZE, 8,
			  EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
			  EGL_NONE
		};    

		static const int pbufferWidth = 9;
		static const int pbufferHeight = 9;

		static const EGLint pbufferAttribs[] = {
			EGL_WIDTH, pbufferWidth,
			EGL_HEIGHT, pbufferHeight,
			EGL_NONE,
		};
		// 1. Initialize EGL
		EGLDisplay eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

		EGLint major, minor;

		eglInitialize(eglDpy, &major, &minor);

		// 2. Select an appropriate configuration
		EGLint numConfigs;
		EGLConfig eglCfg;

		eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

		// 3. Create a surface
		EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, 
												   pbufferAttribs);

		// 4. Bind the API
		eglBindAPI(EGL_OPENGL_API);

		// 5. Create a context and make it current
		EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, 
										   NULL);

		eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

		// from now on use your OpenGL context
	}

	void initGL(bool createContext) {
		if(createContext)
			createGLContext();
		glewExperimental=true; // Needed in core profile
		if (glewInit() != GLEW_OK) {
			throw runtime_error("Failed to initialize GLEW\n");
		}
	}
	
	void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
		glViewport(0, 0, width, height);
	}
	GLFWwindow* initGLWindowed(int w, int h) {
		glewExperimental = true; // Needed for core profile
		if( !glfwInit() ) {
			fprintf( stderr, "Failed to initialize GLFW\n" );
			return nullptr;
		}
		
		glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 
		
		
		GLFWwindow* window; // (In the accompanying source code, this variable is global for simplicity)
		window = glfwCreateWindow(w, h, "libytpmv", NULL, NULL);
		if( window == NULL ) {
			fprintf( stderr, "Failed to open GLFW window.\n" );
			glfwTerminate();
			return nullptr;
		}
		glfwMakeContextCurrent(window);
		initGL(false);
		
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
		
		return window;
	}
	
	uint32_t createTexture() {
		uint32_t tex = 0;
		glGenTextures(1, &tex);
		assert(glGetError()==GL_NO_ERROR);
		return tex;
	}
	void deleteTexture(uint32_t texture) {
		glDeleteTextures(1, &texture);
		assert(glGetError()==GL_NO_ERROR);
	}
	void setTextureImage(uint32_t texture, const void* image, int w, int h) {
		glPixelStorei(GL_UNPACK_ALIGNMENT,4);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		assert(glGetError()==GL_NO_ERROR);
	}
}
