// the uglies are kept in this file while the rest of the library
// deals in sane and easy to understand APIs

#include <ytpmv/mmutil.H>
#include <sstream>
#include <string>
#include <iostream>
#include <stdio.h>
#include <gst/gst.h>
#include <gio/gio.h>

using namespace std;
namespace ytpmv {
	static int _init__() {
		const gchar *nano_str;
		guint major, minor, micro, nano;

		gst_init (NULL, NULL);
		gst_version (&major, &minor, &micro, &nano);

		if (nano == 1) nano_str = "(CVS)";
		else if (nano == 2) nano_str = "(Prerelease)";
		else nano_str = "";

		fprintf (stderr, "This program is linked against GStreamer %d.%d.%d %s\n",
				major, minor, micro, nano_str);
		return 0;
	}
	static int _sssss = _init__();
	
	
	static void on_pad_added(GstElement *decodebin,
							 GstPad *pad,
							 gpointer data) {
		GstElement *convert = (GstElement *) data;
		fprintf(stderr, "pad added\n");
		GstCaps *caps;
		GstStructure *str;
		GstPad *audiopad;

		audiopad = gst_element_get_static_pad(convert, "sink");
		if (GST_PAD_IS_LINKED(audiopad)) {
			g_object_unref(audiopad);
			return;
		}

		caps = gst_pad_query_caps(pad, NULL);
		str = gst_caps_get_structure(caps, 0);
		fprintf(stderr, "here %s\n",gst_structure_get_name(str));
		if (!g_strrstr(gst_structure_get_name(str), "audio")) {
			gst_caps_unref(caps);
			gst_object_unref(audiopad);
			fprintf(stderr, "ERROR 1\n");
			return;
		}
		gst_caps_unref(caps);
		gst_pad_link(pad, audiopad);
		g_object_unref(audiopad);
		fprintf(stderr, "pad linked\n");
	}

	static gboolean bus_call(GstBus *bus,
							 GstMessage *msg,
							 gpointer data) {
		GMainLoop *loop = (GMainLoop*)data;

		switch (GST_MESSAGE_TYPE(msg)) {
			case GST_MESSAGE_EOS:
				fprintf(stderr, "End of stream\n");
				g_main_loop_quit(loop);
				break;
			case GST_MESSAGE_ERROR: {
				gchar  *debug;
				GError *error;

				gst_message_parse_error(msg, &error, &debug);
				g_free (debug);

				g_printerr("Error: %s\n", error->message);
				//g_error_free(error);

				g_main_loop_quit(loop);
				
				throw runtime_error(error->message);
				break;
			}
			default:
				break;
		}
		return true;
	}
	
	// TODO(xaxaxa): convert all this code to use gst_parse_launch() instead
	// TODO(xaxaxa): set AudioSource speed based on systemSRate and file srate
	AudioSource* loadAudio(const char* file, int systemSRate) {
		GstElement *pipeline, *source, *decode, *sink, *convert;
		int rate = systemSRate;
		int channels = CHANNELS;
		GMainLoop *loop;
		GstBus *bus;
		guint bus_watch_id;
		GMemoryOutputStream *stream;
		gpointer out_data;

		// loop
		loop = g_main_loop_new(NULL, false);
		// pipeline
		pipeline = gst_pipeline_new("test_pipeline");
		// sink
		stream = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new(NULL, 0, (GReallocFunc)g_realloc, (GDestroyNotify)g_free));
		sink = gst_element_factory_make ("giostreamsink", "sink");
		g_object_set(G_OBJECT(sink), "stream", stream, NULL);
		// source
		source = gst_element_factory_make("filesrc", "source");
		g_object_set(G_OBJECT(source), "location", file, NULL);
		// convert
		convert = gst_element_factory_make("audioconvert", "convert");
		// decode
		decode = gst_element_factory_make("decodebin", "decoder");
		// link decode to convert
		g_signal_connect(decode, "pad-added", G_CALLBACK(on_pad_added), convert);

		// bus
		bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// add elements into pipeline
		gst_bin_add_many(GST_BIN(pipeline), source, decode, convert, sink, NULL);
		// link source to decode
		gst_element_link(source, decode);
		//gst_element_link(convert, sink);
		// caps
		GstCaps *caps;
		caps = gst_caps_new_simple("audio/x-raw",
									"format", G_TYPE_STRING, "S16LE",
								   "channels", G_TYPE_INT, channels,
								   "layout", G_TYPE_STRING, "interleaved",
								   NULL);
		// link convert to sink
		gst_element_link_filtered(convert, sink, caps);
		gst_caps_unref(caps);
		// start playing
		gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

