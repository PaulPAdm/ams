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

export function AudioPipelineChart({ reports }) {
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
        <YAxis tick={axisTick} stroke={chartTheme.axis} width={48} allowDecimals={false} />
        <Tooltip contentStyle={tooltipContentStyle} labelFormatter={formatDateTime} />
        <Legend />
        <Line
          type="monotone"
          dataKey="audioQueueDepth"
          name="Queue depth"
          stroke={chartTheme.accent}
          dot={false}
          connectNulls
          isAnimationActive={false}
        />
        <Line
          type="monotone"
          dataKey="audioDroppedChunks"
          name="Dropped chunks"
          stroke={chartTheme.danger}
          dot={false}
          connectNulls
          isAnimationActive={false}
        />
      </LineChart>
    </ResponsiveContainer>
  );
}
