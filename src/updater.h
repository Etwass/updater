#pragma once

#define NET_OK 0
#define NET_ERROR -1

typedef void *NET_HANDLE;
typedef size_t (*NET_FN_WRITE_CALLBACK)(void *ptr, size_t size, size_t nmemb, void *userdata);
typedef struct tagCONNECTION_CONFIG
  {
    const char *url;
    const char *username;
    const char *password;
    unsigned int port;
  } CONNECTION_CONFIG;

NET_HANDLE net_create();
int net_init(NET_HANDLE);
int net_download(NET_HANDLE, const CONNECTION_CONFIG *, NET_FN_WRITE_CALLBACK, void *);
void net_cleanup(NET_HANDLE);
void net_destroy(NET_HANDLE);
