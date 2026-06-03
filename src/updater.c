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

#define LISTING               1
#define DOWNLOAD              (!LISTING)

typedef struct tagCURL_DATA
  {
    CURL *curl;
    CONNECTION_CONFIG config;
  } CURL_DATA;

int determine_protocol(const char *);
int tune_curl_for_protocol(CURL *, int, const CONNECTION_CONFIG *, curl_off_t, int);
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
      return (((CURL_DATA *)handle)->curl = curl_easy_init()) ? NET_OK : NET_UNKNOWN_ERROR;
    return NET_UNKNOWN_ERROR;
  }
int net_get_listing(NET_HANDLE handle, const CONNECTION_CONFIG *cfg, NET_FN_WRITE fn_callback, void *data)
  {
    if(handle && cfg && fn_callback)
      {
        int result = 0;
        size_t url_len = strlen(cfg->url);
        CURL_DATA *curl_handle = (CURL_DATA *)handle;
        char *good_url = 0;

        if(tune_curl_for_protocol(curl_handle->curl, determine_protocol(cfg->url), cfg, 0, LISTING) != NET_OK)
          {
            if(0)
              {
                fprintf(stderr, "Unsupported protocol in URL: %s\n", cfg->url);
                return NET_UNKNOWN_ERROR;
              }
            printf("Warning: Unsupported protocol in URL: %s\nProceeding with default settings...\n", cfg->url);
            tune_curl_for_protocol(curl_handle->curl, PROTOCOL_FTP_CODE, cfg, 0, LISTING);
          }
        if(*(cfg->url + url_len - 1) != '/')
          {
            good_url = malloc(url_len + 2);

            if(!good_url)
              {
                fprintf(stderr, "malloc() failed\n");
                return NET_UNKNOWN_ERROR;
              }
            sprintf(good_url, "%s/", cfg->url);
          }
        curl_easy_setopt(curl_handle->curl, CURLOPT_URL, (good_url ? good_url : cfg->url));
        if(cfg->port)
          curl_easy_setopt(curl_handle->curl, CURLOPT_PORT, cfg->port);
        curl_easy_setopt(curl_handle->curl, CURLOPT_DIRLISTONLY, cfg->listing_only);
        curl_easy_setopt(curl_handle->curl, CURLOPT_USERNAME, (cfg->user ? cfg->user : NET_LOGIN));
        curl_easy_setopt(curl_handle->curl, CURLOPT_PASSWORD, (cfg->password ? cfg->password : NET_PASSWORD));
        curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEFUNCTION, fn_callback);
        if(data)curl_easy_setopt(curl_handle->curl, CURLOPT_WRITEDATA, data);
        //result = (curl_easy_perform(curl_handle->curl) == CURLE_OK) ? NET_OK : NET_ERROR;
        if((result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_UNKNOWN_ERROR) != NET_OK)
          {
            curl_easy_setopt(curl_handle->curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
            curl_easy_setopt(curl_handle->curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYHOST, 0L);
            result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_UNKNOWN_ERROR;
          }
        if(good_url)
          free(good_url);
        return result;
      }
    return NET_UNKNOWN_ERROR;
  }
