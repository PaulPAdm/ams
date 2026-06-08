#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdbool.h>

int initialize_network(void);
int connect_to_wifi(const char *ssid, const char *password);
void disconnect_wifi(void);
bool is_wifi_connected(void);

#endif // WIFI_SERVICE_H