		// iterate
		fprintf(stderr, "RUNNING GSTREAMER PIPELINE FOR AUDIO: %s\n", file);
		g_main_loop_run(loop);

		// out of the main loop, clean up nicely
		fprintf(stderr, "Returned, stopping playback\n");
		gst_element_set_state(pipeline, GST_STATE_NULL);

		fprintf(stderr, "Deleting pipeline\n");
		gst_object_unref(GST_OBJECT(pipeline));
		g_source_remove (bus_watch_id);
		g_main_loop_unref(loop);

		// get data
		fprintf(stderr, "get data\n");
		out_data = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(stream));

		unsigned long size = g_memory_output_stream_get_size(G_MEMORY_OUTPUT_STREAM(stream));
		unsigned long sizeData = g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(stream));
		std::cerr << "stream size: " << size << std::endl;
		std::cerr << "stream data size: " << sizeData << std::endl;

		// access data and store in vector
		AudioSource* as = new AudioSource();
		double scale = 1./32767.;
		as->name = file;
		as->pitch = 1.;
		as->tempo = 1.;
		as->sample.resize(sizeData/2);
		for (unsigned long i = 0; i < sizeData/2; ++i) {
			as->sample[i] = float(((int16_t*)out_data)[i])*scale;
			//fprintf(stderr, "%d\n", int(((int16_t*)out_data)[i]));
		}
		return as;
	}
	
	static void on_pad_added_video(GstElement *decodebin,
							 GstPad *pad,
							 gpointer data) {
		GstElement *convert = (GstElement *) data;
		fprintf(stderr, "pad added\n");
		GstCaps *caps;
		GstStructure *str;
		GstPad *videopad;

		videopad = gst_element_get_static_pad(convert, "sink");
		if (GST_PAD_IS_LINKED(videopad)) {
			g_object_unref(videopad);
			return;
		}

		caps = gst_pad_query_caps(pad, NULL);
		str = gst_caps_get_structure(caps, 0);
		fprintf(stderr, "here %s\n",gst_structure_get_name(str));
		if (!g_strrstr(gst_structure_get_name(str), "video")) {
			gst_caps_unref(caps);
			gst_object_unref(videopad);
			fprintf(stderr, "ERROR 1\n");
			return;
		}
		
		gst_caps_unref(caps);
		gst_pad_link(pad, videopad);
		g_object_unref(videopad);
		fprintf(stderr, "pad linked\n");
	}
	
	// TODO(xaxaxa): convert all this code to use gst_parse_launch() instead
	// TODO(xaxaxa): set VideoSource speed based on systemFPS and file fps
	GMemoryOutputStream* loadVideo(const char* file, int& width, int& height) {
		GstElement *pipeline, *source, *decode, *sink, *convert;
		GMainLoop *loop;
		GstBus *bus;
		guint bus_watch_id;
		GMemoryOutputStream *stream;

		// loop
		loop = g_main_loop_new(NULL, false);
		// pipeline
		pipeline = gst_pipeline_new("test_pipeline");
		// sink
		stream = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new(NULL, 0, (GReallocFunc)g_realloc, (GDestroyNotify)g_free));
		sink = gst_element_factory_make ("giostreamsink", "sink");
		g_object_set(G_OBJECT(sink), "stream", stream, NULL);
		// source
		source = gst_element_factory_make("filesrc", "source");
		g_object_set(G_OBJECT(source), "location", file, NULL);
		// convert
		convert = gst_element_factory_make("videoconvert", "convert");
		// decode
		decode = gst_element_factory_make("decodebin", "decoder");
		// link decode to convert
		g_signal_connect(decode, "pad-added", G_CALLBACK(on_pad_added_video), convert);

		// bus
		bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// add elements into pipeline
		gst_bin_add_many(GST_BIN(pipeline), source, decode, convert, sink, NULL);
		// link source to decode
		gst_element_link(source, decode);
		//gst_element_link(convert, sink);
		// caps
		GstCaps *caps;
		caps = gst_caps_new_simple("video/x-raw",
									"format", G_TYPE_STRING, "RGB",
								   "interlace-mode", G_TYPE_STRING, "progressive",
								   NULL);
		// link convert to sink
		gst_element_link_filtered(convert, sink, caps);
		gst_caps_unref(caps);
		// start playing
		gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

		// iterate
		fprintf(stderr, "RUNNING GSTREAMER PIPELINE FOR VIDEO: %s\n", file);
		g_main_loop_run(loop);
		
		
		// retrieve the dimensions of the video
		GstPad* sinkPad = gst_element_get_static_pad (sink, "sink");
		GstCaps* sinkCaps = gst_pad_get_current_caps (sinkPad);
		GstStructure* sinkCapsStruct = gst_caps_get_structure(sinkCaps, 0);
		fprintf(stderr, "pad caps: %s\n",  gst_caps_to_string (sinkCaps));
		
		if((!gst_structure_get_int (sinkCapsStruct, "width", &width))
			|| (!gst_structure_get_int (sinkCapsStruct, "height", &height))) {
			throw runtime_error(string("No Width/Height are Available in the Incoming Stream Data !! file: ") + file + "\n");
		}
		fprintf(stderr, "dimensions: %d x %d\n", width, height);

		// out of the main loop, clean up nicely
		fprintf(stderr, "Returned, stopping playback\n");
		gst_element_set_state(pipeline, GST_STATE_NULL);

		fprintf(stderr, "Deleting pipeline\n");
		gst_object_unref(GST_OBJECT(pipeline));
		g_source_remove (bus_watch_id);
		g_main_loop_unref(loop);
		return stream;
	}
	
	ImageArraySource* loadVideo(const char* file) {
		int width = 0, height = 0;
		GMemoryOutputStream *stream = loadVideo(file, width, height);
		
		// get data
		fprintf(stderr, "get data\n");
		char* out_data = (char*)g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(stream));
		
		unsigned long size = g_memory_output_stream_get_size(G_MEMORY_OUTPUT_STREAM(stream));
		unsigned long sizeData = g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(stream));
		std::cerr << "stream size: " << size << std::endl;
		std::cerr << "stream data size: " << sizeData << std::endl;

		// access data and store in vector
		int imgBytes = width*height*3;
		
		ImageArraySource* vs = new ImageArraySource();
		vs->name = file;
		vs->fps = 30.;
		
		for(int i=0; i<sizeData; i+=imgBytes) {
			int bytesLeft = sizeData-i;
			if(bytesLeft < imgBytes) break;
			vs->frames.push_back({width, height, {out_data + i, imgBytes}, 0});
		}
		g_object_unref(stream);
		return vs;
	}
	
	// FIXME(xaxaxa): UGLY!!!!!! does not clean up gstreamer pipeline properly
	void* runLoopThread(void* v) {
		GMainLoop* loop = (GMainLoop*)v;
		g_main_loop_run(loop);
		return nullptr;
	}
	// BUG(xaxaxa): this code is really awful both in terms of efficiency and structure;
	// there are 3 data copies of the video data made: write() to pipe, read() from pipe,
	// and when the image is copied into the texture. 
	// To fix this properly it is necessary to write a custom gstreamer element (ew).
	// There is no way to get video width and height. To fix that you need to listen for some sort of event
	// on the gstreamer event loop.
	void loadVideoToFD(const char* file, int fd) {
		GstElement *pipeline, *source, *decode, *sink, *convert;
		GMainLoop *loop;
		GstBus *bus;
		guint bus_watch_id;

		// loop
		loop = g_main_loop_new(NULL, false);
		// pipeline
		pipeline = gst_pipeline_new("test_pipeline");
		// sink
		sink = gst_element_factory_make ("fdsink", "sink");
		g_object_set(G_OBJECT(sink), "fd", fd, NULL);
		// source
		source = gst_element_factory_make("filesrc", "source");
		g_object_set(G_OBJECT(source), "location", file, NULL);
		// convert
		convert = gst_element_factory_make("videoconvert", "convert");
		// decode
		decode = gst_element_factory_make("decodebin", "decoder");
		// link decode to convert
		g_signal_connect(decode, "pad-added", G_CALLBACK(on_pad_added_video), convert);

		// bus
		bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
		bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
		gst_object_unref(bus);

		// add elements into pipeline
		gst_bin_add_many(GST_BIN(pipeline), source, decode, convert, sink, NULL);
		// link source to decode
		gst_element_link(source, decode);
		//gst_element_link(convert, sink);
		// caps
		GstCaps *caps;
		caps = gst_caps_new_simple("video/x-raw",
									"format", G_TYPE_STRING, "RGB",
								   "interlace-mode", G_TYPE_STRING, "progressive",
								   NULL);
		// link convert to sink
		gst_element_link_filtered(convert, sink, caps);
		gst_caps_unref(caps);
		// start playing
		gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

		
		// retrieve the dimensions of the video
		/*GstPad* sinkPad = gst_element_get_static_pad (sink, "sink");
		GstCaps* sinkCaps = gst_pad_get_current_caps (sinkPad);
		GstStructure* sinkCapsStruct = gst_caps_get_structure(sinkCaps, 0);
		fprintf(stderr, "pad caps: %s\n",  gst_caps_to_string (sinkCaps));
		
		if((!gst_structure_get_int (sinkCapsStruct, "width", &w))
			|| (!gst_structure_get_int (sinkCapsStruct, "height", &h))) {
			throw runtime_error(string("No Width/Height are Available in the Incoming Stream Data !! file: ") + file + "\n");
		}
		fprintf(stderr, "dimensions: %d x %d\n", w, h);*/

		// iterate
		fprintf(stderr, "RUNNING GSTREAMER PIPELINE FOR VIDEO: %s\n", file);
		//g_main_loop_run(loop);
		
		pthread_t pth;
		pthread_create(&pth, nullptr, runLoopThread, loop);
		
		/*
		// out of the main loop, clean up nicely
		fprintf(stderr, "Returned, stopping playback\n");
		gst_element_set_state(pipeline, GST_STATE_NULL);

		fprintf(stderr, "Deleting pipeline\n");
		gst_object_unref(GST_OBJECT(pipeline));
		g_source_remove (bus_watch_id);
		g_main_loop_unref(loop);*/
	}
	
	void encodeVideo(int audioFD, int videoFD, int w, int h, double fps, int srate, int outFD) {
		string desc = string("fdsrc fd=") + to_string(videoFD) +
					" ! rawvideoparse use-sink-caps=false width="+to_string(w)+" height="+to_string(h)+" framerate="+to_string((int)fps)+"/1 format=7"
					" ! videoconvert ! x264enc bitrate=8192 ! mp4mux fragment-duration=1000 streamable=true name=mux ! fdsink fd=" + to_string(outFD)
					+ " fdsrc name=fdsrc_audio fd=" + to_string(audioFD) + 
					" ! rawaudioparse pcm-format=GST_AUDIO_FORMAT_S16LE num-channels=2 interleaved=true sample-rate="+to_string(srate)+" ! audioconvert ! lamemp3enc ! mux.";
		fprintf(stderr, "%s\n", desc.c_str());
		GError* err = nullptr;
		GstElement* pipeline = gst_parse_launch(desc.c_str(), &err);
		if(err != nullptr) {
			throw runtime_error(err->message);
		}
		
		GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE) {
			gst_object_unref (pipeline);
			throw runtime_error("gstreamer error; Unable to set the pipeline to the playing state.");
		}
		
		GstBus* bus = gst_element_get_bus (pipeline);
		GstMessage* msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
		bool hasError = false;
		
		if (msg != NULL) {
			GError *err;
			gchar *debug_info;

			switch (GST_MESSAGE_TYPE (msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error (msg, &err, &debug_info);
				g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
				g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
				g_clear_error (&err);
				g_free (debug_info);
				hasError = true;
				break;
			default:
				break;
			}
			gst_message_unref (msg);
		}
		
		gst_object_unref (bus);
		gst_element_set_state (pipeline, GST_STATE_NULL);
		gst_object_unref (pipeline);
		
		if(hasError) throw runtime_error("gstreamer error; see log");
	}
	
	
	
	
	void MemoryVideoSource::prepare() {
		if(frames.size() != 0) return;
		
		int width = 0, height = 0;
		GMemoryOutputStream *stream = loadVideo(file.c_str(), width, height);
		
		// get data
		fprintf(stderr, "get data\n");
		char* out_data = (char*)g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(stream));
		unsigned long size = g_memory_output_stream_get_size(G_MEMORY_OUTPUT_STREAM(stream));
		unsigned long sizeData = g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(stream));
		
		int stride = (width*3 + 3)/4*4;
		int imgBytes = stride*height;
		
		for(int i=0; i<sizeData; i+=imgBytes) {
			int bytesLeft = sizeData-i;
			if(bytesLeft < imgBytes) break;
			uint32_t tex = createTexture();
			setTextureImage(tex, out_data+i, width, height);
			frames.push_back(tex);
		}
		g_object_unref(stream);
	}
	int32_t MemoryVideoSource::getFrame(double timeSeconds) {
		int i = clamp((int)round(timeSeconds*speed*fps), 0, int(frames.size())-1);
		return frames.at(i);
	}
	void MemoryVideoSource::releaseFrame(uint32_t texture) {
		// nothing to do
	}
	MemoryVideoSource::~MemoryVideoSource() {
		for(uint32_t tex: frames)
			deleteTexture(tex);
	}
}
