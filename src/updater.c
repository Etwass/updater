#include "updater.h"

#include <curl/curl.h>
#include <malloc.h>

#define NET_LOGIN             "anonymous"
#define NET_PASSWORD          NET_LOGIN"@"NET_LOGIN".com"

#define PROTOCOL_FTP          "ftp://"
#define PROTOCOL_FTPS         "ftps://"
#define PROTOCOL_SFTP         "sftp://"
#define PROTOCOL_HTTP         "http://"
#define PROTOCOL_HTTPS        "https://"

#define PROTOCOL_FTP_UNKNOWN  -1
#define PROTOCOL_FTP_CODE     0
#define PROTOCOL_FTPS_CODE    1
#define PROTOCOL_SFTP_CODE    2
#define PROTOCOL_HTTP_CODE    3
#define PROTOCOL_HTTPS_CODE   4

typedef struct tagCURL_DATA
  {
    CURL *curl;
    CONNECTION_CONFIG config;
  } CURL_DATA;

int determine_protocol(const char *);
int tune_curl_for_protocol(CURL *, int);
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
        char *good_url = 0;

        if(tune_curl_for_protocol(curl_handle->curl, determine_protocol(cfg->url)) != NET_OK)
          {
            fprintf(stderr, "Unsupported protocol in URL: %s\n", cfg->url);
            return NET_ERROR;
          }
        if(*(cfg->url + url_len - 1) != '/')
          {
            good_url = malloc(url_len + 2);

            if(!good_url)
              {
                fprintf(stderr, "malloc() failed\n");
                return NET_ERROR;
              }
            sprintf(good_url, "%s/", cfg->url);
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_URL, (good_url ? good_url : cfg->url));
        if(cfg->port)
          curl_easy_setopt(curl_handle->curl, CURLOPT_PORT, cfg->port);
        curl_easy_setopt(curl_handle->curl, CURLOPT_DIRLISTONLY, cfg->listing_only);
//        if(cfg->user)
//          curl_easy_setopt(curl_handle->curl, CURLOPT_USERNAME, cfg->user);
//        if(cfg->password)
//          curl_easy_setopt(curl_handle->curl, CURLOPT_PASSWORD, cfg->password);
        curl_easy_setopt(curl_handle->curl, CURLOPT_USERNAME, (cfg->user ? cfg->user : NET_LOGIN));
        curl_easy_setopt(curl_handle->curl, CURLOPT_PASSWORD, (cfg->password ? cfg->password : NET_PASSWORD));
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEFUNCTION, fn_callback);
        if(data)curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEDATA, data);
        //result = (curl_easy_perform(curl_handle->curl) == CURLE_OK) ? NET_OK : NET_ERROR;
        if((result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_ERROR) != NET_OK)
          {
            curl_easy_setopt(curl_handle->curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
            curl_easy_setopt(curl_handle->curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYHOST, 0L);
            result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_ERROR;
          }
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
        PROGRESS_DATA progress_data = {.downloaded = offset};

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
        if(cfg->port)
          curl_easy_setopt(curl_handle->curl, CURLOPT_PORT, cfg->port);
        curl_easy_setopt(curl_handle->curl, CURLOPT_USERNAME, (cfg->user ? cfg->user : NET_LOGIN));
        curl_easy_setopt(curl_handle->curl, CURLOPT_PASSWORD, (cfg->password ? cfg->password : NET_PASSWORD));
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
            curl_easy_setopt(curl_handle->curl, CURLOPT_XFERINFODATA, &progress_data);
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEFUNCTION, fn_write);
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEDATA, stream);
        curl_easy_setopt(curl_handle->curl, CURLOPT_FTP_USE_EPSV, 0L);
        //curl_easy_setopt(curl_handle->curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        //curl_easy_setopt(curl_handle->curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
        //curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
        //curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYHOST, 0L);
        if((result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_ERROR) != NET_OK)
          {
            curl_easy_setopt(curl_handle->curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
            curl_easy_setopt(curl_handle->curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYHOST, 0L);
            result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_ERROR;
          }
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

int determine_protocol(const char *url)
  {
    if(strncmp(url, PROTOCOL_FTP, strlen(PROTOCOL_FTP)) == 0)
      return PROTOCOL_FTP_CODE;
    else if(strncmp(url, PROTOCOL_FTPS, strlen(PROTOCOL_FTPS)) == 0)
      return PROTOCOL_FTPS_CODE;
    else if(strncmp(url, PROTOCOL_SFTP, strlen(PROTOCOL_SFTP)) == 0)
      return PROTOCOL_SFTP_CODE;
    else if(strncmp(url, PROTOCOL_HTTP, strlen(PROTOCOL_HTTP)) == 0)
      return PROTOCOL_HTTP_CODE;
    else if(strncmp(url, PROTOCOL_HTTPS, strlen(PROTOCOL_HTTPS)) == 0)
      return PROTOCOL_HTTPS_CODE;
    return PROTOCOL_FTP_UNKNOWN;
  }
int tune_curl_for_protocol(CURL *curl, int code)
  {
    switch(code)
      {
        case PROTOCOL_FTP_CODE:
          curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);
          break;
        case PROTOCOL_FTPS_CODE:
          curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
          curl_easy_setopt(curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
          break;
        case PROTOCOL_SFTP_CODE:
          ;
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
