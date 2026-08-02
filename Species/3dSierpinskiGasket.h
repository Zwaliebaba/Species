#pragma once


class Sierpinski3D
{
public:
	Vector3			*m_points;
	unsigned int	m_numPoints;

	Sierpinski3D(unsigned int _numPoints);
	~Sierpinski3D();

	void Render(float _scale);
};



