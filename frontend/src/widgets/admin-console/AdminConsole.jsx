import { useAdminConsole } from '@/features/admin-console/model/useAdminConsole';
import { AdminHeader } from '@/widgets/admin-console/AdminHeader';
import { AudioRecordsDrawer } from '@/widgets/admin-console/AudioRecordsDrawer';
import { CreatedDeviceModal } from '@/widgets/admin-console/CreatedDeviceModal';
import { DeviceFormModal } from '@/widgets/admin-console/DeviceFormModal';
import { DeviceRegistryPanel } from '@/widgets/admin-console/DeviceRegistryPanel';
import { IncidentAdminPanel } from '@/widgets/admin-console/IncidentAdminPanel';
import { PeaksDrawer } from '@/widgets/admin-console/PeaksDrawer';

export function AdminConsole({ onOpenOperations }) {
  const admin = useAdminConsole();

  const confirmAndRun = async (message, action) => {
    if (!window.confirm(message)) {
      return;
    }

    try {
      await action();
    } catch (error) {
      window.alert(error.message || 'Operation failed.');
    }
  };

  return (
    <div className="admin-console">
      <div className="admin-console__layout">
        <AdminHeader
          deviceCount={admin.devices.length}
          incidentCount={admin.incidents.length}
          isRefreshing={admin.isRefreshing}
          onCreateDevice={admin.openCreateForm}
          onOpenGlobalPeaks={admin.openGlobalPeaksDrawer}
          onOpenOperations={onOpenOperations}
          onRefresh={() => admin.refreshDashboard()}
          systemHealth={admin.systemHealth}
        />

        <DeviceRegistryPanel
          devices={admin.devices}
          error={admin.devicesError}
          onDelete={(deviceId) => confirmAndRun(`Delete device ${deviceId}?`, () => admin.deleteDeviceById(deviceId))}
          onEdit={admin.openEditForm}
          onOpenAudio={admin.openDeviceAudioDrawer}
          onOpenPeaks={admin.openDevicePeaksDrawer}
        />

        <IncidentAdminPanel
          error={admin.incidentsError}
          incidents={admin.incidents}
          onClear={() => confirmAndRun(`Delete all ${admin.incidents.length} suspicious incidents?`, admin.clearAllIncidents)}
          onDelete={(incidentId) => confirmAndRun(`Delete suspicious incident #${incidentId}?`, () => admin.removeIncidentById(incidentId))}
          onRefresh={() => admin.refreshDashboard()}
        />
      </div>

      {admin.isFormOpen ? (
        <DeviceFormModal
          device={admin.formDevice}
          isSaving={admin.isSavingDevice}
          onClose={admin.closeForm}
          onSubmit={admin.saveDevice}
        />
      ) : null}

      {admin.createdDeviceId ? (
        <CreatedDeviceModal deviceId={admin.createdDeviceId} onClose={admin.closeCreatedDeviceModal} />
      ) : null}

      <PeaksDrawer
        state={admin.peaksState}
        onClear={() => confirmAndRun('Delete all peaks globally?', admin.clearAllPeaks)}
        onClose={admin.closePeaksDrawer}
        onDelete={(peakId) => confirmAndRun(`Delete peak #${peakId}?`, () => admin.removePeakById(peakId))}
        onRefresh={admin.refreshPeaksDrawer}
      />

      <AudioRecordsDrawer
        state={admin.audioState}
        onClear={() => confirmAndRun(
          `Delete all acoustic events for device ${admin.audioState.deviceId}?`,
          () => admin.clearAudioRecordsForDevice(admin.audioState.deviceId),
        )}
        onClose={admin.closeAudioDrawer}
        onDelete={(recordId) => confirmAndRun(`Delete acoustic event #${recordId}?`, () => admin.removeAudioRecordById(recordId))}
        onRefresh={admin.refreshAudioDrawer}
      />
    </div>
  );
}
