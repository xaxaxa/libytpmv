#include <stdio.h>
#include <ytpmv/modparser.H>
#include <ytpmv/audiorenderer.H>
#include <ytpmv/common.H>
#include <math.h>
#include <unistd.h>

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
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	
	// convert to audio segments
	vector<AudioSegment> segments;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		segments.push_back(AudioSegment(n, ins, inf.bpm));
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

