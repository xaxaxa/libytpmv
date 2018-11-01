
#LIBYTPMV=modparser.C audiorenderer.C samplecache.C mmutil.C framerenderer.C videorenderer.C simple.C -lGL -lGLEW -lEGL -lX11 -lgbm -lSoundTouch -lpthread -lasound `pkg-config --cflags --libs gstreamer-1.0 gio-2.0`

LIBYTPMV=common.o glutil.o texturecache.o modparser.o audiorenderer.o samplecache.o mmutil.o framerenderer.o framerenderer2.o videorenderer.o simple.o
LIBS= -lglfw -lGL -lGLEW -lEGL -lX11 -lgbm -lSoundTouch -lpthread -lasound `pkg-config --libs gstreamer-1.0 gio-2.0`

CFLAGS ?= -O2

CC_FLAGS = $(CFLAGS) -Iinclude -Wall --std=c++0x `pkg-config --cflags gstreamer-1.0 gio-2.0` -fno-omit-frame-pointer
LD_FLAGS = $(LIBS) $(LDFLAGS)

libytpmv.a: $(LIBYTPMV)
	ar rcs libytpmv.a $(LIBYTPMV)

clean:
	rm -f *.a *.o

%.o: %.C
	g++ -c $(CC_FLAGS) $< -o $@

test1: test1.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)

test2: test2.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)

test3: test3.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)

test4: test4.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)

test5: test5.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)

test6: test6.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)

test7: test7.C libytpmv.a
	g++ -o $@ $^ $(CC_FLAGS) $(LIBS)
