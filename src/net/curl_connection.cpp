#include "curl_connection.h"

CurlConnection::CurlConnection(const ConnectionConfig &cfg) : m_config(cfg)
  {
    // Глобальная инициализация cURL должна быть где-то в одном месте приложения:
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }

CURL *CurlConnection::createEasyHandle() const
  {
    CURL *curl = curl_easy_init();
    if(!curl)
      return nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, m_config.url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME, m_config.username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, m_config.password.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, m_config.connectTimeoutSec);

    if(m_config.transferTimeoutSec > 0)
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_config.transferTimeoutSec);

    if(m_config.useSsl)
      {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, m_config.verifyPeer ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, m_config.verifyHost ? 2L : 0L);
      }

    return curl;
  }
