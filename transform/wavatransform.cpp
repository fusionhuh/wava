#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wavatransform.h>


double wava_plan::band_lower_cutoff_freq[15] = {50, 190, 300, 435/*A*/, 461/*A#*/, 488/*B*/, 518/*C*/, 549/*C#*/, 583/*D*/, 617/*D#*/, 654/*E*/, 693/*F*/, 734/*F#*/, 778 /*G*/, 826/*G#*/};
double wava_plan::band_upper_cutoff_freq[15] = {180, 200, 310,445,      471,       498,      528,      559,       593,      627,       664,      703,      744,       788,       836};
double wava_plan::band_harmonics_count[15] = { 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };

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
	wava_plan::freq_bin_size = (plan.rate/2)/(8192 + 1);

	wava_plan::calibration = 1.0775;

	plan.audio_channels = channels;
	plan.rate = rate;

	plan.noise_reduction = noise_reduction;

	plan.input_buffer_size = 8192 * plan.audio_channels; 

	// Hann Window multipliers
	for (int i = 0; i < 8192; i++) {
		plan.hann_multiplier[i] = 0.5 * (1 - cos(2*PI * i / (8192-1)));	
	}

	// Allocate + initialize buffers
	plan.input_buffer = new double[plan.input_buffer_size]();

	plan.l_sample_data_raw = allocate_initialized_fftw_real(16384);
	plan.l_sample_data = allocate_initialized_fftw_real(16384);
	plan.l_output_data = allocate_initialized_fftw_complex(8192 + 1);
	plan.l_output_data_magnitude = std::vector<double>(8192 + 1);	

    plan.plan_left = fftw_plan_dft_r2c_1d(16384, plan.l_sample_data, plan.l_output_data, FFTW_MEASURE);

	if (plan.audio_channels == 2) { // two channels
		plan.r_sample_data_raw = allocate_initialized_fftw_real(16384);
		plan.r_sample_data = allocate_initialized_fftw_real(16384);
		plan.r_output_data = allocate_initialized_fftw_complex(8192 + 1);
		plan.r_output_data_magnitude = std::vector<double>(8192 + 1);

		plan.plan_right = fftw_plan_dft_r2c_1d(16384, plan.r_sample_data, plan.r_output_data, FFTW_MEASURE);
	}

	// then some band processing stuff?

	return plan;
}

double calculate_band_value(int freq_band, const std::vector<double>& magnitudes) {
	double inverse = 1.0l/16384.0l;

	int harmonics = wava_plan::band_harmonics_count[freq_band];

	double band_lower_cutoff = wava_plan::band_lower_cutoff_freq[freq_band];
	double band_upper_cutoff = wava_plan::band_upper_cutoff_freq[freq_band];
	double center_freq = (band_lower_cutoff + band_upper_cutoff)/2;

	int start_index = (band_lower_cutoff / wava_plan::freq_bin_size);
	int end_index = (band_upper_cutoff / wava_plan::freq_bin_size);

	double curr_band_sum = 0;

	for (int a = 0; a < harmonics + 1; a++) {
		for (int b = start_index; b <= end_index; b++) {
			if (a >= 3) {
				double domain_span = band_upper_cutoff - band_lower_cutoff;
				double curr_freq = (wava_plan::freq_bin_size * b) - band_lower_cutoff;

				double normalized_x = (curr_freq / domain_span) - 0.5;

				double curr_magnitude = magnitudes[b] * (1 - (abs(normalized_x * 1.8)) * (abs(normalized_x * 1.8))); // (1 - (abs(x))^2)

				curr_band_sum += (curr_magnitude > 70) ? (curr_magnitude - 70) : 0;
			}
			else {

				curr_band_sum += magnitudes[b] * inverse > 70 ? magnitudes[b] * inverse - 70 : 0;
			}
			//if (magnitude > 200) printf("curr_magnitude is: %f\n", magnitude);
		}
		center_freq += center_freq/(a + 1);
		band_lower_cutoff = center_freq * 2 - 10;
		band_upper_cutoff = center_freq * 2 + 10;
	}
	return curr_band_sum/((band_upper_cutoff - band_lower_cutoff));
}

std::vector<double> downsample(const std::vector<double>& magnitudes, int harmonic) {
	std::vector<double> downsampled_magnitudes(magnitudes.size()/harmonic);

	for (int i = 0; i < downsampled_magnitudes.size(); i++) {
		downsampled_magnitudes[i] = magnitudes[i * harmonic];
	}

	return downsampled_magnitudes;
}

std::vector<double> calculate_hps(const std::vector<double>& magnitudes, int harmonics) {
	int hps_output_size = (magnitudes.size()/2); // 3 is  the temp number of harmonics for now
	std::vector<double> hps_output(hps_output_size);

	std::vector<double> hps2 = downsample(magnitudes, 2);
	std::vector<double> hps3 = downsample(magnitudes, 3);
	std::vector<double> hps4 = downsample(magnitudes, 4);

	for (int i = 0; i < hps_output_size; i++) {
		hps_output[i] = magnitudes[i] * hps2[i];
	}

	return hps_output;
}

