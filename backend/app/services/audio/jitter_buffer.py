import collections
import logging
import time
import struct
import numpy as np

logger = logging.getLogger(__name__)

class JitterBuffer:
    """
    Adaptive jitter buffer for PCM audio.
    Format: mono, s16le, 16 kHz.
    Frame: 10 ms (160 samples, 320 bytes).
    
    Parameters:
    - start_delay: 80-120 ms
    - target_delay: 60-100 ms
    - reorder_window: 20-30 ms
    - max_hold: 200 ms
    """
    def __init__(self, start_delay_ms=100, target_delay_ms=80, reorder_window_ms=30, max_hold_ms=200):
        self.frame_size_ms = 10
        self.sample_rate = 16000
        self.samples_per_frame = 160
        self.bytes_per_sample = 2
        self.frame_bytes = self.samples_per_frame * self.bytes_per_sample # 320 bytes
        
        self.start_delay_frames = start_delay_ms // self.frame_size_ms
        self.target_delay_frames = target_delay_ms // self.frame_size_ms
        self.reorder_window_frames = reorder_window_ms // self.frame_size_ms
        self.max_hold_frames = max_hold_ms // self.frame_size_ms
        
        self.buffer = {} # seq -> frame_data
        self.last_played_seq = -1
        self.is_started = False
        
        self.last_frame_data = b'\x00' * self.frame_bytes
        self.plc_count = 0
        
        # Diagnostics
        self.stats = {
            "received": 0,
            "lost": 0,
            "reordered": 0,
            "underrun": 0,
            "overrun": 0,
            "jitters": [], # To calculate p50/p95
            "last_arrival_time": None,
            "last_seq": -1
        }

    def push(self, seq, data):
        """Push a frame with a sequence number."""
        now = time.time()
        self.stats["received"] += 1
        
        # Calculate jitter (inter-arrival time variation)
        if self.stats["last_arrival_time"] is not None:
            arrival_delta = (now - self.stats["last_arrival_time"]) * 1000 # ms
            # Sequence difference
            seq_delta = (seq - self.stats["last_seq"]) % 65536
            if seq_delta > 32768: seq_delta -= 65536 # Handle wrap around
            
            # Expected delta is seq_delta * 10ms
            jitter = abs(arrival_delta - seq_delta * self.frame_size_ms)
            self.stats["jitters"].append(jitter)
            if len(self.stats["jitters"]) > 1000:
                self.stats["jitters"].pop(0)
            
            # Detect out-of-order
            if seq_delta < 0:
                self.stats["reordered"] += 1

        self.stats["last_arrival_time"] = now
        self.stats["last_seq"] = seq

        # Check if the packet is too late
        if self.is_started:
            diff = (seq - self.last_played_seq) % 65536
            if 32768 < diff: # seq is in the past
                logger.debug(f"Late packet seq {seq} (last played {self.last_played_seq}), dropping.")
                return

        if seq in self.buffer:
            return # Duplicate
            
        if len(self.buffer) >= self.max_hold_frames:
            # Overrun, drop oldest
            oldest_seq = min(self.buffer.keys())
            del self.buffer[oldest_seq]
            self.stats["overrun"] += 1

        self.buffer[seq] = data
        
        if not self.is_started and len(self.buffer) >= self.start_delay_frames:
            self.is_started = True
            # Find the smallest seq in the buffer to start from
            self.last_played_seq = min(self.buffer.keys()) - 1
            logger.info(f"Jitter buffer started/resynced for device (seq start {self.last_played_seq+1}) with {len(self.buffer)} frames.")

    def pop(self):
        """Pop the next frame in sequence. Returns frame_data."""
        if not self.is_started:
            return None # Buffering...

        next_seq = (self.last_played_seq + 1) % 65536
        
        if next_seq in self.buffer:
            data = self.buffer.pop(next_seq)
            self.last_played_seq = next_seq
            self.last_frame_data = data
            self.plc_count = 0
            return data
        else:
            # Check if any later packets are in the buffer (gap detection)
            # has_later = any((s - next_seq) % 65536 < 32768 for s in self.buffer.keys())
            
            # Optimization: check if buffer is not empty. 
            # If it's not empty, it MUST have some 's' such that (s - next_seq) % 65536 is small.
            # If it's empty, we might need to resync if it stays empty for too long.
            
            if not self.buffer:
                # Buffer is empty. If we've been doing PLC for a while, maybe we should stop and wait for buffering again.
                if self.plc_count > self.max_hold_frames: # Use max_hold as a threshold for resync (200ms)
                    logger.info("Jitter buffer underrun for too long, resetting to buffering state.")
                    self.is_started = False
                    # IMPORTANT: Reset last_played_seq so next push doesn't think it's late
                    self.last_played_seq = -1
                    self.plc_count = 0
                    return None
            
            # Underrun occurs if the buffer is empty or if we have a gap
            self.stats["underrun"] += 1
            
            # Gap detection: if there's a packet in buffer that is "ahead" but not too far
            has_later = any((s - next_seq) % 65536 < 100 for s in self.buffer.keys())
            if has_later:
                self.stats["lost"] += 1
                self.last_played_seq = next_seq # Jump over the gap
            
            # PLC Logic: repeat last frame with attenuation
            self.plc_count += 1
            attenuation = max(0.0, 1.0 - (self.plc_count * 0.2)) # 5 frames (50ms) to silence
            
            if attenuation > 0:
                # Apply attenuation to last_frame_data
                samples = np.frombuffer(self.last_frame_data, dtype=np.int16).astype(np.float32)
                samples *= attenuation
                plc_data = samples.astype(np.int16).tobytes()
                return plc_data
            else:
                return b'\x00' * self.frame_bytes

    def get_diagnostics(self):
        jitters = self.stats["jitters"]
        p50 = np.percentile(jitters, 50) if jitters else 0
        p95 = np.percentile(jitters, 95) if jitters else 0
        
        expected = self.stats["received"] + self.stats["lost"]
        loss_pct = (self.stats["lost"] / max(1, expected)) * 100
        
        return {
            "received": self.stats["received"],
            "lost": self.stats["lost"],
            "loss_pct": round(loss_pct, 2),
            "reordered": self.stats["reordered"],
            "underrun": self.stats["underrun"],
            "overrun": self.stats["overrun"],
            "jitter_p50": round(p50, 2),
            "jitter_p95": round(p95, 2),
            "buffer_size": len(self.buffer),
            "end_to_end_est_ms": len(self.buffer) * self.frame_size_ms + (jitters[-1] if jitters else 0)
        }
