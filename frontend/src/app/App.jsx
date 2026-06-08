import { useConsoleView } from '@/app/model/useConsoleView';
import { AdminConsole } from '@/widgets/admin-console/AdminConsole';
import { DeviceHealthConsole } from '@/widgets/device-health/DeviceHealthConsole';
import { OperationsConsole } from '@/widgets/operations-console/OperationsConsole';

export default function App() {
  const { openAdmin, openHealth, openOperations, view } = useConsoleView();

  if (view === 'health') {
    return (
      <div className="app-root app-root--admin">
        <DeviceHealthConsole onOpenAdmin={openAdmin} />
      </div>
    );
  }

  return (
    <div className={view === 'admin' ? 'app-root app-root--admin' : 'app-root app-root--operations'}>
      {view === 'admin' ? (
        <AdminConsole onOpenHealth={openHealth} onOpenOperations={openOperations} />
      ) : (
        <OperationsConsole onOpenAdmin={openAdmin} />
      )}
    </div>
  );
}
