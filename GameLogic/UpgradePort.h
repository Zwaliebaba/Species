#pragma once


#include "Building.h"


namespace Species
{
  class UpgradePort : public Building
  {
    public:
      UpgradePort();
  };


  class PrimaryUpgradePort : public Building
  {
    public:
      int m_controlTowersOwned;

    public:
      PrimaryUpgradePort();

      void ReprogramComplete();
  };
} // namespace Species
