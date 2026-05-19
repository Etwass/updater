#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <time.h>

#include "updater.h"


//"args": [
//  "--url",
//  "ftp://ftp.scene.org/",
//  "--user",
//  "ftp-93129.cloudx",
//  "--password",
//  "zD0eF5aT9s",
//  "--port",
//  "1500",
//  "--filename",
//  "welcome.msg",
//  "--progress",
//  "on",
//  "--listing",
//  "full",
//  "--help"
//]

void show_usage(const char *);

int progress_callback(void *, net_off_t, net_off_t, net_off_t, net_off_t);
size_t write_callback(void *, size_t, size_t, void *);
size_t write_data(void *, size_t, size_t, FILE *);

int get_listing(NET_HANDLE,const CONNECTION_CONFIG *);
int download_file(const char *, NET_HANDLE,const CONNECTION_CONFIG *);

int main(int argc, char *argv[])
  {
#define TASK_FLAG_LISTING 0x01
#define TASK_FLAG_DOWNLOAD 0x02
    NET_HANDLE net_handle;
    CONNECTION_CONFIG config = {0};
    int result = 0;
    int task_flags = 0;

    if(argc == 1)
      {
        show_usage(argv[0]);
        return 0;
      }
    for(int i = 1; i < argc; i++)
      {
        if(strcmp(argv[i], "--help") == 0)
          {
            show_usage(argv[0]);
            return 0;
          }
        if(strcmp(argv[i], "--url") == 0)
          config.url = argv[++i];
          else if(strcmp(argv[i], "--user") == 0)
            config.user = argv[++i];
            else if(strcmp(argv[i], "--password") == 0)
              config.password = argv[++i];
              else if(strcmp(argv[i], "--port") == 0)
                config.port = (unsigned int)atoi(argv[++i]);
                else if(strcmp(argv[i], "--filename") == 0)
                  {
                    config.filename = argv[++i];
                    task_flags |= TASK_FLAG_DOWNLOAD;
                  }
                  else if(strcmp(argv[i], "--progress") == 0)
                    config.progress = strcmp(argv[++i], "on") == 0 ? 1 : 0;
                    else if(strcmp("--listing",argv[i]) == 0)
                      {
                        config.listing_only = strcmp(argv[++i], "full") == 0 ? 0 : 1;
                        task_flags |= TASK_FLAG_LISTING;
                      }
      }
    if(task_flags == (TASK_FLAG_DOWNLOAD | TASK_FLAG_LISTING))
      {
        printf
          (
            "Error: --filename and --listing cannot be used together.\n"
            "Please specify only one operation.\n"
          );
        return 1;
      }
    net_handle = net_create();
    if(task_flags & TASK_FLAG_LISTING)get_listing(net_handle, &config);
    if(task_flags & TASK_FLAG_DOWNLOAD)download_file(config.filename, net_handle, &config);
    net_destroy(net_handle);
    return result;
#undef TASK_FLAG_DOWNLOAD
#undef TASK_FLAG_LISTING
  }

void show_usage(const char *prog_name)
  {
    printf
      (
        "Usage: %s [OPTIONS]\n"
        "Options:\n"
        "  --url <URL>             Specify the URL for FTP operations\n"
        "  --user <USERNAME>       Specify the username for FTP authentication\n"
        "  --password <PASSWORD>   Specify the password for FTP authentication\n"
        "  --port <PORT>           Specify the port number for FTP connection\n"
        "  --filename <filename>   Specify the filename to download\n"
        "  --progress <on|off>     Enable or disable progress display\n"
        "  --listing <full|short>  Select listing format\n"
        "  --help                  Show help and exit\n",
        prog_name
      );
  }

int progress_callback(void *clientp, net_off_t dltotal, net_off_t dlnow, net_off_t ultotal, net_off_t ulnow)
  {
#define TIME_INTERVAL 1
    static time_t start_time = 0;
    int current_time;

    if(!start_time)
      start_time = time(NULL);
    if((current_time = time(NULL)) - start_time < 1 && dlnow < dltotal)return 0;
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
    FILE *file = fopen(filename, "wb");

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
    if(net_download(net_handle, cfg, (NET_FN_PROGRESS)(cfg->progress?progress_callback:NULL), (NET_FN_WRITE_CALLBACK)write_data, file) != NET_OK)
      {
        fprintf(stderr, "Failed to download file\n");
        result = 1;
      }
    net_cleanup(net_handle);
    fclose(file);
    return result;
  }
