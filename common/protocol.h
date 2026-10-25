#ifndef CHESS_PROTOCOL_H
#define CHESS_PROTOCOL_H
#include "net.h"
#include "cJSON.h"
#define FRAME_MAX 16384
#define STREAM_CAP (FRAME_MAX * 4)
typedef struct { unsigned char data[STREAM_CAP]; size_t size; } Stream;
/* Feed bounded bytes; next returns 1 (JSON), 0 (incomplete), -1 (invalid). */
int stream_feed(Stream *s, const void *bytes, size_t count);
int stream_next(Stream *s, cJSON **json);
int stream_queue(Stream *s, const cJSON *json);
/* Flush nonblocking output: 0 success/pending, -1 disconnected. */
int stream_flush(Stream *s, socket_t fd);
const char *json_string(const cJSON *j, const char *key);
cJSON *message(const char *type);
#endif
