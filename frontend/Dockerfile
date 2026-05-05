# Этап сборки (build stage)
FROM node:20-alpine as build-stage

WORKDIR /app

# Копируем файлы манифеста и устанавливаем зависимости
COPY package*.json ./
RUN npm install

# Копируем остальные файлы проекта и собираем его
COPY . .
RUN npm run build

# Этап выполнения (production stage)
FROM nginx:stable-alpine as production-stage

# Копируем результат сборки из предыдущего этапа в директорию Nginx
COPY --from=build-stage /app/dist /usr/share/nginx/html

# Копируем конфигурационный файл Nginx
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Прокидываем 80 порт наружу из контейнера
EXPOSE 80

# Запускаем Nginx в режиме демона
CMD ["nginx", "-g", "daemon off;"]
