#include <ytpmv/videorenderer.H>
#include <ytpmv/framerenderer2.H>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>
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
	
	// returns a fast key based on the addresses of the shader strings
	uint64_t shaderKey(const string* shader, const string* vertexShader, const string* fragmentShader) {
		return ((uint64_t)shader) ^ ((uint64_t)vertexShader) ^ ((uint64_t)fragmentShader);
	}
	uint64_t shaderKey(const VideoSegment& seg) {
		return shaderKey(seg.shader, seg.vertexShader, seg.fragmentShader);
	}
	// returns a hashed key based on the shader text body
	size_t shaderHashKey(const VideoSegment& seg) {
		size_t ret = 0;
		hash<string> hashFn;
		if(seg.shader != nullptr) ret ^= hashFn(*seg.shader);
		if(seg.vertexShader != nullptr) ret ^= hashFn(*seg.vertexShader);
		if(seg.fragmentShader != nullptr) ret ^= hashFn(*seg.fragmentShader);
		return ret;
	}
	
	class ShaderProgramCache {
	public:
		vector<string> shaders;
		unordered_map<size_t, int> shaderIDs;
		unordered_map<uint64_t, int> shaderIDs2;
		
		void buildCache(const vector<VideoSegment>& segments) {
			// find all used shader code strings
			int nextIndex = 0;
			int i=-1;
			for(const VideoSegment& seg: segments) {
				i++;
				// we identify a segment's shader program by the tuple (shader, vertexShader, fragmentShader);
				// if several segments have the same tuple then the shader program can be reused.
				uint64_t key = shaderKey(seg);
				if(shaderIDs2.find(key) != shaderIDs2.end()) continue;
				
				size_t hashKey = shaderHashKey(seg);
				if(shaderIDs.find(hashKey) != shaderIDs.end()) {
					shaderIDs2[key] = shaderIDs[hashKey];
					continue;
				}
				shaderIDs[hashKey] = nextIndex;
				shaderIDs2[key] = nextIndex;
				
				if(seg.vertexShader == nullptr)
					shaders.push_back(defaultVertexShader);
				else shaders.push_back(*seg.vertexShader);
				
				if(seg.fragmentShader != nullptr)
					shaders.push_back(*seg.fragmentShader);
				else if(seg.shader != nullptr)
					shaders.push_back(FrameRenderer2_generateCode(*seg.shader, MAXUSERPARAMS));
				else shaders.push_back(defaultFragmentShader);
				
				nextIndex++;
			}
			PRNT(0, "%d unique shader keys\n", (int)shaderIDs2.size());
			PRNT(0, "%d shader programs\n", nextIndex);
		}
		int getShaderProgramIndex(const VideoSegment& seg) {
			assert(shaderIDs2.find(shaderKey(seg)) != shaderIDs2.end());
			return shaderIDs2[shaderKey(seg)];
		}
	};

	void renderVideo(const vector<VideoSegment>& segments, double fps, int w, int h, function<void(uint8_t* data)> writeFrame) {
		FrameRenderer fr(w,h);
		ShaderProgramCache shaderCache;
		
		shaderCache.buildCache(segments);
		fr.setRenderers(shaderCache.shaders);
		
		// convert note list into note event list
		vector<NoteEventV> events;
		for(int i=0;i<(int)segments.size();i++) {
			if(segments[i].startSeconds >= segments[i].endSeconds) continue;
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
				PRNT(0, "videoclip on: %5d:  dur %3.2fs\n", evt.segmentIndex, s.durationSeconds());
			} else {
				notesActive.erase(evt.segmentIndex);
				PRNT(0, "videoclip off:%5d\n", evt.segmentIndex);
			}
			
			if(curTimeFrames >= nextTimeFrames) continue;
			int durationFrames = nextTimeFrames-curTimeFrames;
			
			// collect renderers and parameters for this region
			vector<int> enabledRenderers;
			vector<int> instanceCount;
			vector<vector<float> > params;
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& s = segments.at((*it).first);
				enabledRenderers.push_back(shaderCache.getShaderProgramIndex(s));
				params.push_back(s.shaderParams);
				instanceCount.push_back(s.instances);
			}
			fr.setEnabledRenderers(enabledRenderers);
			fr.setInstanceCount(instanceCount);
			fr.setUserParams(params);
			
			int j=0;
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& s = segments.at((*it).first);
				fr.setVertexes(j, s.vertexes, s.vertexVarSizes);
				j++;
			}
			
			// render frames in this region
			vector<float> relTimeSeconds(notesActive.size());
			vector<uint32_t> textures(notesActive.size());
			for(int j=0; j<durationFrames; j++) {
				int k=0;
				double timeSeconds = double(curTimeFrames+j)/fps;
				for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
					const VideoSegment& seg = segments.at((*it).first);
					double t = timeSeconds-seg.startSeconds;
					
					textures[k] = seg.source->getFrame(t*seg.speed + seg.offsetSeconds);
					fr.setImage(k, textures[k]);
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
				fr.setUserParams(params);
				string imgData = fr.render();
				
				// release textures
				k=0;
				for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
					const VideoSegment& seg = segments.at((*it).first);
					seg.source->releaseFrame(textures[k]);
					k++;
				}
				
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
		ShaderProgramCache shaderCache;
		
		
		map<int, int, SegmentCompare> notesActive; // map from note index to start time in frames
		vector<vector<float> > curUserParams;
		VideoRendererState(const vector<VideoSegment>& segments, int w, int h, double fps, double systemFPS):
			fr(w,h), segments(segments), fps(fps), systemFPS(systemFPS),
			notesActive(SegmentCompare(segments)) {
			
			lastEventIndex = 0;
			curFrame = INT_MIN;
			
			shaderCache.buildCache(segments);
			fr.setRenderers(shaderCache.shaders);
			
			// prepare all sources
			unordered_set<VideoSource*> sources;
			for(const VideoSegment& seg: segments)
				sources.insert(seg.source);
			for(VideoSource* source: sources)
				source->prepare();
			
			// convert note list into note event list
			for(int i=0;i<(int)segments.size();i++) {
				if(segments[i].startSeconds >= segments[i].endSeconds) continue;
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
		void interpolateKeyframes(vector<float>& out, const vector<float>& kf1, const vector<float>& kf2,
									double t1, double t2, double t) {
			int sz = (int)out.size();
			
			double a = (t1==t2) ? 0. : clamp((t-t1)/(t2-t1),0.,1.);
			double b = 1. - a;
			for(int i=0; i<sz; i++)
				out[i] = b*kf1.at(i) + a*kf2.at(i);
		}
		string drawFrame(bool render = false) {
			string ret;
			int k=0;
			double timeSeconds = curFrame/fps;
			vector<float> relTimeSeconds(notesActive.size());
			vector<uint32_t> textures(notesActive.size());
			
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& seg = segments.at((*it).first);
				
				// find source frame
				double relTime = timeSeconds-seg.startSeconds;
				textures[k] = seg.source->getFrame(relTime*seg.speed + seg.offsetSeconds);
				fr.setImage(k, textures[k]);
				
				// set parameters
				relTimeSeconds[k] = float(timeSeconds-seg.startSeconds);
				
				// find current keyframe
				int kfIndex = -1;
				double kfTime = 0.;
				const vector<float>* kfLeft = &seg.shaderParams;
				for(int i=0; i<(int)seg.keyframes.size(); i++) {
					if(seg.keyframes[i].relTimeSeconds <= relTime)
						kfIndex = i;
					else break;
				}
				if(kfIndex >= 0) {
					kfLeft = &seg.keyframes[kfIndex].shaderParams;
					kfTime = seg.keyframes[kfIndex].relTimeSeconds;
				}
				// find next keyframe
				const vector<float>* kfRight = kfLeft;
				double kfRightTime = kfTime;
				if((kfIndex+1) < (int)seg.keyframes.size()) {
					kfRight = &seg.keyframes[kfIndex+1].shaderParams;
					kfRightTime = seg.keyframes[kfIndex+1].relTimeSeconds;
				}
				
				if(seg.interpolateKeyframes != nullptr)
					seg.interpolateKeyframes(curUserParams.at(k), *kfLeft, *kfRight, kfTime, kfRightTime, relTime);
				else interpolateKeyframes(curUserParams.at(k), *kfLeft, *kfRight, kfTime, kfRightTime, relTime);
				
				k++;
			}
			fr.setTime(float(timeSeconds), relTimeSeconds);
			fr.setUserParams(curUserParams);
			if(render) ret = fr.render();
			else fr.draw();
			
			// release textures
			k=0;
			for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
				const VideoSegment& seg = segments.at((*it).first);
				seg.source->releaseFrame(textures[k]);
				k++;
			}
			return ret;
		}
		// returns true if we are still within bounds of the video
		bool advanceTo(int frame) {
			if(frame <= curFrame) return true;
			
			bool encounteredEvent = false;
			//PRNT(0, "%d events\n", (int)events.size());
			// go through events until one beyond the requested frame is encountered
			while(lastEventIndex < (int)events.size()) {
				NoteEventV& evt = events[lastEventIndex];
				
				if(evt.t > frame) break;
				
				// if the event's start time is after curFrame then we haven't processed it yet
				if(evt.t > curFrame) {
					PRNT(1, "event %d: ", lastEventIndex);
					if(!evt.off) { // note on
						notesActive[evt.segmentIndex] = evt.t;
						const VideoSegment& s = segments.at(evt.segmentIndex);
						PRNT(1, "videoclip on: %5d:  dur %3.2fs\n", evt.segmentIndex, s.durationSeconds());
					} else {
						notesActive.erase(evt.segmentIndex);
						PRNT(1, "videoclip off:%5d\n", evt.segmentIndex);
					}
					encounteredEvent = true;
				}
				lastEventIndex++;
			}
			if(lastEventIndex > 0)
				lastEventIndex--;
			
			// if the current event is the last event, we are past the end of the video
			if(lastEventIndex >= (int(events.size())-1)) return false;
			
			curFrame = frame;
			
			if(encounteredEvent) {
				// collect renderers and parameters for this region
				vector<int> enabledRenderers;
				vector<int> instanceCount;
				int lastZIndex = -(1<<29);
				curUserParams.clear();
				for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
					const VideoSegment& s = segments.at((*it).first);
					
					enabledRenderers.push_back(shaderCache.getShaderProgramIndex(s));
					curUserParams.push_back(s.shaderParams);
					instanceCount.push_back(s.instances);
					assert(s.zIndex >= lastZIndex);
					lastZIndex = s.zIndex;
				}
				fr.setEnabledRenderers(enabledRenderers);
				fr.setInstanceCount(instanceCount);
				fr.setUserParams(curUserParams);
				
				int i=0;
				for(auto it = notesActive.begin(); it!=notesActive.end(); it++) {
					const VideoSegment& s = segments.at((*it).first);
					fr.setVertexes(i, s.vertexes, s.vertexVarSizes);
					i++;
				}
			}
			return true;
		}
	};
	
	void renderVideo2(const vector<VideoSegment>& segments, double fps, double startSeconds, int w, int h, function<void(uint8_t* data)> writeFrame) {
		VideoRendererState r(segments, w, h, fps, fps);
		int frame = (int)round(startSeconds*fps);
		r.fr.setRenderToInternal();
		while(r.advanceTo(frame)) {
			string tmp = r.drawFrame(true);
			writeFrame((uint8_t*)tmp.data());
			frame++;
		}
	}
	
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
	int VideoRendererTimeDriven::concurrentSegments() {
		return (int)st->notesActive.size();
	}
}
