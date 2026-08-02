#pragma once


#include "Building.h"


class UpgradePort: public Building
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

