#include <ytpmv/framerenderer.H>
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

#include <ytpmv/glcontext.H>

using namespace glm;
using namespace std;

namespace ytpmv {
	string VertexShaderCode = "#version 330 core\n\
	layout(location = 0) in vec3 vertexPosition_modelspace;\
	void main(){\
	gl_Position.xyz = vertexPosition_modelspace;\
	gl_Position.w = 1.0;\
	}\
	";

	GLuint loadShader(string FragmentShaderCode) {
		// Create the shaders
		GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
		GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
		GLint Result = GL_FALSE;
		int InfoLogLength;
		
		// Compile Vertex Shader
		fprintf(stderr, "Compiling vertex shader\n");
		char const * VertexSourcePointer = VertexShaderCode.c_str();
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
	FrameRenderer::FrameRenderer(int w, int h): w(w), h(h) {
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
		
		
		// An array of 4 vectors which represents 4 vertices
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
	}
	string FrameRenderer_generateCode(vector<string> code, int maxConcurrent, int maxParams) {
		string shader = "\n\
			#version 330 core\n\
			in vec4 gl_FragCoord;\n\
			out vec3 color; \n\
			int paramOffset; \n\
			uniform vec2 coordBase;\n\
			uniform mat2 coordTransform;\n\
			uniform vec2 resolution; \n\
			uniform float secondsAbs;\n\
			uniform float secondsRelArr[" + to_string(maxConcurrent) + "]; \n\
			uniform int enabledRendererCount;\n\
			uniform sampler2D images[" + to_string(maxConcurrent) + "];\n\
			uniform int enabledRenderers[" + to_string(maxConcurrent) + "];\n\
			uniform float extraParams[" + to_string(maxConcurrent*maxParams) + "];\n";
		
		
		// returns the user parameter i for the current renderer invocation
		shader +=
			"float param(int i){ \n\
				return extraParams[paramOffset+i];\n\
			}\n";
		
		// all the user render functions, each named renderFunc$i
		for(int i=0; i<(int)code.size(); i++) {
			shader += "vec4 renderFunc" + to_string(i) + "(vec2 pos, float secondsRel, sampler2D image) {\n"
						+ code[i] + "\n}";
		}
		
		// a function that takes a renderer id and calls it
		shader +=
			"vec4 renderDispatch(int id, vec2 pos, float secondsRel, sampler2D image){ \n\
				switch(id) {\n";
		for(int i=0; i<(int)code.size(); i++) {
			shader += "case " + to_string(i) + ":\n";
			shader += "return renderFunc" + to_string(i) + "(pos, secondsRel, image);\n";
			shader += "break;";
		}
		shader +=
			"	}\n\
			}";
		return shader;
	}
	void FrameRenderer::setRenderers(vector<string> code, int maxConcurrent, int maxParams) {
		string shader = FrameRenderer_generateCode(code, maxConcurrent, maxParams);
		
		// main function
		shader +=
			"void main(){ \n\
				vec2 uv = gl_FragCoord.xy*coordTransform + coordBase;\n\
				vec4 tmp;\n\
				color = vec3(0.0,0.0,0.0);\n";
		for(int i=0; i<maxConcurrent; i++) {
			string I = to_string(i);
			shader +=
				"if(" + I + " < enabledRendererCount) {\n\
					paramOffset = " + to_string(i*maxParams) + ";\n\
					tmp = renderDispatch(enabledRenderers[" + I + "], uv, secondsRelArr[" + I + "], images[" + I + "]);\n\
					color = color*(1-tmp.a) + tmp.rgb*tmp.a;\n\
				}\n";
		}
		shader += 
			"}\n";
		/*
		shader +=
			"void main(){ \n\
				vec2 uv = gl_FragCoord.xy/resolution;\n\
				vec4 tmp;\n\
				color = vec3(0.0,0.0,0.0);\n\
				for(int i=0; i<enabledRendererCount; i++) {\n\
					paramOffset = i*" + to_string(maxParams) + ";\n\
					tmp = renderDispatch(enabledRenderers[i], uv, images[i]);\n\
					color = color*(1-tmp.a) + tmp.rgb*tmp.a;\n\
				}\n\
			}\n";*/

		//fprintf(stderr, "%s\n", shader.c_str());;
		programID = loadShader(shader);
		glUseProgram(programID);
		this->maxConcurrent = maxConcurrent;
		this->maxParams = maxParams;
		
		GLint loc = glGetUniformLocation(programID, "resolution");
		if(loc < 0) fprintf(stderr, "glGetUniformLocation() can not find resolution variable\n");
		glUniform2f(loc, (float)w, (float)h);
		
		textures.resize(maxConcurrent);
		glGenTextures(maxConcurrent, &textures[0]);
		assert(glGetError()==GL_NO_ERROR);
		
		setRenderToInternal();
	}
	void FrameRenderer::setEnabledRenderers(vector<int> enabledRenderers) {
		GLint loc1 = glGetUniformLocation(programID, "enabledRendererCount");
		GLint loc2 = glGetUniformLocation(programID, "enabledRenderers");
		if(loc1 < 0) throw runtime_error("glGetUniformLocation() can not find enabledRendererCount variable");
		if(loc2 < 0) throw runtime_error("glGetUniformLocation() can not find enabledRenderers variable");
		
		assert(int(enabledRenderers.size()) <= maxConcurrent);
		
		glUniform1i(loc1, (int)enabledRenderers.size());
		
		GLint rendererIDs[maxConcurrent];
		for(int i=0; i<maxConcurrent; i++)
			rendererIDs[i] = -1;
		for(int i=0; i<(int)enabledRenderers.size(); i++)
			rendererIDs[i] = enabledRenderers[i];
		
		glUniform1iv(loc2, maxConcurrent, rendererIDs);
	}
	void FrameRenderer::setUserParams(float* params) {
		GLint loc = glGetUniformLocation(programID, "extraParams");
		if(loc < 0) {
			fprintf(stderr, "glGetUniformLocation() can not find extraParams variable\n");
			return;
		}
		glUniform1fv(loc, maxConcurrent*maxParams, params);
	}
	void FrameRenderer::setUserParams(vector<vector<float> > params) {
		assert(int(params.size()) <= maxConcurrent);
		
		float tmp[maxConcurrent*maxParams];
		memset(tmp,0,sizeof(tmp));
		
		int n=0;
		for(auto& arr: params) {
			int i=0;
			assert(int(arr.size()) <= maxParams);
			for(float val: arr) {
				tmp[n*maxParams + i] = val;
				i++;
			}
			n++;
		}
		setUserParams(tmp);
	}
	void FrameRenderer::setImages(vector<Image> images) {
		assert(int(images.size()) <= maxConcurrent);
		int sz=(int)images.size();
		for(int i=0; i<sz; i++) {
			const Image& img = images[i];
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, textures[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.w, img.h, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data.data());
			glGenerateMipmap(GL_TEXTURE_2D);
			assert(glGetError()==GL_NO_ERROR);
		}
		GLint loc = glGetUniformLocation(programID, "images");
		if(loc < 0) {
			fprintf(stderr, "glGetUniformLocation() can not find images variable\n");
			return;
		}
		int textureIDs[maxConcurrent];
		for(int i=0; i<maxConcurrent; i++)
			textureIDs[i] = i;
		glUniform1iv(loc, maxConcurrent, textureIDs);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer::setTime(float secondsAbs, const float* secondsRel) {
		GLint loc = glGetUniformLocation(programID, "secondsRelArr");
		GLint loc2 = glGetUniformLocation(programID, "secondsAbs");
		if(loc >= 0)
			glUniform1fv(loc, maxConcurrent, secondsRel);
		if(loc2 >= 0)
			glUniform1f(loc2, secondsAbs);
	}
	void FrameRenderer::draw() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		// 1st attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
		   0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		   3,                  // size
		   GL_FLOAT,           // type
		   GL_FALSE,           // normalized?
		   0,                  // stride
		   (void*)0            // array buffer offset
		);
		// Draw the triangle !
		glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vertices
		glDisableVertexAttribArray(0);
		assert(glGetError()==GL_NO_ERROR);
	}
	string FrameRenderer::render() {
		draw();
		
		string ret;
		ret.resize(w*h*4);
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		glReadPixels(0,0,w,h,  GL_RGBA,  GL_UNSIGNED_INT_8_8_8_8_REV, (void*)ret.data());
		
		assert(glGetError()==GL_NO_ERROR);
		//fprintf(stderr, "%d\n", (int)glGetError());
		return ret;
	}
	void FrameRenderer::setRenderToScreen() {
		float mat[6] = {1./w, 0.,
						0., -1./h,
						0., 1.};
		setTransform(mat);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer::setRenderToInternal() {
		float mat[6] = {1./w, 0.,
						0., 1./h,
						0., 0.};
		setTransform(mat);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer::setTransform(float* mat) {
		GLint loc = glGetUniformLocation(programID, "coordTransform");
		if(loc >= 0)
			glUniformMatrix2fv(loc, 1, false, mat);
		
		loc = glGetUniformLocation(programID, "coordBase");
		if(loc >= 0)
			glUniform2f(loc, mat[4], mat[5]);
	}
}
