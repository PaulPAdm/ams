import { useEffect, useState } from 'react';

const CONSOLE_VIEWS = {
  admin: 'admin',
  health: 'health',
  operations: 'operations',
};

const VIEW_PATHS = {
  admin: '/admin/',
  health: '/admin/health',
  operations: '/',
};

function readViewFromLocation() {
  if (typeof window === 'undefined') {
    return CONSOLE_VIEWS.operations;
  }

  const { pathname } = window.location;

  if (pathname.startsWith('/admin/health')) {
    return CONSOLE_VIEWS.health;
  }

  return pathname.startsWith('/admin') ? CONSOLE_VIEWS.admin : CONSOLE_VIEWS.operations;
}

export function useConsoleView() {
  const [view, setView] = useState(readViewFromLocation);

  useEffect(() => {
    const handleLocationChange = () => {
      setView(readViewFromLocation());
    };

    window.addEventListener('popstate', handleLocationChange);
    return () => {
      window.removeEventListener('popstate', handleLocationChange);
    };
  }, []);

  const navigateTo = (nextView, search = '') => {
    const nextPath = (VIEW_PATHS[nextView] || VIEW_PATHS.operations) + search;

    if (window.location.pathname + window.location.search !== nextPath) {
      window.history.pushState({}, '', nextPath);
    }

    setView(nextView);
  };

  return {
    openAdmin: () => navigateTo(CONSOLE_VIEWS.admin),
    openHealth: (deviceId) => navigateTo(
      CONSOLE_VIEWS.health,
      deviceId ? `?device=${encodeURIComponent(deviceId)}` : '',
    ),
    openOperations: () => navigateTo(CONSOLE_VIEWS.operations),
    view,
  };
}
