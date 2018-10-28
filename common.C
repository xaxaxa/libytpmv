#include <ytpmv/common.H>

namespace ytpmv {
	AudioSegment::AudioSegment(const Note& n, const Instrument& ins, double bpm) {
		startSeconds = n.start.absRow*60/bpm;
		endSeconds = n.end.absRow*60/bpm;
		pitch = pow(2,(n.pitchSemitones + ins.tuningSemitones)/12.);
		if(pitch<1.) tempo = pitch;
		else tempo = sqrt(pitch);
		for(int k=0; k<CHANNELS; k++)
			amplitude[k] = pow(10,n.amplitudeDB/20.);
		sampleData = ins.sampleData.data();
		sampleLength = ins.sampleData.length();
		copyKeyFrames(n.keyframes, pow(2,ins.tuningSemitones/12.), bpm);
	}
	AudioSegment::AudioSegment(const Note& n, const AudioSource& src, double bpm) {
		startSeconds = n.start.absRow*60/bpm;
		endSeconds = n.end.absRow*60/bpm;
		pitch = pow(2,n.pitchSemitones/12.) * src.pitch;
		tempo = src.tempo;
		for(int k=0; k<CHANNELS; k++)
			amplitude[k] = pow(10,n.amplitudeDB/20.);
		sampleData = src.sample.data();
		sampleLength = src.sample.length();
		copyKeyFrames(n.keyframes, src.pitch, bpm);
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
	
	
	VideoSegment::VideoSegment(const Note& n, const VideoSource& src, double bpm) {
		startSeconds = n.start.absRow*60/bpm;
		endSeconds = n.end.absRow*60/bpm;
		speed = src.speed;
		source = src.frames.data();
		sourceFrames = src.frames.size();
		
		vertexes = genRectangle(-1, -1, 1, 1);
		vertexVarSizes[0] = 3;
		vertexVarSizes[1] = 2;
		vertexVarSizes[2] = 0;
		
		zIndex = 0;
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
};
