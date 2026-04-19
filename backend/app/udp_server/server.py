import socket
import threading
import logging
from app.udp_server.handlers import PacketHandler

logger = logging.getLogger(__name__)

class UDPServer:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.server_socket = None
        self._is_running = False

    def start(self):
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            
            # Tune OS/Socket buffers
            # Recommendation for Linux sysctl:
            # sysctl -w net.core.rmem_max=4194304
            # sysctl -w net.core.rmem_default=1048576
            try:
                self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4194304)
                actual_buf = self.server_socket.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
                logger.info(f"UDP socket receive buffer set to {actual_buf} bytes")
                if actual_buf < 4194304:
                    logger.warning("Socket buffer size is less than requested 4MB. Consider tuning OS: sysctl -w net.core.rmem_max=4194304")
            except Exception as e:
                logger.warning(f"Could not set UDP SO_RCVBUF: {e}")

            self.server_socket.bind((self.host, self.port))
            self._is_running = True
            logger.info(f"UDP Server started and listening on {self.host}:{self.port}")

            handler = PacketHandler()

            while self._is_running:
                try:
                    self.server_socket.settimeout(1.0)
                    try:
                        data, client_address = self.server_socket.recvfrom(4096)
                        if data:
                            logger.debug(f"Received {len(data)} bytes from {client_address}")
                            handler.handle(data, client_address)
                    except socket.timeout:
                        handler._cleanup_sessions()
                except socket.error as e:
                    if self._is_running:
                        logger.error(f"Socket error during recvfrom: {e}")
                    break
        except Exception as e:
            logger.error(f"Failed to start UDP Server: {e}")
        finally:
            self.stop()

    def stop(self):
        self._is_running = False
        if self.server_socket:
            self.server_socket.close()
            self.server_socket = None
            logger.info("UDP Server stopped")

def start_udp_server_thread(host: str, port: int):
    server = UDPServer(host, port)
    thread = threading.Thread(target=server.start, daemon=True)
    thread.start()
    return server
