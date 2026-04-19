# Этап 1: Сборка
FROM node:20-alpine AS build

# Устанавливаем рабочую директорию
WORKDIR /app

# Копируем файлы зависимостей
# Используем package*.json для захвата обоих файлов, если они есть
COPY package*.json ./

# Устанавливаем зависимости (npm ci гарантирует установку точных версий из lock-файла)
RUN npm ci

# Копируем остальные файлы проекта
COPY . .

# Выполняем сборку
RUN npm run build

# Этап 2: Раздача статики через Nginx
FROM nginx:stable-alpine

# Копируем кастомный конфиг Nginx
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Копируем статику из этапа сборки
COPY --from=build /app/dist /usr/share/nginx/html

# Прокидываем порт 80
EXPOSE 80

# Запускаем nginx без перехода в фоновый режим для работы внутри контейнера
CMD ["nginx", "-g", "daemon off;"]
