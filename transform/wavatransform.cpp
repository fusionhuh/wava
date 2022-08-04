#include "wavatransform.h"

//#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

double wava_plan::band_lower_cutoff_freq[15] = {50, 190, 300, 430/*A*/, 456/*A#*/, 483/*B*/, 513/*C*/, 544/*C#*/, 578/*D*/, 612/*D#*/, 649/*E*/, 688/*F*/, 729/*F#*/, 773 /*G*/, 821/*G#*/};
double wava_plan::band_upper_cutoff_freq[15] = {60, 200, 310,450,      476,       503,      533,      564,       598,      632,       669,      708,      749,       793,       841};
double wava_plan::band_octaves_count[15] = { 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };

double wava_plan::freq_bin_size = 0;
double wava_plan::calibration = 0;

int wava_plan::freq_bands = 15;

wava_plan::~wava_plan() {
	fftw_free(l_sample_data);
	fftw_free(l_sample_data_raw);	
	fftw_free(l_output_data);

	if (audio_channels == 2) {
		fftw_free(r_sample_data);
		fftw_free(r_sample_data_raw);
		fftw_free(r_output_data);
	}

	delete [] input_buffer;
}

double* allocate_initialized_fftw_real(int size) {
	double* pointer = fftw_alloc_real(size); memset(pointer, 0, size);
	return pointer;
}	

fftw_complex* allocate_initialized_fftw_complex(int size) {
	fftw_complex* pointer = fftw_alloc_complex(size); memset(pointer, 0, size);
	return pointer;
}

struct wava_plan wava_init(unsigned int rate, int channels, double noise_reduction, int low_cut_off, int high_cut_off) {
	struct wava_plan plan;

	// error checks placed here

	plan.rate = rate;
	//plan.freq_bin_size = (plan.rate/2)/(8192/2);
	wava_plan::freq_bin_size = (plan.rate/2)/(8192/2);

	wava_plan::calibration = 1.0775;

	plan.audio_channels = channels;
	plan.rate = rate;

	plan.noise_reduction = noise_reduction;

	plan.input_buffer_size = 8192 * plan.audio_channels; 

	// Hann Window multipliers
	for (int i = 0; i < 8192; i++) {
		plan.hann_multiplier[i] = 0.5 * (1 - cos(2*PI * i / (plan.input_buffer_size-1)));	
	}

	// Allocate + initialize buffers
	plan.input_buffer = new double[plan.input_buffer_size]();

	plan.l_sample_data_raw = allocate_initialized_fftw_real(8192);
    	plan.l_sample_data = allocate_initialized_fftw_real(8192);
    	plan.l_output_data = allocate_initialized_fftw_complex(8192/2 + 1);	

    plan.plan_left = fftw_plan_dft_r2c_1d(8192, plan.l_sample_data, plan.l_output_data, FFTW_MEASURE);

	if (plan.audio_channels == 2) { // two channels
		plan.r_sample_data_raw = allocate_initialized_fftw_real(8192);
		plan.r_sample_data = allocate_initialized_fftw_real(8192);
		plan.r_output_data = allocate_initialized_fftw_complex(8192/2 + 1);
		
		plan.plan_right = fftw_plan_dft_r2c_1d(8192, plan.r_sample_data, plan.r_output_data, FFTW_MEASURE);
	}

	// then some band processing stuff?

	return plan;
}

