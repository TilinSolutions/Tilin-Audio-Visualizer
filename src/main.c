#include <windows.h>         // Windows-specific header (keep intact)
#include <SDL3/SDL.h>
#include <fftw3.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fallback definition if M_PI is not defined.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Audio and FFT parameters.
#define SAMPLE_RATE 44100
#define FFT_SIZE 4096
#define BINS (FFT_SIZE/2 + 1)
#define FFT_DELAY_MS (FFT_SIZE * 1000 / SAMPLE_RATE)  // Delay (milliseconds) for processing thread

// Global FFTW plan so that we only create it once.
static fftwf_plan g_fft_plan = NULL;

/*
    AppState structure holds shared state for video rendering,
    audio processing, and thread synchronization.
*/
typedef struct {
    SDL_AudioDeviceID audio_device;
    
    // Ring buffer to store incoming audio samples.
    float audio_buffer[FFT_SIZE];
    int audio_buffer_index;   // Next write position in the ring buffer.
    SDL_Mutex* audio_mutex;   // Protects audio_buffer

    // FFT processing buffers.
    float* fft_input;         // Contiguous FFT input window.
    fftwf_complex* fft_output; // Result of FFT.
    SDL_Mutex* fft_mutex;      // Protects fft_output

    // SDL window and renderer for visualization.
    SDL_Window* window;
    SDL_Renderer* renderer;
    
    // Application state flag.
    bool running;
} AppState;

/*
    HSLtoRGB: Simple conversion from HSL to RGB values.
    Used for our colorful "rainbow" spectrum display.
*/
void HSLtoRGB(float h, float s, float l, Uint8* r, Uint8* g, Uint8* b) {
    float c = (1 - fabsf(2 * l / 100 - 1)) * (s / 100);
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = l / 100 - c / 2;
    float r1, g1, b1;
    if (h < 60) {
        r1 = c; g1 = x; b1 = 0;
    } else if (h < 120) {
        r1 = x; g1 = c; b1 = 0;
    } else if (h < 180) {
        r1 = 0; g1 = c; b1 = x;
    } else if (h < 240) {
        r1 = 0; g1 = x; b1 = c;
    } else if (h < 300) {
        r1 = x; g1 = 0; b1 = c;
    } else {
        r1 = c; g1 = 0; b1 = x;
    }
    *r = (Uint8)((r1 + m) * 255);
    *g = (Uint8)((g1 + m) * 255);
    *b = (Uint8)((b1 + m) * 255);
}

