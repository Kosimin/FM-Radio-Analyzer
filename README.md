# FM-Radio-Analyzer
An Arduino-based FM radio receiver with real-time FFT audio analysis  and OLED display, built using the RDA5807M module and Arduino R4 Minima.


- The project is implemented by combining signal reception, processing, and display in a structured manner.
- First, A headphone wire is used as an antenna to capture FM radio signals from the environment.
- These signals are fed into the RDA5807M module, which is controlled by the Arduino through I²C communication to select the desired frequency.
- Once tuned, the module demodulates the FM signal and produces an audio output.
- This audio signal is then sent to the Arduino for further processing.
- The Arduino samples the signal and applies a 64-point Fast Fourier Transform (FFT) to identify the dominant frequency components present in the audio.
- The processed results, along with additional parameters such as the tuned frequency and signal strength (RSSI), are displayed on an OLED screen.
- A push button is included to allow the user to change the frequency, making the system interactive and easy to operate.
- Overall, the methodology focuses on capturing FM signals, converting them into usable audio data, analyzing the signal using digital processing techniques, and presenting the results in real time.
