#include "pch.h"

#include <math.h>
#include <stdarg.h>

#include "MathUtils.h"
#include "SphereRenderer.h"
#include "TextRenderer.h"
#include "Debug.h"

#include "DebugRender.h"
#include "WorldPointers.h"


#ifdef DEBUG_RENDER_ENABLED


void RenderSquare2d(float x, float y, float size, RGBAColour const& _col)
{
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  int v[4];
  glGetIntegerv(GL_VIEWPORT, &v[0]);
  float left = v[0];
  float right = v[0] + v[2];
  float bottom = v[1] + v[3];
  float top = v[1];
  gluOrtho2D(left, right, bottom, top);
  glMatrixMode(GL_MODELVIEW);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  if (_col.a != 255)
  {
    glEnable(GL_BLEND);
  }

  glColor4ubv(_col.GetData());

  glBegin(GL_QUADS);
  glVertex2f(x - size, y - size);
  glVertex2f(x + size, y - size);
  glVertex2f(x + size, y + size);
  glVertex2f(x - size, y + size);
  glEnd();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
}


void RenderCube(DirectX::XMFLOAT3 const& _centre, float _sizeX, float _sizeY, float _sizeZ, RGBAColour const& _col)
{
  float halfX = _sizeX * 0.5f;
  float halfY = _sizeY * 0.5f;
  float halfZ = _sizeZ * 0.5f;

  if (_col.a != 255)
  {
    glEnable(GL_BLEND);
  }

  glColor4ubv(_col.GetData());
  glEnable(GL_LINE_SMOOTH);
  glLineWidth(2.0f);
  glBegin(GL_LINE_LOOP);
  glVertex3f(_centre.x - halfX, _centre.y - halfY, _centre.z - halfZ);
  glVertex3f(_centre.x - halfX, _centre.y + halfY, _centre.z - halfZ);
  glVertex3f(_centre.x + halfX, _centre.y + halfY, _centre.z - halfZ);
  glVertex3f(_centre.x + halfX, _centre.y - halfY, _centre.z - halfZ);
  glEnd();

  glBegin(GL_LINE_LOOP);
  glVertex3f(_centre.x - halfX, _centre.y - halfY, _centre.z + halfZ);
  glVertex3f(_centre.x - halfX, _centre.y + halfY, _centre.z + halfZ);
  glVertex3f(_centre.x + halfX, _centre.y + halfY, _centre.z + halfZ);
  glVertex3f(_centre.x + halfX, _centre.y - halfY, _centre.z + halfZ);
  glEnd();

  glBegin(GL_LINES);
  glVertex3f(_centre.x - halfX, _centre.y - halfY, _centre.z - halfZ);
  glVertex3f(_centre.x - halfX, _centre.y - halfY, _centre.z + halfZ);
  glEnd();

  glBegin(GL_LINES);
  glVertex3f(_centre.x - halfX, _centre.y + halfY, _centre.z - halfZ);
  glVertex3f(_centre.x - halfX, _centre.y + halfY, _centre.z + halfZ);
  glEnd();

  glBegin(GL_LINES);
  glVertex3f(_centre.x + halfX, _centre.y + halfY, _centre.z - halfZ);
  glVertex3f(_centre.x + halfX, _centre.y + halfY, _centre.z + halfZ);
  glEnd();

  glBegin(GL_LINES);
  glVertex3f(_centre.x + halfX, _centre.y - halfY, _centre.z - halfZ);
  glVertex3f(_centre.x + halfX, _centre.y - halfY, _centre.z + halfZ);
  glEnd();

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
}


