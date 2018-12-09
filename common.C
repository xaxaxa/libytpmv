#include <ytpmv/common.H>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace ytpmv {
	int verbosity = 0;
	
	void ImageSource::prepare() {
		if(_tex != 0) return;
		_tex = createTexture();
		setTextureImage(_tex, img->data.data(), img->w, img->h);
	}
	int32_t ImageSource::getFrame(double timeSeconds) {
		return _tex;
	}
	void ImageSource::releaseFrame(uint32_t texture) {
		// nothing to do
	}
	ImageSource::~ImageSource() {
		deleteTexture(_tex);
	}
	
	void ImageArraySource::prepare() {
		// nothing to do
	}
	int32_t ImageArraySource::getFrame(double timeSeconds) {
		int i = clamp((int)round(timeSeconds*speed*fps), 0, int(frames.size())-1);
		Image& img = frames.at(i);
		return cache.getTexture(img.data.data(), img.w, img.h);
	}
	void ImageArraySource::releaseFrame(uint32_t texture) {
		// nothing to do
	}
	
	AudioSegment::AudioSegment() {
		for(int k=0; k<CHANNELS; k++)
			amplitude[k] = 1.;
	}
	AudioSegment::AudioSegment(const Note& n, const Instrument& ins, double bpm): AudioSegment() {
		startSeconds = n.start.toSeconds(bpm);
		endSeconds = n.end.toSeconds(bpm);
		pitch = pow(2,(n.pitchSemitones + ins.tuningSemitones)/12.);
		tempo = pitch;
		sampleData = ins.sampleData.data();
		sampleLength = ins.sampleData.length();
		addInitialKeyFrame(n);
		copyKeyFrames(n.keyframes, pow(2,ins.tuningSemitones/12.), bpm);
	}
	AudioSegment::AudioSegment(const Note& n, const AudioSource* src, double bpm): AudioSegment() {
		startSeconds = n.start.toSeconds(bpm);
		endSeconds = n.end.toSeconds(bpm);
		pitch = pow(2,n.pitchSemitones/12.) * src->pitch;
		tempo = src->tempo;
		sampleData = src->sample.data();
		sampleLength = src->sample.length();
		addInitialKeyFrame(n);
		copyKeyFrames(n.keyframes, src->pitch, bpm);
	}
	AudioSegment::AudioSegment(const Note& n, const Source* src, double bpm):
			AudioSegment(n,src->audio,bpm) {
		pitch *= src->pitch;
		tempo *= src->tempo;
		for(int k=0; k<CHANNELS; k++)
			amplitude[k] *= pow(10,src->amplitudeDB/20.);
	}
	void AudioSegment::addInitialKeyFrame(const Note& n) {
		AudioKeyFrame kf1;
		kf1.relTimeSeconds = 0.;
		for(int k=0; k<CHANNELS; k++)
			kf1.amplitude[k] = pow(10,n.amplitudeDB/20.);
		kf1.pitch = 1.;
		keyframes.push_back(kf1);
	}
	void AudioSegment::copyKeyFrames(const vector<NoteKeyFrame>& kfs, double pitch, double bpm) {
		for(const NoteKeyFrame& kf: kfs) {
			AudioKeyFrame kf2;
			kf2.relTimeSeconds = kf.relTimeRows*60/bpm;
			for(int k=0; k<CHANNELS; k++)
				kf2.amplitude[k] = pow(10,kf.amplitudeDB/20.);
			kf2.pitch = pow(2,kf.pitchSemitones/12.) * pitch;
			keyframes.push_back(kf2);
		}
	}
	
	vector<float> genRectangle(float x1, float y1, float x2, float y2) {
		float tx1=0., ty1=0., tx2=1., ty2=1.;
		// vertex attribute layout must be (x,y,z) (textureX,textureY)
		return {
		   x1, y1, 0.0f, tx1, ty1,
		   x2, y1, 0.0f, tx2, ty1,
		   x2, y2, 0.0f, tx2, ty2,
		   
		   x2, y2, 0.0f, tx2, ty2,
		   x1, y2, 0.0f, tx1, ty2,
		   x1, y1, 0.0f, tx1, ty1
		};
		/*
		return {
		   x1, y1, 0.0f, x1, y1,
		   x2, y1, 0.0f, x2, y1,
		   x2, y2, 0.0f, x2, y2,
		   
		   x2, y2, 0.0f, x2, y2,
		   x1, y2, 0.0f, x1, y2,
		   x1, y1, 0.0f, x1, y1
		};*/
	}
	
	vector<float> genRectangleWithCenter(float x1, float y1, float x2, float y2, float tx1, float ty1, float tx2, float ty2, float z) {
		float cx = (x1+x2)/2., cy = (y1+y2)/2., cz = z;
		// vertex attribute layout must be (x,y,z) (cx,cy,cz) (textureX,textureY)
		return {
		   x1, y1, z, cx, cy, cz, tx1, ty1,
		   x2, y1, z, cx, cy, cz, tx2, ty1,
		   x2, y2, z, cx, cy, cz, tx2, ty2,
		   
		   x2, y2, z, cx, cy, cz, tx2, ty2,
		   x1, y2, z, cx, cy, cz, tx1, ty2,
		   x1, y1, z, cx, cy, cz, tx1, ty1
		};
	}
	
	vector<float> genRectangle(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3) {
		float tx1=0., ty1=0., tx2=1., ty2=1.;
		// vertex attribute layout must be (x,y,z) (textureX,textureY)
		
		
		/* (x1,y1,z1)   (x2,y2,z2)
		 *      |------------|
		 *      |            |
		 *      |            |
		 *      |------------|
		 *              (x3,y3,z3)
		 */
		float cx = (x1+x3), cy = (y1+y3), cz = (z1+z3);
		float x4 = cx-x2, y4 = cy-y2, z4 = cz-z2;
		return {
		   x1, y1, z1, tx1, ty1,
		   x2, y2, z2, tx2, ty1,
		   x3, y3, z3, tx2, ty2,
		   
		   x3, y3, z3, tx2, ty2,
		   x4, y4, z4, tx1, ty2,
		   x1, y1, z1, tx1, ty1
		};
	}
	
	vector<float> genParallelpiped() {
		// vertex attribute layout must be (x,y,z) (textureX,textureY)
		
		vector<float> lower = genRectangle(-1,-1,-1,1,-1,-1,1,1,-1);
		vector<float> upper = genRectangle(-1,-1,1,1,-1,1,1,1,1);
		vector<float> left = genRectangle(-1,-1,-1,-1,-1,1,-1,1,1);
		vector<float> right = genRectangle(1,-1,-1,1,-1,1,1,1,1);
		vector<float> top = genRectangle(-1,-1,-1,1,-1,-1,1,-1,1);
		vector<float> bottom = genRectangle(-1,1,-1,1,1,-1,1,1,1);
		
		vector<float> res;
		res.reserve(5*6*6);
		res.insert(res.end(),lower.begin(),lower.end());
		res.insert(res.end(),upper.begin(),upper.end());
		res.insert(res.end(),left.begin(),left.end());
		res.insert(res.end(),right.begin(),right.end());
		res.insert(res.end(),top.begin(),top.end());
		res.insert(res.end(),bottom.begin(),bottom.end());
		return res;
	}
	
	VideoSegment::VideoSegment() {
		startSeconds = 0.;
		endSeconds = 0.;
		vertexes = genRectangle(-1, -1, 1, 1);
		vertexVarSizes[0] = 3;
		vertexVarSizes[1] = 2;
		vertexVarSizes[2] = 0;
	}
	VideoSegment::VideoSegment(const Note& n, VideoSource* src, double bpm) {
		startSeconds = n.start.toSeconds(bpm);
		endSeconds = n.end.toSeconds(bpm);
		source = src;
		vertexes = genRectangle(-1, -1, 1, 1);
		vertexVarSizes[0] = 3;
		vertexVarSizes[1] = 2;
		vertexVarSizes[2] = 0;
	}
	VideoSegment::VideoSegment(VideoSource* src, double startSeconds, double endSeconds) {
		this->startSeconds = startSeconds;
		this->endSeconds = endSeconds;
		source = src;
		vertexes = genRectangle(-1, -1, 1, 1);
		vertexVarSizes[0] = 3;
		vertexVarSizes[1] = 2;
		vertexVarSizes[2] = 0;
	}
	VideoSegment::VideoSegment(const Note& n, Source* src, double bpm):
		VideoSegment(n, src->video, bpm) {
		speed *= src->tempo;
	}
	
	std::string get_file_contents(const char *filename) {
		std::ifstream in(filename, std::ios::in | std::ios::binary);
		if(in) {
			std::ostringstream contents;
			contents << in.rdbuf();
			in.close();
			return(contents.str());
		}
		throw(errno);
	}
	int readAll(int fd,void* buf, int len) {
		uint8_t* buf1=(uint8_t*)buf;
		int off=0;
		int r;
		while(off<len) {
			if((r=read(fd,buf1+off,len-off))<=0) break;
			off+=r;
		}
		return off;
	}
};
