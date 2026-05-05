import { CheckCircle2, Copy, X } from 'lucide-react';

export function CreatedDeviceModal({ deviceId, onClose }) {
  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(deviceId);
      window.alert('Device ID copied to clipboard.');
    } catch {
      window.alert(deviceId);
    }
  };

  return (
    <div className="modal-backdrop">
      <div className="modal-card modal-card--compact">
        <button type="button" className="modal-card__close" onClick={onClose} aria-label="Close device ID modal">
          <X size={18} />
        </button>

        <div className="modal-card__success-icon">
          <CheckCircle2 size={40} />
        </div>

        <div className="modal-card__copy-block">
          <strong>Device registered</strong>
          <p>Store the generated hardware identifier before provisioning the device in the field.</p>
          <code className="admin-code admin-code--large">{deviceId}</code>
        </div>

        <div className="admin-form__actions">
          <button type="button" className="button-secondary" onClick={handleCopy}>
            <Copy size={16} />
            Copy ID
          </button>
          <button type="button" className="button-primary button-primary--inline" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
