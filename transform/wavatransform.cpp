#include "wavatransform.h"

//#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

double* allocate_initialized_fftw_real(int size) {
	double* pointer = fftw_alloc_real(size); memset(pointer, 0, size);
	return pointer;
}	

fftw_complex* allocate_initialized_fftw_complex(int size) {
	fftw_complex* pointer = fftw_alloc_complex(size); memset(pointer, 0, size);
	return pointer;
}

struct wava_plan* wava_init(int frequency_bands, unsigned int rate, int channels,
	double noise_reduction, int low_cut_off, int high_cut_off) {
	struct wava_plan* plan = new wava_plan;

	// error checks placed here

	plan->rate = rate;
	plan->freq_bin_size = (plan->rate/2) / ( (8192/2) - 1);

	plan->frequency_bands = frequency_bands;
	plan->audio_channels = channels;
	plan->rate = rate;

	plan->noise_reduction = noise_reduction;

	plan->input_buffer_size = 8192 * plan->audio_channels; 

	// Hann Window multipliers
	plan->hann_multiplier = new double[8192];
	for (int i = 0; i < 8192; i++) {
		plan->hann_multiplier[i] = 0.5 * (1 - cos(2*PI * i / (plan->input_buffer_size-1)));	
	}

	// Allocate + initialize buffers
	plan->input_buffer = new double[plan->input_buffer_size]();

	plan->l_sample_data_raw = allocate_initialized_fftw_real(8192);
    	plan->l_sample_data = allocate_initialized_fftw_real(8192);
    	plan->l_output_data = allocate_initialized_fftw_complex(8192);	

    plan->plan_left = fftw_plan_dft_r2c_1d(8192, plan->l_sample_data, plan->l_output_data, FFTW_MEASURE);

	if (plan->audio_channels == 2) { // two channels
		plan->r_sample_data_raw = allocate_initialized_fftw_real(8192);
		plan->r_sample_data = allocate_initialized_fftw_real(8192);
		plan->r_output_data = allocate_initialized_fftw_complex(8192);
		
		plan->plan_right = fftw_plan_dft_r2c_1d(8192, plan->r_sample_data, plan->r_output_data, FFTW_MEASURE);
	}

	// then some band processing stuff?

	return plan;
}

void wava_execute(double* wava_in, int new_samples, double* wava_out, struct wava_plan* plan) {
	
	// do not overflow
	if (new_samples > plan->input_buffer_size) {
		new_samples = plan->input_buffer_size;
	}

	bool silence = true; // not entirely sure what function this plays
	if (new_samples > 0) {
		// something framerate related
		//
		//
		// shifting input buffer
		for (uint16_t n = plan->input_buffer_size - 1; n >= new_samples; n--) {
			plan->input_buffer[n] = plan->input_buffer[n - new_samples];
		}

		// fill the input buffer
		for (uint16_t n = 0; n < new_samples; n++) {
			plan->input_buffer[new_samples - n - 1] = wava_in[n];
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
		if (plan->audio_channels == 2) {
			plan->l_sample_data_raw[n] = plan->input_buffer[n*2];
			plan->r_sample_data_raw[n] = plan->input_buffer[n*2 + 1];
		}
		else {
			plan->l_sample_data_raw[n] = plan->input_buffer[n];
		}
	}

	// Actually applying Hann Window
	for (int i = 0; i < 8192; i++) {
		plan->l_sample_data[i] = plan->hann_multiplier[i] * plan->l_sample_data_raw[i]; 
		if (plan->audio_channels == 2) plan->r_sample_data[i] = plan->hann_multiplier[i] * plan->r_sample_data_raw[i];
	}

	// Execute FFT
	fftw_execute(plan->plan_left);
	if (plan->audio_channels == 2) fftw_execute(plan->plan_right);

	for (int a = 0; a < plan->frequency_bands; a++) {
		int band_lower_cutoff = plan->band_lower_cutoff_freq[a];
		int band_upper_cutoff = plan->band_upper_cutoff_freq[a];

		// just going to handle left ("main") channel for now
		int start_index = band_lower_cutoff / plan->freq_bin_size;
		int end_index = band_upper_cutoff / plan->freq_bin_size;

		double curr_band_sum_l = 0;

		for (int b = start_index; b <= end_index; b++) {
			curr_band_sum_l += hypot(plan->l_output_data[b][0], plan->l_output_data[b][1]);	
		}

		wava_out[a] = curr_band_sum_l/((band_upper_cutoff - band_lower_cutoff)*plan->rate); 
	}
}

void wava_destroy(struct wava_plan* plan) {
	fftw_free(plan->l_sample_data);
	fftw_free(plan->l_sample_data_raw);	
	fftw_free(plan->l_output_data);

	if (plan->audio_channels == 2) {
		fftw_free(plan->r_sample_data);
		fftw_free(plan->r_sample_data_raw);
		fftw_free(plan->r_output_data);
	}

	delete [] (plan->hann_multiplier);
	delete plan;
}
