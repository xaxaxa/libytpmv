#include "include/videorenderer.H"
#include "include/framerenderer.H"
#include <algorithm>
#include <unordered_map>
#include <assert.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

using namespace std;
namespace ytpmv {
	struct NoteEventV {
		int t;
		int segmentIndex;
		bool off;
	};
	static bool operator<(const NoteEventV& a, const NoteEventV& b) {
		return a.t < b.t;
	}

	void renderVideo(const vector<VideoSegment>& segments, int fps, int w, int h, function<void(uint8_t* data)> writeFrame) {
		FrameRenderer fr(w,h);
		vector<string> shaders;
		unordered_map<size_t, int> shaderIDs;
		unordered_map<const char*, int> shaderIDs2;
		hash<std::string> hashFn;
		
		// find all used shader code strings
		int nextIndex = 0;
		int i=-1;
		for(const VideoSegment& seg: segments) {
			i++;
			if(shaderIDs2.find(seg.shader.data()) != shaderIDs2.end()) continue;
			size_t key = hashFn(seg.shader);
			if(shaderIDs.find(key) != shaderIDs.end()) {
				shaderIDs2[seg.shader.data()] = shaderIDs[key];
				continue;
			}
			if(seg.shader == "") {
				throw logic_error("segment " + to_string(i) + " does not have shader code");
			}
			shaderIDs[key] = nextIndex;
			shaderIDs2[seg.shader.data()] = nextIndex;
			shaders.push_back(seg.shader);
			nextIndex++;
		}
		fr.setRenderers(shaders);
		
		// convert note list into note event list
		vector<NoteEventV> events;
		for(int i=0;i<(int)segments.size();i++) {
			NoteEventV evt;
			evt.t = (int)round(segments[i].startSeconds*fps);
			evt.off = false;
			evt.segmentIndex = i;
			events.push_back(evt);
			
			evt.t = segments[i].endSeconds;
			evt.off = true;
			events.push_back(evt);
		}
		
		// sort based on event time
		sort(events.begin(), events.end());
		
		basic_string<float> buf;
		int evts = (int)events.size();
		
		// go through all note events and render the regions between events
		
		unordered_map<int, int> notesActive; // map from note index to start time in frames
		for(int i=0;i<evts-1;i++) {
			NoteEventV evt = events[i];
			int curTimeFrames = events[i].t;
			int nextTimeFrames = events[i+1].t;
			
			if(!evt.off) { // note on
				notesActive[evt.segmentIndex] = curTimeFrames;
				const VideoSegment& s = segments.at(evt.segmentIndex);
				fprintf(stderr, "videoclip on: %5d:  dur %3.2fs\n", evt.segmentIndex, s.durationSeconds());
			} else {
				notesActive.erase(evt.segmentIndex);
				fprintf(stderr, "videoclip off:%5d\n", evt.segmentIndex);
			}
			
			if(curTimeFrames >= nextTimeFrames) continue;
			int durationFrames = nextTimeFrames-curTimeFrames;
			
			// collect renderers and parameters for this region
			vector<int> enabledRenderers;
			vector<vector<float> > params;
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& s = segments.at((*it).first);
				int shaderID = shaderIDs2[s.shader.data()];
				enabledRenderers.push_back(shaderID);
				params.push_back(s.shaderParams);
			}
			fr.setEnabledRenderers(enabledRenderers);
			fr.setUserParams(params);
			
			// render frames in this region
			vector<Image> images;
			float relTimeSeconds[fr.maxConcurrent];
			images.resize(notesActive.size());
			for(int j=0; j<durationFrames; j++) {
				int k=0;
				double timeSeconds = double(curTimeFrames+j)/fps;
				for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
					const VideoSegment& seg = segments.at((*it).first);
					int srcFrameNum = (int)round(j*seg.speed);
					if(srcFrameNum >= seg.sourceFrames) srcFrameNum = seg.sourceFrames-1;
					images[k] = seg.source[srcFrameNum];
					relTimeSeconds[k] = float(timeSeconds-seg.startSeconds);
					k++;
				}
				fr.setTime(float(timeSeconds), relTimeSeconds);
				fr.setImages(images);
				fr.draw();
				writeFrame(nullptr);
			}
		}
	}
	
	class VideoRendererState {
	public:
		FrameRenderer fr;
		const vector<VideoSegment>& segments;
		vector<string> shaders;
		unordered_map<size_t, int> shaderIDs;
		unordered_map<const char*, int> shaderIDs2;
		vector<NoteEventV> events;
		unordered_map<int, int> notesActive; // map from note index to start time in frames
		double fps, systemFPS;
		int lastEventIndex;
		int curFrame;
		VideoRendererState(const vector<VideoSegment>& segments, int w, int h, double fps, double systemFPS): fr(w,h), segments(segments), fps(fps), systemFPS(systemFPS) {
			lastEventIndex = 0;
			curFrame = -1;
			// find all used shader code strings
			hash<string> hashFn;
			int nextIndex = 0;
			int i=-1;
			for(const VideoSegment& seg: segments) {
				i++;
				if(shaderIDs2.find(seg.shader.data()) != shaderIDs2.end()) continue;
				size_t key = hashFn(seg.shader);
				if(shaderIDs.find(key) != shaderIDs.end()) {
					shaderIDs2[seg.shader.data()] = shaderIDs[key];
					continue;
				}
				if(seg.shader == "") {
					throw logic_error("segment " + to_string(i) + " does not have shader code");
				}
				shaderIDs[key] = nextIndex;
				shaderIDs2[seg.shader.data()] = nextIndex;
				shaders.push_back(seg.shader);
				nextIndex++;
			}
			fr.setRenderers(shaders,16,8);
			
			// convert note list into note event list
			for(int i=0;i<(int)segments.size();i++) {
				NoteEventV evt;
				evt.t = (int)round(segments[i].startSeconds*fps);
				evt.off = false;
				evt.segmentIndex = i;
				events.push_back(evt);
				
				evt.t = (int)round(segments[i].endSeconds*fps);
				evt.off = true;
				events.push_back(evt);
			}
			
			// sort based on event time
			sort(events.begin(), events.end());
			//fprintf(stderr, "%d events\n", (int)events.size());
		}
		void drawFrame() {
			int k=0;
			double timeSeconds = curFrame/fps;
			NoteEventV& evt = events[lastEventIndex];
			vector<Image> images;
			float relTimeSeconds[fr.maxConcurrent];
			assert(int(notesActive.size()) <= fr.maxConcurrent);
			
			images.resize(notesActive.size());
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& seg = segments.at((*it).first);
				int segStartFrame = (int)round(seg.startSeconds*fps);
				double srcSpeedMultiplier = systemFPS/fps;
				int srcFrameNum = (int)round((curFrame-segStartFrame)*seg.speed*srcSpeedMultiplier);
				if(srcFrameNum >= seg.sourceFrames) srcFrameNum = seg.sourceFrames-1;
				images[k] = seg.source[srcFrameNum];
				relTimeSeconds[k] = float(timeSeconds-seg.startSeconds);
				k++;
			}
			fr.setTime(float(timeSeconds), relTimeSeconds);
			fr.setImages(images);
			fr.draw();
		}
		void advanceTo(int frame) {
			frame += events[0].t;
			
			if(frame <= curFrame) return;
			//fprintf(stderr, "%d events\n", (int)events.size());
			// go through events until one beyond the requested frame is encountered
			while(lastEventIndex < (int)events.size()) {
				NoteEventV& evt = events[lastEventIndex];
				
				if(evt.t > frame) break;
				
				// if the event's start time is after curFrame then we haven't processed it yet
				if(evt.t > curFrame) {
					fprintf(stderr, "event %d: ", lastEventIndex);
					if(!evt.off) { // note on
						notesActive[evt.segmentIndex] = evt.t;
						const VideoSegment& s = segments.at(evt.segmentIndex);
						fprintf(stderr, "videoclip on: %5d:  dur %3.2fs\n", evt.segmentIndex, s.durationSeconds());
					} else {
						notesActive.erase(evt.segmentIndex);
						fprintf(stderr, "videoclip off:%5d\n", evt.segmentIndex);
					}
					// collect renderers and parameters for this region
					vector<int> enabledRenderers;
					vector<vector<float> > params;
					for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
						const VideoSegment& s = segments.at((*it).first);
						int shaderID = shaderIDs2[s.shader.data()];
						enabledRenderers.push_back(shaderID);
						params.push_back(s.shaderParams);
					}
					fr.setEnabledRenderers(enabledRenderers);
					fr.setUserParams(params);
				}
				lastEventIndex++;
			}
			if(lastEventIndex > 0)
				lastEventIndex--;
			curFrame = frame;
		}
	};
	
	VideoRendererTimeDriven::VideoRendererTimeDriven(const vector<VideoSegment>& segments, int w, int h, double fps, double systemFPS) {
		st = new VideoRendererState(segments,w,h,fps,systemFPS);
	}
	VideoRendererTimeDriven::~VideoRendererTimeDriven() {
		delete st;
	}
	
	void VideoRendererTimeDriven::drawFrame() {
		st->drawFrame();
	}
	void VideoRendererTimeDriven::advanceTo(int frame) {
		st->advanceTo(frame);
	}
}
