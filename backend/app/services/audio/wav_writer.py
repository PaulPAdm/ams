import wave
import os
import threading
import time
import io
from datetime import datetime
import logging
from app.core.config import settings

logger = logging.getLogger(__name__)

class WavWriter:
    def __init__(self, sample_rate=None, channels=None, bit_depth=None, storage_path=None):
        self.sample_rate = sample_rate or settings.AUDIO_SAMPLE_RATE
        self.channels = channels or settings.AUDIO_CHANNELS
        self.bit_depth = bit_depth or settings.AUDIO_BIT_DEPTH
        self.storage_path = storage_path or settings.AUDIO_STORAGE_PATH
        
        if not os.path.exists(self.storage_path):
            os.makedirs(self.storage_path, exist_ok=True)

    def create_file_name(self, device_id):
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        safe_id = str(device_id).replace('.', '_')
        return os.path.join(self.storage_path, f"record_{safe_id}_{timestamp}.wav")

    def save_raw_to_wav(self, raw_data, file_path):
        try:
            with wave.open(file_path, 'wb') as wav_file:
                wav_file.setnchannels(self.channels)
                wav_file.setsampwidth(self.bit_depth // 8)
                wav_file.setframerate(self.sample_rate)
                wav_file.writeframes(raw_data)
            logger.info(f"Saved {len(raw_data)} bytes to {file_path}")
        except Exception as e:
            logger.error(f"Error saving WAV file {file_path}: {e}")

class AudioStreamProcessor:
    def __init__(self, device_id):
        self.device_id = device_id
        self.writer = WavWriter()
        self.file_path = self.writer.create_file_name(device_id)
        self.wav_file = None
        self._is_open = False
        self.frames_count = 0
        self.last_analysis_time = time.time()
        self.analysis_buffer = bytearray()
        self._lock = threading.Lock()

    def open(self):
        try:
            self.wav_file = wave.open(self.file_path, 'wb')
            self.wav_file.setnchannels(self.writer.channels)
            self.wav_file.setsampwidth(self.writer.bit_depth // 8)
            self.wav_file.setframerate(self.writer.sample_rate)
            self._is_open = True
            self.frames_count = 0
            self.last_analysis_time = time.time()
            with self._lock:
                self.analysis_buffer = bytearray()
            
            logger.info(f"Started recording to {self.file_path} (Sample Rate: {self.writer.sample_rate})")
        except Exception as e:
            logger.error(f"Failed to open WAV file for writing: {e}")

    def write(self, data):
        """Write data to the WAV file and internal analysis buffer (sliding window)."""
        if self._is_open:
            try:
                self.wav_file.writeframes(data)
                
                with self._lock:
                    # Update sliding window buffer
                    self.analysis_buffer.extend(data)
                    
                    # Keep only last N seconds based on settings
                    bytes_per_sec = self.writer.sample_rate * (self.writer.bit_depth // 8) * self.writer.channels
                    max_bytes = bytes_per_sec * settings.AUDIO_ANALYSIS_WINDOW_SEC
                    
                    if len(self.analysis_buffer) > max_bytes:
                        # Keep only last max_bytes
                        del self.analysis_buffer[:-max_bytes]
                
                bytes_per_frame = (self.writer.bit_depth // 8) * self.writer.channels
                self.frames_count += len(data) // bytes_per_frame
            except Exception as e:
                logger.error(f"Error writing frames: {e}")

    def get_current_duration(self):
        return self.frames_count / self.writer.sample_rate

    def get_analysis_chunk(self):
        """
        Returns the current sliding window buffer as an in-memory WAV file (io.BytesIO).
        Does not clear the buffer to maintain the sliding window for next checks.
        """
        with self._lock:
            if len(self.analysis_buffer) == 0:
                return None
            # Copy data to avoid "Existing exports of data" error during writeframes
            buffer_copy = bytes(self.analysis_buffer)

        # Create in-memory WAV file
        bio = io.BytesIO()
        try:
            with wave.open(bio, 'wb') as chunk_file:
                chunk_file.setnchannels(self.writer.channels)
                chunk_file.setsampwidth(self.writer.bit_depth // 8)
                chunk_file.setframerate(self.writer.sample_rate)
                chunk_file.writeframes(buffer_copy)
            
            # Position for reading from the beginning
            bio.seek(0)
            return bio
        except Exception as e:
            logger.error(f"Error creating in-memory analysis chunk: {e}")
            return None

    def close(self):
        if self.wav_file:
            try:
                self.wav_file.close()
                logger.info(f"Closed recording file {self.file_path}")
            except Exception as e:
                logger.error(f"Error closing WAV file {self.file_path}: {e}")
            finally:
                self.wav_file = None
                self._is_open = False
                with self._lock:
                    self.analysis_buffer = bytearray()
