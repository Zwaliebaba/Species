#ifndef _included_powerstation_h
#define _included_powerstation_h

#include "Building.h"


class Shape;
class ShapeFragment;
class ShapeMarker;


class Powerstation : public Building
{
protected:
    int					m_linkedBuildingId;

public:
    Powerstation		();

	void Initialise		(Building *_template);

    bool Advance		();
    void Render			(float predictionTime);

    int  GetBuildingLink();
    void SetBuildingLink(int _buildingId);

	void Read( TextReader *_in, bool _dynamic );
	void Write( FileWriter *out );
};


#endif