/*
    initialize_sdl: Initializes SDL (video, audio, and events), creates a window,
    a hardware-accelerated renderer, and opens an audio device.

    Returns true if everything initializes correctly; otherwise false.
*/
bool initialize_sdl(AppState* state) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return false;
    }
    
    // Create a window (SDL3 now takes only width and height for positioning).
    state->window = SDL_CreateWindow("Audio Visualizer", 1024, 768,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!state->window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Create a hardware-accelerated renderer.
    state->renderer = SDL_CreateRenderer(state->window, NULL);
    if (!state->renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Setup the desired audio specification.
    SDL_AudioSpec desired_spec = {0};
    desired_spec.freq = SAMPLE_RATE;
    desired_spec.format = SDL_AUDIO_S16;
    desired_spec.channels = 1;
    // Note: In SDL3, the callback will be attached after opening the device.
    
    state->audio_device = SDL_OpenAudioDevice(NULL, &desired_spec);
    if (state->audio_device == 0) {
        fprintf(stderr, "Audio device open failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Create a mutex to protect the audio ring buffer.
    state->audio_mutex = SDL_CreateMutex();
    if (!state->audio_mutex) {
        fprintf(stderr, "Audio mutex creation failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Initialize the ring buffer index.
    state->audio_buffer_index = 0;
    
    // Continue running.
    state->running = true;
    return true;
}

/*
    audio_callback: Registered as the SDL audio callback.
    Converts 16-bit signed audio data to floats and writes them into a circular buffer.
*/
void audio_callback(void* userdata, Uint8* stream, int len) {
    AppState* state = (AppState*)userdata;
    int samples = len / sizeof(int16_t);
    int16_t* audio_data = (int16_t*)stream;
    
    SDL_LockMutex(state->audio_mutex);
    for (int i = 0; i < samples; i++) {
        float sample = audio_data[i] / 32768.0f;
        state->audio_buffer[state->audio_buffer_index] = sample;
        state->audio_buffer_index = (state->audio_buffer_index + 1) % FFT_SIZE;
    }
    SDL_UnlockMutex(state->audio_mutex);
}

/*
    process_audio: Constructs a contiguous FFT window from the ring buffer,
    applies a Hann window to the signal, and executes the FFT.
*/
void process_audio(AppState* state) {
    // Build a contiguous FFT input window from the circular ring buffer.
    SDL_LockMutex(state->audio_mutex);
    int idx = state->audio_buffer_index;
    for (int i = 0; i < FFT_SIZE; i++) {
        state->fft_input[i] = state->audio_buffer[(idx + i) % FFT_SIZE];
    }
    SDL_UnlockMutex(state->audio_mutex);
    
    // Apply a Hann window to smooth the edges and reduce leakage.
    for (int i = 0; i < FFT_SIZE; i++) {
        float hann = 0.5f * (1 - cosf(2 * M_PI * i / (FFT_SIZE - 1)));
        state->fft_input[i] *= hann;
    }
    
    // Create the FFTW plan on the first call.
    if (g_fft_plan == NULL) {
        g_fft_plan = fftwf_plan_dft_r2c_1d(FFT_SIZE, state->fft_input,
                                             state->fft_output, FFTW_MEASURE);
    }
    
    // Execute the FFT, and protect the FFT output with a mutex.
    SDL_LockMutex(state->fft_mutex);
    fftwf_execute(g_fft_plan);
    SDL_UnlockMutex(state->fft_mutex);
}

/*
    render_spectrum: Renders the frequency spectrum as vertical, rainbow-colored bars.
*/
void render_spectrum(SDL_Renderer* renderer, fftwf_complex* fft_data) {
    // Clear the renderer with a black background.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    int win_w, win_h;
    SDL_GetRendererOutputSize(renderer, &win_w, &win_h);
    
    const float bin_width = (float)win_w / BINS;
    const float max_bar_height = win_h * 0.8f;
    
    for (int i = 0; i < BINS; i++) {
        float real = fft_data[i][0];
        float imag = fft_data[i][1];
        float magnitude = sqrtf(real * real + imag * imag);
        float db = 10 * log10f(magnitude + 1e-6f);
        
        float bar_height = fmaxf(0, (db + 80) / 80 * max_bar_height);
        
        // Map the current bin to a hue value for rainbow coloring.
        float hue = ((float)i / BINS) * 360;
        Uint8 r, g, b;
        HSLtoRGB(hue, 100, 50, &r, &g, &b);
        
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_Rect bar = {
            .x = (int)(i * bin_width),
            .y = win_h - (int)bar_height,
            .w = (int)(bin_width - 2),
            .h = (int)bar_height
        };
        SDL_RenderFillRect(renderer, &bar);
    }
    
    SDL_RenderPresent(renderer);
}

/*
    audio_processing_thread: Performs continuous FFT processing in a separate thread.
    This offloads computation from the main rendering loop.
*/
int audio_processing_thread(void* data) {
    AppState* state = (AppState*)data;
    while (state->running) {
        process_audio(state);
        SDL_Delay(FFT_DELAY_MS);
    }
    return 0;
}

/*
    cleanup: Releases all dynamically allocated resources and shuts down SDL.
*/
void cleanup(AppState* state) {
    if (state->fft_input) {
        free(state->fft_input);
        state->fft_input = NULL;
    }
    if (state->fft_output) {
        fftwf_free(state->fft_output);
        state->fft_output = NULL;
    }
    if (state->fft_mutex) {
        SDL_DestroyMutex(state->fft_mutex);
        state->fft_mutex = NULL;
    }
    if (state->audio_mutex) {
        SDL_DestroyMutex(state->audio_mutex);
        state->audio_mutex = NULL;
    }
    if (state->renderer) {
        SDL_DestroyRenderer(state->renderer);
        state->renderer = NULL;
    }
    if (state->window) {
        SDL_DestroyWindow(state->window);
        state->window = NULL;
    }
    if (state->audio_device) {
        SDL_CloseAudioDevice(state->audio_device);
        state->audio_device = 0;
    }
    if (g_fft_plan) {
        fftwf_destroy_plan(g_fft_plan);
        g_fft_plan = NULL;
    }
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    AppState state = {0};
    
    // Initialize SDL and set up video, audio, and related resources.
    if (!initialize_sdl(&state)) {
        return EXIT_FAILURE;
    }
    
    // Set the audio callback (attach our audio_callback to the opened device).
    SDL_SetAudioCallback(state.audio_device, audio_callback, &state);
    
    // Allocate the FFT buffers.
    state.fft_input = (float*)malloc(sizeof(float) * FFT_SIZE);
    if (!state.fft_input) {
        fprintf(stderr, "Failed to allocate FFT input buffer.\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }
    state.fft_output = fftwf_malloc(sizeof(fftwf_complex) * BINS);
    if (!state.fft_output) {
        fprintf(stderr, "Failed to allocate FFT output buffer.\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Create a mutex to protect FFT output access.
    state.fft_mutex = SDL_CreateMutex();
    if (!state.fft_mutex) {
        fprintf(stderr, "FFT mutex creation failed: %s\n", SDL_GetError());
        cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Start playback so that the audio callback will be invoked.
    SDL_PlayAudioDevice(state.audio_device);
    
    // Create a separate thread for audio processing.
    SDL_Thread* audio_thread = SDL_CreateThread(audio_processing_thread, "AudioProcessor", &state);
    if (!audio_thread) {
        fprintf(stderr, "Failed to create audio processing thread: %s\n", SDL_GetError());
        cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Main loop: Process SDL events and render the frequency spectrum.
    SDL_Event event;
    while (state.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                state.running = false;
            }
        }
        
        // Safely copy the FFT output for rendering.
        fftwf_complex fft_snapshot[BINS];
        SDL_LockMutex(state.fft_mutex);
        memcpy(fft_snapshot, state.fft_output, sizeof(fftwf_complex) * BINS);
        SDL_UnlockMutex(state.fft_mutex);
        
        render_spectrum(state.renderer, fft_snapshot);
        SDL_Delay(1000 / 60); // Limit to roughly 60 FPS.
    }
    
    // Wait for the audio processing thread to exit.
    SDL_WaitThread(audio_thread, NULL);
    cleanup(&state);
    fftwf_cleanup();
    
    return EXIT_SUCCESS;
}
