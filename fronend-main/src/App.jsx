import React, { useState, useMemo, useEffect } from 'react';
import { Map, Marker, Source, Layer } from 'react-map-gl';
import { Mic, AlertTriangle, Activity, Battery, Radio, X, List, Globe, ShieldAlert, AlertCircle, Menu, Settings } from 'lucide-react';
import * as turf from '@turf/turf';
import 'mapbox-gl/dist/mapbox-gl.css';

const MAPBOX_TOKEN = import.meta.env.VITE_MAPBOX_TOKEN || '';
const SENSORS_DATA = [
  { id: 1, lat: 52.2220, lng: 21.0070, name: "Node 01 - Politechnika", battery: 85, status: "Online" },
  { id: 2, lat: 52.2250, lng: 21.0150, name: "Node 02 - City Center", battery: 92, status: "Online" },
  { id: 3, lat: 52.2180, lng: 21.0120, name: "Node 03 - Plac Zbawiciela", battery: 45, status: "Online" },
  { id: 4, lat: 52.2280, lng: 21.0020, name: "Node 04 - Central Station", battery: 78, status: "Online" },
  { id: 5, lat: 52.2150, lng: 21.0000, name: "Node 05 - Koszyki", battery: 12, status: "Low Battery" },
];

function App() {
  const [viewState, setViewState] = useState({
    longitude: 21.0122, latitude: 52.2220, zoom: 14
  });

  const [incident, setIncident] = useState(null);
  const [history, setHistory] = useState([]);
  const [notifications, setNotifications] = useState([]);
  const [selectedSensor, setSelectedSensor] = useState(null);
  const [liveDb, setLiveDb] = useState(45);
  const [isMenuOpen, setIsMenuOpen] = useState(false);
  const [sensors, setSensors] = useState(SENSORS_DATA);
  const [suspiciousIncidents, setSuspiciousIncidents] = useState([]);
  const [hoveredIncident, setHoveredIncident] = useState(null);
  const [isSimulationMode, setIsSimulationMode] = useState(false);

  const displaySensors = useMemo(() => isSimulationMode ? SENSORS_DATA : sensors, [isSimulationMode, sensors]);

  // Fetch Suspicious Incidents
  useEffect(() => {
    const fetchIncidents = async () => {
      try {
        const response = await fetch('/api/suspicious_incidents/');
        if (response.ok) {
          const data = await response.json();
          // Filter incidents with missing coordinates and ensure data is an array
          const validIncidents = Array.isArray(data)
            ? data.filter(inc => inc.lat !== null && inc.lon !== null)
            : [];
          setSuspiciousIncidents(validIncidents);
        } else {
          console.error(`Server error: ${response.status} ${response.statusText}`);
        }
      } catch (error) {
        console.error("Network error fetching suspicious incidents:", error);
      }
    };

    fetchIncidents();
    const interval = setInterval(fetchIncidents, 10000); // Polling every 10s
    return () => clearInterval(interval);
  }, []);

  // Fetch Sensors
  useEffect(() => {
    const fetchSensors = async () => {
      try {
        const response = await fetch('/api/locations/');
        if (response.ok) {
          const data = await response.json();
          if (Array.isArray(data) && data.length > 0) {
            const mappedSensors = data.map(s => ({
              id: s.id,
              lat: s.lat,
              lng: s.lon, // OpenAPI uses 'lon', our code uses 'lng'
              name: s.name,
              battery: 100,
              status: "Online"
            }));
            setSensors(mappedSensors);
          }
        }
      } catch (error) {
        console.warn("Failed to fetch sensors from server, using default data.", error);
      }
    };
    fetchSensors();
  }, []);

  // Audio Level Simulation
  useEffect(() => {
    const interval = setInterval(() => {
      setLiveDb(Math.floor(Math.random() * (75 - 40 + 1) + 40));
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  // Notification Cleanup
  useEffect(() => {
    if (notifications.length > 0) {
      const timer = setTimeout(() => {
        setNotifications(prev => prev.slice(1));
      }, 5000);
      return () => clearTimeout(timer);
    }
  }, [notifications]);

  const handleMapClick = (e) => {
    if (!isSimulationMode) return;

    const { lng, lat } = e.lngLat;
    const sorted = displaySensors.map(s => ({
      ...s,
      dist: turf.distance(turf.point([lng, lat]), turf.point([s.lng, s.lat]))
    })).sort((a, b) => a.dist - b.dist);

    const active = sorted.slice(0, 3);
    const type = ["SCREAM", "CRASH", "GUNSHOT", "EXPLOSION"][Math.floor(Math.random() * 4)];
    const confidence = Math.floor(Math.random() * 15 + 85);
    const timestamp = new Date().toLocaleTimeString();

    const newIncident = { lng, lat, activeSensors: active, type, confidence, timestamp };

    setIncident(newIncident);
    setHistory(prev => [newIncident, ...prev].slice(0, 5));

    // Trigger Notification
    setNotifications(prev => [...prev, {
      id: Date.now(),
      title: "PRIORITY 1 ALERT",
      message: `${type} detected with ${confidence}% confidence. Dispatching units.`,
      type: "emergency"
    }]);
  };

  const triangulationLines = useMemo(() => {
    if (!incident) return null;
    return turf.featureCollection(incident.activeSensors.map(s =>
      turf.lineString([[incident.lng, incident.lat], [s.lng, s.lat]]))
    );
  }, [incident]);

  const toggleSimulationMode = () => {
    setSelectedSensor(null);
    setIncident(null);
    setIsSimulationMode(prev => !prev);
  };

  return (
    <div style={{ width: '100vw', height: '100vh', backgroundColor: '#000', color: '#0f0', fontFamily: 'monospace', overflow: 'hidden', position: 'relative' }}>
      <Map
        {...viewState}
        onMove={evt => setViewState(evt.viewState)}
        onClick={handleMapClick}
        style={{ width: '100%', height: '100%' }}
        mapStyle="mapbox://styles/mapbox/dark-v11"
        mapboxAccessToken={MAPBOX_TOKEN}
      >
        {triangulationLines && (
          <Source type="geojson" data={triangulationLines}>
            <Layer id="lines" type="line" paint={{ 'line-color': '#0f0', 'line-width': 1, 'line-dasharray': [4, 4] }} />
          </Source>
        )}

        {displaySensors.map(s => (
          <Marker key={s.id} latitude={s.lat} longitude={s.lng}>
            <div
              onClick={(e) => { e.stopPropagation(); setSelectedSensor(s); }}
              style={{ cursor: 'pointer', color: incident?.activeSensors.some(as => as.id === s.id) ? '#f00' : '#0f0' }}
            >
              <Mic size={selectedSensor?.id === s.id ? 32 : 22} />
            </div>
          </Marker>
        ))}

        {incident && (
          <Marker latitude={incident.lat} longitude={incident.lng}>
            <AlertTriangle size={30} color="#f00" className="animate-pulse" />
          </Marker>
        )}

        {suspiciousIncidents.map(inc => (
          <Marker key={inc.id} latitude={inc.lat} longitude={inc.lon}>
            <div
              onMouseEnter={() => setHoveredIncident(inc)}
              onMouseLeave={() => setHoveredIncident(null)}
              style={{
                cursor: 'pointer',
                color: '#f00',
                background: 'rgba(255, 0, 0, 0.1)',
                padding: '6px',
                borderRadius: '50%',
                border: '1px solid #f00',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                boxShadow: '0 0 15px rgba(255, 0, 0, 0.4)',
                transition: 'all 0.3s cubic-bezier(0.4, 0, 0.2, 1)',
                transform: hoveredIncident?.id === inc.id ? 'scale(1.2)' : 'scale(1)',
                zIndex: hoveredIncident?.id === inc.id ? 10 : 1,
                position: 'relative'
              }}
            >
              <div className="radar-pulse"></div>
              <AlertCircle size={20} className={hoveredIncident?.id === inc.id ? "" : "animate-pulse"} />

              {hoveredIncident?.id === inc.id && (
                <div style={{
                  position: 'absolute',
                  bottom: '120%',
                  left: '50%',
                  transform: 'translateX(-50%)',
                  background: 'rgba(0, 0, 0, 0.95)',
                  border: '1px solid #f00',
                  borderRadius: '4px',
                  padding: '12px',
                  width: '240px',
                  boxShadow: '0 0 25px rgba(255, 0, 0, 0.3)',
                  pointerEvents: 'none',
                  zIndex: 100,
                  backdropFilter: 'blur(10px)',
                  animation: 'popIn 0.2s ease-out'
                }}>
                  <div style={{
                    color: '#f00',
                    fontSize: '10px',
                    fontWeight: 'bold',
                    marginBottom: '8px',
                    letterSpacing: '1px',
                    borderBottom: '1px solid rgba(255, 0, 0, 0.3)',
                    paddingBottom: '4px',
                    display: 'flex',
                    justifyContent: 'space-between'
                  }}>
                    <span>SUSPICIOUS_INCIDENT_{inc.id}</span>
                    <span>{new Date(inc.created_at).toLocaleTimeString()}</span>
                  </div>
                  <div style={{ color: '#fff', fontSize: '13px', lineHeight: '1.4', fontFamily: 'monospace' }}>
                    {inc.description || 'No description available'}
                  </div>
                  <div style={{
                    marginTop: '8px',
                    fontSize: '9px',
                    color: '#f00',
                    opacity: 0.7,
                    display: 'flex',
                    gap: '10px'
                  }}>
                    <span>LAT: {inc.lat.toFixed(4)}</span>
                    <span>LON: {inc.lon.toFixed(4)}</span>
                  </div>

                  {/* Arrow for the tooltip */}
                  <div style={{
                    position: 'absolute',
                    top: '100%',
                    left: '50%',
                    marginLeft: '-5px',
                    borderWidth: '5px',
                    borderStyle: 'solid',
                    borderColor: '#f00 transparent transparent transparent'
                  }}></div>
                </div>
              )}
            </div>
          </Marker>
        ))}
      </Map>

      {/* --- NOTIFICATION STACK --- */}
      <div style={{ position: 'absolute', top: 20, right: 20, display: 'flex', flexDirection: 'column', gap: 10, zIndex: 100 }}>
        {notifications.map(note => (
          <div key={note.id} className="notification-toast">
            <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 5 }}>
              <ShieldAlert size={18} color="#f00" />
              <span style={{ color: '#f00', fontWeight: 'bold', fontSize: 12 }}>{note.title}</span>
            </div>
            <div style={{ fontSize: 11, color: '#fff' }}>{note.message}</div>
          </div>
        ))}
      </div>

      {/* --- TOP LEFT: Branding & Compact Settings Menu --- */}
      <div
        onMouseEnter={() => setIsMenuOpen(true)}
        onMouseLeave={() => setIsMenuOpen(false)}
        style={{ position: 'absolute', top: 20, left: 20, zIndex: 60, display: 'flex', flexDirection: 'column', gap: 5 }}
      >
        {/* Main Branding Block */}
        <div style={{ padding: '15px 20px', background: 'rgba(0,0,0,0.85)', border: '1px solid #0f0', borderRadius: 8, display: 'flex', alignItems: 'center', gap: 12, cursor: 'pointer', backdropFilter: 'blur(5px)' }}>
          <Menu size={24} color={isMenuOpen ? '#fff' : '#0f0'} style={{ transition: 'color 0.2s' }} />
          <div>
            <h2 style={{ margin: 0, fontSize: 18 }}>CITY AUDIO MONITOR</h2>
            <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
              <div style={{ fontSize: 10, opacity: 0.6 }}>SYSTEM_STATUS: ACTIVE</div>
              <div
                onClick={(e) => { e.stopPropagation(); toggleSimulationMode(); }}
                style={{
                  fontSize: 10,
                  color: isSimulationMode ? '#fff' : '#0f0',
                  background: isSimulationMode ? '#f00' : 'rgba(0, 255, 0, 0.1)',
                  padding: '2px 8px',
                  borderRadius: 4,
                  border: `1px solid ${isSimulationMode ? '#f00' : '#0f0'}`,
                  cursor: 'pointer',
                  fontWeight: 'bold',
                  display: 'flex',
                  alignItems: 'center',
                  gap: 5,
                  transition: 'all 0.2s'
                }}
              >
                <div style={{ width: 8, height: 8, borderRadius: '50%', background: isSimulationMode ? '#fff' : '#0f0', animation: isSimulationMode ? 'pulse 1s infinite' : 'none' }}></div>
                SIMULATION: {isSimulationMode ? 'ON' : 'OFF'}
              </div>
            </div>
          </div>
        </div>

        {/* Dropdown Menu (Appears on Hover) */}
        {isMenuOpen && (
          <div className="fade-in" style={{ padding: '15px', background: 'rgba(0,0,0,0.9)', border: '1px solid #333', borderRadius: 8, width: '100%', boxSizing: 'border-box' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, color: '#888', fontWeight: 'bold', marginBottom: 10, fontSize: 12 }}>
              <Settings size={14} /> PREFERENCES
            </div>
            <div className="menu-item" style={{ display: 'flex', alignItems: 'center', gap: 10, fontSize: 12, cursor: 'pointer', padding: '8px 0' }}>
              <Globe size={16} /> Language: English (US)
            </div>
          </div>
        )}
      </div>

      {/* --- ADDED: SENSOR PROPERTIES PANEL --- */}
      {selectedSensor && (
        <div style={{ position: 'absolute', top: 110, left: 20, width: 320, background: 'rgba(0,0,0,0.85)', border: `1px solid ${selectedSensor.battery < 20 ? '#ffaa00' : '#0f0'}`, padding: 15, borderRadius: 8, zIndex: 50, backdropFilter: 'blur(5px)' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 15, color: selectedSensor.battery < 20 ? '#ffaa00' : '#0f0', borderBottom: '1px solid #333', paddingBottom: 10 }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, fontWeight: 'bold', fontSize: 14 }}>
              <Radio size={16} /> TELEMETRY_DATA
            </div>
            <X size={18} style={{ cursor: 'pointer' }} onClick={() => setSelectedSensor(null)} />
          </div>

          <div style={{ fontSize: 12, display: 'grid', gap: 8 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ opacity: 0.5 }}>UID:</span>
              <span>NODE-{String(selectedSensor.id).padStart(3, '0')}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ opacity: 0.5 }}>LOCATION:</span>
              <span style={{ textAlign: 'right' }}>{selectedSensor.name}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ opacity: 0.5 }}>COORDS:</span>
              <span>{selectedSensor.lat.toFixed(4)}, {selectedSensor.lng.toFixed(4)}</span>
            </div>

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: 5, padding: '5px 0', borderTop: '1px dashed #333' }}>
              <span style={{ opacity: 0.5, display: 'flex', alignItems: 'center', gap: 5 }}><Battery size={14} /> POWER:</span>
              <span style={{ color: selectedSensor.battery < 20 ? '#ffaa00' : '#0f0', fontWeight: 'bold' }}>{selectedSensor.battery}%</span>
            </div>

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '5px 0' }}>
              <span style={{ opacity: 0.5, display: 'flex', alignItems: 'center', gap: 5 }}><Activity size={14} /> LIVE DB:</span>
              <span>{liveDb} dB</span>
            </div>

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '5px 0' }}>
              <span style={{ opacity: 0.5 }}>STATUS:</span>
              <span style={{ color: selectedSensor.status === 'Online' ? '#0f0' : '#ffaa00' }}>{selectedSensor.status}</span>
            </div>
          </div>
        </div>
      )}

      {/* BOTTOM LEFT: Current Incident */}
      {incident && (
        <div style={{ position: 'absolute', bottom: 30, left: 20, width: 320, background: 'rgba(255,0,0,0.1)', border: '1px solid #f00', padding: 20, borderRadius: 8, zIndex: 10, backdropFilter: 'blur(10px)' }}>
          <div style={{ color: '#f00', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: 10 }}>
            <AlertTriangle size={18} /> ANOMALY DETECTED
          </div>
          <div style={{ fontSize: 13, marginTop: 10 }}>
            TYPE: {incident.type}<br />
            CONFIDENCE: {incident.confidence}%
          </div>
          <button onClick={() => setIncident(null)} style={{ marginTop: 15, width: '100%', padding: 8, background: '#f00', color: '#fff', border: 'none', cursor: 'pointer', fontWeight: 'bold' }}>DISMISS</button>
        </div>
      )}

      {/* BOTTOM RIGHT: History Log */}
      <div style={{ position: 'absolute', bottom: 30, right: 20, width: 320, background: 'rgba(0,0,0,0.85)', border: '1px solid #333', padding: 15, borderRadius: 8, zIndex: 10 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 15, color: '#888' }}>
          <List size={16} /> <span style={{ fontSize: 11 }}>SYSTEM LOG HISTORY</span>
        </div>
        <div style={{ display: 'grid', gap: 8 }}>
          {history.length === 0 && <div style={{ fontSize: 10, color: '#444' }}>No data...</div>}
          {history.map((item, idx) => (
            <div key={idx} style={{ padding: 6, background: 'rgba(255,255,255,0.02)', borderLeft: '2px solid #0f0', fontSize: 10 }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: idx === 0 && incident ? '#f00' : '#0f0' }}>{item.type}</span>
                <span style={{ opacity: 0.5 }}>{item.timestamp}</span>
              </div>
            </div>
          ))}
        </div>
      </div>

      <style>{`
        .animate-pulse { animation: pulse 1.5s infinite; }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.3; } 100% { opacity: 1; } }

        .notification-toast {
          width: 280px;
          background: rgba(0,0,0,0.95);
          border: 1px solid #f00;
          padding: 12px;
          border-radius: 4px;
          box-shadow: 0 0 20px rgba(255,0,0,0.2);
          animation: slideIn 0.3s ease-out;
        }

        @keyframes slideIn {
          from { transform: translateX(100%); opacity: 0; }
          to { transform: translateX(0); opacity: 1; }
        }

        .fade-in {
          animation: fadeIn 0.2s ease-in;
        }

        @keyframes fadeIn {
          from { opacity: 0; transform: translateY(-5px); }
          to { opacity: 1; transform: translateY(0); }
        }

        @keyframes popIn {
          from { opacity: 0; transform: translateX(-50%) translateY(10px) scale(0.95); }
          to { opacity: 1; transform: translateX(-50%) translateY(0) scale(1); }
        }

        .radar-pulse {
          position: absolute;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          border-radius: 50%;
          border: 2px solid rgba(255, 0, 0, 0.6);
          animation: radar 2s linear infinite;
          pointer-events: none;
        }

        @keyframes radar {
          0% { transform: scale(1); opacity: 0.8; }
          100% { transform: scale(3); opacity: 0; }
        }

        .menu-item:hover { color: #fff; background: rgba(255,255,255,0.1); border-radius: 4px; padding-left: 5px; transition: all 0.2s; }
      `}</style>
    </div>
  );
}

export default App;
