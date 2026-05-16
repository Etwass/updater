#include "updater.h"

#include <cstdio>

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
  {
    return fwrite(ptr, size, nmemb, stream);
  }

int main()
  {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    const char *url = "ftp://ftp.scene.org/pub/graphics/ascii/5th_dynasty/5d!arop2.txt";
    const char *outfilename = "file.txt";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl)
      {
        fp = fopen(outfilename, "wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        // Если требуется аутентификация:
        //curl_easy_setopt(curl, CURLOPT_USERNAME, "ftp-93129.cloudx");
        //curl_easy_setopt(curl, CURLOPT_USERPWD, "zD0eF5aT9s");
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        if(res != CURLE_OK)
          {
            fprintf(stderr, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
            return 1;
          }
      }
    curl_global_cleanup();
    return 0;
  }
