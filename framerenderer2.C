#include <ytpmv/framerenderer2.H>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <stdexcept>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
//#include <GLES3/gl31.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


using namespace glm;
using namespace std;

// libglew-dev libglfw3-dev libglm-dev  	libgles2-mesa-dev


namespace ytpmv {
	string VertexShaderCode2 = "#version 330 core\n\
	layout(location = 0) in vec3 myPos;\
	uniform vec2 coordBase;\n\
	uniform mat2 coordTransform;\n\
	smooth out vec2 uv;\n\
	void main(){\
		gl_Position.xyz = myPos;\
		gl_Position.w = 1.0;\
		uv = coordBase + myPos.xy * coordTransform;\n\
	}\
	";

	GLuint loadShader2(string FragmentShaderCode) {
		// Create the shaders
		GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
		GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
		GLint Result = GL_FALSE;
		int InfoLogLength;
		
		// Compile Vertex Shader
		fprintf(stderr, "Compiling vertex shader\n");
		char const * VertexSourcePointer = VertexShaderCode2.c_str();
		glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
		glCompileShader(VertexShaderID);

		// Check Vertex Shader
		glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
			glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
			fprintf(stderr, "%s\n", &VertexShaderErrorMessage[0]);
		}

		// Compile Fragment Shader
		fprintf(stderr, "Compiling fragment shader\n");
		char const * FragmentSourcePointer = FragmentShaderCode.c_str();
		glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
		glCompileShader(FragmentShaderID);

