#pragma once

#include <curl/curl.h>

CURLcode connectToServer(const char *url);
CURLcode downloadFile(const char *url, const char *outfilename);
void disconnectFromServer(CURL *curl);

class Updater final
  {
    public:
      Updater(const char *url, const char *outfilename);
      Updater(const char *url, unsigned int port, const char *outfilename);
      ~Updater();
      bool update();
      private:
      CURL *curl;
      const char *url;
      const char *outfilename;
  };
