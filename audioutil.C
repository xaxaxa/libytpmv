#include <ytpmv/audioutil.H>
#include <sstream>

using namespace std;
namespace ytpmv {
	template<class T> basic_string<float> loadSample(const char* file, double scale, bool mono) {
		basic_string<float> ret;
		basic_ifstream<T> in(file, std::ios::in | std::ios::binary);
		if(!in) throw runtime_error(strerror(errno));
		
		basic_ostringstream<T> contents;
		contents << in.rdbuf();
		in.close();
		auto s = contents.str();
		
		if(mono) ret.resize(s.length()*2);
		else ret.resize(s.length());
		
		T* inBuf = (T*)s.data();
		float* outBuf = (float*)ret.data();
		
		if(mono) {
			for(int i=0; i<ret.length(); i++) {
				outBuf[i*2] = (T)(inBuf[i]*scale);
				outBuf[i*2+1] = outBuf[i*2];
			}
		} else {
			for(int i=0; i<ret.length(); i++)
				outBuf[i] = (T)(inBuf[i]*scale);
		}
		return ret;
	}
	basic_string<double> loadSample(const char* file, int bits, int channels) {
		bool mono;
		if(channels == 1) mono = true;
		else if(channels == 2) mono = false;
		else {
			throw logic_error("channel count "+to_string(channels)+" is not supported");
		}
		
		if(bits == 8) {
			return loadSample<int8_t>(file, 1./127., mono);
		} else if(bits == 16) {
			return loadSample<int16_t>(file, 1./32767., mono);
		} else if(bits == 32) {
			return loadSample<float>(file, 1., mono);
		}
		throw logic_error("bit count "+to_string(bits)+" is not supported");
	}
}
