/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef UBSHAPERECOGNIZER_H
#define UBSHAPERECOGNIZER_H

#include <QList>
#include <QPointF>

/**
 * @brief Recognizes simple hand-drawn shapes (circle, ellipse, triangle,
 * rectangle, square, quadrilateral) and gives a clean version of them.
 */
class UBShapeRecognizer
{
    public:
        struct Shape
        {
            enum Type { None, Circle, Ellipse, Triangle, Rectangle, Square, Quadrilateral };

            Type type = None;
            QList<QPointF> vertices;   // polygons: the corners
            QPointF center;            // circle / ellipse
            qreal radiusX = 0;
            qreal radiusY = 0;
            qreal angle = 0;           // ellipse rotation, in degrees

            //! Points of the outline, closed (first point repeated at the end)
            QList<QPointF> outline(int ellipseSegments = 96) const;
        };

        //! @param minSize minimal diagonal of the drawing, in the same unit as the points
        static Shape recognize(const QList<QPointF>& points, qreal minSize);
};

#endif // UBSHAPERECOGNIZER_H
