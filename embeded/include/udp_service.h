#ifndef UDP_SERVICE_H
#define UDP_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int udp_setup_endpoint(const char *server_ip, int server_port);
bool udp_send_audio_mono16(const char *device_id, const int16_t *samples, size_t sample_count);
bool is_udp_ready(void);
void udp_close(void);

#endif // UDP_SERVICE_H
