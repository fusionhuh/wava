#include <math.h>
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

#include <fftw3.h>

#include "input/common.h"
#include "input/pulse.h"

#include "transform/wavatransform.h"

#include "output/graphics.h"
#include "output/cli.h"

// PRIORITIES AND NOTES AT THE MOMENT:

// Might want to look into monstercat filter?

int main(int argc, char **argv)
{
	srand(time(0)); // set rand() seed
	int time = 0;

	// main loop	
	while (true) {
		printf ("wava.cpp flag 1\n");
		// LOAD CONFIG VALUES
		std::vector<Shape*> shapes = generate_rand_shapes(1, 0, wava_plan::freq_bands);

		wava_screen::light_smoothness = 8;
		// config values end

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
		printf("here 1\n");
		// starting pulsemusic listener
		int thr_id = pthread_create(&p_thread, NULL, input_pulse, (void*) &audio);
		printf("here 2\n");
		audio.rate = 44100;
		//break

		while (true) { // while (!reloadConf) 
			int curr_screen_x = 40; 
			int curr_screen_y = 40; // could be changed in next loop

			struct wava_plan plan = wava_init(44100, 2, 0.77, 50, 10000);

			wava_screen screen(curr_screen_x, curr_screen_y);

			printf("here!!\n");
			while (true) { // while (!resizeTerminal)
				pthread_mutex_lock(&audio.lock);
				std::vector<double> wava_out = wava_execute(audio.wava_in, audio.samples_counter, plan);
				normalize_vector(wava_out);

				if (audio.samples_counter > 0) audio.samples_counter = 0;
				pthread_mutex_unlock(&audio.lock);
				render_cli_frame(shapes, screen, wava_out);
				//printf("base is: %f mid is: %f treble is: %f\n", wava_out[0] * 1.25, wava_out[1], wava_out[2]);
				usleep(3000);
				time+=1;        
			}	
		}

		for (int i = 0; i < shapes.size(); i++) delete shapes[i];
	}
	return 0;
}


