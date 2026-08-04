
#pragma once

#include <vector>

#include "Building.h"


class Tree : public Building
{
protected:
    int     m_branchDisplayListId;
    int     m_leafDisplayListId;
    void    RenderBranch   ( Vector3 _from, Vector3 _to, int _iterations,
                             bool _calcRadius, bool _renderBranch, bool _renderLeaf );

    Vector3 m_hitcheckCentre;
    float   m_hitcheckRadius;
    int     m_numLeafs;

    float   m_fireDamage;
    float   m_onFire;
    bool    m_burnSoundPlaying;

    float   GetActualHeight( float _predictionTime );

	unsigned char m_leafColourArray[4];
	unsigned char m_branchColourArray[4];

public:
    float   m_height;
    float   m_budsize;
    float   m_pushUp;
    float   m_pushOut;
    int     m_iterations;
    int     m_seed;
    int     m_leafColour;
    int     m_branchColour;
	int		m_leafDropRate;

public:
    Tree();
	~Tree();

    void Initialise     ( Building *_template );
    void SetDetail      ( int _detail );

    bool Advance        ();

	void DeleteDisplayLists();
    void Generate       ();
    void Render         ( float _predictionTime );
    void RenderAlphas   ( float _predictionTime );
    void RenderHitCheck ();

    bool PerformDepthSort(DirectX::XMFLOAT3& _centrePos);

    void Damage         ( float _damage );

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);
    bool DoesShapeHit(Shape* _shape, DirectX::XMFLOAT4X4 _transform);
    bool DoesRayHit(DirectX::XMFLOAT3 const& _rayStart, DirectX::XMFLOAT3 const& _rayDir, float _rayLen = 1e10, DirectX::XMFLOAT3* _pos = nullptr,
                    DirectX::XMFLOAT3* _norm = nullptr); // pos/norm will not always be available

    void ListSoundEvents(std::vector<const char*>* _list);

    void Read           ( TextReader *_in, bool _dynamic );
    void Write          ( FileWriter *_out );
};


