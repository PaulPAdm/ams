# Dockerfile
FROM python:3.11-slim

# Установка системных зависимостей для psycopg2 и GIS (если нужно)
RUN apt-get update && apt-get install -y \
    libpq-dev \
    gcc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Установка зависимостей через pip (можно через poetry, но для Docker проще pip)
COPY pyproject.toml .
RUN pip install poetry && \
    poetry config virtualenvs.create false && \
    poetry install --no-interaction --no-ansi --no-root

# Копирование кода приложения
COPY . .

# Команда для запуска
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
