#include "include/simple.H"
#include "include/framerenderer.H"
#include "include/videorenderer.H"
#include "include/audiorenderer.H"
#include <functional>
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
	class Player {
	public:
		// not the real fps; only the video time resolution of the video renderer
		static constexpr double fps = 240.;
		
		int srate;
		
		// the fps that the video segment speeds are calculated based on
		double systemFPS;
		
		GLFWwindow* window;
		snd_pcm_t* alsaHandle;
		
		// the offset between the monotonic clock time and the playback time relative
		// to the start of the playback;
		// that is, getTimeMicros() - offsetClockTimeMicros will return the current
		// playback time in microseconds
		volatile uint64_t offsetClockTimeMicros;
		
		const vector<AudioSegment>& audioSegments;
		VideoRendererTimeDriven videoRenderer;
		
		
		Player(const vector<AudioSegment>& audio, int srate,
			const vector<VideoSegment>& video, int w, int h, double systemFPS):
				srate(srate),
				systemFPS(systemFPS),
				window(initGLWindowed(w,h)),
				audioSegments(audio),
				videoRenderer(video, w, h, fps, systemFPS) {
			
			offsetClockTimeMicros = 0;
			
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
				
				snd_pcm_writei(alsaHandle, data, len/CHANNELS);
				samplesWritten += len/CHANNELS;
			});
		}
		void videoThread() {
			do {
				uint64_t offs = offsetClockTimeMicros;
				
				int frame;
				if(offs == 0) frame = 0;
				else {
					uint64_t t = getTimeMicros() - offs;
					frame = (int)round(double(t)*1e-6*fps);
				}
				
				videoRenderer.advanceTo(frame);
				//fprintf(stderr, "drawing frame %d\n", frame);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				videoRenderer.drawFrame();
				
				glfwSwapBuffers(window);
				glfwPollEvents();
			} while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
			   glfwWindowShouldClose(window) == 0 );
		}
	};
	void* _audioThread(void* v) {
		Player* p = (Player*)v;
		p->audioThread();
		return NULL;
	}
	void play(const vector<AudioSegment>& audio, const vector<VideoSegment>& video) {
		Player p(audio, 44100, video, 800, 500, 30);
		pthread_t th;
		pthread_create(&th, NULL, _audioThread, &p);
		p.videoThread();
		pthread_join(th, NULL);
	}
};
