#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

#include "updater.h"

int progress_callback(void *, net_off_t, net_off_t, net_off_t, net_off_t);
size_t write_callback(void *, size_t, size_t, void *);
size_t write_data(void *, size_t, size_t, FILE *);

int get_listing(NET_HANDLE,const CONNECTION_CONFIG *);
int download_file(const char *, NET_HANDLE,const CONNECTION_CONFIG *);

int main(int argc, char *argv[])
  {
    int result = 0;
    NET_HANDLE net_handle = net_create();
    CONNECTION_CONFIG config = {0};
//      {
//        //.url = "ftp://ftp.scene.org/ls-lR"//,
//        .url = "ftp://ftp.scene.org/pub/mags/8-bit_memoirs/8bitmemoirs-issue1-english_.iso"//,
//        //.url = "ftp://ftp.scene.org/welcome.msg"//,
//        //.url = "ftp://ftp.scene.org/incoming/music/groups/kahvicollective/kahvi500_planet_boelex-twenty_(mp3).zip"//,
//        //.username = "username",
//        //.password = "password",
//        //.port = 21
//      };

    config.url = "ftp://ftp.scene.org/incoming/";
    get_listing(net_handle, &config);
    config.url = "ftp://ftp.scene.org/incoming/music/groups/kahvicollective/kahvi500_planet_boelex-twenty_(mp3).zip";
    download_file("",net_handle, &config);
    net_destroy(net_handle);
    return result;
  }

int progress_callback(void *clientp, net_off_t dltotal, net_off_t dlnow, net_off_t ultotal, net_off_t ulnow)
  {
#define TIME_INTERVAL 1
    static time_t start_time = 0;
    int current_time;

    if(!start_time)
      start_time = time(NULL);
    if((current_time = time(NULL)) - start_time < 1)return 0;
    if(dltotal > 0)
      {
        double progress = (double)dlnow / dltotal * 100.0;

        printf("\rDownload: %.2f%% (%lld/%lld bite)", progress, dlnow, dltotal);
      }
    start_time = current_time;
    return 0;
#undef TIME_INTERVAL
  }

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data)
  {
#define BLOCK_SIZE      1024
  size_t extra_size = size * nmemb;
    LISTING_BUFFER *lb = (LISTING_BUFFER *)data;
    char *ptr_new = (char *)realloc(lb->data, lb->size + extra_size + 1);

    if(!ptr_new)
      {
        fprintf(stderr, "realloc() failed\n");
        return 0;
      }
    lb->data = ptr_new;
    memcpy(lb->data + lb->size, ptr, extra_size);
    lb->size += extra_size;
    lb->data[lb->size] = '\0';
    return extra_size;
#undef BLOCK_SIZE
  }

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *file)
  {
    size_t res = fwrite(ptr, size, nmemb, file);

    if(ferror(file))
      perror("Write error");
    return res;
  }

int get_listing(NET_HANDLE net_handle,const CONNECTION_CONFIG *cfg)
  {
    LISTING_BUFFER listing = {.size = 0, .capacity = 0,.data = NULL};

    if(net_init(net_handle) != NET_OK)
      {
        fprintf(stderr, "Failed to initialize network handle\n");
        return 1;
      }
    net_get_listing(net_handle, cfg, write_callback, &listing);
    net_cleanup(net_handle);
    printf("Directory listing:\n\n%s\n", listing.data);
    if(listing.data)free(listing.data);
    return 0;
  }
int download_file(const char *filename, NET_HANDLE net_handle, const CONNECTION_CONFIG *cfg)
  {
    int result = 0;
    FILE *file = fopen("downloaded_file.", "wb");

    if(!file)
      {
        fprintf(stderr, "Failed to open file for writing\n");
        return 1;
      }
    if(net_init(net_handle) != NET_OK)
      {
        fprintf(stderr, "Failed to initialize network handle\n");
        return 1;
      }
    if(net_download(net_handle, cfg, (NET_FN_PROGRESS)progress_callback, (NET_FN_WRITE_CALLBACK)write_data, file) != NET_OK)
      {
        fprintf(stderr, "Failed to download file\n");
        result = 1;
      }
    net_cleanup(net_handle);
    fclose(file);
    return result;
  }
