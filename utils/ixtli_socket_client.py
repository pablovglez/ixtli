"""
Requirerments:
- OpenCV
- Numpy
- PyAudio
- Icecream

Instructions:
- Install portaudio19-dev: sudo apt install portaudio19-dev
- Install the required packages: pip install icecream numpy pyaudio opencv-python
- Set the ESP32_IP environment variable to the real ESP32's IP address or set it to 'DEFAULT_IP'

"""

import socket
import threading
import time
import cv2
import numpy as np
import pyaudio
import re
import os
from icecream import ic

# --- Configuration ---
DEFAULT_IP = '192.168.12.185'
ESP32_IP = os.environ.get('ESP32_IP', DEFAULT_IP)
VIDEO_PORT = 16385
AUDIO_PORT = 16386
ENABLE_AUDIO = False  # Set to False to disable audio playback

# --- Audio Configuration ---
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 16000
CHUNK = 1024


def video_stream_task():
    """
    Connects to the video socket, receives and parses JPEG frames, and displays them.
    """
    while True:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                ic(f"[Video] Connecting to {ESP32_IP}:{VIDEO_PORT}...")
                s.connect((ESP32_IP, VIDEO_PORT))
                ic("[Video] Connected. Receiving video stream...")

                buffer = b''
                while True:
                    # Read data from the socket
                    data = s.recv(4096)
                    if not data:
                        ic("[Video] Stream ended.")
                        break
                    buffer += data

                    # Search for the header and find content length
                    header_match = re.search(b'Content-Length: (\\d+)\\r\\n\\r\\n', buffer)
                    if header_match:
                        content_length = int(header_match.group(1))
                        header_end = header_match.end()

                        # Check if we have the full frame in the buffer
                        if len(buffer) >= header_end + content_length:
                            # Extract the JPEG frame data
                            frame_data = buffer[header_end: header_end + content_length]

                            # Remove the processed frame and header from the buffer
                            buffer = buffer[header_end + content_length:]

                            try:
                                # Decode and display the frame
                                frame = cv2.imdecode(np.frombuffer(frame_data, dtype=np.uint8), cv2.IMREAD_COLOR)
                                if frame is not None:
                                    cv2.imshow('ESP32 Video Stream', frame)
                                else:
                                    ic("[Video] Failed to decode frame.")
                            except Exception as e:
                                ic(f"[Video] Error decoding frame: {e}")

                    # Press 'q' to exit the video window
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

        except ConnectionRefusedError:
            ic("[Video] Connection refused. Retrying in 5 seconds...")
            time.sleep(5)
        except Exception as e:
            ic(f"[Video] An error occurred: {e}. Retrying in 5 seconds...")
            time.sleep(5)

    cv2.destroyAllWindows()
    ic("[Video] Stream stopped.")


def audio_stream_task():
    """
    Connects to the audio socket, receives raw audio, and plays it.
    """
    p = pyaudio.PyAudio()
    stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, output=True, frames_per_buffer=CHUNK)

    while True:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                ic(f"[Audio] Connecting to {ESP32_IP}:{AUDIO_PORT}...")
                s.connect((ESP32_IP, AUDIO_PORT))
                ic("[Audio] Connected. Receiving audio stream...")

                while True:
                    data = s.recv(CHUNK)
                    if not data:
                        break
                    stream.write(data)

        except ConnectionRefusedError:
            ic("[Audio] Connection refused. Retrying in 5 seconds...")
            time.sleep(5)
        except Exception as e:
            ic(f"[Audio] An error occurred: {e}. Retrying in 5 seconds...")
            time.sleep(5)

    stream.stop_stream()
    stream.close()
    p.terminate()
    ic("[Audio] Stream stopped.")


if __name__ == "__main__":
    if ESP32_IP == 'YOUR_ESP32_IP':
        ic("Please update the ESP32_IP variable with your device's IP address.")
    else:
        # Create and start threads
        video_thread = threading.Thread(target=video_stream_task)
        video_thread.start()

        if ENABLE_AUDIO:
            audio_thread = threading.Thread(target=audio_stream_task)
            audio_thread.start()
            audio_thread.join()

        video_thread.join()
        ic("Client has shut down.")
