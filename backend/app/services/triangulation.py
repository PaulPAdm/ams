import numpy as np
from scipy.optimize import least_squares
from pyproj import Transformer
from typing import List, Tuple
from sqlalchemy.orm import Session
from geoalchemy2.shape import from_shape, to_shape
from shapely.geometry import Point
import logging

from app import models

logger = logging.getLogger(__name__)

# Sound speed in m/s
V_SOUND = 343.0

WGS84_SRID = 4326
METRIC_SRID = 3857  # Metric projection used for triangulation math.

transformer_to_metric = Transformer.from_crs(f"EPSG:{WGS84_SRID}", f"EPSG:{METRIC_SRID}", always_xy=True)
transformer_to_wgs84 = Transformer.from_crs(f"EPSG:{METRIC_SRID}", f"EPSG:{WGS84_SRID}", always_xy=True)

def triangulate_tdoa(mic_coords: np.ndarray, arrival_times: np.ndarray) -> Tuple[float, float]:
    """
    Performs TDOA (Time Difference of Arrival) triangulation.
    mic_coords: numpy array shape (N, 2) in meters.
    arrival_times: numpy array shape (N,) in seconds (timestamp).
    Returns (x, y) in meters.
    """
    def equations(point, mic_coords, arrival_times):
        # Distances from the assumed point to the microphones
        distances = np.linalg.norm(mic_coords - point, axis=1)
        # Calculated time differences relative to the first microphone
        theoretical_tdoa = (distances - distances[0]) / V_SOUND
        # Actual time differences
        actual_tdoa = arrival_times - arrival_times[0]
        return theoretical_tdoa - actual_tdoa

    # Initial guess - mean of microphone coordinates
    initial_guess = np.mean(mic_coords, axis=0)
    
    res = least_squares(equations, initial_guess, args=(mic_coords, arrival_times))
    return res.x[0], res.x[1]

def process_recent_peaks(db: Session, window_seconds: float = 0.5):
    """
    Searches for groups of peaks that arrived in a narrow time window from different devices,
    and starts triangulation.
    """
    # 1. Get the latest unprocessed peaks.
    peaks = (
        db.query(models.Peak)
        .filter(models.Peak.processed.is_(False))
        .order_by(models.Peak.peak_time_ns.desc())
        .limit(50)
        .all()
    )
    if len(peaks) < 3:
        return

    # Group peaks into short-lived events starting from the freshest entries.
    processed_ids = set()
    
    for i in range(len(peaks)):
        if peaks[i].id in processed_ids:
            continue
            
        current_event_peaks = [peaks[i]]
        processed_ids.add(peaks[i].id)
        
        ref_time_ns = peaks[i].peak_time_ns
        
        for j in range(i + 1, len(peaks)):
            if peaks[j].id in processed_ids:
                continue
            
            diff_seconds = abs(peaks[j].peak_time_ns - ref_time_ns) / 1_000_000_000.0
            if diff_seconds <= window_seconds:
                # Check if there is already a peak from this device in the current event
                if any(p.device_id == peaks[j].device_id for p in current_event_peaks):
                    continue
                
                current_event_peaks.append(peaks[j])
                processed_ids.add(peaks[j].id)
        
        if len(current_event_peaks) >= 3:
            try:
                perform_triangulation_for_event(db, current_event_peaks)
            except Exception as e:
                logger.error("Error during triangulation: %s", e)
            finally:
                for peak in current_event_peaks:
                    peak.processed = True
                db.commit()
        else:
            # Not enough devices in this time window — mark as processed so they
            # don't accumulate and slow down future triangulation queries.
            for peak in current_event_peaks:
                peak.processed = True
            db.commit()

def perform_triangulation_for_event(db: Session, event_peaks: List[models.Peak]):
    mic_coords_metric = []
    arrival_times = []
    
    for peak in event_peaks:
        device = db.query(models.Location).filter(models.Location.id == peak.device_id).first()
        if not device or not device.geom:
            continue
            
        # Get coordinates from PostGIS
        point = to_shape(device.geom)
        # Convert to meters
        mx, my = transformer_to_metric.transform(point.x, point.y)
        mic_coords_metric.append([mx, my])
        # Time in seconds
        arrival_times.append(peak.peak_time_ns / 1_000_000_000.0)
        
    if len(mic_coords_metric) < 3:
        return
        
    mic_coords_metric = np.array(mic_coords_metric)
    arrival_times = np.array(arrival_times)
    
    # Calculate
    res_x, res_y = triangulate_tdoa(mic_coords_metric, arrival_times)
    
    # Convert back to WGS84
    lon, lat = transformer_to_wgs84.transform(res_x, res_y)
    
    # Save result as suspicious incident (sound of interest)
    new_incident = models.SuspiciousIncident(
        description="Triangulated Sound (Possible incident)",
        geom=from_shape(Point(lon, lat), srid=WGS84_SRID)
    )
    db.add(new_incident)
    db.commit()
    logger.info(f"Successfully triangulated sound source: {lat}, {lon}")
