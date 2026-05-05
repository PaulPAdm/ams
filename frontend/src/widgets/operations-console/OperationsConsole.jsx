import { useMemo, useState } from 'react';
import { appConfig, isMapConfigured } from '@/app/config/env';
import { useIncidentFeed } from '@/entities/incident/model/useIncidentFeed';
import { useSensors } from '@/entities/sensor/model/useSensors';
import { useSimulationController } from '@/features/simulation/model/useSimulationController';
import { useLiveAudioLevel } from '@/features/telemetry/model/useLiveAudioLevel';
import { DashboardHeader } from '@/widgets/dashboard/DashboardHeader';
import { HistoryPanel } from '@/widgets/dashboard/HistoryPanel';
import { IncidentSummaryPanel } from '@/widgets/dashboard/IncidentSummaryPanel';
import { NotificationStack } from '@/widgets/dashboard/NotificationStack';
import { OperationsMap } from '@/widgets/operations-map/OperationsMap';
import { SensorDetailsPanel } from '@/widgets/dashboard/SensorDetailsPanel';
import { SystemHealthPanel } from '@/widgets/dashboard/SystemHealthPanel';

function buildTimelineItems(simulationHistory, suspiciousIncidents) {
  const simulationItems = simulationHistory.map((incident) => ({
    id: `simulation-${incident.id}`,
    title: incident.classification,
    subtitle: `${incident.confidence}% confidence, ${incident.activeSensors.length} nodes involved`,
    timestamp: incident.createdAt,
    tone: 'danger',
    origin: 'Simulation',
  }));

  const backendItems = suspiciousIncidents.map((incident) => ({
    id: `backend-${incident.id}`,
    title: incident.classification,
    subtitle: incident.description,
    timestamp: incident.createdAt,
    tone: 'warning',
    origin: 'Backend feed',
  }));

  return [...simulationItems, ...backendItems]
    .sort((left, right) => new Date(right.timestamp) - new Date(left.timestamp))
    .slice(0, appConfig.simulation.historyLimit + 4);
}

export function OperationsConsole({ onOpenAdmin }) {
  const [viewState, setViewState] = useState(appConfig.mapView);
  const [selectedSensorId, setSelectedSensorId] = useState(null);
  const [hoveredIncidentId, setHoveredIncidentId] = useState(null);

  const {
    error: incidentsError,
    incidents: suspiciousIncidents,
    isLoading: incidentsLoading,
    lastUpdated,
  } = useIncidentFeed();
  const {
    error: sensorsError,
    isLoading: sensorsLoading,
    sensors,
    source: sensorSource,
  } = useSensors();
  const liveAudioLevel = useLiveAudioLevel({
    intervalMs: appConfig.intervals.telemetryMs,
    minDb: appConfig.simulation.minDb,
    maxDb: appConfig.simulation.maxDb,
  });
  const simulation = useSimulationController({ sensors });

  const selectedSensor = useMemo(
    () => sensors.find((sensor) => sensor.id === selectedSensorId) || null,
    [selectedSensorId, sensors],
  );

  const focusIncident = simulation.activeIncident || suspiciousIncidents[0] || null;
  const timelineItems = useMemo(
    () => buildTimelineItems(simulation.history, suspiciousIncidents),
    [simulation.history, suspiciousIncidents],
  );

  const handleMapClick = (lngLat) => {
    setSelectedSensorId(null);
    simulation.handleMapClick(lngLat);
  };

  const handleToggleSimulation = () => {
    setSelectedSensorId(null);
    simulation.toggleEnabled();
  };

  return (
    <div className="app-shell">
      <div className="app-shell__map-layer">
        <OperationsMap
          activeIncident={simulation.activeIncident}
          hoveredIncidentId={hoveredIncidentId}
          onHoverIncident={setHoveredIncidentId}
          onMapClick={handleMapClick}
          onSelectSensor={(sensor) => setSelectedSensorId(sensor.id)}
          onViewStateChange={setViewState}
          selectedSensorId={selectedSensorId}
          suspiciousIncidents={suspiciousIncidents}
          sensors={sensors}
          viewState={viewState}
        />
      </div>

      <div className="app-shell__overlay">
        <div className="dashboard-layout">
          <DashboardHeader
            appTitle={appConfig.appTitle}
            className="dashboard-layout__header"
            incidentCount={suspiciousIncidents.length + (simulation.activeIncident ? 1 : 0)}
            mapConfigured={isMapConfigured}
            onOpenAdmin={onOpenAdmin}
            onToggleSimulation={handleToggleSimulation}
            sensorCount={sensors.length}
            sensorSource={sensorSource}
            simulationEnabled={simulation.isEnabled}
            zoomLevel={viewState.zoom}
          />

          <SystemHealthPanel
            className="dashboard-layout__status"
            incidentsError={incidentsError}
            incidentsLoading={incidentsLoading}
            lastUpdated={lastUpdated}
            selectedSensor={selectedSensor}
            sensorsError={sensorsError}
            sensorsLoading={sensorsLoading}
            sensorSource={sensorSource}
          />

          <SensorDetailsPanel
            className="dashboard-layout__sensor"
            liveAudioLevel={liveAudioLevel}
            selectedSensor={selectedSensor}
          />

          <IncidentSummaryPanel
            activeIncident={simulation.activeIncident}
            className="dashboard-layout__incident"
            focusIncident={focusIncident}
            onDismissActiveIncident={simulation.dismissActiveIncident}
          />

          <HistoryPanel className="dashboard-layout__history" items={timelineItems} />

          <NotificationStack
            className="dashboard-layout__notifications"
            notifications={simulation.notifications}
            onDismiss={simulation.dismissNotification}
          />
        </div>
      </div>
    </div>
  );
}
