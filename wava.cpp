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

#include "graphics.h"

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 8096

double fftw_data[BUFFER_SIZE] = {0};
int curr_fftw_data_size = 0;

std::mutex mtx;

struct fftw_analysis {
  float average_bass_value = 0;
};

fftw_analysis analyze_fftw_data() {
  if (curr_fftw_data_size <= 0) { return (fftw_analysis) {0}; }

  float highest_bass_frequency = 200;
  int index = (int) ((highest_bass_frequency/(SAMPLE_RATE/2)) * curr_fftw_data_size);

  float average_value = 0;

  for (int i = 0; i < index; ++i) {
    //printf ("inside loop, data value is: %f\n", fftw_data[i]);
    average_value+=fftw_data[i];
  }

  average_value/=index;

  //printf ("Average value is: %f", average_value);

  return (fftw_analysis) {average_value};
}

int access_fftw_buffer(bool write, double* data, int data_length /*only applicable for writes*/, fftw_analysis &analysis) {
  if (!(mtx.try_lock())) { // accessing failed, locked by other thread
    mtx.unlock();
    //printf ("locked\n");
    return -1; // failure code
  } 

  else {
    if(write) { // pa thread is trying to write
      if (data_length > BUFFER_SIZE) {
        printf ("ERROR!\n"); // should be irrelevant soon
        exit(-1);
      }
      //printf ("data length is: %d\n", data_length);
      for (int i = 0; i < data_length; i++) {
        fftw_data[i] = data[i];
        curr_fftw_data_size = data_length;
      }
      mtx.unlock();
      return 0;
    }
    else { // draw thread is trying to read
      analysis = analyze_fftw_data();
      mtx.unlock();
      return 0;
    }
  }
}

void pa_stream_notify_cb(pa_stream *stream, void* userdata)
{
    const pa_stream_state state = pa_stream_get_state(stream);
    switch (state) {
    case PA_STREAM_FAILED:
	std::cout << "Stream failed\n";
	break;
    case PA_STREAM_READY:
	std::cout << "Stream ready\n";
	break;
    default:
	std::cout << "Stream state: " << state << std::endl;
    }
}

void pa_stream_read_cb(pa_stream *stream, const size_t nbytes, void* userdata)
{
    // Careful when to pa_stream_peek() and pa_stream_drop()!
    // c.f. https://www.freedesktop.org/software/pulseaudio/doxygen/stream_8h.html#ac2838c449cde56e169224d7fe3d00824
    int16_t *data = nullptr;
    size_t actualbytes = 0;
    if (pa_stream_peek(stream, (const void**)&data, &actualbytes) != 0) {
        std::cerr << "Failed to peek at stream data\n";
        return;
    }

    if (data == nullptr && actualbytes == 0) {
	  // No data in the buffer, ignore.
        return;
    } else if (data == nullptr && actualbytes > 0) {
        // Hole in the buffer. We must drop it.
      if (pa_stream_drop(stream) != 0) {
          std::cerr << "Failed to drop a hole! (Sounds weird, doesn't it?)\n";
          return;
      }
    }

    // process data
    fftw_complex *out;
    fftw_plan p;

    double* in = (double*) malloc(sizeof(double) * actualbytes);

    for (int i = 0; i < actualbytes; ++i) in[i] = (double) data[i];
    
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * actualbytes);    
    p = fftw_plan_dft_r2c_1d(actualbytes, in, out, FFTW_ESTIMATE);

    fftw_execute(p);
    // process frequency data for this "frame"

    double largest_amplitude = .01;
    double* amplitude_data = (double*) malloc(sizeof(double) * actualbytes);

    for (int i = 0; i < (actualbytes/2) + 1; i++) {
      double cos_amp = out[i][0], sin_amp = out[i][1];

      amplitude_data[i] = sqrt(cos_amp*cos_amp + sin_amp*sin_amp);

      if (amplitude_data[i] > largest_amplitude) largest_amplitude = amplitude_data[i];
    }

    for (int i = 0; i < (actualbytes/2) + 1; i++) amplitude_data[i]/=largest_amplitude;

    //printf("here\n");

    fftw_analysis shut_the_fuck_up_gcc = (fftw_analysis) {0}; // <- this is HORRENDOUS PLEASE FIX THIS
    int err = access_fftw_buffer(true, amplitude_data, actualbytes, shut_the_fuck_up_gcc); // <- this is seriously flawed and needs work

    fftw_destroy_plan(p);
    free(in); fftw_free(out); free(amplitude_data);

    if (pa_stream_drop(stream) != 0) {
	    std::cerr << "Failed to drop data after peeking.\n";
    }
}

void pa_server_info_cb(pa_context *ctx, const pa_server_info *info, void* userdata)
{
    std::cout << "Default sink: " << info->default_sink_name << std::endl;

    pa_sample_spec spec;
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = 44100;
    spec.channels = 1;
    // Use pa_stream_new_with_proplist instead?
    pa_stream *stream = pa_stream_new(ctx, "output monitor", &spec, nullptr);

    pa_stream_set_state_callback(stream, &pa_stream_notify_cb, userdata);
    pa_stream_set_read_callback(stream, &pa_stream_read_cb, userdata);

    std::string monitor_name(info->default_sink_name);
    monitor_name += ".monitor";
    if (pa_stream_connect_record(stream, monitor_name.c_str(), nullptr, PA_STREAM_NOFLAGS) != 0) {
      std::cerr << "connection fail\n";
      return;
    }

    std::cout << "Connected to " << monitor_name << std::endl;
}

void pa_context_notify_cb(pa_context *ctx, void* userdata)
{
    const pa_context_state state = pa_context_get_state(ctx);
    switch (state) {
    case PA_CONTEXT_READY:
	std::cout << "Context ready\n";
	pa_context_get_server_info(ctx, &pa_server_info_cb, userdata);
	break;
    case PA_CONTEXT_FAILED:
	std::cout << "Context failed\n";
	break;
    default:
	std::cout << "Context state: " << state << std::endl;
    }
}

int draw () {
  float A = 0, B = 0;
  float speed = 0.004;
  int time = 0;

  //main drawing loop

  fftw_analysis analysis;

  while (true)
  {
    if (time == (int) 1/speed) { A = 0; B = 0; time = 0; }
    
    int err = access_fftw_buffer(false, NULL, 0, analysis);
    
    if (err == 0) {
      //printf ("Average bass value: %f\n", analysis.average_bass_value);
    }

    render_frame (A, B, 2 + (0.5 * analysis.average_bass_value * analysis.average_bass_value));
    usleep (1000);
    A += 3.14 * 2 * speed; 
    B += 3.14 * 2 * speed;
    time += 1;
  }
}

int main(int argc, char **argv)
{

  void* userdata;

  pa_mainloop *loop = pa_mainloop_new();
  pa_mainloop_api *api = pa_mainloop_get_api(loop);
  pa_context *ctx = pa_context_new(api, "padump");
  pa_context_set_state_callback(ctx, &pa_context_notify_cb, userdata);
  if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
    std::cerr << "PA connection failed.\n";
    return -1;
  }

  // pa_stream_disconnect(..)
  std::thread t1(draw);
  std::thread t2(pa_mainloop_run, loop, nullptr);

  t1.join();

  // gotta fix the segfault that occurs here

  pa_context_disconnect(ctx);
  pa_mainloop_free(loop);

  return 0;
}


