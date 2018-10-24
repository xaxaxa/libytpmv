#include <stdio.h>
#include <ytpmv/modparser.H>
#include <ytpmv/audiorenderer.H>
#include <ytpmv/common.H>
#include <ytpmv/simple.H>
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
	if(argc < 2) {
		fprintf(stderr, "usage: %s file.mod\n", argv[0]);
		return 1;
	}
	string buf = get_file_contents(argv[1]);
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "", 2.08/30);
	addSource("ha", "sources/ha2_21070.wav", "", 2.107/30);
	
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	
	// convert to audio segments
	vector<AudioSegment> segments;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		switch(n.instrument) {
			case 1: src = getSource("o"); n.pitchSemitones+=12; n.amplitudeDB += 3; break;
			case 2: src = getSource("ha"); pan=(n.start.row%4)?0.8:0.2; break;
			case 3: src = getSource("lol"); pan=0.3; n.pitchSemitones+=12; break;
			case 4: src = getSource("lol"); pan=0.7; break;
			case 5: src = getSource("lol"); n.pitchSemitones+=12; break;
			case 6: src = getSource("lol"); break;
			default: src = getSource("o"); break;
		}
		if(n.channel == 0) n.amplitudeDB += 9;
		AudioSegment as(n, src->audio, inf.bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		
		segments.push_back(as);
	}
	renderAudio(segments,44100, [](float* data, int len) {
		int16_t buf[len];
		for(int i=0;i<len;i++) {
			float tmp = data[i]*256*32;
			if(tmp > 32767) tmp = 32767;
			if(tmp < -32767) tmp = -32767;
			buf[i] = (int16_t)tmp;
		}
		write(1,buf,sizeof(buf));
	});
	return 0;
}

