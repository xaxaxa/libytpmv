#include <ytpmv/simple.H>
#include <ytpmv/framerenderer.H>
#include <ytpmv/videorenderer.H>
#include <ytpmv/audiorenderer.H>
#include <ytpmv/mmutil.H>
#include <functional>
#include <map>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

using namespace std;
namespace ytpmv {
	// returns time in microseconds
	static uint64_t getTimeMicros() {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return uint64_t(ts.tv_sec)*1000000 + uint64_t(ts.tv_nsec)/1000;
	}
	
	bool loadAudioOnly = false;
	map<string, Source> sources;
	void loadSource(Source& src, string audioFile, string videoFile,
					double audioPitch, double audioTempo, double videoSpeed) {
		src.hasAudio = false;
		src.hasVideo = false;
		if(audioFile != "") {
			src.audio = loadAudio(audioFile.c_str(), 44100);
			src.audio.pitch *= audioPitch;
			src.audio.tempo *= audioTempo;
			src.hasAudio = true;
		}
		if(videoFile != "" && !loadAudioOnly) {
			src.video = loadVideo(videoFile.c_str(), 30);
			src.video.speed *= videoSpeed;
			src.hasVideo = true;
		}
	}
	Source* addSource(string name, string audioFile, string videoFile,
					double audioPitch, double audioTempo, double videoSpeed) {
		auto& src = sources[name];
		src.name = name;
		loadSource(src, audioFile, videoFile, audioPitch, audioTempo, videoSpeed);
		return &src;
	}
	Source* getSource(string name) {
		auto it = sources.find(name);
		if(it == sources.end()) throw runtime_error(string("getSource(): source ") + name + " not found");
		return &(*it).second;
	}
	void trimSource(string name, double startTimeSeconds, double lengthSeconds) {
		Source& src = *getSource(name);
		if(src.hasAudio) {
			int startSamples = (int)round(startTimeSeconds*44100);
			int endSamples = (lengthSeconds==-1.)?string::npos: startSamples+(int)round(lengthSeconds*44100);
			src.audio.sample = src.audio.sample.substr(startSamples*CHANNELS, endSamples*CHANNELS);
		}
	}
	
	class Player {
	public:
		// not the real fps; only the video time resolution of the video renderer
		static constexpr double fps = 240.;
		
		int srate;
		
		// the fps that the video segment speeds are calculated based on
		double systemFPS;
		
		GLFWwindow* window = nullptr;
		snd_pcm_t* alsaHandle;
		
		// the offset between the monotonic clock time and the playback time relative
		// to the start of the playback;
		// that is, getTimeMicros() - offsetClockTimeMicros will return the current
		// playback time in microseconds
		volatile uint64_t offsetClockTimeMicros;
		
		const vector<AudioSegment>& audioSegments;
		VideoRendererTimeDriven* videoRenderer = nullptr;
		
		const PlaybackSettings& settings;
		
		
		Player(const vector<AudioSegment>& audio, int srate,
			const vector<VideoSegment>& video, int w, int h, double systemFPS,
			const PlaybackSettings& settings):
				srate(srate),
				systemFPS(systemFPS),
				audioSegments(audio),
				settings(settings) {
			
			offsetClockTimeMicros = getTimeMicros() + 10000000;
			
			if(video.size() > 0) {
				window = initGLWindowed(w,h);
				videoRenderer = new VideoRendererTimeDriven(video, w, h, fps, systemFPS);
			}
			
			// open audio device
			int err;
			if ((err = snd_pcm_open(&alsaHandle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
				throw runtime_error(string("Playback open error: ") + snd_strerror(err));
			}
			if ((err = snd_pcm_set_params(alsaHandle,
										SND_PCM_FORMAT_FLOAT_LE,
										SND_PCM_ACCESS_RW_INTERLEAVED,
										CHANNELS,	// channels
										srate,		// sample rate
										1,			// resample ratio
										50000)		// latency in us
										) < 0) {
				throw runtime_error(string("Playback open error: ") + snd_strerror(err));
			}
		}
		void audioThread() {
			int64_t samplesWritten = 0;
			renderAudio(audioSegments, srate, [this, &samplesWritten](float* data, int len) {
				uint64_t t = getTimeMicros();
				uint64_t tPlayback = round(double(samplesWritten)*1e6/srate);
				offsetClockTimeMicros = t-tPlayback;
				
				samplesWritten += len/CHANNELS;
				
				if(double(samplesWritten)/srate < settings.skipToSeconds) return;
				
				for(int i=0; i<len; i++) data[i] *= settings.volume;
				snd_pcm_writei(alsaHandle, data, len/CHANNELS);
			});
		}
		void videoThread() {
			uint64_t renderDelay = 0;
			uint64_t lastPrint = 0;
			uint64_t lastFrames = 0;
			uint64_t frames = 0;
			
			do {
				uint64_t offs = offsetClockTimeMicros;
				int frame;
				if(offs == 0) frame = 0;
				else {
					int64_t t = int64_t(getTimeMicros()) - offs - renderDelay;
					frame = (int)round(double(t)*1e-6*fps);
				}
				
				//fprintf(stderr, "ADVANCE TO FRAME %d\n", frame);
				if(!videoRenderer->advanceTo(frame)) break;
				
				uint64_t micros1 = getTimeMicros();
				
				videoRenderer->drawFrame();
				glfwSwapBuffers(window);
				
				renderDelay = getTimeMicros() - micros1;
				
				if((micros1 - lastPrint) >= 1000000) {
					fprintf(stderr, "\033[44;37mFRAMERATE: %d fps; RENDERDELAY: %d us\033[0m\n",
						int(frames-lastFrames), int(renderDelay));
					lastPrint = micros1;
					lastFrames = frames;
				}
				
				glfwPollEvents();
				frames++;
			} while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
			   glfwWindowShouldClose(window) == 0 );
		}
	};
	void* _audioThread(void* v) {
		Player* p = (Player*)v;
		p->audioThread();
		return NULL;
	}
	
