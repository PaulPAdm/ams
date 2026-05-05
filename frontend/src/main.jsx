import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import 'mapbox-gl/dist/mapbox-gl.css';
import App from '@/app/App.jsx';
import { appConfig } from '@/app/config/env';
import '@/app/styles/index.css';

document.title = appConfig.htmlTitle;
document.documentElement.lang = appConfig.locale;

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
