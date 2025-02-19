# Tilin Audio Visualizer

Tilin Audio Visualizer is a real-time audio visualizer application developed using SDL3 and FFTW3. It processes audio input and displays a colorful frequency spectrum visualization. This project is built as a Windows-only application and is a testament to advanced audio processing, multithreading, and real-time graphics rendering in C.

## Features

- **Real-Time Audio Processing:** Captures and processes live audio using SDL3.
- **FFT Analysis:** Utilizes FFTW3 to compute the frequency spectrum from the audio data.
- **Spectrum Visualization:** Renders the frequency spectrum with a dynamic, rainbow color mapping.
- **Multithreading:** Employs a separate audio processing thread for continuous data analysis.
- **Modern CMake Build:** Uses a CMake build system for configuration and cross-module integration.

## Requirements

- **Platform:** Windows only.
- **Dependencies:**
  - [SDL3](https://www.libsdl.org/)
  - [FFTW3](http://www.fftw.org/)
- **Compiler:** A C compiler that supports C11.
- **Build System:** CMake (version 3.20 or higher)

## Getting Started

### 1. Clone the Repository

Clone this repository to your local machine:
bash
git clone https://[github.com/TilinSolutions/Tilin_Audio_Visualizer](https://github.com/TilinSolutions/Tilin-Audio-Visualizer)
cd Tilin_Audio_Visualizer

### 2. Build the Application

Create a build directory, run CMake to configure the project, and build it:

bash
mkdir build
cd build
cmake ..
cmake --build .

### 3. Run the Application

Run the executable:

bash
./Tilin_Audio_Visualizer.exe

**Note:** Ensure that you run this on a Windows system and that the SDL3 and FFTW3 libraries are properly installed.

## Code Overview

- **src/main.c**

  - Initializes SDL (video, audio, events) and sets up a hardware-accelerated renderer.
  - Opens the audio device and sets an audio callback for real-time audio processing.
  - Processes audio data using FFTW3 to compute the frequency spectrum.
  - Renders a visualization of the audio spectrum using a dynamic rainbow color mapping.

- **lib/CMakeLists.txt**
  - Defines project dependencies on SDL3 and FFTW3.
  - Configures the build process via CMake, ensuring portability and ease of integration.

## Authors & Credits

This project is developed by **Tilin Solutions** – a team composed of:

- **xuyaxaki**
- **Akoza**
- **Peppy**

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgements

- Thanks to the SDL and FFTW communities for their high-quality, open-source libraries that made this project possible.
- Special thanks to all contributors, mentors, and supporters who provided feedback and guidance.

---

_This project is Windows-only and has been tailored specifically for the Windows environment._
