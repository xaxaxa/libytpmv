#ifndef LIBYTPMV_TEXTURECACHE_H
#define LIBYTPMV_TEXTURECACHE_H
#include <unordered_map>
#include <stdint.h>
using namespace std;

namespace ytpmv {
	class TextureCache {
	public:
		unordered_map<const void*, uint32_t> entries;
		TextureCache() {}
		~TextureCache();
		uint32_t getTexture(const void* data, int w, int h);
	};
}

#endif
