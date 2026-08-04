// Inlines

inline const DirectX::XMFLOAT3& NetworkUpdate::GetWorldPos() const { return m_teamControls.m_mousePos; }

inline DirectX::XMFLOAT3& NetworkUpdate::GetWorldPos() { return m_teamControls.m_mousePos; }
