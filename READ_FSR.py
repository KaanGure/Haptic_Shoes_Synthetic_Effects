import pyaudio
import wave
import analyse
import numpy
import os
import time


# Open audio in channel and return stream
def openStream():
    CHUNK = 1024
    FORMAT = pyaudio.paInt16
    CHANNELS = 2
    RATE = 11025#44100
    RECORD_SECONDS = 5
    WAVE_OUTPUT_FILENAME = "output.wav"

    p = pyaudio.PyAudio()

    stream = p.open(format=FORMAT,
                    channels=CHANNELS,
                    input_device_index = 1,
                    rate=RATE,
                    input=True)
                    #frames_per_buffer=CHUNK)

    print("* recording")
    return stream


# Read signal from stream and return loudness of signal
def getFSRLoudness(stream):
    rawsamps = stream.read(1024, exception_on_overflow=False)
    samps = numpy.fromstring(rawsamps, dtype=numpy.int16)
    return analyse.loudness(samps)


if __name__ == "__main__":
    stream = openStream()

    while True:
        loudness = getFSRLoudness(stream)

        print("FSR: %f [dB]" % loudness)
        time.sleep(0.1)
