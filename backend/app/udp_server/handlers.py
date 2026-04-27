import logging
import time
import os
import threading
from app.services.audio.wav_writer import AudioStreamProcessor
from app.services.audio.analysis import AudioAnalysisService
from app.db.session import SessionLocal
from app.models.audio_record import AudioRecord
from app.models.suspicious_incident import SuspiciousIncident
from app.models.location import Location
from app.core.config import settings

logger = logging.getLogger(__name__)

class PacketHandler:
    def __init__(self, session_timeout=5):
        self.sessions = {} # device_id -> (processor, last_seen)
        self.session_timeout = session_timeout

    def _save_audio_record(self, device_id, file_path):
        """Saves or updates audio record record in the database."""
        db = SessionLocal()
        try:
            # Check if this file already has a record
            existing = db.query(AudioRecord).filter(AudioRecord.file_path == file_path).first()
            if not existing:
                db_record = AudioRecord(
                    device_id=device_id,
                    file_path=file_path
                )
                db.add(db_record)
                db.commit()
                logger.info(f"Saved initial audio record to DB for device {device_id}")
        except Exception as e:
            logger.error(f"Failed to save audio record to DB for {device_id}: {e}")
            db.rollback()
        finally:
            db.close()

    def _cleanup_sessions(self):
        now = time.time()
        to_remove = []
        for device_id, session_data in self.sessions.items():
            processor, last_seen = session_data
            if now - last_seen > self.session_timeout:
                logger.info(f"Session timeout for {device_id}, closing WAV file.")
                # Perform final analysis before closing
                self._analyze_session_chunk(processor)
                processor.close()
                
                # Ensure the record is saved one last time (though it should have been saved on start)
                self._save_audio_record(device_id, processor.file_path)
                
                to_remove.append(device_id)
        for device_id in to_remove:
            del self.sessions[device_id]

    def _analyze_session_chunk(self, processor):
        audio_chunk = processor.get_analysis_chunk()
        if not audio_chunk:
            return

        try:
            logger.info(f"Analyzing in-memory audio chunk for device {processor.device_id}...")
            text = AudioAnalysisService.analyze_audio(audio_chunk)
            logger.info(f"Recognized text for device {processor.device_id}: '{text}'")
            if AudioAnalysisService.contains_help_request(text):
                logger.warning(f"HELP REQUEST DETECTED from device {processor.device_id}: {text}")
                self._create_suspicious_incident(processor.device_id, text)
        except Exception as e:
            logger.error(f"Error during audio analysis for device {processor.device_id}: {e}")
        finally:
            if audio_chunk and hasattr(audio_chunk, 'close'):
                audio_chunk.close()

    def _create_suspicious_incident(self, device_id, description):
        db = SessionLocal()
        try:
            # Get device location
            location = db.query(Location).filter(Location.id == device_id).first()
            if location:
                incident = SuspiciousIncident(
                    description=description,
                    geom=location.geom
                )
                db.add(incident)
                db.commit()
                logger.info(f"Created suspicious incident record for device {device_id}")
            else:
                logger.warning(f"Could not find location for device {device_id}. Creating suspicious incident without geometry.")
                incident = SuspiciousIncident(
                    description=description,
                    geom=None
                )
                db.add(incident)
                db.commit()
                logger.info(f"Created suspicious incident record (no location) for device {device_id}")
        except Exception as e:
            logger.error(f"Failed to create suspicious incident: {e}")
            db.rollback()
        finally:
            db.close()

    def handle(self, data, client_address):
        try:
            # Protocol: [device_id][0x00][PCM]
            null_byte_index = data.find(b'\x00')
            if null_byte_index == -1:
                logger.warning(f"Invalid packet format from {client_address}: no null byte separator.")
                return
            
            device_id = data[:null_byte_index].decode('utf-8')
            payload = data[null_byte_index + 1:]
            
            if not payload:
                logger.debug(f"Received empty payload from device {device_id} ({client_address})")
                return

            # Session management
            if device_id not in self.sessions:
                logger.info(f"New UDP audio stream for device {device_id} from {client_address}")
                processor = AudioStreamProcessor(device_id)
                processor.open()
                if not processor.is_open:
                    logger.error(f"Unable to start audio stream processor for device {device_id}")
                    return
                self.sessions[device_id] = [processor, time.time()]
                # Save record to DB immediately so it shows up in recordings list
                self._save_audio_record(device_id, processor.file_path)
            else:
                self.sessions[device_id][1] = time.time()

            processor = self.sessions[device_id][0]
            
            # Parsing: check for sequence number (2 bytes) at the beginning of payload
            # Standard frame is 320 bytes PCM. 322 bytes means 2 bytes seq + 320 bytes PCM.
            # Even if we don't use seq for jitter buffer, we need to skip it to get clean PCM.
            if len(payload) == 322:
                audio_samples = payload[2:]
            else:
                audio_samples = payload

            processor.write(audio_samples)

            # Periodic analysis check
            current_time = time.time()
            if current_time - processor.last_analysis_time >= settings.AUDIO_ANALYSIS_INTERVAL_SEC:
                # Update time BEFORE starting thread to prevent multiple threads for same window
                processor.last_analysis_time = current_time
                # Run analysis in a separate thread to not block UDP reception
                threading.Thread(target=self._analyze_session_chunk, args=(processor,), daemon=True).start()
            
            # Always check for timeouts to ensure records are saved even if no new packets arrive for some time
            self._cleanup_sessions()
            
        except Exception as e:
            logger.error(f"Error handling UDP packet from {client_address}: {e}")
