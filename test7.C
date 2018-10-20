#include <stdio.h>
#include "include/modparser.H"
#include "include/audiorenderer.H"
#include "include/mmutil.H"
#include "include/common.H"
#include "include/simple.H"
#include <math.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include <algorithm>
#include <map>

using namespace std;
using namespace ytpmv;

map<string, Source> sources;
void addSource(string name, string audioFile, string videoFile,
				double audioPitch=1., double audioTempo=1., double videoSpeed=1.) {
	auto& src = sources[name];
	src.name = name;
	src.hasAudio = false;
	src.hasVideo = false;
	if(audioFile != "") {
		src.audio = loadAudio(audioFile.c_str(), 44100);
		src.audio.pitch *= audioPitch;
		src.audio.tempo *= audioTempo;
		src.hasAudio = true;
	}
	if(videoFile != "") {
		src.video = loadVideo(videoFile.c_str(), 30);
		src.video.speed *= videoSpeed;
		src.hasVideo = true;
	}
}
Source* getSource(string name) {
	auto it = sources.find(name);
	if(it == sources.end()) throw runtime_error(string("getSource(): source ") + name + " not found");
	return &(*it).second;
}


int main(int argc, char** argv) {
	if(argc < 2) {
		fprintf(stderr, "usage: %s file.mod\n", argv[0]);
		return 1;
	}
	string buf = get_file_contents(argv[1]);
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "", 2.08/30);
	addSource("ha", "sources/ha2_21070.wav", "", 2.107/30);
	addSource("hum", "sources/yX5TIDLvMyw_640_67.mkv",
					"sources/yX5TIDLvMyw_640_67.mkv", 2.8/30, 1., 0.2);
	addSource("ya", "sources/drink.mp4",
					"sources/drink.mp4", 2.45/30, 1., 1.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5/30, 2., 1.);
	
	string shader =
		"vec2 mypos = vec2(param(0), param(1));\n\
		vec2 mysize = vec2(0.2+secondsRel/10,0.2+secondsRel/10);\n\
		vec2 myend = mypos+mysize;\n\
		vec2 relpos = (pos-mypos)/mysize;\n\
		if(pos.x>=mypos.x && pos.y>=mypos.y \n\
			&& pos.x<myend.x && pos.y<myend.y) \n\
			return vec4(texture2D(image, relpos).rgb, 0.5);\n\
		return vec4(0,0,0,0);\n";

	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	
	VideoSource imgSource = {"source 1", {img1}, 1.0};
	
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm*1.13;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		switch(n.instrument) {
			case 1: src = getSource("hum"); n.pitchSemitones+=24; n.amplitudeDB += 3; break;
			case 2: src = getSource("ha"); pan=(n.start.row%4)?0.8:0.2; break;
			case 3: src = getSource("lol"); pan=0.3; n.pitchSemitones+=12; break;
			case 4: src = getSource("aaaa"); pan=0.7; n.amplitudeDB += 6; break;
			case 5: src = getSource("lol"); n.pitchSemitones+=12; break;
			case 6: src = getSource("aaaa"); break;
			default: src = getSource("o"); break;
		}
		if(n.channel == 0) n.amplitudeDB += 9;
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		
		segments.push_back(as);
		
		VideoSegment s(n, src->hasVideo?src->video:imgSource, bpm);
		s.shader = shader;
		s.shaderParams = {n.instrument/8., n.channel/13.};
		videoSegments.push_back(s);
	}
	
	ytpmv::play(segments, videoSegments);
	return 0;
}

