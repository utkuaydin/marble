//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2009      Bastian Holst <bastianholst@gmx.de>
//

#ifndef PHOTOPLUGINMODEL_H
#define PHOTOPLUGINMODEL_H

#include "AbstractDataPluginModel.h"

namespace Marble {

class MarbleDataFacade;

const quint32 numberOfImagesPerFetch = 15;
  
class PhotoPluginModel : public AbstractDataPluginModel
{
    Q_OBJECT
    
 public:
    PhotoPluginModel( QObject *parent = 0 );
 
 protected:
    /**
     * Generates the download url for the description file from the web service depending on
     * the @p box surrounding the view and the @p number of files to show.
     **/
    QUrl descriptionFileUrl( GeoDataLatLonAltBox *box,
                             MarbleDataFacade *facade,
                             qint32 number = 10 );
       
    /**
     * The reimplementation has to parse the @p file and should generate widgets. This widgets
     * have to be scheduled to downloadWidgetData or could be directly added to the list,
     * depending on if they have to download information to be shown.
     **/
    void parseFile( QByteArray file );
};

}

#endif //PHOTOPLUGINMODEL_H
