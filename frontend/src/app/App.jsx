import { useConsoleView } from '@/app/model/useConsoleView';
import { AdminConsole } from '@/widgets/admin-console/AdminConsole';
import { OperationsConsole } from '@/widgets/operations-console/OperationsConsole';

export default function App() {
  const { openAdmin, openOperations, view } = useConsoleView();

  return (
    <div className={view === 'admin' ? 'app-root app-root--admin' : 'app-root app-root--operations'}>
      {view === 'admin' ? (
        <AdminConsole onOpenOperations={openOperations} />
      ) : (
        <OperationsConsole onOpenAdmin={openAdmin} />
      )}
    </div>
  );
}
