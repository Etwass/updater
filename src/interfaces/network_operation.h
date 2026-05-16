#pragma once

class INetworkOperation
  {
    public:
      virtual ~INetworkOperation() = default;
      virtual void execute() = 0;
  };
