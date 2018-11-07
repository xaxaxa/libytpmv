#include <ytpmv/framerenderer2.H>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;
using namespace std;

// libglew-dev libglfw3-dev libglm-dev  	libgles2-mesa-dev


namespace ytpmv {
	string defaultVertexShader = R"aaaaa(
		#version 330 core
		layout(location = 0) in vec3 myPos;
		layout(location = 1) in vec2 texPos;
		uniform vec2 coordBase;
		uniform mat2 coordTransform;
		smooth out vec2 uv;
		void main(){
			gl_Position.xy = coordBase + myPos.xy * coordTransform;
			gl_Position.z = myPos.z;
			gl_Position.w = 1.0;
			uv = texPos;
		}
		)aaaaa";
	string defaultFragmentShader = R"aaaaa(
		#version 330 core
		smooth in vec2 uv;
		out vec4 color; 
		uniform sampler2D image;
		void main() {
			color = vec4(texture2D(image, uv).rgb, 1.0);
		}
		)aaaaa";

	GLuint loadShader2(string VertexShaderCode, string FragmentShaderCode) {
		// Create the shaders
		GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
		GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
		GLint Result = GL_FALSE;
		int InfoLogLength;
		
		// Compile Vertex Shader
		PRNT(0, "Compiling vertex shader\n");
		char const * VertexSourcePointer = VertexShaderCode.c_str();
		glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
		glCompileShader(VertexShaderID);

		// Check Vertex Shader
		glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
			glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
			PRNT(0, "%s\n", &VertexShaderErrorMessage[0]);
		}

		// Compile Fragment Shader
		PRNT(0, "Compiling fragment shader\n");
		char const * FragmentSourcePointer = FragmentShaderCode.c_str();
		glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
		glCompileShader(FragmentShaderID);

		// Check Fragment Shader
		glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if ( InfoLogLength > 0 ){
			std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
			glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
			PRNT(0, "%s\n", &FragmentShaderErrorMessage[0]);
		}

		// Link the program
		PRNT(0, "Linking program\n");
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
			PRNT(0, "%s\n", &ProgramErrorMessage[0]);
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
		PRNT(0, "number of texture units: %d\n", textureUnits);
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
			uniform float userParams[" + to_string(maxParams) + "];\n";
		
		
		// returns the user parameter i for the current renderer invocation
		shader +=
			"float param(int i){ \n\
				return userParams[i];\n\
			}\n";
		
		shader += "vec4 renderFunc(vec2 pos) {\n"
					+ code + "\n}";
		shader += "void main(){ \n\
						color = renderFunc(uv);\n\
					}\n";
		return shader;
	}
	void FrameRenderer2::setRenderers(vector<string> shaders, int maxConcurrent, int maxParams) {
		this->maxParams = maxParams;
		this->maxConcurrent = maxConcurrent;
		
		int programs = (int)shaders.size()/2;
		programID.resize(programs);
		for(int i=0; i<programs; i++) {
			programID.at(i) = loadShader2(shaders[i*2], shaders[i*2+1]);
			
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
		vertexes.resize(enabledRenderers.size());
		vertexVariableSizes.resize(enabledRenderers.size());
		for(int i=0; i<(int)vertexes.size(); i++) {
			vertexes[i] = nullptr;
			vertexVariableSizes[i] = nullptr;
		}
	}
	void FrameRenderer2::setInstanceCount(vector<int> instanceCount) {
		this->instanceCount = instanceCount;
	}
	void FrameRenderer2::setUserParams(vector<vector<float> > params) {
		assert(int(params.size()) <= maxConcurrent);
		userParams = params;
	}
	void FrameRenderer2::setImages(const vector<const Image*>& images) {
		assert(images.size() == enabledRenderers.size());
		for(int i=0; i<(int)images.size(); i++) {
			setImage(i, *images[i]);
		}
	}
	void FrameRenderer2::setImage(int invocation, const Image& img) {
		// select the texture unit
		// start from unit 1 because unit 0 is reserved for image data transfers
		glActiveTexture(GL_TEXTURE0 + invocation + 1);
		
		// if data is empty the image is already in a texture
		if(img.data.length() == 0) {
			glBindTexture(GL_TEXTURE_2D, img.texture);
			return;
		}
		
		const char* data = img.data.data();
		// texture cache
		if(textures.find(data) != textures.end()) {
			glBindTexture(GL_TEXTURE_2D, textures[data]);
			return;
		}
		glGenTextures(1, &textures[img.data.data()]);
		glBindTexture(GL_TEXTURE_2D, textures[img.data.data()]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.w, img.h, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data.data());
		glGenerateMipmap(GL_TEXTURE_2D);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer2::setImage(int invocation, int texture) {
		glActiveTexture(GL_TEXTURE0 + invocation + 1);
		glBindTexture(GL_TEXTURE_2D, texture);
	}
	void FrameRenderer2::setTime(float secondsAbs, const vector<float>& secondsRel) {
		this->secondsAbs = secondsAbs;
		this->secondsRel = secondsRel;
	}
	void FrameRenderer2::setVertexes(int invocation, const vector<float>& vertexArray, const int* varSizes) {
		vertexes.at(invocation) = &vertexArray;
		vertexVariableSizes.at(invocation) = varSizes;
	}
	void FrameRenderer2::draw() {
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.0f, 20.0f);
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		
		// copy vertex data into buffer
		int N = (int)enabledRenderers.size();
		vector<float> vertexBuffer;
		
		int totalVert = 0;
		for(int i=0; i<N; i++)
			totalVert += (int) vertexes.at(i)->size();
		vertexBuffer.reserve(totalVert);
		
		for(int i=0; i<N; i++) {
			const vector<float>& tmp = *vertexes.at(i);
			vertexBuffer.insert(vertexBuffer.end(),tmp.begin(),tmp.end());
		}
		glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size()*sizeof(float),
					vertexBuffer.data(), GL_STATIC_DRAW);
		
		
		
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthFunc(GL_LEQUAL);    // Set the type of depth-test
		//glShadeModel(GL_SMOOTH);   // Enable smooth shading
		//glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // Nice perspective corrections
		
		assert(glGetError()==GL_NO_ERROR);
		
		int vertexOffs = 0;
		for(int i=0; i<N; i++) {
			const vector<float>& vert = *vertexes.at(i);
			const int* varSizes = vertexVariableSizes.at(i);
			int j = enabledRenderers[i];
			glUseProgram(programID.at(j));
			
			// set vertexes
			
			// calculate number of vertexes
			int totalVarSize = 0;
			for(int varIndex=0; varSizes[varIndex] != 0; varIndex++)
				totalVarSize += varSizes[varIndex];
			int nVertex = int(vert.size()) / totalVarSize;
			
			//PRNT(0, "nVertex %d\n", nVertex);
			
			int varOffs = vertexOffs;
			for(int varIndex=0; varSizes[varIndex] != 0; varIndex++) {
				int varSize = varSizes[varIndex];
				//PRNT(0, "glVertexAttribPointer %d %d %d\n", varIndex, varSize, varOffs);
				glVertexAttribPointer(
				   varIndex,           // variable id. must match the "layout" of the variable in the shader.
				   varSize,            // number of elements per vertex
				   GL_FLOAT,           // type
				   GL_FALSE,           // normalized?
				   totalVarSize * sizeof(float),       // stride
				   (void*)(varOffs * sizeof(float))	   // array buffer offset
				);
				varOffs += varSize;
			}
			
			assert(glGetError()==GL_NO_ERROR);
			
			// set parameters
			if(int(userParams.size()) > i) {
				GLint loc = glGetUniformLocation(programID.at(j), "userParams");
				if(loc >= 0) glUniform1fv(loc, userParams.at(i).size(), userParams.at(i).data());
			}
			// set image
			{
				GLint loc = glGetUniformLocation(programID.at(j), "image");
				glUniform1i(loc, i+1); // see setImage()
			}
			// set time
			{
				GLint loc = glGetUniformLocation(programID.at(j), "secondsRel");
				GLint loc2 = glGetUniformLocation(programID.at(j), "secondsAbs");
				if(loc >= 0) glUniform1f(loc, secondsRel.at(i));
				if(loc2 >= 0) glUniform1f(loc2, secondsAbs);
			}
			// set transform matrix
			{
				GLint loc = glGetUniformLocation(programID.at(j), "proj");
				if(loc >= 0) glUniformMatrix4fv(loc, 1, false, glm::value_ptr(proj));
			}
			// draw
			glDrawArraysInstanced(GL_TRIANGLES, 0, nVertex, instanceCount.at(i));
			
			vertexOffs += vert.size();
		}
		assert(glGetError()==GL_NO_ERROR);
	}
	string FrameRenderer2::render() {
		draw();
		
		string ret;
		ret.resize(w*h*4);
		glPixelStorei(GL_PACK_ALIGNMENT,4);
		glReadPixels(0,0,w,h,  GL_RGBA,  GL_UNSIGNED_INT_8_8_8_8_REV, (void*)ret.data());
		
		assert(glGetError()==GL_NO_ERROR);
		//PRNT(0, "%d\n", (int)glGetError());
		return ret;
	}
	void FrameRenderer2::setRenderToScreen() {
		float mat[6] = {1., 0.,
						0., -1.,
						0., 0.};
		setTransform(mat);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		assert(glGetError()==GL_NO_ERROR);
	}
	void FrameRenderer2::setRenderToInternal() {
		float mat[6] = {1., 0.,
						0., 1.,
						0., 0.};
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
