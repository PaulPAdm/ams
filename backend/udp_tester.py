import socket
import time
import struct
import random

def send_test_audio(device_id, seq, samples, host='127.0.0.1', port=5000):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Format: [device_id][0x00][seq_le_2b][audio samples 320b]
    # seq is 2 bytes Little Endian
    header = device_id.encode('utf-8') + b'\x00' + struct.pack('<H', seq)
    packet = header + samples
    
    sock.sendto(packet, (host, port))
    sock.close()

def simulate_stream(device_id, num_packets=100, loss_rate=0.05, jitter_range=(0, 0.02)):
    print(f"Starting simulation for {device_id} with {loss_rate*100}% loss and {jitter_range[1]*1000}ms max jitter")
    
    # Generate fake 10ms PCM frame (sine wave)
    sample_rate = 16000
    freq = 440
    t = [i/sample_rate for i in range(160)]
    samples = [int(10000 * 0.5 * (1.0 + 1.0)) for i in range(160)] # Dummy data
    # Real sine wave
    import math
    pcm_data = b''
    for i in range(160):
        val = int(16383 * math.sin(2 * math.pi * freq * i / sample_rate))
        pcm_data += struct.pack('<h', val)

    for i in range(num_packets):
        if random.random() < loss_rate:
            print(f"Simulating loss of packet {i}")
            continue
        
        # Simulate jitter
        jitter = random.uniform(jitter_range[0], jitter_range[1])
        if jitter > 0:
            time.sleep(jitter)
            
        send_test_audio(device_id, i % 65536, pcm_data)
        
        # Standard interval is 10ms
        time.sleep(0.01)

if __name__ == "__main__":
    device_id = "test_device_jitter"
    simulate_stream(device_id, num_packets=200, loss_rate=0.05, jitter_range=(0, 0.015))
    print("Sent 200 packets. Wait for timeout (5s) to see diagnostics in logs.")
