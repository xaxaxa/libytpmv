#include <ytpmv/videorenderer.H>
#include <ytpmv/framerenderer2.H>
#include <algorithm>
#include <map>
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
	
	
	// comparator for segment id that will sort by zIndex of the video segments
	struct SegmentCompare {
		const vector<VideoSegment>& segments;
		SegmentCompare(const vector<VideoSegment>& segments): segments(segments) {
			
		}
		bool operator()(int a, int b) {
			if(this->segments.at(a).zIndex < this->segments.at(b).zIndex) return true;
			if(this->segments.at(a).zIndex > this->segments.at(b).zIndex) return false;
			return a<b;
		}
	};
	
	uint64_t shaderKey(const string& shader, const string& vertexShader, const string& fragmentShader) {
		return ((uint64_t)shader.data()) ^ ((uint64_t)vertexShader.data()) ^ ((uint64_t)fragmentShader.data());
	}
	uint64_t shaderKey(const VideoSegment& seg) {
		return shaderKey(seg.shader, seg.vertexShader, seg.fragmentShader);
	}

	void renderVideo(const vector<VideoSegment>& segments, double fps, int w, int h, function<void(uint8_t* data)> writeFrame) {
		FrameRenderer fr(w,h);
		vector<string> shaders;
		unordered_map<size_t, int> shaderIDs;
		unordered_map<uint64_t, int> shaderIDs2;
		hash<std::string> hashFn;
		
		// find all used shader code strings
		int nextIndex = 0;
		int i=-1;
		for(const VideoSegment& seg: segments) {
			i++;
			// we identify a segment's shader program by the tuple (shader, vertexShader, fragmentShader);
			// if several segments have the same tuple then the shader program can be reused.
			uint64_t key = shaderKey(seg);
			if(shaderIDs2.find(key) != shaderIDs2.end()) continue;
			
			size_t hashKey = hashFn(seg.shader + seg.vertexShader + seg.fragmentShader);
			if(shaderIDs.find(hashKey) != shaderIDs.end()) {
				shaderIDs2[key] = shaderIDs[hashKey];
				continue;
			}
			if(seg.shader == "" && seg.fragmentShader == "") {
				throw logic_error("segment " + to_string(i) + " does not have shader code");
			}
			shaderIDs[hashKey] = nextIndex;
			shaderIDs2[key] = nextIndex;
			
			if(seg.vertexShader == "")
				shaders.push_back(defaultVertexShader);
			else shaders.push_back(seg.vertexShader);
			
			if(seg.fragmentShader == "")
				shaders.push_back(FrameRenderer2_generateCode(seg.shader, MAXUSERPARAMS));
			else shaders.push_back(seg.fragmentShader);
			
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
			
			evt.t = (int)round(segments[i].endSeconds*fps);
			evt.off = true;
			events.push_back(evt);
		}
		
		// sort based on event time
		sort(events.begin(), events.end());
		
		basic_string<float> buf;
		int evts = (int)events.size();
		
		// go through all note events and render the regions between events
		
		// map from note index to start time in frames
		map<int, int, SegmentCompare> notesActive(SegmentCompare{segments});
		
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
				int shaderID = shaderIDs2[shaderKey(s)];
				enabledRenderers.push_back(shaderID);
				params.push_back(s.shaderParams);
			}
			fr.setEnabledRenderers(enabledRenderers);
			fr.setUserParams(params);
			
			int j=0;
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& s = segments.at((*it).first);
				fr.setVertexes(j, s.vertexes, s.vertexVarSizes);
				j++;
			}
			
			// render frames in this region
			vector<const Image*> images;
			vector<float> relTimeSeconds(fr.maxConcurrent);
			images.resize(notesActive.size());
			for(int j=0; j<durationFrames; j++) {
				int k=0;
				double timeSeconds = double(curTimeFrames+j)/fps;
				for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
					const VideoSegment& seg = segments.at((*it).first);
					double t = timeSeconds-seg.startSeconds;
					
					int srcFrameNum = (int)round(t*fps*seg.speed);
					if(srcFrameNum >= seg.sourceFrames) srcFrameNum = seg.sourceFrames-1;
					if(srcFrameNum < 0) srcFrameNum = 0;
					
					images[k] = &seg.source[srcFrameNum];
					relTimeSeconds[k] = float(t);
					
					// find current keyframe
					for(auto& kf: seg.keyframes) {
						if(kf.relTimeSeconds <= t)
							params.at(k) = kf.shaderParams;
						else break;
					}
					
					k++;
				}
				fr.setTime(float(timeSeconds), relTimeSeconds);
				fr.setImages(images);
				fr.setUserParams(params);
				string imgData = fr.render();
				writeFrame((uint8_t*)imgData.data());
			}
		}
	}
	
	class VideoRendererState {
	public:
		FrameRenderer fr;
		const vector<VideoSegment>& segments;
		double fps, systemFPS;
		int lastEventIndex;
		int curFrame;
		
		vector<NoteEventV> events;
		vector<string> shaders;
		unordered_map<size_t, int> shaderIDs;
		unordered_map<int64_t, int> shaderIDs2;
		
		
		
		map<int, int, SegmentCompare> notesActive; // map from note index to start time in frames
		vector<vector<float> > curUserParams;
		VideoRendererState(const vector<VideoSegment>& segments, int w, int h, double fps, double systemFPS):
			fr(w,h), segments(segments), fps(fps), systemFPS(systemFPS),
			notesActive(SegmentCompare(segments)) {
			
			lastEventIndex = 0;
			curFrame = -1;
			
			// find all used shader code strings
			hash<string> hashFn;
			int nextIndex = 0;
			int i=-1;
			for(const VideoSegment& seg: segments) {
				i++;
				// we identify a segment's shader program by the tuple (shader, vertexShader, fragmentShader);
				// if several segments have the same tuple then the shader program can be reused.
				uint64_t key = shaderKey(seg);
				if(shaderIDs2.find(key) != shaderIDs2.end()) continue;
				
				size_t hashKey = hashFn(seg.shader + seg.vertexShader + seg.fragmentShader);
				if(shaderIDs.find(hashKey) != shaderIDs.end()) {
					shaderIDs2[key] = shaderIDs[hashKey];
					continue;
				}
				if(seg.shader == "" && seg.fragmentShader == "") {
					throw logic_error("segment " + to_string(i) + " does not have shader code");
				}
				shaderIDs[hashKey] = nextIndex;
				shaderIDs2[key] = nextIndex;
				
				if(seg.vertexShader == "")
					shaders.push_back(defaultVertexShader);
				else shaders.push_back(seg.vertexShader);
				
				if(seg.fragmentShader == "")
					shaders.push_back(FrameRenderer2_generateCode(seg.shader, MAXUSERPARAMS));
				else shaders.push_back(seg.fragmentShader);
				
				nextIndex++;
			}
			fr.setRenderers(shaders);
			
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
			
			// draw to screen
			fr.setRenderToScreen();
		}
		void drawFrame() {
			int k=0;
			double timeSeconds = curFrame/fps;
			vector<const Image*> images;
			vector<float> relTimeSeconds(fr.maxConcurrent);
			assert(int(notesActive.size()) <= fr.maxConcurrent);
			
			images.resize(notesActive.size());
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& seg = segments.at((*it).first);
				
				// find source frame
				double relTime = timeSeconds-seg.startSeconds;
				int segStartFrame = (int)round(seg.startSeconds*fps);
				double srcSpeedMultiplier = systemFPS/fps;
				int srcFrameNum = (int)round((curFrame-segStartFrame)*seg.speed*srcSpeedMultiplier);
				if(srcFrameNum >= seg.sourceFrames) srcFrameNum = seg.sourceFrames-1;
				
				// set parameters
				images[k] = &seg.source[srcFrameNum];
				relTimeSeconds[k] = float(timeSeconds-seg.startSeconds);
				
				// find current keyframe
				for(auto& kf: seg.keyframes) {
					if(kf.relTimeSeconds <= relTime)
						curUserParams.at(k) = kf.shaderParams;
					else break;
				}
				k++;
			}
			fr.setTime(float(timeSeconds), relTimeSeconds);
			fr.setImages(images);
			fr.setUserParams(curUserParams);
			fr.draw();
		}
		// returns true if we are still within bounds of the video
		bool advanceTo(int frame) {
			frame += events[0].t;
			
			if(frame <= curFrame) return true;
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
					int lastZIndex = -(1<<29);
					curUserParams.clear();
					for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
						const VideoSegment& s = segments.at((*it).first);
						int shaderID = shaderIDs2[shaderKey(s)];
						enabledRenderers.push_back(shaderID);
						curUserParams.push_back(s.shaderParams);
						assert(s.zIndex >= lastZIndex);
						lastZIndex = s.zIndex;
					}
					fr.setEnabledRenderers(enabledRenderers);
					fr.setUserParams(curUserParams);
					
					int i=0;
					for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
						const VideoSegment& s = segments.at((*it).first);
						fr.setVertexes(i, s.vertexes, s.vertexVarSizes);
						i++;
					}
				}
				lastEventIndex++;
			}
			if(lastEventIndex > 0)
				lastEventIndex--;
			
			// if the current event is the last event, we are past the end of the video
			if(lastEventIndex >= (int(events.size())-1)) return false;
			
			curFrame = frame;
			return true;
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
	bool VideoRendererTimeDriven::advanceTo(int frame) {
		return st->advanceTo(frame);
	}
}
