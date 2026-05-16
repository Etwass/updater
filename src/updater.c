#include "updater.h"

#include <curl/curl.h>
#include <malloc.h>

typedef struct tagCURL_DATA
  {
    CURL *curl;
    CONNECTION_CONFIG config;
  } CURL_DATA;

NET_HANDLE net_create()
  {
    CURL_DATA *curl_handle = (CURL *)malloc(sizeof(CURL_DATA));

    if(curl_handle)
      {
        memset(curl_handle, 0, sizeof(CONNECTION_CONFIG));
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return (NET_HANDLE)curl_handle;
      }
    return NULL;
  }
int net_init(NET_HANDLE handle)
  {
    if(handle)
      return (((CURL_DATA *)handle)->curl = curl_easy_init()) ? NET_OK : NET_ERROR;
    return NET_ERROR;
  }
int net_download(NET_HANDLE handle, const CONNECTION_CONFIG *config, NET_FN_WRITE_CALLBACK fn_write, void *stream)
  {
    if(handle && config && fn_write)
      {
        CURL_DATA *curl_handle = (CURL_DATA *)handle;

        curl_easy_setopt(curl_handle->curl, CURLOPT_URL, config->url);
        if(config->username && config->password)
          {
            size_t userpwd_len = strlen(config->username) + strlen(config->password) + 2; // +2 for ':' and null terminator
            char *userpwd = malloc(userpwd_len);

            snprintf(userpwd, userpwd_len, "%s:%s", config->username, config->password);
            curl_easy_setopt(curl_handle->curl, CURLOPT_USERPWD, userpwd);
            free(userpwd);
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEFUNCTION, fn_write);
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEDATA, stream);
        return (curl_easy_perform(curl_handle->curl) == CURLE_OK) ? NET_OK : NET_ERROR;
      }
    return NET_ERROR;
  }
void net_cleanup(NET_HANDLE handle)
  {
    if(handle)
      {
        CURL_DATA *curl_handle = (CURL_DATA *)handle;

        if(curl_handle->curl)
          curl_easy_cleanup(curl_handle->curl);
      }
  }
void net_destroy(NET_HANDLE handle)
  {
    if(handle)
      {
        CURL_DATA *curl_handle = (CURL_DATA *)handle;

        free(curl_handle);
        curl_global_cleanup();
      }
   }
