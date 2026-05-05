import { ShieldAlert, X } from 'lucide-react';
import { cx } from '@/shared/lib/cx';
import { formatClock } from '@/shared/lib/format';

export function NotificationStack({ className, notifications, onDismiss }) {
  if (!notifications.length) {
    return null;
  }

  return (
    <div className={cx('notification-stack', className)}>
      {notifications.map((notification) => (
        <article key={notification.id} className="toast">
          <div className="toast__header">
            <div>
              <span className="toast__eyebrow">Simulation alert</span>
              <strong>{notification.title}</strong>
            </div>
            <button
              type="button"
              className="toast__dismiss"
              onClick={() => onDismiss(notification.id)}
              aria-label="Dismiss notification"
            >
              <X size={14} />
            </button>
          </div>
          <p>{notification.message}</p>
          <div className="toast__meta">
            <ShieldAlert size={14} />
            <span>{formatClock(notification.createdAt)}</span>
          </div>
        </article>
      ))}
    </div>
  );
}