	void parseOptions(int argc, char** argv) {
		if(argc > 1) {
			if(strcmp(argv[1], "play") == 0) {
			} else if(strcmp(argv[1], "playaudio") == 0) {
				loadAudioOnly = true;
			} else if(strcmp(argv[1], "renderaudio") == 0) {
				loadAudioOnly = true;
			} else if(strcmp(argv[1], "render") == 0) {
			} else {
				fprintf(stderr, "usage: %s (play|playaudio|render|renderaudio)\n", argv[0]);
				exit(1);
			}
		}
	}
	
	void play(const vector<AudioSegment>& audio, const vector<VideoSegment>& video, const PlaybackSettings& settings) {
		Player p(audio, 44100, video, 896, 504, 30, settings);
		pthread_t th;
		pthread_create(&th, NULL, _audioThread, &p);
		if(video.size() > 0)
			p.videoThread();
		pthread_join(th, NULL);
	}
	
	struct renderInfo {
		int audioPipe[2], videoPipe[2];
		const vector<AudioSegment>* audio;
		const vector<VideoSegment>* video;
		const PlaybackSettings* settings;
		int w,h;
		double fps;
		int srate;
	};
	
	void* renderAudioThread(void* v) {
		renderInfo& inf = *(renderInfo*)v;
		
		renderAudio(*inf.audio,inf.srate, [&inf](float* data, int len) {
			int16_t buf[len];
			for(int i=0;i<len;i++) {
				float tmp = data[i] * float(inf.settings->volume) * 32767.;
				if(tmp > 32767) tmp = 32767;
				if(tmp < -32767) tmp = -32767;
				buf[i] = (int16_t)tmp;
			}
			write(inf.audioPipe[1],buf,sizeof(buf));
		});
		close(inf.audioPipe[1]);
		return NULL;
	}
	void* encodeVideoThread(void* v) {
		renderInfo& inf = *(renderInfo*)v;
		encodeVideo(inf.audioPipe[0], inf.videoPipe[0], inf.w, inf.h, inf.fps, inf.srate, 1);
		return NULL;
	}
	
	void render(const vector<AudioSegment>& audio, const vector<VideoSegment>& video, const PlaybackSettings& settings) {
		initGL(true);
		
		renderInfo inf;
		assert(pipe(inf.audioPipe) == 0);
		assert(pipe(inf.videoPipe) == 0);
		inf.audio = &audio;
		inf.video = &video;
		inf.settings = &settings;
		inf.w = 1920;
		inf.h = 1080;
		inf.fps = 30;
		inf.srate = 44100;
		
		
		
		pthread_t th1, th2;
		
		assert(pthread_create(&th2, NULL, &encodeVideoThread, &inf) == 0);
		assert(pthread_create(&th1, NULL, &renderAudioThread, &inf) == 0);
		
		renderVideo(video, inf.fps, inf.w, inf.h, [&inf](uint8_t* data) {
			write(inf.videoPipe[1],data,inf.w * inf.h * 4);
		});
		close(inf.videoPipe[1]);
		
		pthread_join(th1, nullptr);
		pthread_join(th2, nullptr);
	}
	
	int run(int argc, char** argv, const vector<AudioSegment>& audio, const vector<VideoSegment>& video, const PlaybackSettings& settings) {
		if(argc > 1) {
			if(strcmp(argv[1], "play") == 0) {
				play(audio, video, settings);
				return 0;
			} else if(strcmp(argv[1], "playaudio") == 0) {
				play(audio, {}, settings);
				return 0;
			} else if(strcmp(argv[1], "renderaudio") == 0) {
				renderInfo inf;
				inf.audioPipe[1] = 1;	// write to stdout
				inf.audio = &audio;
				inf.video = nullptr;
				inf.settings = &settings;
				inf.srate = 44100;
				renderAudioThread(&inf);
				return 0;
			} else if(strcmp(argv[1], "render") == 0) {
				render(audio, video, settings);
				return 0;
			} else {
				fprintf(stderr, "usage: %s (play|render)\n", argv[0]);
				return 1;
			}
		} else {
			play(audio, video, settings);
			return 0;
		}
	}
	
	PlaybackSettings defaultSettings;
};