int net_download(NET_HANDLE handle, long int offset, const CONNECTION_CONFIG *cfg, NET_FN_CANT_DOWNLOAD_RESUME fn_cant_download_resume, NET_FN_PROGRESS fn_progress, NET_FN_WRITE fn_write, void *stream)
  {
    if(handle && cfg && fn_write)
      {
        int result = NET_OK;
        char *url_with_filename = 0;
        CURL_DATA *curl_handle = (CURL_DATA *)handle;
        PROGRESS_DATA progress_data = {.downloaded = offset};

        if(tune_curl_for_protocol(curl_handle->curl, determine_protocol(cfg->url), cfg, 0, LISTING) != NET_OK)
          {
            if(0)
              {
                fprintf(stderr, "Unsupported protocol in URL: %s\n", cfg->url);
                return NET_UNKNOWN_ERROR;
              }
            printf("Warning: Unsupported protocol in URL: %s\nProceeding with default settings...\n", cfg->url);
            tune_curl_for_protocol(curl_handle->curl, PROTOCOL_FTP_CODE, cfg, 0, LISTING);
          }
        if(cfg->filename)
          {
            url_with_filename = malloc(strlen(cfg->url) + strlen(cfg->filename) + 1 + 1);
            if(!url_with_filename)
              {
                fprintf(stderr, "malloc() failed\n");
                return NET_UNKNOWN_ERROR;
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
                    result = NET_UNKNOWN_ERROR;
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
        if((result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_UNKNOWN_ERROR) != NET_OK)
          {
            curl_easy_setopt(curl_handle->curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
            curl_easy_setopt(curl_handle->curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle->curl, CURLOPT_SSL_VERIFYHOST, 0L);
            result = curl_easy_perform(curl_handle->curl) == CURLE_OK ? NET_OK : NET_UNKNOWN_ERROR;
          }
NET_DWNL_EXIT:
        if(url_with_filename)
          free(url_with_filename);
        return result;
      }
    return NET_UNKNOWN_ERROR;
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

void setup_curl_for_ftp(CURL *curl)
  {
    /* FTP — без шифрования */
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_NONE);

    /* Пассивный режим (рекомендуется) */
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 1L);

    /* Активный режим (если нужно) */
    /* curl_easy_setopt(curl, CURLOPT_FTPPORT, "-"); */

    /* LIST/NLST */
    /* Управляется через CURLOPT_DIRLISTONLY в вызывающем коде */

    /* Докачка */
    /* Устанавливается через CURLOPT_RESUME_FROM_LARGE */

    /* Тип передачи (ASCII/BINARY) */
    curl_easy_setopt(curl, CURLOPT_TRANSFERTEXT, 0L);  // бинарный режим

    /* Таймауты */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);       // без ограничения
  }
void setup_curl_for_ftps(CURL *curl, int implicit_mode)
  {
    /* Включаем TLS */
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    /* Explicit FTPS (AUTH TLS) */
//    if(!implicit_mode)
//      curl_easy_setopt(curl, CURLOPT_FTPSSLAUTH, CURLFTPAUTH_DEFAULT);
//    else /* Implicit FTPS — соединение сразу TLS */
//      curl_easy_setopt(curl, CURLOPT_FTPSSLAUTH, CURLFTPAUTH_SSL);
    curl_easy_setopt(curl, CURLOPT_FTPSSLAUTH, (implicit_mode ? CURLFTPAUTH_SSL : CURLFTPAUTH_DEFAULT));

      /* Проверка сертификатов */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    /* Если сервер использует самоподписанный сертификат:
       curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
       curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    */

    /* Пассивный режим */
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 1L);

    /* Тип передачи */
    curl_easy_setopt(curl, CURLOPT_TRANSFERTEXT, 0L);

    /* Таймауты */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
  }
void setup_curl_for_sftp(CURL *curl)
  {
    /* SFTP работает через libssh2, SSL не используется */
    /* Никаких FTP-настроек применять нельзя */

    /* Проверка known_hosts (если нужно) */
    /* curl_easy_setopt(curl, CURLOPT_SSH_KNOWNHOSTS, "/home/user/.ssh/known_hosts"); */

    /* Ключи SSH (если используются) */
    /*
    curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE, "/path/to/id_rsa");
    curl_easy_setopt(curl, CURLOPT_SSH_PUBLIC_KEYFILE,  "/path/to/id_rsa.pub");
    */

    /* Докачка */
    /* Работает через CURLOPT_RESUME_FROM_LARGE */

    /* Таймауты */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
  }
int tune_curl_for_protocol(CURL *curl, int protocol_code, const CONNECTION_CONFIG *cfg, curl_off_t resume_offset, int listing)
  {
    char *url_with_filename = 0;

    /* Общие настройки, которые нужны для всех протоколов */
    curl_easy_setopt(curl, CURLOPT_URL, cfg->url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, (cfg->user ? cfg->user : NET_LOGIN));
    curl_easy_setopt(curl, CURLOPT_PASSWORD, (cfg->password ? cfg->password : NET_PASSWORD));

    /* Порт задаётся вызывающим кодом */
    if(cfg->port > 0)
      curl_easy_setopt(curl, CURLOPT_PORT, cfg->port);

    /* LIST/NLST */
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, (listing ? 1L : 0L));

    if(!listing && cfg->filename)
      {
        url_with_filename = malloc(strlen(cfg->url) + strlen(cfg->filename) + 1 + 1);
        if(!url_with_filename)
          {
            fprintf(stderr, "malloc() failed\n");
            return NET_UNKNOWN_ERROR;
          }
        sprintf(url_with_filename, "%s%s%s", cfg->url, (*(cfg->url + strlen(cfg->url) - 1) != '/' ? "/" : ""), cfg->filename);
      }
    /* Докачка */
    if(resume_offset > 0)
      {
        if(ftp_supports_resume(curl, url_with_filename) != NET_OK)
          {
            if(url_with_filename)
              free(url_with_filename);
            return NET_CANT_RESUME;
          }
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resume_offset);
      }

    /* Теперь — протокольная настройка */
    switch (protocol_code)
      {
        case PROTOCOL_FTP_CODE:
          setup_curl_for_ftp(curl);
          break;
        case PROTOCOL_FTPS_CODE:
          /* Определяем implicit/explicit по порту */
          setup_curl_for_ftps(curl, cfg->port == 990);
          break;
        case PROTOCOL_SFTP_CODE:
          setup_curl_for_sftp(curl);
          break;
        default:
          if(url_with_filename)
            free(url_with_filename);
          return NET_UNKNOWN_ERROR;
      }

    if(url_with_filename)
      free(url_with_filename);

    return NET_OK;
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
    return (response == 350 || response == 250 || response == 213 || response == 125 || response == 150) ? NET_OK : NET_UNKNOWN_ERROR;
  } 
