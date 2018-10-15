#include "include/mmutil.H"
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
	
	
	AudioSource loadAudio(const char* file, int systemSRate) {
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
		fprintf(stderr, "Running...\n");
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
		AudioSource as;
		double scale = 1./32767.;
		as.name = file;
		as.pitch = 1.;
		as.tempo = 1.;
		as.sample.resize(sizeData/2);
		for (unsigned long i = 0; i < sizeData/2; ++i) {
			as.sample[i] = float(((int16_t*)out_data)[i])*scale;
			//fprintf(stderr, "%d\n", int(((int16_t*)out_data)[i]));
		}
		return as;
	}
}
