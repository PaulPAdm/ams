# FastAPI + SQLAlchemy + PostGIS

Базовая структура проекта на FastAPI с использованием SQLAlchemy и поддержкой PostGIS.

## Технологии
- **Python 3.11+**
- **FastAPI**
- **SQLAlchemy 2.0**
- **GeoAlchemy2** (PostGIS)
- **PostgreSQL**
- **Docker** (подготовка через pyproject.toml)

## Установка и запуск

1. Клонируйте репозиторий.
2. Установите зависимости (используя poetry):
   ```bash
   poetry install
   ```
3. Настройте подключение к БД в `.env` (или `app/core/config.py`).
4. Запустите приложение:
   ```bash
   uvicorn app.main:app --reload
   ```

## Docker

Для сборки образа используйте:
```bash
docker build -t fastapi-postgis-app .
```
(Не забудьте создать Dockerfile на основе pyproject.toml)

## Структура проекта
- `app/` - Основной код приложения
  - `main.py` - Точка входа, инициализация FastAPI
  - `api/` - Маршруты API (v1)
  - `core/` - Конфигурация и настройки приложения
  - `db/` - Подключение к базе данных и базовые классы моделей
  - `models/` - SQLAlchemy модели (PostGIS)
  - `schemas/` - Pydantic схемы (валидация данных)
- `pyproject.toml` - Зависимости проекта (Poetry)
- `.gitignore` - Список игнорируемых файлов
