//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2009      Bastian Holst <bastianholst@gmx.de>
//

#ifndef KMLMINALTITUDETAGHANDLER_H
#define KMLMINALTITUDETAGHANDLER_H

#include "GeoTagHandler.h"

namespace Marble
{
namespace kml
{

class KmlminAltitudeTagHandler : public GeoTagHandler
{
 public:
    virtual GeoNode* parse(GeoParser&) const;
};

}
}

#endif // KMLMINALTITUDETAGHANDLER_H
