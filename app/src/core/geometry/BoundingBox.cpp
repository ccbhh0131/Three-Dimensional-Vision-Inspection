#include "BoundingBox.h"

#include <algorithm>
#include <cmath>

namespace vision3d {

BoundingBox::BoundingBox()
{
    reset();
}

void BoundingBox::reset()
{
    m_minimum = QVector3D(0.0f, 0.0f, 0.0f);
    m_maximum = QVector3D(0.0f, 0.0f, 0.0f);
    m_empty = true;
}

bool BoundingBox::isEmpty() const
{
    return m_empty;
}

void BoundingBox::expand(const QVector3D& position)
{
    if (!std::isfinite(position.x())
        || !std::isfinite(position.y())
        || !std::isfinite(position.z())) {
        return;
    }

    if (m_empty) {
        m_minimum = position;
        m_maximum = position;
        m_empty = false;
        return;
    }

    m_minimum.setX(std::min(m_minimum.x(), position.x()));
    m_minimum.setY(std::min(m_minimum.y(), position.y()));
    m_minimum.setZ(std::min(m_minimum.z(), position.z()));
    m_maximum.setX(std::max(m_maximum.x(), position.x()));
    m_maximum.setY(std::max(m_maximum.y(), position.y()));
    m_maximum.setZ(std::max(m_maximum.z(), position.z()));
}

QVector3D BoundingBox::minimum() const
{
    return m_minimum;
}

QVector3D BoundingBox::maximum() const
{
    return m_maximum;
}

QVector3D BoundingBox::center() const
{
    if (m_empty) {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }
    return (m_minimum + m_maximum) * 0.5f;
}

QVector3D BoundingBox::extent() const
{
    if (m_empty) {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }
    return m_maximum - m_minimum;
}

} // namespace vision3d
