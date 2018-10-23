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

int main(int argc, char** argv) {
	string buf = get_file_contents("test3.mod");
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08/30);
	addSource("ha", "sources/ha2_21070.wav", "", 2.107/30);
	addSource("hum", "sources/bass1.mkv",
					"sources/bass1.mkv", 2.8/30, 1., 0.2);
	addSource("ya", "sources/drink.mp4",
					"sources/drink.mp4", 2.45/30, 1., 1.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5/30, 2., 2.);
	
	string shader =
		"float sizeScale = param(8) * secondsRel;\n\
		float opacityScale = param(4) + param(9) * secondsRel;\n\
		opacityScale = clamp(opacityScale,0.0,1.0);\n\
		float radius = param(5);\n\
		vec2 aspect = vec2(resolution.x/resolution.y, 1.0);\n\
		vec2 mypos = vec2(param(0)-sizeScale, param(1)-sizeScale);\n\
		vec2 mysize = vec2(param(2)+sizeScale*2,param(3)+sizeScale*2);\n\
		vec2 velocity = vec2(param(6), param(7));\n\
		mypos += velocity * secondsRel;\n\
		vec2 myend = mypos+mysize;\n\
		vec2 relpos = (pos-mypos)/mysize;\n\
		float dist = length((relpos-vec2(0.5,0.5))*aspect)*2;\n\
		float opacity = clamp(radius-pow(dist,5), 0.0, 1.0);\n\
		if(pos.x>=mypos.x && pos.y>=mypos.y \n\
			&& pos.x<myend.x && pos.y<myend.y) \n\
			return vec4(texture2D(image, relpos).rgb, opacityScale*opacity);\n\
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
		
		// select source
		switch(n.instrument) {
			case 1: src = getSource("hum"); n.pitchSemitones+=36; n.amplitudeDB += 5; break;
			case 2: src = getSource("ha"); pan=(n.start.row%4)?0.8:0.2; break;
			case 3: src = getSource("lol"); pan=0.3; n.pitchSemitones+=12; break;
			case 4: src = getSource("aaaa"); pan=0.7; n.amplitudeDB += 3; break;
			case 5: src = getSource("lol"); n.pitchSemitones+=12; break;
			case 6: src = getSource("aaaa"); n.pitchSemitones+=12; break;
			default: src = getSource("o"); break;
		}
		if(n.channel == 0) n.amplitudeDB += 9;
		
		VideoSegment vs(n, src->hasVideo?src->video:imgSource, bpm);
		
		
		// set video clip position and size
		//								x		y		w		h		opacity		radius,	vx,	vy,	sizeScale	opacityScale
		vector<float> shaderParams = {0.,		0.,		1.,		1.,		0.7,		100.,	0.,	0.,	0.1,		0.};
		if(n.channel >= 1 && n.channel <= 9) {
			shaderParams[0] = float((n.channel-1)/3)/3. + 1./3/2 - 0.4/2;
			shaderParams[1] = float((n.channel-1)%3)/3. + 1./3/2 - 0.4/2;
			shaderParams[2] = 0.4;
			shaderParams[3] = 0.4;
			shaderParams[4] = 1.;
			shaderParams[5] = 1.;
		}
		if(n.channel == 8) {
			shaderParams[0] = 2./3 + 1./3/2 - 0.4/2;
			shaderParams[1] = float(n.start.row-56)/8.;
			if(n.start.row == 0) {
				shaderParams[0] = 0.;
				shaderParams[1] = 0.;
				shaderParams[2] = 1.;
				shaderParams[3] = 1.;
				shaderParams[4] = 0.8;
				shaderParams[5] = 100.;
			}
		}
		if(n.channel == 9) {
			vs.endSeconds += 1.;
			shaderParams[6] = 0.;	// velocity x
			shaderParams[7] = -1.8;	// velocity y
			shaderParams[8] = 0.;	// size scaling per time
			shaderParams[9] = -2.;	// opacity change per time
		}
		
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		segments.push_back(as);
		
		vs.shader = shader;
		vs.shaderParams = shaderParams;
		vs.zIndex = n.channel;
		videoSegments.push_back(vs);
	}
	defaultSettings.volume = 1./3;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

