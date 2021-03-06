//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2008      Torsten Rahn   <rahn@kde.org>
//


#include "GeoDataLinearRing.h"
#include "GeoDataLinearRing_p.h"

#include "MarbleMath.h"
#include "MarbleDebug.h"

namespace Marble
{

GeoDataLinearRing::GeoDataLinearRing( TessellationFlags f )
    : GeoDataLineString( new GeoDataLinearRingPrivate( f ) )
{
}

GeoDataLinearRing::GeoDataLinearRing( const GeoDataGeometry & other )
  : GeoDataLineString( other )
{
}

GeoDataLinearRing::~GeoDataLinearRing()
{
}

bool GeoDataLinearRing::isClosed() const
{
    return true;
}

qreal GeoDataLinearRing::length( qreal planetRadius, int offset ) const
{
    qreal  length = GeoDataLineString::length( planetRadius, offset );

    return length + planetRadius * distanceSphere( last(), first() );
}

bool GeoDataLinearRing::contains( const GeoDataCoordinates &coordinates ) const
{
    // Quick bounding box check
    if ( !latLonAltBox().contains( coordinates ) ) {
        return false;
    }

    int const points = size();
    bool inside = false; // also true for points = 0
    int j = points - 1;

    for ( int i=0; i<points; ++i ) {
        GeoDataCoordinates const & one = at( i );
        GeoDataCoordinates const & two = at( j );

        if ( ( one.longitude() < coordinates.longitude() && two.longitude() >= coordinates.longitude() ) ||
             ( two.longitude() < coordinates.longitude() && one.longitude() >= coordinates.longitude() ) ) {
            if ( one.latitude() + ( coordinates.longitude() - one.longitude()) / ( two.longitude() - one.longitude()) * ( two.latitude()-one.latitude() ) < coordinates.latitude() ) {
                inside = !inside;
            }
        }

        j = i;
    }

    return inside;
}

}