		// Check Fragment Shader
		glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
			glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
			fprintf(stderr, "%s\n", &FragmentShaderErrorMessage[0]);
		}

		// Link the program
		fprintf(stderr, "Linking program\n");
		GLuint ProgramID = glCreateProgram();
		glAttachShader(ProgramID, VertexShaderID);
		glAttachShader(ProgramID, FragmentShaderID);
		glLinkProgram(ProgramID);

		// Check the program
		glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
		glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			std::vector<char> ProgramErrorMessage(InfoLogLength+1);
			glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
			fprintf(stderr, "%s\n", &ProgramErrorMessage[0]);
		}
		glDetachShader(ProgramID, VertexShaderID);
		glDetachShader(ProgramID, FragmentShaderID);
		glDeleteShader(VertexShaderID);
		glDeleteShader(FragmentShaderID);

		return ProgramID;
	}
	FrameRenderer2::FrameRenderer2(int w, int h): w(w), h(h) {
		//_init_opengl__2();
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, w, h);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
		
		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			throw runtime_error("framebuffer not complete\n");
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		
		glViewportIndexedf(0,0,0,w,h);
		
		// Dark blue background
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		
		GLuint VertexArrayID;
		glGenVertexArrays(1, &VertexArrayID);
		glBindVertexArray(VertexArrayID);
		
		
		// An array of 6 vectors which represents 6 vertices
		float x1=-1,y1=-1,x2=1,y2=1;
		static const GLfloat g_vertex_buffer_data[] = {
		   x1, y1, 0.0f,
		   x2, y1, 0.0f,
		   x2, y2, 0.0f,
		   
		   x2, y2, 0.0f,
		   x1, y2, 0.0f,
		   x1, y1, 0.0f,
		};
		
		// This will identify our vertex buffer
		//GLuint vertexbuffer;
		// Generate 1 buffer, put the resulting identifier in vertexbuffer
		glGenBuffers(1, &vertexbuffer);
		// The following commands will talk about our 'vertexbuffer' buffer
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		// Give our vertices to OpenGL.
		glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
		
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &textureUnits);
		fprintf(stderr, "number of texture units: %d\n", textureUnits);
		textureUnitContents.resize(textureUnits);
	}
	string FrameRenderer2_generateCode(string code, int maxParams) {
		string shader = "\n\
			#version 330 core\n\
			in vec4 gl_FragCoord;\n\
			smooth in vec2 uv;\n\
			out vec4 color; \n\
			uniform vec2 resolution;\n\
			uniform float secondsAbs;\n\
			uniform float secondsRel; \n\
			uniform sampler2D image;\n\
			uniform float extraParams[" + to_string(maxParams) + "];\n";
		
		
		// returns the user parameter i for the current renderer invocation
		shader +=
			"float param(int i){ \n\
				return extraParams[i];\n\
			}\n";
		
		shader += "vec4 renderFunc(vec2 pos) {\n"
					+ code + "\n}";
		return shader;
	}
	void FrameRenderer2::setRenderers(vector<string> code, int maxConcurrent, int maxParams) {
		this->maxParams = maxParams;
		this->maxConcurrent = maxConcurrent;
		programID.resize(code.size());
		for(int i=0; i<(int)code.size(); i++) {
			string shader = FrameRenderer2_generateCode(code[i], maxParams);
			
			// main function
			shader +=
				"void main(){ \n\
					vec4 tmp;\n\
					color = renderFunc(uv);\n\
				}\n";

			//fprintf(stderr, "%s\n", shader.c_str());;
			programID.at(i) = loadShader2(shader);
			
			glUseProgram(programID.at(i));
			GLint loc = glGetUniformLocation(programID.at(i), "resolution");
			if(loc >= 0) glUniform2f(loc, w, h);
		}
		/*textures.resize(maxConcurrent);
		glGenTextures(maxConcurrent, &textures[0]);*/
		assert(glGetError()==GL_NO_ERROR);
		setRenderToInternal();
	}
	void FrameRenderer2::setEnabledRenderers(vector<int> enabledRenderers) {
		this->enabledRenderers = enabledRenderers;
	}
	void FrameRenderer2::setUserParams(vector<vector<float> > params) {
		assert(int(params.size()) <= maxConcurrent);
		userParams = params;
	}
	void FrameRenderer2::setImages(const vector<const Image*>& images) {
		assert(int(images.size()) <= maxConcurrent);
		int sz=(int)images.size();
		this->images.resize(sz);
		for(int i=0; i<sz; i++) {
			const Image& img = *images[i];
			const char* data = img.data.data();
			
			// select a texture unit
			int textureUnit = -1;
			if(persistTextureUnits) {
				auto it = textureUnitMap.find(data);
				if(it == textureUnitMap.end()) {
					// select texture unit
					textureUnit = nextTextureUnit++;
					if(nextTextureUnit >= textureUnits) nextTextureUnit=0;
					
					// clear it
					textureUnitMap.erase(textureUnitContents.at(textureUnit));
					
					// add to cache
					textureUnitMap[data] = textureUnit;
					textureUnitContents.at(textureUnit) = data;
				} else {
					textureUnit = (*it).second;
					goto cont;
				}
				fprintf(stderr, "selected texture unit %d\n", textureUnit);
			} else {
				textureUnit = i;
			}
			
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			
			//int fuck=0;
			//glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &fuck);
			//fprintf(stderr, "%d\n", fuck);
			
			// texture cache
			if(textures.find(img.data.data()) != textures.end()) {
				glBindTexture(GL_TEXTURE_2D, textures[data]);
				//fprintf(stderr, "CACHE HIT\n");
				goto cont;
			}
			//fprintf(stderr, "CACHE MISS\n");
			glGenTextures(1, &textures[img.data.data()]);
			glBindTexture(GL_TEXTURE_2D, textures[img.data.data()]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.w, img.h, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data.data());
			glGenerateMipmap(GL_TEXTURE_2D);
			assert(glGetError()==GL_NO_ERROR);
		
		cont:
			this->images[i] = textureUnit;
		}
	}
	void FrameRenderer2::setTime(float secondsAbs, const float* secondsRel) {
		this->secondsAbs = secondsAbs;
		this->secondsRel = vector<float>(secondsRel, secondsRel+enabledRenderers.size());
	}
	void FrameRenderer2::draw() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		// 1st attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glEnable(GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glVertexAttribPointer(
		   0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		   3,                  // size
		   GL_FLOAT,           // type
		   GL_FALSE,           // normalized?
		   0,                  // stride
		   (void*)0            // array buffer offset
		);
		assert(glGetError()==GL_NO_ERROR);
		
		for(int i=0; i<(int)enabledRenderers.size(); i++) {
			int j = enabledRenderers[i];
			glUseProgram(programID.at(j));
			// set parameters
			if(userParams.size() > i) {
				GLint loc = glGetUniformLocation(programID.at(j), "extraParams");
				if(loc >= 0) glUniform1fv(loc, userParams.at(i).size(), userParams.at(i).data());
			}
			// set image
			{
				GLint loc = glGetUniformLocation(programID.at(j), "image");
				glUniform1i(loc, this->images.at(i));
			}
			// set time
			{
				GLint loc = glGetUniformLocation(programID.at(j), "secondsRel");
				GLint loc2 = glGetUniformLocation(programID.at(j), "secondsAbs");
				if(loc >= 0) glUniform1f(loc, secondsRel.at(i));
				if(loc2 >= 0) glUniform1f(loc2, secondsAbs);
			}
			// draw
			glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vertices
		}
		glDisableVertexAttribArray(0);
		assert(glGetError()==GL_NO_ERROR);
	}
	string FrameRenderer2::render() {
		draw();
		
		string ret;
		ret.resize(w*h*4);
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		glReadPixels(0,0,w,h,  GL_RGBA,  GL_UNSIGNED_INT_8_8_8_8_REV, (void*)ret.data());
		
		assert(glGetError()==GL_NO_ERROR);
		//fprintf(stderr, "%d\n", (int)glGetError());
		return ret;
	}
	void FrameRenderer2::setRenderToScreen() {
		float mat[6] = {0.5, 0.,
						0., -0.5,
						0.5, 0.5};
		setTransform(mat);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer2::setRenderToInternal() {
		float mat[6] = {0.5, 0.,
						0., 0.5,
						0.5, 0.5};
		setTransform(mat);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer2::setTransform(float* mat) {
		for(int i=0; i<(int)programID.size(); i++) {
			glUseProgram(programID[i]);
			GLint loc = glGetUniformLocation(programID[i], "coordTransform");
			if(loc >= 0)
				glUniformMatrix2fv(loc, 1, false, mat);
			
			loc = glGetUniformLocation(programID[i], "coordBase");
			if(loc >= 0)
				glUniform2f(loc, mat[4], mat[5]);
			assert(glGetError()==GL_NO_ERROR);
		}
	}
}
