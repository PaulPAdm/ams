import { cx } from '@/shared/lib/cx';

export function Badge({ tone = 'neutral', className, children }) {
  return <span className={cx('badge', `badge--${tone}`, className)}>{children}</span>;
}
