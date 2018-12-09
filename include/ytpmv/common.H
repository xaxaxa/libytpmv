#ifndef LIBYTPMV_COMMON_H
#define LIBYTPMV_COMMON_H
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <functional>
#include <ytpmv/glutil.H>
#include <ytpmv/texturecache.H>
using namespace std;

namespace ytpmv {
	// number of (interleaved) channels in all in-memory sample data
	static constexpr int CHANNELS=2;
	// maximum number of user parameters in a shader
	static constexpr int MAXUSERPARAMS=16;
	
	// -1: quiet
	// 0: normal
	// 1: verbose
	extern int verbosity;
	
	#define PRNT(minVerbosity, ...) if(verbosity >= minVerbosity) { fprintf(stderr, __VA_ARGS__); }
	
	template<class T> T clamp(T val, T min, T max) {
		if(val < min) val = min;
		if(val > max) val = max;
		return val;
	}
	
	struct Time {
		// number of patterns full before this note
		int seq;
		// row index of this note in current pattern
		int row;
		// total rows since beginning of song
		int absRow;
		// from 0 to 1; timing offset in the specified row
		double rowOffset;
		
		double toSeconds(double bpm) const {
			return (double(absRow)+rowOffset)*60./bpm;
		}
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
		
		// absolute amplitude
		double amplitudeDB;
		// relative to note pitch
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
		
		// chord info; this may come from the 0xx (arpeggio) command
		double chord1,chord2,chord3; // semitones relative to the root note
		int chordCount = 0;	// how many tones there are above the root note
		
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
	
	class VideoSource {
	public:
		string name;
		double speed = 1.;
		
		// offset into the original video (without speed change applied)
		double offsetSeconds = 0.;
		
		// called at start of movie
		virtual void prepare()=0;
		
		// returns a opengl texture for this frame;
		// should take into account speed and offsetSeconds:
		// timeInVideo = timeSeconds*speed + offsetSeconds
		virtual int32_t getFrame(double timeSeconds)=0;
		
		// called when a texture returned by getFrame is no longer needed
		virtual void releaseFrame(uint32_t texture)=0;
		
		virtual ~VideoSource() {}
	};
	
	struct Image {
		int w, h;
		
		// all images are in RGB888 format (24bpp), with stride aligned to 4 bytes
		string data;
		
		// if data is empty, the image is in a texture
		int texture;
	};
	class ImageSource: public VideoSource {
	public:
		const Image* img;
		int _tex = 0;
		ImageSource(const Image* img): img(img) {}
		virtual ~ImageSource() override;
		virtual void prepare() override;
		virtual int32_t getFrame(double timeSeconds) override;
		virtual void releaseFrame(uint32_t texture) override;
	};
	class ImageArraySource: public VideoSource {
	public:
		vector<Image> frames;
		double fps = 30.;
		
		TextureCache cache;
		
		ImageArraySource() {}
		virtual void prepare() override;
		virtual int32_t getFrame(double timeSeconds) override;
		virtual void releaseFrame(uint32_t texture) override;
	};
	class Source {
	public:
		string name;
		double pitch = 1., tempo = 1., amplitudeDB = 0.;
		AudioSource* audio = nullptr;
		VideoSource* video = nullptr;
		bool hasAudio() { return audio != nullptr; }
		bool hasVideo() { return video != nullptr; }
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
		
		AudioSegment();
		AudioSegment(const Note& n, const Instrument& ins, double bpm);
		AudioSegment(const Note& n, const AudioSource* src, double bpm);
		AudioSegment(const Note& n, const Source* src, double bpm);
		
		void addInitialKeyFrame(const Note& n);
		void copyKeyFrames(const vector<NoteKeyFrame>& kfs, double pitch, double bpm);
		
		double durationSeconds() const {
			return endSeconds - startSeconds;
		}
	};
	
	vector<float> genRectangle(float x1, float y1, float x2, float y2);
	vector<float> genRectangleWithCenter(float x1, float y1, float x2, float y2, float tx1=0., float ty1=0., float tx2=1., float ty2=1., float z=0.);
	
	vector<float> genRectangle(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3);
	vector<float> genParallelpiped();
	
	struct VideoKeyFrame {
		// time of this keyframe relative to segment start
		double relTimeSeconds;
		// user parameters after this keyframe
		vector<float> shaderParams;
	};
	class VideoSegment {
	public:
		double startSeconds, endSeconds;
		double offsetSeconds = 0.0; // offset into source video
		double speed = 1.0; // 1 => original speed
		VideoSource* source = nullptr;
		int zIndex = 0;
		
		// array holding all vertex attributes;
		// layout is:
		// V0var0_0, V0var0_1, V0var0_2, V0var1_0, V0var1_1, V1var0_0, ...
		// (vertex variables are interleaved)
		vector<float> vertexes;
		int instances = 1;
		
		// number of elements in each vertex attribute variable; null terminated
		// e.g. {3, 2, 0} for a x,y,z vertex coordinate and a x,y texture coordinate
		int vertexVarSizes[8];
		
		
		// OLD API. DO NOT USE. use vertexShader and fragmentShader instead.
		// a function body that returns a vec4 (a,r,g,b) value;
		// parameters passed in are:
		// - pos: vec2; absolute coordinates (x,y) from 0.0 to 1.0
		// - image: sampler2D; source frame image
		// - secondsAbs: float; seconds since the start of video
		// - secondsRel: float; seconds since the start of the video segment
		// - resolution: vec2; video resolution (w,h) in pixels
		// functions accessible are:
		// - float param(int i); returns user parameter i
		string* shader = nullptr;
		
		// the full vertex shader code; if blank, the default pass-through shader is used
		// variables available:
		/*
			uniform vec2 coordBase;
			uniform mat2 coordTransform;
			smooth out vec2 uv;
		 */
		string* vertexShader = nullptr;
		
		// the full fragment shader code; if blank, .shader will be used
		// variables available (to use, you need to declare these in your shader code):
		/* 
		    in vec4 gl_FragCoord;		// absolute coordinates in pixels
			smooth in vec2 uv;			// relative coordinates, supplied by vertex shader
			out vec4 color;				// pixel output (r,g,b,a)
		    uniform vec2 resolution;	// screen resolution in pixels
			uniform float secondsAbs;	// seconds since start of video
			uniform float secondsRel;	// seconds since start of segment
			uniform sampler2D image;	// a frame from the supplied video
			uniform float userParams[MAXUSERPARAMS];	// user parameters
		 */
		string* fragmentShader = nullptr;
		
		// user parameters
		vector<float> shaderParams;
		vector<VideoKeyFrame> keyframes;
		
		function<void(vector<float>& out, const vector<float>& kf1, const vector<float>& kf2,
						double t1, double t2, double t)> interpolateKeyframes;
		
		VideoSegment();
		
		// create a video segment with the timing info from n, and
		// using source src.
		VideoSegment(const Note& n, VideoSource* src, double bpm);
		VideoSegment(const Note& n, Source* src, double bpm);
		
		// create a video segment using source src.
		VideoSegment(VideoSource* src, double startSeconds, double endSeconds);
		
		double durationSeconds() const {
			return endSeconds - startSeconds;
		}
	};
	
	class PlaybackSettings {
	public:
		double volume=1.; // linear volume; 1 => no change
		double skipToSeconds=0.;
	};
	
	std::string get_file_contents(const char *filename);
	int readAll(int fd,void* buf, int len);
}
#endif
