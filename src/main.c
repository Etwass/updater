//#include <curl/curl.h>
#include <stdio.h>

#include "updater.h"

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *file)
  {
    size_t res = fwrite(ptr, size, nmemb, file);

    if(ferror(file))
      perror("Write error");
    return res;
  }

int main()
  {
    int result = 0;
    FILE *file;
    NET_HANDLE net_handle = net_create();
    CONNECTION_CONFIG config =
      {
        //.url = "ftp://ftp.scene.org/incoming/test/20221115_232625.mp4"//,
        .url = "ftp://ftp.scene.org/ls-lR"//,
        //.url = "ftp://ftp.scene.org/welcome.msg"//,
        //.url = "ftp://ftp.scene.org/incoming/music/groups/kahvicollective/kahvi500_planet_boelex-twenty_(mp3).zip"//,
        //.username = "username",
        //.password = "password",
        //.port = 21
      };

    file = fopen("downloaded_file.", "wb");
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
    if(net_download(net_handle, &config, (NET_FN_WRITE_CALLBACK)write_data, file) != NET_OK)
      {
        fprintf(stderr, "Failed to download file\n");
        result = 1;
      }
    fclose(file);
    net_cleanup(net_handle);
    net_destroy(net_handle);
    return result;
  }
