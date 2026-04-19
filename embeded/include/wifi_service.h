#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

int initialize_network(void);
int connect_to_wifi(const char *ssid, const char *password);

#endif // WIFI_SERVICE_H
