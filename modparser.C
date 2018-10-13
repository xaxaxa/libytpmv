#include "include/modparser.H"
#include <math.h>
#include <stdexcept>
#include <arpa/inet.h>

using namespace std;
namespace ytpmv {
	static string getString(const void* data, int maxLen) {
		string tmp((char*)data, maxLen);
		int l = strlen((char*)data);
		if(l < tmp.length())
			tmp.resize(l);
		return tmp;
	}
	static uint16_t getShort(const void* data) {
		return ntohs(*(uint16_t*)data);
	}
	
	struct PlayerState {
		int channels;
		int curSeq;
		int curRow;
		int curRowAbs;
		vector<int> activeNotes;
		vector<double> defaultVolumes;
		vector<Note>* outNotes;
		PlayerState(int channels, vector<Note>* outNotes) {
			this->channels = channels;
			this->outNotes = outNotes;
			curSeq = 0;
			curRow = 0;
			curRowAbs = 0;
			activeNotes.resize(channels, -1);
			defaultVolumes.resize(0);
		}
	};
	void parseModPattern(const uint8_t* patternData, int rows, PlayerState& ps) {
		int rowBytes = ps.channels*4;
		for(int row=0; row<rows; row++) {
			const uint8_t* rowData = patternData + row*rowBytes;
			bool shouldBreak = false;
			for(int channel=0; channel<ps.channels; channel++) {
				const uint8_t* entryData = (uint8_t*)rowData + channel*4;
				
				// parse note entry
				uint32_t entry = (uint32_t(entryData[0]) << 24) |
								(uint32_t(entryData[1]) << 16) |
								(uint32_t(entryData[2]) << 8) |
								(uint32_t(entryData[3]) << 0);
				
				int instrumentID = (entry >> 28) << 4;
				int notePeriod = (entry << 4) >> 20;
				instrumentID |= (entryData[2] >> 4);
				int effect = entry & 0b111111111111;
				
				//printf("%5d %5x    ", notePeriod, instrumentID);
				
				if(effect == 0xC00) {
					// note off
					ps.activeNotes.at(channel) = -1;
					continue;
				}
				if(notePeriod > 0) {
					double frequencyNormalized = 856./double(notePeriod);
					double semitones = log2(frequencyNormalized)*12.;
					
					// append or update note
				
					double dB = ps.defaultVolumes.at(instrumentID);
					if((effect & 0xf00) == 0xC00) {
						dB = log10(double(effect&0xff)/64.)*10;
					}
					Note n;
					n.start = {ps.curSeq, ps.curRow, ps.curRowAbs, 0.};
					n.end = {ps.curSeq, ps.curRow+1, ps.curRowAbs+1, 0.};
					n.channel = channel;
					n.instrument = instrumentID;
					n.pitchSemitones = semitones;
					n.amplitudeDB = dB;
					ps.activeNotes.at(channel) = ps.outNotes->size();
					ps.outNotes->push_back(n);
				} else {
					int j = ps.activeNotes.at(channel);
					if(j >= 0) {
						ps.outNotes->at(j).end = {ps.curSeq, ps.curRow+1, ps.curRowAbs+1, 0.};
					}
				}
				if(effect == 0xd00) shouldBreak = true;
			}
			//printf("\n");
			ps.curRow++;
			ps.curRowAbs++;
			if(shouldBreak) break;
		}
		ps.curRow = 0;
		ps.curSeq++;
		for(int channel=0; channel<ps.channels; channel++) {
			int j = ps.activeNotes.at(channel);
			if(j >= 0) {
				ps.outNotes->at(j).end = {ps.curSeq, ps.curRow, ps.curRowAbs, 0.};
			}
		}
	}
	void parseMod(const uint8_t* inData, int inLen, SongInfo& outInf,
				vector<Instrument>& outInstruments, vector<Note>& outNotes) {

		// http://coppershade.org/articles/More!/Topics/Protracker_File_Format/
		
		if(inLen < 1084)
			throw runtime_error((string(".mod file must be at least 1084 bytes, but is ") + to_string(inLen) + " bytes").c_str());
		outInf.name = getString(inData, 20);
		outInf.bpm = 320;
		
		// parse samples
		vector<double> defaultVolumes;
		for(int i=0;i<31;i++) {
			const uint8_t* instrData = inData + 20 + i*30;
			
			Instrument ins;
			double dB;
			
			ins.id = i+1;
			ins.name = getString(instrData, 22);
			int sampleLen = int(getShort(instrData + 22))*2;
			int8_t fineTune = *(int8_t*)(instrData + 24);
			uint8_t volume = *(uint8_t*)(instrData + 25);
			
			// sign extend
			fineTune <<= 4;
			fineTune >>= 4;
			
			ins.tuningSemitones = double(fineTune)/8.;
			if(volume == 0) dB = 0.;
			else dB = log10(double(volume)/64.)*10;
			
			ins.sampleData.resize(sampleLen);
			defaultVolumes.push_back(dB);
			outInstruments.push_back(ins);
		}
		
		// parse sequence table
		int songLength = *(uint8_t*)(inData + 950);
		uint8_t* seqTable = (uint8_t*)inData + 952;
		int seqTableLength = 128;
		if(songLength > seqTableLength) songLength = seqTableLength;
		
		int channels = 4;
		string songType = getString(inData + 1080, 4);
		if(songType == "M.K.") {
			channels = 4;
		} else if(songType == "M!K!") {
			channels = 4;
		} else if(songType == "6CHN") {
			channels = 6;
		} else if(songType == "8CHN") {
			channels = 8;
		} else if(songType == "CD81" || songType == "OKTA" || songType == "OCTA") {
			channels = 8;
		} else if(songType == "TDZ1") {
			channels = 1;
		} else if(songType == "TDZ2") {
			channels = 2;
		} else if(songType == "TDZ3") {
			channels = 3;
		} else if(songType == "FLT4") {
			channels = 4;
		} else if(songType == "FLT8") {
			channels = 8;
		} else if(songType.length() == 4 && songType.substr(1) == "CHN") {
			channels = atoi(songType.substr(0,1).c_str());
		} else if(songType.length() == 4 && songType.substr(2) == "CH") {
			channels = atoi(songType.substr(0,2).c_str());
		} else if(songType.length() == 4 && songType.substr(2) == "CN") {
			channels = atoi(songType.substr(0,2).c_str());
		} else {
			throw runtime_error(string("bad .mod type string: ") + songType);
		}
		
		// find highest pattern number
		int nPatterns = 0;
		for(int i=0; i<seqTableLength; i++) {
			int tmp = seqTable[i] + 1;
			if(tmp > nPatterns) nPatterns = tmp;
		}
		
		int patternRows = 64;
		int patternBytes = patternRows*channels*4;
		
		int minLen = 1084+patternBytes*nPatterns;
		if(inLen < minLen)
			throw runtime_error((string(".mod file should be at least ") + to_string(minLen) + " bytes, but is " + to_string(inLen) + " bytes").c_str());
		
		// render song
		PlayerState ps(channels, &outNotes);
		ps.defaultVolumes = defaultVolumes;
		for(int i=0; i<songLength; i++) {
			int pattern = seqTable[i];
			//printf("%d\n", pattern);
			const uint8_t* patternData = inData + 1084 + patternBytes*pattern;
			parseModPattern(patternData, patternRows, ps);
		}
		
		int sampleStart = 1084+patternBytes*nPatterns;
		
		fprintf(stderr, "%d %d\n", inLen, sampleStart);
		
		// load samples
		for(int i=0;i<31;i++) {
			Instrument& ins = outInstruments[i];
			int sampleBytes = ins.sampleData.length();
			fprintf(stderr, "%d\n", sampleBytes);
			if(sampleStart + sampleBytes > inLen)
				throw runtime_error("eof when reading sample data");
			
			double* outData = (double*)ins.sampleData.data();
			int8_t* sampleData = (int8_t*)(inData + sampleStart);
			for(int j=0;j<(int)ins.sampleData.length();j++)
				outData[j] = (sampleData[j])/127.;
			
			sampleStart += ins.sampleData.length();
		}
		
		// extend samples to at least 8KB each
		for(int i=0;i<31;i++) {
			Instrument& ins = outInstruments[i];
			if(ins.sampleData.length() == 0) continue;
			
			const uint8_t* instrData = inData + 20 + i*30;
			uint16_t repIndex = getShort(instrData + 26)*2;
			uint16_t repLen = getShort(instrData + 28)*2;
			if(repLen == 0) continue;
			while(ins.sampleData.length() < 1024*8) {
				ins.sampleData.append(ins.sampleData.substr(repIndex, repLen));
			}
		}
	}
}