int adjust_freq_to_note(double freq) {
	int note = -1;
	if (freq > 0) {
		while (freq > wava_plan::band_upper_cutoff_freq[14]) { freq *= 0.5; } // bring within note band ranges
		while (freq < wava_plan::band_lower_cutoff_freq[3]) { freq *= 2; }
	}

	for (int i = 3; i < 14; i++) {
		if (freq < wava_plan::band_upper_cutoff_freq[i] && freq > wava_plan::band_lower_cutoff_freq[i]) {
			note = i;
			break;
		}

	}
	return note;
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
	/*for (int i = 0; i < 16384; i++) {
		plan.input_buffer[i] = wava_in[i];
	}*/

	// Fill the input buffers
    // only fill up half of sample arrays because the other half is padded
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
	// again, only apply window to half of sample data because of padding
	for (int i = 0; i < 8192; i++) {
		plan.l_sample_data[i] = plan.hann_multiplier[i] * plan.l_sample_data_raw[i]; 
		if (plan.audio_channels == 2) plan.r_sample_data[i] = plan.hann_multiplier[i] * plan.r_sample_data_raw[i];
	}

	for (int i = 0; i < 8192; i++) {
		plan.l_sample_data[i + 8192] = 0;
	}

	// Execute FFT
	fftw_execute(plan.plan_left);
	//if (plan.audio_channels == 2) fftw_execute(plan.plan_right);

	std::vector<double> wava_out(plan.freq_bands);

	for (int i = 0; i < (8192 + 1); i++) {
		plan.l_output_data_magnitude[i] = hypot(plan.l_output_data[i][0], plan.l_output_data[i][1]);
		//if (plan.audio_channels == 2) plan.r_output_data_magnitude[i] = hypot(plan.r_output_data[i][0], plan.r_output_data[i][1]);
	}

	std::vector<double> hps_output = calculate_hps(plan.l_output_data_magnitude, 3);

	/*int max_freq_index = -1;
	double max_magnitude = -1;
	for (int i = 0; i < hps_output.size(); i++) {
		if (hps_output[i] > max_magnitude) {
			max_freq_index = i;
			max_magnitude = hps_output[i];
		}
	}

	// clear out values at and around largest frequency magnitude
	hps_output[max_freq_index] = 0;
	if (max_freq_index > 0) hps_output[max_freq_index - 1] = 0;
	if (max_freq_index < hps_output.size() - 1) hps_output[max_freq_index + 1] = 0;

	double max_freq = ((float) (44100/2)/(hps_output.size())) * max_freq_index;	

	if (max_freq < 150 && max_freq > 20) {
		wava_out[0] = max_magnitude;
	}

	int note = adjust_freq_to_note(max_freq);
	if (note != -1) wava_out[note] = max_magnitude;

	for (int i = 0, notes = 3; (i < hps_output.size() && notes > 0); i++) {
		if (hps_output[i] > 0.85 * max_magnitude) {
			int new_note = adjust_freq_to_note( ((float) ((44100/2)/(hps_output.size()))) * i ); 

			//printf("new note: %d", new_note);
			if (new_note != -1) {
				wava_out[new_note] = hps_output[i]; 
				hps_output[i] = 0;
				if (i > 0) hps_output[i - 1] = 0;
				if (i < hps_output.size() - 1) hps_output[i + 1] = 0;

				notes--;

				printf("note found: %d", note);
			}

			// clear area out values at and around new peak

		}
	}*/

	for (int i = 0; i < hps_output.size(); i++) {
		if (hps_output[i] > 20000000000) {
			int note = adjust_freq_to_note( ((double) ((44100/2)/(hps_output.size()))) * i );
			if (note != -1) {
				if (hps_output[i] > wava_out[note] * 9000000000000) wava_out[note] = hps_output[i] * 1/9000000000000;
			}
		}
	}
	for (int i = 0; i < 10; i++) {
		printf("freq band %d: %f\n", i, wava_out[i]);
	}
	printf("\x1b[H");
	//printf("max magnitude is: %f\n", max_magnitude);
	//wava_out[0] = calculate_band_value(1, plan.l_output_data_magnitude);

	// Normalize output vector
	/*double magnitude = 0;
	for (int i = 0; i < wava_out.size(); i++) {
		magnitude += wava_out[i] * wava_out[i];
	}
	magnitude = sqrt(magnitude);*/
	for (int i = 0; i < wava_out.size(); i++) {
		//if (wava_out[i] > 0 && magnitude > 0) wava_out[i] /= magnitude;
		//if (wava_out[i] < plan.prev_wava_out[i] && (plan.prev_wava_out[i] - wava_out[i]) > 0.05) wava_out[i] = plan.prev_wava_out[i] - 0.05;
		plan.prev_wava_out[i] = wava_out[i];
	}
	

	//printf("largest frequency component computed with hps: %f\n", max_freq); // 1.0775 is a calibration constant

	//printf("estimated note: %d000000000\n", note);
	//printf("magnitude of C band: %f\n", wava_out[6]);

	// printf("\x1b[H");


	return wava_out;
}
