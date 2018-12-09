#include <ytpmv/modparser.H>
#include <math.h>
#include <stdexcept>
#include <arpa/inet.h>

using namespace std;
namespace ytpmv {
	static string getString(const void* data, int maxLen) {
		string tmp((char*)data, maxLen);
		int l = strlen((char*)data);
		if(l < int(tmp.length()))
			tmp.resize(l);
		return tmp;
	}
	static uint16_t getShort(const void* data) {
		return ntohs(*(uint16_t*)data);
	}
	
	struct PlayerState {
		int channels;
		int curSeq = 0;
		int curRow = 0;
		int curRowAbs = 0;
		double bpm = 0;
		vector<int> activeNotes;
		vector<double> defaultVolumes;
		vector<Note>* outNotes;
		vector<int> lastInstrument;
		
		double tickDuration = 1./50;
		int ticksPerRow = 6;
		// when tickMode is true, curRow and curRowAbs are the number
		// of ticks from the beginning of the pattern and beginning of the song
		bool tickMode = false;
		
		PlayerState(int channels, vector<Note>* outNotes) {
			this->channels = channels;
			this->outNotes = outNotes;
			activeNotes.resize(channels, -1);
			defaultVolumes.resize(0);
			lastInstrument.resize(channels);
		}
	};
	void parseModPattern(const uint8_t* patternData, int rows, PlayerState& ps) {
		int rowBytes = ps.channels*4;
		int lastNote[ps.channels];
		int incr = 1;
		memset(lastNote,0,sizeof(lastNote));
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
				
				
				if(effect == 0xC00 || effect == 0xEC0) {
					// note off
					ps.activeNotes.at(channel) = -1;
					continue;
				}
				if((effect & 0xF00) == 0xF00) {
					uint8_t tmp = effect&0xff;
					if(tmp != 0) {
						if(tmp < 0x20) {
							ps.ticksPerRow = effect&0xff;
						} else {
							ps.tickDuration = 2.5/double(effect&0xff);
							if(ps.tickMode) {
								ps.bpm = 60./ps.tickDuration;
								PRNT(0, "row %d, channel %d, effect %x: bpm %f\n", row, channel, effect, ps.bpm);
							}
						}
						if(!ps.tickMode) {
							ps.bpm = 60./(ps.ticksPerRow*ps.tickDuration);
							PRNT(0, "row %d, channel %d, effect %x: bpm %f\n", row, channel, effect, ps.bpm);
						}
					}
				}
				incr = ps.tickMode ? ps.ticksPerRow : 1;
				
				// if an instrument is specified without a note, start a new note with the last pitch
				if(notePeriod == 0 && instrumentID != 0) {
					notePeriod = lastNote[channel];
				}
				if(notePeriod > 0) {
					lastNote[channel] = notePeriod;
					double frequencyNormalized = 856./double(notePeriod);
					double semitones = log2(frequencyNormalized)*12.;
					
					// append or update note
					
					if(instrumentID == 0) instrumentID = ps.lastInstrument.at(channel);
					double dB = ps.defaultVolumes.at(instrumentID-1);
					if((effect & 0xf00) == 0xC00) {
						dB = log10(double(effect&0xff)/64.)*20;
					}
					Note n;
					n.start = {ps.curSeq, ps.curRow, ps.curRowAbs, 0.};
					n.end = {ps.curSeq, ps.curRow+incr, ps.curRowAbs+incr, 0.};
					n.channel = channel;
					n.instrument = instrumentID;
					n.pitchSemitones = semitones;
					n.amplitudeDB = dB;
					
					if((effect & 0xf00) == 0x000 && effect != 0) {
						// arpeggio
						n.chordCount = 2;
						n.chord1 = (effect&0xf0)>>8;
						n.chord2 = (effect&0xf);
					}
					
					ps.activeNotes.at(channel) = ps.outNotes->size();
					ps.outNotes->push_back(n);
					ps.lastInstrument.at(channel) = instrumentID;
				} else {
					int j = ps.activeNotes.at(channel);
					if(j >= 0) {
						Note& n = ps.outNotes->at(j);
						n.end = {ps.curSeq, ps.curRow+incr, ps.curRowAbs+incr, 0.};
						// set volume
						if((effect & 0xF00) == 0xC00) {
							double dB = log10(double(effect&0xff)/64.)*20;
							
							double lastVolume = n.amplitudeDB;
							if(n.keyframes.size() > 0) lastVolume = n.keyframes.back().amplitudeDB;
							
							n.keyframes.push_back({double(ps.curRowAbs-n.start.absRow), lastVolume, 0.});
							n.keyframes.push_back({double(ps.curRowAbs-n.start.absRow), dB, 0.});
						}
						// volume slide
						if((effect & 0xFF0) == 0xEA0) {
							int amount = effect&0xf;
							double lastVolume = n.amplitudeDB;
							if(n.keyframes.size() > 0) lastVolume = n.keyframes.back().amplitudeDB;
							
							double amplitude = pow(10, lastVolume/20.) * 64.;
							amplitude += amount;
							
							double dB = log10(amplitude/64.)*20;
							
							n.keyframes.push_back({double(ps.curRowAbs-n.start.absRow), dB, 0.});
						}
					}
				}
				
				
				if(effect == 0xd00) shouldBreak = true;
			}
			ps.curRow += incr;
			ps.curRowAbs += incr;
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
				vector<Instrument>& outInstruments, vector<Note>& outNotes, bool tickMode) {

		// http://coppershade.org/articles/More!/Topics/Protracker_File_Format/
		
		if(inLen < 1084)
			throw runtime_error((string(".mod file must be at least 1084 bytes, but is ") + to_string(inLen) + " bytes").c_str());
		outInf.name = getString(inData, 20);
		outInf.bpm = 125*4;
		if(tickMode) outInf.bpm *= 6;
		
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
			
			ins.tuningSemitones = double(fineTune)/7. - 41;
			if(volume == 0) dB = 0.;
			else dB = log10(double(volume)/64.)*20;
			
			ins.sampleData.resize(sampleLen*CHANNELS);
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
		ps.tickMode = tickMode;
		for(int i=0; i<songLength; i++) {
			int pattern = seqTable[i];
			const uint8_t* patternData = inData + 1084 + patternBytes*pattern;
			parseModPattern(patternData, patternRows, ps);
			
			if(i==0 && ps.bpm != 0) outInf.bpm = ps.bpm;
		}
		
		
		
		int sampleStart = 1084+patternBytes*nPatterns;
		
		// load samples
		for(int i=0;i<31;i++) {
			Instrument& ins = outInstruments[i];
			int samples = ins.sampleData.length()/CHANNELS;
			//fprintf(stderr, "%d\n", sampleBytes);
			if(sampleStart + samples > inLen)
				throw runtime_error("eof when reading sample data");
			
			float* outData = const_cast<float*>(ins.sampleData.data());
			int8_t* sampleData = (int8_t*)(inData + sampleStart);
			for(int j=0;j<samples;j++) {
				for(int k=0; k<CHANNELS; k++)
					outData[j*CHANNELS+k] = (sampleData[j])/127.;
			}
			
			sampleStart += samples;
		}
		
		// extend samples to be at least 16K samples each
		for(int i=0;i<31;i++) {
			Instrument& ins = outInstruments[i];
			if(ins.sampleData.length() == 0) continue;
			
			const uint8_t* instrData = inData + 20 + i*30;
			uint16_t repIndex = getShort(instrData + 26)*2;
			uint16_t repLen = getShort(instrData + 28)*2;
			if(repLen == 0) continue;
			while(ins.sampleData.length() < 1024*64) {
				ins.sampleData.append(ins.sampleData.substr(repIndex*CHANNELS, repLen*CHANNELS));
			}
		}
	}
}
