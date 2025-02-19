#include <SDL3/SDL.h>
#include <fftw3.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

// Add this fallback definition in case _USE_MATH_DEFINES doesn't work
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define FFT_SIZE 4096
#define BINS (FFT_SIZE/2 + 1)

// Shared state structure updated for SDL3
typedef struct {
    SDL_AudioDeviceID audio_device;
    float audio_buffer[FFT_SIZE];
    SDL_Mutex* audio_mutex;  // Updated type name for SDL3
    bool running;
    SDL_Window* window;
    SDL_Renderer* renderer;
} AppState;

// Audio callback prototype
void audio_callback(void* userdata, Uint8* stream, int len);

// Helper function for HSL to RGB conversion (stub implementation)
void HSLtoRGB(float h, float s, float l, Uint8* r, Uint8* g, Uint8* b) {
    // Simple conversion algorithm (placeholder) example:
    float c = (1 - fabs(2 * l / 100 - 1)) * (s / 100);
    float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
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

bool initialize_sdl(AppState* state) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return false;
    }

    // In SDL3 the window API now takes only width and height (no x/y)
    SDL_Window* window = SDL_CreateWindow(
        "Audio Visualizer",
        1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    // Create hardware-accelerated renderer.
    // NOTE: In SDL3 the renderer creation API does not take an index parameter.
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        NULL
    );

    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Store window and renderer in our state
    state->window = window;
    state->renderer = renderer;

    // Update audio device setup
    SDL_AudioSpec desired_spec = {
        .freq = SAMPLE_RATE,
        .format = SDL_AUDIO_S16,
        .channels = 1
    };

    // Update audio device open call for SDL3
    state->audio_device = SDL_OpenAudioDevice(
        NULL,  // Default device
        &desired_spec
    );

    if (state->audio_device == 0) {
        fprintf(stderr, "Audio device open failed: %s\n", SDL_GetError());
        return false;
    }

    // Set the callback after opening the device
    SDL_SetAudioCallback(state->audio_device, audio_callback, state);

    // Verify actual audio format
    SDL_AudioSpec obtained_spec;
    if (SDL_GetAudioDeviceSpec(state->audio_device, false, &obtained_spec) != 0) {
        fprintf(stderr, "Warning: Got unexpected audio format\n");
    }

    // Create mutex for audio buffer access
    state->audio_mutex = SDL_CreateMutex();
    if (!state->audio_mutex) {
        fprintf(stderr, "Mutex creation failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void audio_callback(void* userdata, Uint8* stream, int len) {
    AppState* state = (AppState*)userdata;
    const int samples = len / sizeof(int16_t);
    int16_t* audio_data = (int16_t*)stream;

    SDL_LockMutex(state->audio_mutex);
    
    // Convert to floating point [-1, 1] and apply Hann window
    for (int i = 0; i < samples; i++) {
        float sample = audio_data[i] / 32768.0f;
        float window = 0.5f * (1 - cosf(2 * M_PI * i / (samples - 1)));
        state->audio_buffer[i] = sample * window;
    }

    SDL_UnlockMutex(state->audio_mutex);
}

void process_audio(AppState* state, fftwf_complex* fft_out) {
    static fftwf_plan plan = NULL;
    
    if (!plan) {
        plan = fftwf_plan_dft_r2c_1d(
            FFT_SIZE,
            state->audio_buffer,
            fft_out,
            FFTW_MEASURE
        );
    }

    SDL_LockMutex(state->audio_mutex);
    fftwf_execute(plan);
    SDL_UnlockMutex(state->audio_mutex);
}

void render_spectrum(SDL_Renderer* renderer, fftwf_complex* fft_out) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    const float bin_width = (float)w / BINS;
    const float max_height = h * 0.8f;

    for (int i = 0; i < BINS; i++) {
        // Calculate magnitude (log scale)
        float magnitude = sqrtf(
            fft_out[i][0] * fft_out[i][0] + 
            fft_out[i][1] * fft_out[i][1]
        );
        float db = 10 * log10f(magnitude + 1e-6f);
        
        // Normalize and scale
        float height = fmaxf(0, (db + 80) / 80 * max_height);
        
        // Rainbow color mapping
        float hue = (float)i / BINS * 360;
        Uint8 r, g, b;
        HSLtoRGB(hue, 100, 50, &r, &g, &b);
        
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_Rect bar = {
            .x = i * bin_width,
            .y = h - height,
            .w = (int)(bin_width - 2),
            .h = (int)height
        };
        SDL_RenderFillRect(renderer, &bar);
    }
    
    SDL_RenderPresent(renderer);
}

int audio_thread(void* data) {
    AppState* state = (AppState*)data;
    // This thread can process continuous audio if needed.
    while (state->running) {
        fftwf_complex* dummy_fft = fftwf_malloc(sizeof(fftwf_complex) * BINS);
        process_audio(state, dummy_fft);
        fftwf_free(dummy_fft);
        SDL_Delay(FFT_SIZE * 1000 / SAMPLE_RATE); // Process at audio rate
    }
    return 0;
}

int main(int argc, char* argv[]) {
    AppState state = {0};
    state.running = true;

    if (!initialize_sdl(&state)) return 1;

    fftwf_complex* fft_out = fftwf_malloc(sizeof(fftwf_complex) * BINS);

    // Update audio device open call
    SDL_PlayAudioDevice(state.audio_device);  // Changed from SDL_PauseAudioDevice

    // Optionally start audio processing thread
    SDL_Thread* processor = SDL_CreateThread(audio_thread, "AudioProcessor", &state);

    // Main loop
    SDL_Event event;
    while (state.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                state.running = false;
            }
        }

        process_audio(&state, fft_out);
        render_spectrum(state.renderer, fft_out);
        
        // Maintain 60 FPS
        SDL_Delay(1000 / 60);
    }

    // Cleanup
    fftwf_free(fft_out);
    SDL_DestroyMutex(state.audio_mutex);
    SDL_CloseAudioDevice(state.audio_device);

    SDL_WaitThread(processor, NULL);

    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
    return 0;
}