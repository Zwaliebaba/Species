#pragma once


class Building;
class Engineer;


class LocationInput
{
private:
	void	AdvanceRadarDishControl(Building *_building);
	void	AdvanceNoSelection();
    void	AdvanceTeamControl();

public:

    bool    GetObjectUnderMouse( WorldObjectId &_id, int _teamId );

	void	Advance();
	void	Render();
};


