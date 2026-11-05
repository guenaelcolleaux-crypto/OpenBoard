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


#include "UBShapeRecognizer.h"

#include <QLineF>
#include <QRectF>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    qreal distance(const QPointF& a, const QPointF& b)
    {
        return QLineF(a, b).length();
    }

    qreal distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
    {
        const QPointF ab = b - a;
        const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();

        if (len2 <= 0)
            return distance(p, a);

        qreal t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
        t = qBound(qreal(0), t, qreal(1));
        return distance(p, a + t * ab);
    }

    qreal pathLength(const QList<QPointF>& points)
    {
        qreal length = 0;
        for (int i = 1; i < points.size(); ++i)
            length += distance(points[i - 1], points[i]);
        return length;
    }

    //! n points regularly spaced along the polyline
    QList<QPointF> resample(const QList<QPointF>& points, int n)
    {
        QList<QPointF> result;
        const qreal step = pathLength(points) / (n - 1);

        if (step <= 0)
            return result;

        result << points.first();
        qreal accumulated = 0;
        QPointF previous = points.first();

        for (int i = 1; i < points.size(); ++i)
        {
            QPointF current = points[i];
            qreal d = distance(previous, current);

            while (accumulated + d >= step && d > 0)
            {
                const qreal t = (step - accumulated) / d;
                const QPointF q = previous + t * (current - previous);
                result << q;
                previous = q;
                d = distance(previous, current);
                accumulated = 0;
            }

            accumulated += d;
            previous = current;
        }

        while (result.size() < n)
            result << points.last();

        return result.mid(0, n);
    }

    //! Ramer-Douglas-Peucker on an open polyline, keeping indexes
    void simplify(const QList<QPointF>& points, int first, int last, qreal epsilon, QList<int>& kept)
    {
        qreal maxDistance = 0;
        int index = -1;

        for (int i = first + 1; i < last; ++i)
        {
            const qreal d = distanceToSegment(points[i], points[first], points[last]);
            if (d > maxDistance)
            {
                maxDistance = d;
                index = i;
            }
        }

        if (index >= 0 && maxDistance > epsilon)
        {
            simplify(points, first, index, epsilon, kept);
            kept << index;
            simplify(points, index, last, epsilon, kept);
        }
    }

    //! interior angle at b, in degrees (180 = straight)
    qreal angleAt(const QPointF& a, const QPointF& b, const QPointF& c)
    {
        const QLineF ba(b, a);
        const QLineF bc(b, c);
        qreal angle = qAbs(ba.angleTo(bc));
        if (angle > 180)
            angle = 360 - angle;
        return angle;
    }

    qreal meanDistanceToPolygon(const QList<QPointF>& points, const QList<QPointF>& polygon)
    {
        qreal total = 0;

        for (const QPointF& p : points)
        {
            qreal best = std::numeric_limits<qreal>::max();
            for (int i = 0; i < polygon.size(); ++i)
                best = qMin(best, distanceToSegment(p, polygon[i], polygon[(i + 1) % polygon.size()]));
            total += best;
        }

        return total / points.size();
    }

    QPointF rotate(const QPointF& p, const QPointF& center, qreal degrees)
    {
        const qreal r = qDegreesToRadians(degrees);
        const qreal c = qCos(r), s = qSin(r);
        const QPointF d = p - center;
        return center + QPointF(d.x() * c - d.y() * s, d.x() * s + d.y() * c);
    }

    //! Makes the edges that are almost horizontal or vertical exactly so
    void straightenEdges(QList<QPointF>& vertices, qreal tolerance)
    {
        const int n = vertices.size();

        for (int i = 0; i < n; ++i)
        {
            QPointF& a = vertices[i];
            QPointF& b = vertices[(i + 1) % n];
            qreal angle = std::fmod(QLineF(a, b).angle(), 180.0);

            if (angle < tolerance || angle > 180 - tolerance)
            {
                const qreal y = (a.y() + b.y()) / 2;
                a.setY(y);
                b.setY(y);
            }
            else if (qAbs(angle - 90) < tolerance)
            {
                const qreal x = (a.x() + b.x()) / 2;
                a.setX(x);
                b.setX(x);
            }
        }
    }

    UBShapeRecognizer::Shape rectangleFrom(const QList<QPointF>& corners)
    {
        UBShapeRecognizer::Shape shape;

        QPointF center;
        for (const QPointF& p : corners)
            center += p;
        center /= corners.size();

        // mean orientation of the 4 edges, modulo 90 degrees (circular mean of 4*angle)
        qreal sx = 0, sy = 0;
        for (int i = 0; i < 4; ++i)
        {
            const qreal a = qDegreesToRadians(4 * QLineF(corners[i], corners[(i + 1) % 4]).angle());
            sx += qCos(a);
            sy += qSin(a);
        }
        qreal orientation = qRadiansToDegrees(qAtan2(sy, sx)) / 4; // between -45 and 45

        if (qAbs(orientation) < 10)
            orientation = 0;

        // in the frame of the rectangle, average the opposite sides
        QList<QPointF> local;
        for (const QPointF& p : corners)
            local << rotate(p, center, orientation);

        QList<qreal> xs, ys;
        for (const QPointF& p : local)
        {
            xs << p.x();
            ys << p.y();
        }
        std::sort(xs.begin(), xs.end());
        std::sort(ys.begin(), ys.end());

        qreal left = (xs[0] + xs[1]) / 2, right = (xs[2] + xs[3]) / 2;
        qreal top = (ys[0] + ys[1]) / 2, bottom = (ys[2] + ys[3]) / 2;
        qreal width = right - left, height = bottom - top;

        shape.type = UBShapeRecognizer::Shape::Rectangle;

        if (qAbs(width - height) / qMax(width, height) < 0.15)
        {
            const qreal side = (width + height) / 2;
            const QPointF c((left + right) / 2, (top + bottom) / 2);
            left = c.x() - side / 2; right = c.x() + side / 2;
            top = c.y() - side / 2; bottom = c.y() + side / 2;
            shape.type = UBShapeRecognizer::Shape::Square;
        }

        for (const QPointF& p : { QPointF(left, top), QPointF(right, top), QPointF(right, bottom), QPointF(left, bottom) })
            shape.vertices << rotate(p, center, -orientation);

        return shape;
    }
}

