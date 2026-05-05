import { cx } from '@/shared/lib/cx';

export function Panel({ as: component = 'section', tone = 'default', className, children }) {
  const panelClassName = cx('panel', tone !== 'default' && `panel--${tone}`, className);

  if (component === 'section') {
    return <section className={panelClassName}>{children}</section>;
  }

  const CustomComponent = component;
  return <CustomComponent className={panelClassName}>{children}</CustomComponent>;
}
