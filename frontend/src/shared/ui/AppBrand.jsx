import { appConfig } from '@/app/config/env';
import { cx } from '@/shared/lib/cx';

export function AppBrand({ size = 'md', className }) {
  return (
    <div className={cx('app-brand', size !== 'md' && `app-brand--${size}`, className)}>
      <span className="app-brand__short">{appConfig.appShortName}</span>
      <span className="app-brand__full">{appConfig.appName}</span>
    </div>
  );
}
