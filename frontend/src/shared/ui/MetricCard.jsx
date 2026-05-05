import { cx } from '@/shared/lib/cx';

export function MetricCard({ label, value, hint, tone = 'default', className }) {
  return (
    <article className={cx('metric-card', tone !== 'default' && `metric-card--${tone}`, className)}>
      <span className="metric-card__label">{label}</span>
      <strong className="metric-card__value">{value}</strong>
      {hint ? <span className="metric-card__hint">{hint}</span> : null}
    </article>
  );
}
