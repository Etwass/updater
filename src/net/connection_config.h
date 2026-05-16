#pragma once

#include <string>

struct ConnectionConfig
  {
    std::string url;
    std::string username;
    std::string password;

    long connectTimeoutSec = 10;
    long transferTimeoutSec = 0; // 0 = без ограничения

    bool useSsl = false;
    bool verifyPeer = true;
    bool verifyHost = true;
  };
