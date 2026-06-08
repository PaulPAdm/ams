// Concrete colors mirroring the CSS theme tokens in app/styles/index.css.
// Recharts needs literal stroke/fill values, so we keep them in sync here.
export const chartTheme = {
  accent: '#7dd3fc',
  accentStrong: '#38bdf8',
  warning: '#f7b955',
  danger: '#ff7a59',
  success: '#57d49a',
  grid: 'rgba(151, 194, 255, 0.14)',
  axis: 'rgba(221, 233, 255, 0.48)',
  tooltipBg: 'rgba(7, 18, 31, 0.96)',
  tooltipBorder: 'rgba(151, 194, 255, 0.34)',
};

export const tooltipContentStyle = {
  background: chartTheme.tooltipBg,
  border: `1px solid ${chartTheme.tooltipBorder}`,
  borderRadius: 8,
  color: '#f4f8ff',
};

export const axisTick = { fill: chartTheme.axis, fontSize: 12 };