QList<QPointF> UBShapeRecognizer::Shape::outline(int ellipseSegments) const
{
    QList<QPointF> result;

    if (type == Circle || type == Ellipse)
    {
        for (int i = 0; i <= ellipseSegments; ++i)
        {
            const qreal t = 2 * M_PI * i / ellipseSegments;
            result << rotate(center + QPointF(radiusX * qCos(t), radiusY * qSin(t)), center, angle);
        }
    }
    else if (!vertices.isEmpty())
    {
        result = vertices;
        result << vertices.first();
    }

    return result;
}

UBShapeRecognizer::Shape UBShapeRecognizer::recognize(const QList<QPointF>& input, qreal minSize)
{
    Shape none;

    // remove duplicates
    QList<QPointF> points;
    for (const QPointF& p : input)
    {
        if (points.isEmpty() || distance(points.last(), p) > 0.01)
            points << p;
    }

    if (points.size() < 8)
        return none;

    QRectF box(points.first(), QSizeF(0, 0));
    for (const QPointF& p : points)
        box |= QRectF(p, QSizeF(0.001, 0.001));

    const qreal diagonal = QLineF(box.topLeft(), box.bottomRight()).length();
    if (diagonal < minSize)
        return none;

    // the drawing must be closed: the end comes back near the start
    // (or crosses the beginning of the stroke)
    qreal closingDistance = distance(points.first(), points.last());
    for (int i = 0; i < points.size() / 5; ++i)
        closingDistance = qMin(closingDistance, distance(points[i], points.last()));

    if (closingDistance > 0.2 * diagonal)
        return none;

    // when the end goes past the start, cut the part drawn twice
    {
        int cut = -1;
        qreal best = 0.2 * diagonal;
        for (int i = points.size() * 7 / 10; i < points.size(); ++i)
        {
            const qreal d = distance(points[i], points.first());
            if (d < best)
            {
                best = d;
                cut = i;
            }
        }
        if (cut > 0 && cut < points.size() - 1)
            points = points.mid(0, cut + 1);
    }

    QList<QPointF> closed = points;
    closed << points.first();

    const int n = 96;
    const QList<QPointF> samples = resample(closed, n + 1).mid(0, n);
    if (samples.size() < n)
        return none;

    // ---------------------------------------------------------------- ellipse fit
    QPointF centroid;
    for (const QPointF& p : samples)
        centroid += p;
    centroid /= n;

    qreal sxx = 0, syy = 0, sxy = 0;
    for (const QPointF& p : samples)
    {
        const QPointF d = p - centroid;
        sxx += d.x() * d.x();
        syy += d.y() * d.y();
        sxy += d.x() * d.y();
    }
    sxx /= n; syy /= n; sxy /= n;

    const qreal theta = 0.5 * qAtan2(2 * sxy, sxx - syy);       // major axis direction
    const qreal c = qCos(theta), s = qSin(theta);
    qreal varU = 0, varV = 0;
    for (const QPointF& p : samples)
    {
        const QPointF d = p - centroid;
        const qreal u = d.x() * c + d.y() * s;
        const qreal v = -d.x() * s + d.y() * c;
        varU += u * u;
        varV += v * v;
    }
    Q_UNUSED(varU);
    Q_UNUSED(varV);

    // radii and center from the extent of the drawing along the axes
    qreal minU = 0, maxU = 0, minV = 0, maxV = 0;
    for (const QPointF& p : samples)
    {
        const QPointF d = p - centroid;
        const qreal u = d.x() * c + d.y() * s;
        const qreal v = -d.x() * s + d.y() * c;
        minU = qMin(minU, u); maxU = qMax(maxU, u);
        minV = qMin(minV, v); maxV = qMax(maxV, v);
    }
    const qreal a = (maxU - minU) / 2;
    const qreal b = (maxV - minV) / 2;
    const qreal cu = (maxU + minU) / 2, cv = (maxV + minV) / 2;
    centroid += QPointF(cu * c - cv * s, cu * s + cv * c);

    qreal ellipseError = 1;
    if (a > 0 && b > 0)
    {
        ellipseError = 0;
        for (const QPointF& p : samples)
        {
            const QPointF d = p - centroid;
            const qreal u = d.x() * c + d.y() * s;
            const qreal v = -d.x() * s + d.y() * c;
            ellipseError += qAbs(qSqrt((u / a) * (u / a) + (v / b) * (v / b)) - 1);
        }
        ellipseError /= n;
    }

    // ---------------------------------------------------------------- polygon fit
    // start from the point the farthest from the center: always a corner for a polygon
    int start = 0;
    for (int i = 1; i < n; ++i)
    {
        if (distance(samples[i], centroid) > distance(samples[start], centroid))
            start = i;
    }

    QList<QPointF> loop;
    for (int i = 0; i <= n; ++i)
        loop << samples[(start + i) % n];

    QList<int> kept;
    kept << 0;
    simplify(loop, 0, n, 0.06 * diagonal, kept);
    std::sort(kept.begin(), kept.end());

    QList<QPointF> corners;
    for (int i : kept)
        corners << loop[i];

    // merge corners that are too close or too flat
    bool changed = true;
    while (changed && corners.size() > 3)
    {
        changed = false;
        for (int i = 0; i < corners.size(); ++i)
        {
            const QPointF& prev = corners[(i + corners.size() - 1) % corners.size()];
            const QPointF& next = corners[(i + 1) % corners.size()];

            if (angleAt(prev, corners[i], next) > 150 || distance(corners[i], next) < 0.12 * diagonal)
            {
                corners.removeAt(i);
                changed = true;
                break;
            }
        }
    }

    const qreal polygonError = (corners.size() >= 3) ? meanDistanceToPolygon(samples, corners) / diagonal : 1;

    // ---------------------------------------------------------------- decision
    Shape shape;

    const bool polygonCandidate = (corners.size() == 3 || corners.size() == 4) && polygonError < 0.03;

    // a clear polygon with more corners (pentagon...) is not turned into a circle
    if ((corners.size() == 5 || corners.size() == 6) && polygonError < 0.25 * ellipseError)
        return none;

    if (polygonCandidate && polygonError < 0.6 * ellipseError)
    {
        if (corners.size() == 3)
        {
            shape.type = Shape::Triangle;
            shape.vertices = corners;
            straightenEdges(shape.vertices, 6);
        }
        else
        {
            bool rightAngles = true;
            for (int i = 0; i < 4; ++i)
            {
                if (qAbs(angleAt(corners[(i + 3) % 4], corners[i], corners[(i + 1) % 4]) - 90) > 18)
                    rightAngles = false;
            }

            if (rightAngles)
                shape = rectangleFrom(corners);
            else
            {
                shape.type = Shape::Quadrilateral;
                shape.vertices = corners;
                straightenEdges(shape.vertices, 6);
            }
        }

        return shape;
    }

    if (ellipseError < 0.09)
    {
        shape.center = centroid;

        if (b / a > 0.8)
        {
            shape.type = Shape::Circle;
            shape.radiusX = shape.radiusY = (a + b) / 2;
        }
        else
        {
            shape.type = Shape::Ellipse;
            shape.radiusX = a;
            shape.radiusY = b;
            qreal angle = qRadiansToDegrees(theta);

            // horizontal or vertical when it is almost so
            const qreal snapped = qRound(angle / 90.0) * 90.0;
            if (qAbs(angle - snapped) < 12)
                angle = snapped;

            shape.angle = angle;
        }

        return shape;
    }

    return none;
}