double calculate_band_value(int freq_band, fftw_complex* output_data) {
	double inverse = 1.0l/8192.0l;
	double inverse_calibration = 1/wava_plan::calibration;

	int octaves = wava_plan::band_octaves_count[freq_band];

	double band_lower_cutoff = wava_plan::band_lower_cutoff_freq[freq_band] * 0.5 * inverse_calibration;
	double band_upper_cutoff = wava_plan::band_upper_cutoff_freq[freq_band] * 0.5 * inverse_calibration;
	double center_frequency = (band_lower_cutoff + band_upper_cutoff)/2;

	int start_index = (band_lower_cutoff / wava_plan::freq_bin_size);
	int end_index = (band_upper_cutoff / wava_plan::freq_bin_size);

	double curr_band_sum = 0;

	for (int a = 0; a < octaves + 1; a++) {
		for (int b = start_index; b <= end_index; b++) {
			double magnitude = hypot(output_data[b][0], output_data[b][1])*inverse; // normalize by sample count

			if (a >= 3) {
				double domain_span = band_upper_cutoff - band_lower_cutoff;
				double curr_frequency = (wava_plan::freq_bin_size * b) - band_lower_cutoff;

				double normalized_x = (curr_frequency / domain_span) - 0.5;

				magnitude *= (1 - (abs(normalized_x * 1.8)) * (abs(normalized_x * 1.8))); // (1 - (abs(x))^2)
			}
			curr_band_sum += (magnitude > 50) ? (magnitude - 50) * 1.5 : 0; 

			//curr_band_sum_l += magnitude;
			//if (magnitude > 200) printf("curr_magnitude is: %f\n", magnitude);
		}
		center_frequency *= 2;
		band_lower_cutoff = center_frequency * 2 - 5;
		band_upper_cutoff = center_frequency * 2 + 5;
	}
	return curr_band_sum/(band_upper_cutoff - band_lower_cutoff);
}

std::vector<double> wava_execute(double* wava_in, int new_samples, struct wava_plan &plan) {
	
	// do not overflow
	if (new_samples > plan.input_buffer_size) {
		new_samples = plan.input_buffer_size;
	}

	bool silence = true; // plays a role in sensitivity adjustment
	if (new_samples > 0) {
		// something framerate related
		//
		//
		// shifting input buffer
		for (uint16_t n = plan.input_buffer_size - 1; n >= new_samples; n--) {
			plan.input_buffer[n] = plan.input_buffer[n - new_samples];
		}

		// fill the input buffer
		for (uint16_t n = 0; n < new_samples; n++) {
			plan.input_buffer[new_samples - n - 1] = wava_in[n];
			if (wava_in[n]) {
				silence = false;
			}
		}
	}
	else {
		// frame skip or smth? might be relevant later
	}

	// fill the input buffers
	for (uint16_t n = 0; n < 8192; n++) {
		if (plan.audio_channels == 2) {
			plan.l_sample_data_raw[n] = plan.input_buffer[n*2];
			plan.r_sample_data_raw[n] = plan.input_buffer[n*2 + 1];
		}
		else {
			plan.l_sample_data_raw[n] = plan.input_buffer[n];
		}
	}

	// Actually applying Hann Window
	for (int i = 0; i < 8192; i++) {
		plan.l_sample_data[i] = plan.hann_multiplier[i] * plan.l_sample_data_raw[i]; 
		if (plan.audio_channels == 2) plan.r_sample_data[i] = plan.hann_multiplier[i] * plan.r_sample_data_raw[i];
	}

	// Execute FFT
	fftw_execute(plan.plan_left);
	if (plan.audio_channels == 2) fftw_execute(plan.plan_right);

	std::vector<double> wava_out = std::vector<double>(plan.freq_bands);

	double inverse = 1.0l/8192.0l;
	double inverse_calibration = 1.0l/1.0775;

	for (int a = 0; a < plan.freq_bands; a++) {
		if (plan.audio_channels == 1) {
			wava_out[a] = calculate_band_value(a, plan.l_output_data);
		} 
		else {
			wava_out[a] = (calculate_band_value(a, plan.l_output_data) + calculate_band_value(a, plan.r_output_data))/2;
		}

		if (wava_out[a] < wava_out[a]) {
		}

		plan.prev_wava_out[a] = wava_out[a];
	}
	int greatest_freq = 0;

	for (int i = 0; i < 8192/2 + 1; i++) {
		if (hypot(plan.l_output_data[i][0], plan.l_output_data[i][1]) > hypot(plan.l_output_data[greatest_freq][0], plan.l_output_data[greatest_freq][0])) {
			greatest_freq = i;
		}
	}

	printf("largest frequency component: %f  amplitude is: %f\n", greatest_freq * plan.freq_bin_size * 1.0775, hypot(plan.l_output_data[greatest_freq][0], plan.l_output_data[greatest_freq][1])/44100); // 1.0775 is a calibration constant

	printf("magnitude of C band: %f\n", wava_out[6]);

	printf("\x1b[H");


	return wava_out;
}
