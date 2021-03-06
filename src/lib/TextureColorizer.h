//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2004-2007 Torsten Rahn <tackat@kde.org>
// Copyright 2007      Inge Wallin  <ingwa@kde.org>
//

//
// The TextureColorizer maps the Elevationvalues to Legend Colors.
//

#ifndef MARBLE_TEXTURECOLORIZER_H
#define MARBLE_TEXTURECOLORIZER_H

#include "MarbleGlobal.h"
#include "GeoDataDocument.h"
#include "GeoPainter.h"

#include <QString>
#include <QImage>
#include <QPen>
#include <QBrush>

namespace Marble
{

class VectorComposer;
class ViewportParams;

class TextureColorizer
{
 public:
    TextureColorizer( const QString &seafile,
                      const QString &landfile,
                      VectorComposer *veccomposer );

    virtual ~TextureColorizer(){}

    void addSeaDocument( const GeoDataDocument *seaDocument );

    void addLandDocument( const GeoDataDocument *landDocument );

    void setShowRelief( bool show );

    void drawIndividualDocument( GeoPainter *painter, const GeoDataDocument *document );

    void drawTextureMap( GeoPainter *painter );

    void colorize( QImage *origimg, const ViewportParams *viewport, MapQuality mapQuality );

    void setPixel( const QRgb *coastData, QRgb *writeData, int bump, uchar grey );

 private:
    VectorComposer *const m_veccomposer;
    QList<const GeoDataDocument*> m_seaDocuments;
    QList<const GeoDataDocument*> m_landDocuments;
    QImage m_coastImage;
    uint texturepalette[16][512];
    bool m_showRelief;
    QRgb      m_landColor;
    QRgb      m_seaColor;
};

}

#endif
