import { useEffect, useState } from 'react';

const CONSOLE_VIEWS = {
  admin: 'admin',
  operations: 'operations',
};

const VIEW_PATHS = {
  admin: '/admin/',
  operations: '/',
};

function readViewFromLocation() {
  if (typeof window === 'undefined') {
    return CONSOLE_VIEWS.operations;
  }

  return window.location.pathname.startsWith('/admin')
    ? CONSOLE_VIEWS.admin
    : CONSOLE_VIEWS.operations;
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

  const navigateTo = (nextView) => {
    const nextPath = VIEW_PATHS[nextView] || VIEW_PATHS.operations;

    if (window.location.pathname !== nextPath) {
      window.history.pushState({}, '', nextPath);
    }

    setView(nextView);
  };

  return {
    openAdmin: () => navigateTo(CONSOLE_VIEWS.admin),
    openOperations: () => navigateTo(CONSOLE_VIEWS.operations),
    view,
  };
}
