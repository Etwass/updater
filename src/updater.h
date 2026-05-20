#pragma once
#include <curl/system.h>

#define NET_OK 0
#define NET_ERROR -1

typedef void *NET_HANDLE;
typedef curl_off_t net_off_t;

typedef size_t (*NET_FN_WRITE)(void *, size_t, size_t, void *);
typedef int (*NET_FN_PROGRESS)(void *, net_off_t, net_off_t, net_off_t, net_off_t);
typedef int (*NET_FN_CANT_DOWNLOAD_RESUME)(void *);

typedef struct tagCONNECTION_CONFIG
  {
    const char *url;
    const char *user;
    const char *password;
    unsigned int port;
    const char *filename;
    int progress;
    long int listing_only;
  } CONNECTION_CONFIG;

typedef struct tagLISTING_BUFFER
  {
    size_t size;
    size_t capacity;
    char *data;
  } LISTING_BUFFER;

NET_HANDLE net_create();
int net_init(NET_HANDLE);
int net_get_listing(NET_HANDLE, const CONNECTION_CONFIG *, NET_FN_WRITE, void *);
int net_download(NET_HANDLE, long int, const CONNECTION_CONFIG *, NET_FN_CANT_DOWNLOAD_RESUME, NET_FN_PROGRESS, NET_FN_WRITE, void *);
void net_cleanup(NET_HANDLE);
void net_destroy(NET_HANDLE);
