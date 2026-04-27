import requests
import logging
import difflib
import os
import re
from app.core.config import settings

logger = logging.getLogger(__name__)

class AudioAnalysisService:
    @staticmethod
    def analyze_audio(audio_source) -> str:
        """
        Sends WAV data (path or file-like object) to Whisper API and returns recognized text.
        Uses initial_prompt to improve recognition of trigger words.
        """
        try:
            url = f"{settings.WHISPER_API_URL}/asr"
            params = {
                "task": "transcribe",
                "output": "json",
                "initial_prompt": ", ".join(settings.HELP_KEYWORDS)
            }
            
            # Prepare file for multipart upload
            if isinstance(audio_source, str):
                f = open(audio_source, "rb")
                filename = os.path.basename(audio_source)
                should_close = True
            else:
                # Assume it's a file-like object (e.g. BytesIO)
                f = audio_source
                filename = "chunk.wav"
                should_close = False
                
            try:
                files = {"audio_file": (filename, f, "audio/wav")}
                # Increased timeout to 120s as Whisper on CPU can be slow
                response = requests.post(url, params=params, files=files, timeout=120)
            finally:
                if should_close:
                    f.close()
            
            if response.status_code == 200:
                result = response.json()
                return result.get("text", "").lower()
            else:
                logger.error(f"Whisper API error: {response.status_code} - {response.text}")
                return ""
        except Exception as e:
            logger.error(f"Failed to analyze audio with Whisper: {e}")
            return ""

    @staticmethod
    def contains_help_request(text: str) -> bool:
        """
        Checks if the recognized text contains any of the help keywords.
        Includes a "margin of error" (fuzzy matching) for better detection.
        """
        if not text:
            return False
        
        text_lower = text.lower()
        words = text_lower.split()
        
        for keyword in settings.HELP_KEYWORDS:
            kw = keyword.lower()
            
            # 1. Match full word/phrase boundaries to avoid false positives like
            # "kill" in "skill" or "fire" in "firewall".
            kw_pattern = r"\b" + re.escape(kw) + r"\b"
            if re.search(kw_pattern, text_lower):
                logger.info(f"Help keyword detected (word-boundary): '{keyword}' in text: '{text}'")
                return True
            
            # 2. Fuzzy match for each word in text (covers typos/misrecognition)
            # Threshold 0.8 means 80% similarity
            for word in words:
                # Remove punctuation from word
                clean_word = "".join(c for c in word if c.isalnum())
                if not clean_word:
                    continue
                    
                similarity = difflib.SequenceMatcher(None, kw, clean_word).ratio()
                if similarity >= 0.8:
                    logger.info(f"Help keyword detected (fuzzy, sim={similarity:.2f}): '{keyword}' matched '{clean_word}' in text: '{text}'")
                    return True
                    
        return False