void RenderSphereRings(DirectX::XMFLOAT3 const& _centre, float _radius, RGBAColour const& _col)
{
  glColor3ubv(_col.GetData());

  int const _segs = 20;
  float thetaIncrement = M_PI * 2.0f / (float)_segs;

  static float cx[_segs] = {-1};
  static float sx[_segs];

  int i;

  if (cx[0] == -1)
  {
    for (i = 0; i < _segs; i++)
    {
      float theta = (float)i * thetaIncrement;
      sx[i] = sinf(theta);
      cx[i] = cosf(theta);
    }
  }

  glLineWidth(2.0f);
  glBegin(GL_LINE_LOOP);
  for (i = 0; i < _segs; i++)
  {
    DirectX::XMFLOAT3 pos = _centre;
    pos.x += sx[i] * _radius;
    pos.z += cx[i] * _radius;
    glVertex3fv(&pos.x);
  }
  glEnd();
  glBegin(GL_LINE_LOOP);
  for (i = 0; i < _segs; i++)
  {
    DirectX::XMFLOAT3 pos = _centre;
    pos.y += sx[i] * _radius;
    pos.z += cx[i] * _radius;
    glVertex3fv(&pos.x);
  }
  glEnd();
  glBegin(GL_LINE_LOOP);
  for (i = 0; i < _segs; i++)
  {
    DirectX::XMFLOAT3 pos = _centre;
    pos.x += sx[i] * _radius;
    pos.y += cx[i] * _radius;
    glVertex3fv(&pos.x);
  }
  glEnd();
}


void RenderSphere(DirectX::XMFLOAT3 const& _centre, float _radius, RGBAColour const& _col)
{
  static Sphere aSphere;

  glColor3ubv(_col.GetData());
  aSphere.Render(_centre, _radius);
}


// A point on a circle of _radius about _centre, in the plane spanned by the two
// axes. The three loops below differ only in where the circle sits.
static DirectX::XMVECTOR RingPoint(DirectX::FXMVECTOR _centre, DirectX::FXMVECTOR _axis2, DirectX::FXMVECTOR _axis3, float _theta, float _radius)
{
  DirectX::XMVECTOR point = DirectX::XMVectorMultiplyAdd(_axis2, DirectX::XMVectorReplicate(sinf(_theta) * _radius), _centre);
  return DirectX::XMVectorMultiplyAdd(_axis3, DirectX::XMVectorReplicate(cosf(_theta) * _radius), point);
}


void RenderVerticalCylinder(DirectX::XMFLOAT3 const& _centreBase, DirectX::XMFLOAT3 const& _verticalAxis, float _height, float _radius,
                            RGBAColour const& _col)
{
  DirectX::XMVECTOR const axis1 = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&_verticalAxis));

  // Any vector not parallel to axis1 will do to start the basis off.
  DirectX::XMVECTOR seed = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
  if (DirectX::XMVectorGetX(axis1) > 0.5f)
    seed = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

  DirectX::XMVECTOR const axis3 = DirectX::XMVector3Cross(axis1, seed);
  DirectX::XMVECTOR const axis2 = DirectX::XMVector3Cross(axis1, axis3);

  DirectX::XMVECTOR const base = DirectX::XMLoadFloat3(&_centreBase);

  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_TEXTURE_2D);
  glLineWidth(1.0f);
  glColor3ubv(_col.GetData());

  int const numEdges = 16;

  // Base
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < numEdges; ++i)
  {
    float theta = M_PI * 2.0f * (float)i / (float)numEdges;
    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos, RingPoint(base, axis2, axis3, theta, _radius));
    glVertex3fv(&pos.x);
  }
  glEnd();

  // Top
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < numEdges; ++i)
  {
    float theta = M_PI * 2.0f * (float)i / (float)numEdges;
    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos,
                           RingPoint(DirectX::XMVectorMultiplyAdd(axis1, DirectX::XMVectorReplicate(_height), base), axis2, axis3, theta, _radius));
    glVertex3fv(&pos.x);
  }
  glEnd();

  // Middle
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < numEdges; ++i)
  {
    float theta = M_PI * 2.0f * (float)i / (float)numEdges;
    DirectX::XMVECTOR const onBase = RingPoint(base, axis2, axis3, theta, _radius);

    DirectX::XMFLOAT3 pos;
    DirectX::XMStoreFloat3(&pos, onBase);
    glVertex3fv(&pos.x);
    DirectX::XMStoreFloat3(&pos, DirectX::XMVectorMultiplyAdd(axis1, DirectX::XMVectorReplicate(_height), onBase));
    glVertex3fv(&pos.x);
  }
  glEnd();

  glEnable(GL_LIGHTING);
}


