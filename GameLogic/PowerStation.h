#pragma once

#include "Building.h"


namespace Neuron
{
  class Shape;
  class ShapeFragment;
  class ShapeMarker;
} // namespace Neuron


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


