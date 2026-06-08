import {
  CartesianGrid,
  Legend,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import { formatDateTime } from '@/shared/lib/format';
import { axisTick, chartTheme, tooltipContentStyle } from './chartTheme';

const UNIT_BY_KEY = {
  powerMw: ' mW',
  computedPowerMw: ' mW',
  currentMa: ' mA',
  busVoltageV: ' V',
};

const tooltipFormatter = (value, name, item) => {
  const unit = UNIT_BY_KEY[item?.dataKey] ?? '';
  return [`${value}${unit}`, name];
};

export function PowerChart({ reports }) {
  return (
    <ResponsiveContainer width="100%" height="100%">
      <LineChart data={reports} margin={{ top: 8, right: 12, bottom: 4, left: -8 }}>
        <CartesianGrid stroke={chartTheme.grid} strokeDasharray="3 3" />
        <XAxis
          dataKey="receivedAt"
          tickFormatter={formatDateTime}
          tick={axisTick}
          stroke={chartTheme.axis}
          minTickGap={48}
        />
        <YAxis yAxisId="left" tick={axisTick} stroke={chartTheme.axis} width={48} />
        <YAxis yAxisId="right" orientation="right" tick={axisTick} stroke={chartTheme.axis} width={44} />
        <Tooltip
          contentStyle={tooltipContentStyle}
          labelFormatter={formatDateTime}
          formatter={tooltipFormatter}
        />
        <Legend />
        <Line
          yAxisId="left"
          type="monotone"
          dataKey="powerMw"
          name="Power (mW)"
          stroke={chartTheme.accent}
          dot={false}
          connectNulls
          isAnimationActive={false}
        />
        <Line
          yAxisId="left"
          type="monotone"
          dataKey="currentMa"
          name="Current (mA)"
          stroke={chartTheme.warning}
          dot={false}
          connectNulls
          isAnimationActive={false}
        />
        <Line
          yAxisId="right"
          type="monotone"
          dataKey="busVoltageV"
          name="Bus voltage (V)"
          stroke={chartTheme.success}
          dot={false}
          connectNulls
          isAnimationActive={false}
        />
      </LineChart>
    </ResponsiveContainer>
  );
}
