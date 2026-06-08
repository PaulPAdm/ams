import { LoaderCircle, Plus, Settings2, X } from 'lucide-react';
import { useState } from 'react';
import { formatCoordinateInput, parseCoordinates } from '@/entities/device/model/device';

export function DeviceFormModal({ device, isSaving, onClose, onSubmit }) {
  const isEditing = Boolean(device);
  const [error, setError] = useState(null);

  const handleSubmit = async (event) => {
    event.preventDefault();
    setError(null);

    const formData = new FormData(event.currentTarget);
    const coordinates = parseCoordinates(formData.get('coordinates'));

    if (!coordinates) {
      setError('Invalid coordinates. Use "52.21, 20.98".');
      return;
    }

    try {
      await onSubmit({
        id: formData.get('id'),
        lat: coordinates.lat,
        lon: coordinates.lon,
        name: formData.get('name'),
        tag: formData.get('tag'),
      });
    } catch (err) {
      setError(err.message || 'Failed to save device.');
    }
  };

  return (
    <div className="modal-backdrop">
      <div className="modal-card">
        <button type="button" className="modal-card__close" onClick={onClose} aria-label="Close device form">
          <X size={18} />
        </button>

        <div className="modal-card__heading">
          {isEditing ? <Settings2 size={18} /> : <Plus size={18} />}
          <div>
            <strong>{isEditing ? 'Edit device' : 'Add device'}</strong>
            <p>Register hardware metadata and coordinates in the central registry.</p>
          </div>
        </div>

        <form className="admin-form" onSubmit={handleSubmit}>
          {!isEditing ? (
            <label className="admin-field">
              <span>Device ID</span>
              <input name="id" className="admin-input" placeholder="Leave empty to auto-generate" />
            </label>
          ) : null}

          <label className="admin-field">
            <span>Name</span>
            <input
              name="name"
              required
              className="admin-input"
              defaultValue={device?.name || ''}
              placeholder="Node 06 - Mokotow"
            />
          </label>

          <label className="admin-field">
            <span>Tag</span>
            <input
              name="tag"
              className="admin-input"
              defaultValue={device?.tag || ''}
              placeholder="MKT"
            />
          </label>

          <label className="admin-field">
            <span>Coordinates</span>
            <input
              name="coordinates"
              required
              className="admin-input"
              defaultValue={formatCoordinateInput(device)}
              placeholder="52.21, 20.98"
            />
          </label>

          {error ? (
            <p className="admin-form__error" role="alert">{error}</p>
          ) : null}

          <div className="admin-form__actions">
            <button type="button" className="button-secondary" onClick={onClose}>
              Cancel
            </button>
            <button type="submit" className="button-primary button-primary--inline" disabled={isSaving}>
              {isSaving ? <LoaderCircle size={16} className="is-spinning" /> : null}
              {isEditing ? 'Save changes' : 'Create device'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
