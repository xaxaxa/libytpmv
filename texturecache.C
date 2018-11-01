#include <ytpmv/texturecache.H>
#include <ytpmv/glutil.H>
namespace ytpmv {
	TextureCache::~TextureCache() {
		for(auto& entry: entries) {
			deleteTexture(entry.second);
		}
	}
	uint32_t TextureCache::getTexture(const void* data, int w, int h) {
		auto it = entries.find(data);
		if(it != entries.end()) return (*it).second;
		
		uint32_t tex = createTexture();
		setTextureImage(tex, data, w, h);
		entries[data] = tex;
		return tex;
	}
}
