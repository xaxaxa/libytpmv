#ifndef LIBYTPMV_COMMON_H
#define LIBYTPMV_COMMON_H
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;

namespace ytpmv {
	// number of (interleaved) channels in all in-memory sample data
	static constexpr int CHANNELS=2;
	
	struct Time {
		// number of patterns full before this note
		int seq;
		// row index of this note in current pattern
		int row;
		// total rows since beginning of song
		int absRow;
		// from 0 to 1; timing offset in the specified row
		double rowOffset;
	};
	inline bool operator<(const Time& t1, const Time& t2) {
		if(t1.absRow < t2.absRow) return true;
		if(t1.absRow > t2.absRow) return false;
		return t1.rowOffset < t2.rowOffset;
	}
	inline double operator-(const Time& t1, const Time& t2) {
		return (t1.absRow - t2.absRow) + (t1.rowOffset - t2.rowOffset);
	}
	
	// song info read from mod or midi file
	struct SongInfo {
		string name;
		double bpm; // rows per minute
		double rowDurationSeconds() const {
			return (60./bpm);
		}
	};
	struct NoteKeyFrame {
		double relTimeRows;
		
		// relative to note amplitude and pitch
		double amplitudeDB;
		double pitchSemitones;
	};
	// represents one note from mod/midi file
	class Note {
	public:
		Time start, end;
		int channel;
		int instrument;
		double pitchSemitones; // semitones relative to the sample note (a pitch of 0 means play the sample at original speed)
		double amplitudeDB; // in dB; 0 is default amplitude
		
		// does not include the initial keyframe
		vector<NoteKeyFrame> keyframes;
		
		// returns duration in rows
		double durationRows() const {
			return (end.absRow - start.absRow) + (end.rowOffset - start.rowOffset);
		}
	};
	// represents an instrument or sample from mod file
	struct Instrument {
		int id;
		string name;
		double tuningSemitones; // add this value to all notes played with this instrument
		int amplitudeDB; // in dB; 0 is default amplitude
		basic_string<float> sampleData; // values should be normalized to [-1.0, 1.0]
	};
	class AudioSource {
	public:
		string name;
		basic_string<float> sample; // always 2 channels interleaved; values normalized to [-1.0, 1.0]
		double tempo, pitch; // pitch & tempo correction applied to this source when played; see AudioSegment
	};
	
	struct Image {
		int w,h;
		// all images are in RGB888 format (24bpp)
		string data;
	};
	class VideoSource {
	public:
		string name;
		vector<Image> frames;
		double speed;
	};
	class Source {
	public:
		string name;
		AudioSource audio;
		VideoSource video;
		bool hasAudio, hasVideo;
	};
	
	struct AudioKeyFrame {
		double relTimeSeconds;
		
		// relative to AudioSegment amplitude and pitch
		double amplitude[CHANNELS];
		double pitch;
	};
	
	// a note or segment in the audio timeline; passed into AudioRenderer
	class AudioSegment {
	public:
		double startSeconds, endSeconds;
		double tempo; // linear tempo; 1 => original tempo
		double pitch; // linear pitch; 1 => original pitch, 2 => 12 semitones up, etc
		double amplitude[CHANNELS];
		const float* sampleData;
		int sampleLength; // number of elements in the sampleData array (not number of samples)
		
		vector<AudioKeyFrame> keyframes;
		
		AudioSegment() {}
		AudioSegment(const Note& n, const Instrument& ins, double bpm) {
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
		AudioSegment(const Note& n, const AudioSource& src, double bpm) {
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
		void copyKeyFrames(const vector<NoteKeyFrame>& kfs, double pitch, double bpm) {
			for(const NoteKeyFrame& kf: kfs) {
				AudioKeyFrame kf2;
				kf2.relTimeSeconds = kf.relTimeRows*60/bpm;
				for(int k=0; k<CHANNELS; k++)
					kf2.amplitude[k] = pow(10,kf.amplitudeDB/20.);
				kf2.pitch = pow(2,kf.pitchSemitones/12.) * pitch;
				keyframes.push_back(kf2);
			}
		}
		double durationSeconds() const {
			return endSeconds - startSeconds;
		}
	};
	
	struct VideoKeyFrame {
		// time of this keyframe relative to segment start
		double relTimeSeconds;
		// user parameters after this keyframe
		vector<float> shaderParams;
	};
	class VideoSegment {
	public:
		double startSeconds, endSeconds;
		double speed; // 1 => original speed
		
		// a function body that returns a vec4 (a,r,g,b) value;
		// parameters passed in are:
		// - pos: vec2; absolute coordinates (x,y) from 0.0 to 1.0
		// - image: sampler2D; source frame image
		// - secondsAbs: float; seconds since the start of video
		// - secondsRel: float; seconds since the start of the video segment
		// - resolution: vec2; video resolution (w,h) in pixels
		// functions accessible are:
		// - float param(int i); returns user parameter i
		string shader;
		// user parameters
		vector<float> shaderParams;
		const Image* source;
		int sourceFrames;
		int zIndex;
		
		vector<VideoKeyFrame> keyframes;
		
		VideoSegment() {}
		VideoSegment(const Note& n, const VideoSource& src, double bpm) {
			startSeconds = n.start.absRow*60/bpm;
			endSeconds = n.end.absRow*60/bpm;
			speed = 1.;
			source = src.frames.data();
			sourceFrames = src.frames.size();
			zIndex = 0;
		}
		double durationSeconds() const {
			return endSeconds - startSeconds;
		}
	};
	
	class PlaybackSettings {
	public:
		double volume=1.; // linear volume; 1 => no change
		double skipToSeconds=0.;
	};
	
	inline std::string get_file_contents(const char *filename) {
		std::ifstream in(filename, std::ios::in | std::ios::binary);
		if(in) {
			std::ostringstream contents;
			contents << in.rdbuf();
			in.close();
			return(contents.str());
		}
		throw(errno);
	}
}
#endif
