#include "protocol.h"
#include <stdlib.h>
#include <string.h>
int stream_feed(Stream *s, const void *bytes, size_t n) {
    if (n > STREAM_CAP - s->size) return -1;
    memcpy(s->data + s->size, bytes, n); s->size += n; return 0;
}
int stream_next(Stream *s, cJSON **j) {
    *j = NULL;
    if (s->size < 4) return 0;
    uint32_t n; memcpy(&n, s->data, 4); n = ntohl(n);
    if (!n || n > FRAME_MAX) return -1;
    if (s->size < n + 4) return 0;
    char body[FRAME_MAX + 1]; memcpy(body, s->data + 4, n); body[n] = 0;
    const char *end = NULL;
    *j = cJSON_ParseWithLengthOpts(body, n + 1, &end, 1);
    if (!*j || !cJSON_IsObject(*j) || end != body + n) { cJSON_Delete(*j); *j = NULL; return -1; }
    memmove(s->data, s->data + n + 4, s->size - n - 4); s->size -= n + 4;
    return 1;
}
int stream_queue(Stream *s, const cJSON *j) {
    char *body = cJSON_PrintUnformatted(j);
    if (!body) return -1;
    size_t len = strlen(body);
    if (!len || len > FRAME_MAX || len + 4 > STREAM_CAP - s->size) { free(body); return -1; }
    uint32_t n = htonl((uint32_t)len);
    stream_feed(s, &n, 4); stream_feed(s, body, len); free(body); return 0;
}
int stream_flush(Stream *s, socket_t fd) {
    while (s->size) {
        int n = net_send(fd, s->data, s->size);
        if (n < 0) return net_would_block() ? 0 : -1;
        if (!n) return -1;
        memmove(s->data, s->data + n, s->size - (size_t)n); s->size -= (size_t)n;
    }
    return 0;
}
const char *json_string(const cJSON *j, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
cJSON *message(const char *type) {
    cJSON *j = cJSON_CreateObject(); cJSON_AddStringToObject(j, "type", type); return j;
}
