#include <ytpmv/framerenderer.H>
#include <stdio.h>
#include <unistd.h>

using namespace std;
using namespace ytpmv;
int main(int argc, char** argv) {
	
	string shader =
	"vec2 mypos = vec2(param(0), param(1));\n\
	vec2 mysize = vec2(0.2,0.2);\n\
	vec2 myend = mypos+mysize;\n\
	vec2 relpos = (pos-mypos)/mysize;\n\
	if(pos.x>=mypos.x && pos.y>=mypos.y \n\
		&& pos.x<myend.x && pos.y<myend.y) \n\
		return vec4(texture2D(image, relpos).rgb, 0.5);\n\
	return vec4(0,0,0,0);\n";
	
	//texture2D(image, relpos).rgb
	
	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	
	
	FrameRenderer r(1920,1080);
	r.setRenderers({shader}, 8, 8);
	r.setEnabledRenderers({0,0,0});
	//r.setTime(1.2);
	
	string imgdata;
	
	fprintf(stderr, "rendering\n");
	for(int i=0;i<30*5;i++) {
		r.setUserParams({{.7,.7}, {.35,.38}, {.57,.62}});
		r.setImages({img1, img1, img1});
		imgdata = r.render();
	}
	
	uint32_t* dat = (uint32_t*)imgdata.data();
	
	write(1,imgdata.data(),imgdata.length());
	return 0;
}
