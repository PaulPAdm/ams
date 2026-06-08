import { Suspense, lazy } from 'react';
import { useConsoleView } from '@/app/model/useConsoleView';
import { AdminConsole } from '@/widgets/admin-console/AdminConsole';
import { OperationsConsole } from '@/widgets/operations-console/OperationsConsole';

// Lazy-loaded so the heavy charting bundle (recharts) only loads on the health
// view and never blocks the admin/operations consoles from rendering.
const DeviceHealthConsole = lazy(() =>
  import('@/widgets/device-health/DeviceHealthConsole').then((module) => ({
    default: module.DeviceHealthConsole,
  })),
);

export default function App() {
  const { openAdmin, openHealth, openOperations, view } = useConsoleView();

  if (view === 'health') {
    return (
      <div className="app-root app-root--admin">
        <Suspense fallback={<div className="admin-console" />}>
          <DeviceHealthConsole onOpenAdmin={openAdmin} />
        </Suspense>
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
