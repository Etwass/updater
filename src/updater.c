#include "updater.h"

#include <curl/curl.h>
#include <malloc.h>

typedef struct tagCURL_DATA
  {
    CURL *curl;
    CONNECTION_CONFIG config;
  } CURL_DATA;

int ftp_supports_resume(CURL *, const char *);

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
int net_get_listing(NET_HANDLE handle, const CONNECTION_CONFIG *cfg, NET_FN_WRITE fn_callback, void *data)
  {
    if(handle && cfg && fn_callback)
      {
        int result = 0;
        size_t url_len = strlen(cfg->url);
        CURL_DATA *curl_handle = (CURL_DATA *)handle;
        const char *good_url = 0;

        if(*(cfg->url + url_len - 1) != '/')
          {
            good_url = malloc(url_len + 2);

            if(!good_url)
              {
                fprintf(stderr, "malloc() failed\n");
                return NET_ERROR;
              }
            sprintf(good_url, "%s%s", cfg->url, (*(cfg->url + url_len - 1) != '/' ? "/" : ""));
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_URL, (good_url ? good_url : cfg->url));
        curl_easy_setopt(curl_handle->curl, CURLOPT_DIRLISTONLY, cfg->listing_only);
        if(cfg->user)
          curl_easy_setopt(curl_handle->curl, CURLOPT_USERNAME, cfg->user);
        if(cfg->password)
          curl_easy_setopt(curl_handle->curl, CURLOPT_PASSWORD, cfg->password);
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEFUNCTION, fn_callback);
        if(data)curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEDATA, data);
        result = (curl_easy_perform(curl_handle->curl) == CURLE_OK) ? NET_OK : NET_ERROR;
        if(good_url)
          free(good_url);
        return result;
      }
    return NET_ERROR;
  }
int net_download(NET_HANDLE handle, long int offset, const CONNECTION_CONFIG *cfg, NET_FN_CANT_DOWNLOAD_RESUME fn_cant_download_resume, NET_FN_PROGRESS fn_progress, NET_FN_WRITE fn_write, void *stream)
  {
    if(handle && cfg && fn_write)
      {
        int result = NET_OK;
        char *url_with_filename = 0;
        CURL_DATA *curl_handle = (CURL_DATA *)handle;

        if(cfg->filename)
          {
            url_with_filename = malloc(strlen(cfg->url) + strlen(cfg->filename) + 1 + 1);
            if(!url_with_filename)
              {
                fprintf(stderr, "malloc() failed\n");
                return NET_ERROR;
              }
            sprintf(url_with_filename, "%s%s%s", cfg->url, (*(cfg->url + strlen(cfg->url) - 1) != '/'?"/":""), cfg->filename);
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_URL, (url_with_filename? url_with_filename : cfg->url));
        if(cfg->user)
          curl_easy_setopt(curl_handle->curl, CURLOPT_USERNAME, cfg->user);
        if(cfg->password)
          curl_easy_setopt(curl_handle->curl, CURLOPT_PASSWORD, cfg->password);
        if(offset > 0)
          {
            if(ftp_supports_resume(curl_handle->curl, url_with_filename) != NET_OK)
              {
                if(fn_cant_download_resume && fn_cant_download_resume(stream))
                  {
                    result = NET_ERROR;
                    goto NET_DWNL_EXIT;
                  }
              }
            else
              curl_easy_setopt(curl_handle->curl, CURLOPT_RESUME_FROM_LARGE, offset);
          }
        if(fn_progress)
          {
            curl_easy_setopt(curl_handle->curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl_handle->curl, CURLOPT_XFERINFOFUNCTION, (curl_progress_callback)fn_progress);
            curl_easy_setopt(curl_handle->curl, CURLOPT_XFERINFODATA, NULL);
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEFUNCTION, fn_write);
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEDATA, stream);
        result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_ERROR;
NET_DWNL_EXIT:
        if(url_with_filename)
          free(url_with_filename);
        return result;
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

int ftp_supports_resume(CURL *curl, const char *url)
  {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);              // ничего не скачиваем
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, 1L);   // пробуем REST 1

    CURLcode res = curl_easy_perform(curl);
    long response = 0;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

    // Важно: сбросить настройки после проверки
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, 0L);

    if(res != CURLE_OK)
        return 0; // ошибка — считаем, что докачка не поддерживается

    // FTP-код 350 = OK для REST
    return (response == 350 || response == 250 || response == 213 || response == 125 || response == 150) ? NET_OK : NET_ERROR;
  } 
