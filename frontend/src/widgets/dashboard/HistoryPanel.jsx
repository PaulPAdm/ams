import { List } from 'lucide-react';
import { cx } from '@/shared/lib/cx';
import { formatClock } from '@/shared/lib/format';
import { Panel } from '@/shared/ui/Panel';

export function HistoryPanel({ className, items }) {
  return (
    <Panel className={cx('history-panel', className)}>
      <div className="panel-heading">
        <List size={16} />
        <span>Event timeline</span>
      </div>

      {items.length ? (
        <div className="timeline">
          {items.map((item) => (
            <article key={item.id} className="timeline__item">
              <span className={cx('timeline__dot', `timeline__dot--${item.tone}`)} />
              <div className="timeline__content">
                <div className="timeline__headline">
                  <strong>{item.title}</strong>
                  <span>{formatClock(item.timestamp)}</span>
                </div>
                <p>{item.subtitle}</p>
                <small>{item.origin}</small>
              </div>
            </article>
          ))}
        </div>
      ) : (
        <p className="panel-empty">No events recorded yet.</p>
      )}
    </Panel>
  );
}
