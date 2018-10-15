
LIBYTPMV=modparser.C audiorenderer.C samplecache.C mmutil.C -lSoundTouch  `pkg-config --cflags --libs gstreamer-1.0 gio-2.0`

test1:
	g++ -o test1 test1.C modparser.C --std=c++0x -g3
test2:
	g++ -o test2 test2.C $(LIBYTPMV) --std=c++0x -g3 -Wall
test3:
	g++ -o test3 test3.C $(LIBYTPMV) --std=c++0x -g3 -Wall
