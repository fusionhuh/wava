#include <math.h>
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <quick_arg_parser.hpp>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

#include <libconfig.h++>

#include <fftw3.h>

#include <common.h>
#include <pulse.h>

#include <wavatransform.hpp>

#include <graphics.hpp>
#include <cli.hpp>

// Credit to: https://github.com/Dugy/quick_arg_parser

using namespace libconfig;

struct RenderingArgs : MainArguments<RenderingArgs> {
	int donut_count = option("donuts", 'd', "Number of donuts to be rendered.") = 0;
	int sphere_count = option("spheres", 's', "Number of spheres to be rendered.") = 0;
	int rect_prism_count = option("rect_prisms", 'r', "Number of rectangular prisms to be rendered.") = 0;
	
	int shape_palette = option("shape_palette", 'x', "Color palette for background.") = PRIDE_FLAG_PALETTE;

	float phi_spacing = option("phi_spacing") = 0.9;
	float theta_spacing = option("theta_spacing") = 0.9;
	float prism_spacing = option("prism_spacing") = 0.9;

	int light_smoothness = option("light_smoothness") = -1;

	int bg_palette = option("bg_palette", 'y', "Color palette for background.") = PRIDE_FLAG_PALETTE;

	bool ignore_config = option("ignore_config", 'i');

};

// Things to do tomorrow:

// Add support for config files

// Add support for keystrokes

int main(int argc, char** argv) {
	srand(time(0)); // set rand() seed
	
	RenderingArgs render_args{{argc, argv}};

	Config wava_cfg;
	//wava_cfg.setOptions(Config::OptionAllowScientificNotation); // only works in 1.7

	try {
		std::string path = std::string(getenv("HOME")) + std::string("/.config/wava/wava.cfg");
		wava_cfg.readFile(path.c_str());
	}
	catch(const FileIOException &fioex) {
		std::cerr << "Error occurred while reading config file, it may be missing. Try reinstalling program." << std::endl;
		exit(-1);
	}
	catch(const ParseException &pex) {
		std::cerr << "Error occurred while parsing config gile." << std::endl;
		exit(-1);
	}

	// main loop	
	while (true) {

		// Load config values
		if (!render_args.ignore_config) {
			try {
				render_args.phi_spacing = wava_cfg.lookup("rendering.phi_spacing");
				render_args.theta_spacing = wava_cfg.lookup("rendering.theta_spacing");
				render_args.prism_spacing = wava_cfg.lookup("rendering.prism_spacing");
				render_args.light_smoothness = wava_cfg.lookup("rendering.light_smoothness");
				render_args.bg_palette = wava_cfg.lookup("rendering.bg_palette");

				render_args.donut_count = wava_cfg.lookup("shapes.donut_count");
				render_args.sphere_count = wava_cfg.lookup("shapes.sphere_count");
				render_args.rect_prism_count = wava_cfg.lookup("shapes.rect_prism_count");
				render_args.shape_palette = wava_cfg.lookup("shapes.shape_palette");
			}
			catch(const SettingNotFoundException &nfex) {
				std::cerr << "Error occurred while doing lookup for setting." << std::endl;
				exit(-1);
			}
		}

		std::vector<Shape*> shapes = generate_rand_shapes(1, 0, wava_plan::freq_bands);


		struct audio_data audio;
		memset(&audio, 0, sizeof(audio));

		//audio.source = "temp" // temporary solution

		audio.format = -1; 
		audio.rate = 0;
		audio.samples_counter = 0;
		audio.channels = 2;

		audio.input_buffer_size = 512 * audio.channels;
		audio.wava_buffer_size = audio.input_buffer_size * 8;

		audio.wava_in = new double[audio.wava_buffer_size];

		audio.terminate = 0;

		pthread_t p_thread;

		// here is where we would have a switch for audio input
		// case INPUT_PULSE:

		//if (strcmp(audio.source, "auto") == 0) {
		get_pulse_default_sink((void*) &audio);
		//}
		// starting pulsemusic listener
		int thr_id = pthread_create(&p_thread, NULL, input_pulse, (void*) &audio);
		audio.rate = 44100;
		//break

		while (true) { // while (!reloadConf) 
			int screen_x = 40; 
			int screen_y = 40; // could be changed in next loop

			struct wava_plan plan(44100, 2, 0.77, 50, 10000);

			wava_screen screen(screen_x, screen_y, render_args.theta_spacing, render_args.phi_spacing, render_args.prism_spacing,
				render_args.light_smoothness, render_args.bg_palette);

			while (true) { // while (!resizeTerminal)
				pthread_mutex_lock(&audio.lock);
				std::vector<double> wava_out = wava_execute(audio.wava_in, audio.samples_counter, plan);

				if (audio.samples_counter > 0) audio.samples_counter = 0;
				pthread_mutex_unlock(&audio.lock);

				render_cli_frame(shapes, screen, wava_out);
				

				usleep(1000); // NEED this or some kind of delay to get results that make sense apparently
			}

		}

		for (int i = 0; i < shapes.size(); i++) delete shapes[i];
		delete [] audio.wava_in;
	}
	return 0;
}


