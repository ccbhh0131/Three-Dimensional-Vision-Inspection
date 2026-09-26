#pragma once

#include <QVector3D>

namespace vision3d {

class BoundingBox
{
public:
    BoundingBox();

    void reset();
    bool isEmpty() const;

    void expand(const QVector3D& position);

    QVector3D minimum() const;
    QVector3D maximum() const;
    QVector3D center() const;
    QVector3D extent() const;

private:
    QVector3D m_minimum;
    QVector3D m_maximum;
    bool m_empty = true;
};

} // namespace vision3d