void RenderArrow(DirectX::XMFLOAT3 const& start, DirectX::XMFLOAT3 const& end, float width, RGBAColour const& _col /* =RGBAColour */)
{
  CameraAccess* cam = g_camera;
  DirectX::XMFLOAT3 const cameraPos = cam->GetPos();

  DirectX::XMVECTOR const startV = DirectX::XMLoadFloat3(&start);
  DirectX::XMVECTOR const endV = DirectX::XMLoadFloat3(&end);
  DirectX::XMVECTOR const camV = DirectX::XMLoadFloat3(&cameraPos);

  DirectX::XMVECTOR const midPoint = DirectX::XMVectorScale(DirectX::XMVectorAdd(startV, endV), 0.5f);
  float const midPointToCameraDist = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(camV, midPoint)));

  DirectX::XMVECTOR const startToEnd = DirectX::XMVectorSubtract(startV, endV);
  float const arrowLen = DirectX::XMVectorGetX(DirectX::XMVector3Length(startToEnd));
  DirectX::XMVECTOR const sidewaysDir = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(camV, endV), startToEnd));

  // SetLength on a zero-length vector used to leave (len,0,0) behind; a
  // degenerate arrow now yields a zero head instead, which draws nothing.
  DirectX::XMVECTOR const invDir = DirectX::XMVectorScale(DirectX::XMVector3Normalize(startToEnd), arrowLen * 0.2f);

  if (_col.a != 255)
  {
    glEnable(GL_BLEND);
  }

  glLineWidth(width / midPointToCameraDist * arrowLen);
  glColor3ubv(_col.GetData());

  glBegin(GL_LINES);
  glVertex3fv(&start.x);
  glVertex3fv(&end.x);

  DirectX::XMVECTOR const headBase = DirectX::XMVectorAdd(endV, invDir);
  DirectX::XMVECTOR const halfWidth = DirectX::XMVectorScale(sidewaysDir, arrowLen * 0.1f);

  DirectX::XMFLOAT3 p1, p2;
  DirectX::XMStoreFloat3(&p1, DirectX::XMVectorAdd(headBase, halfWidth));
  DirectX::XMStoreFloat3(&p2, DirectX::XMVectorSubtract(headBase, halfWidth));

  glVertex3fv(&p1.x);
  glVertex3fv(&end.x);
  glVertex3fv(&p2.x);
  glVertex3fv(&end.x);
  glEnd();

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
}


void RenderPointMarker(DirectX::XMFLOAT3 const& point, char const* _fmt, ...)
{
  char buf[512];
  va_list ap;
  va_start(ap, _fmt);
  vsprintf(buf, _fmt, ap);

  DirectX::XMFLOAT3 const end(point.x + 20.0f, point.y + 20.0f, point.z + 20.0f);
  RenderArrow(end, point, 2.0f);
  g_editorFont.DrawText3DCentre(end, 3.0f, buf);
}


void PrintMatrix(const char* _name, GLenum _whichMatrix)
{
  GLdouble matrix[16];

  glGetDoublev(_whichMatrix, matrix);

  DebugTrace("\tMatrix: %s\n", _name);
  for (int row = 0; row < 4; row++)
  {
    DebugTrace("\t\t");
    for (int col = 0; col < 4; col++)
      DebugTrace("% 13.1f ", matrix[col * 4 + row]);
    DebugTrace("\n");
  }
  DebugTrace("\n");
}

void PrintMatrices(const char* _title)
{
  static int numTimes = 0;
  numTimes++;
  if (numTimes > 10)
    return;

  DebugTrace("OpenGL:   "
             "%s\n",
             _title);
  PrintMatrix("Model View", GL_MODELVIEW_MATRIX);
  PrintMatrix("Projection", GL_PROJECTION_MATRIX);
}

#endif // ifdef DEBUG_RENDER_ENABLED
