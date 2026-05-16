#pragma once

#include "connection_config.h"
#include <curl/curl.h>

class CurlConnection
  {
    public:
      explicit CurlConnection(const ConnectionConfig &cfg);

      CURL *createEasyHandle() const;
      const ConnectionConfig &config() const { return m_config; }

    private:
      ConnectionConfig m_config;
  };
