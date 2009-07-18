//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2009      Bastian Holst <bastianholst@gmx.de>
//

// Self
#include "AbstractDataPluginModel.h"

// Qt
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QPointF>
#include <QtCore/QtAlgorithms>
#include <QtCore/QVariant>

// Marble
#include "AbstractDataPluginItem.h"
#include "CacheStoragePolicy.h"
#include "GeoDataCoordinates.h"
#include "GeoDataLatLonAltBox.h"
#include "HttpDownloadManager.h"
#include "MarbleDataFacade.h"
#include "MarbleDirs.h"
#include "ViewportParams.h"

#include <cmath>

namespace Marble {

const QString descriptionPrefix( "description_" );

// Time between two tried description file downloads (we decided not to download anything) in ms
const int timeBetweenTriedDownloads = 500;
// Time between two real description file downloads in ms
const int timeBetweenDownloads = 1500;

// The factor describing how much the box has to be changed to download a new description file.
// A higher factor means more downloads.
const qreal boxComparisonFactor = 16.0;

// Separator to separate the id of the item from the file type
const char fileIdSeparator = '_';
    
class AbstractDataPluginModelPrivate {
 public:
    AbstractDataPluginModelPrivate( const QString& name, AbstractDataPluginModel * parent )
        : m_parent( parent ),
          m_name( name ),
          m_lastBox(),
          m_downloadedBox(),
          m_lastNumber( 0 ),
          m_downloadedNumber( 0 ),
          m_lastDataFacade( 0 ),
          m_downloadTimer( new QTimer( m_parent ) ),
          m_descriptionFileNumber( 0 ),
          m_itemSettings(),
          m_downloadManager( 0 )
    {
    }
    
    ~AbstractDataPluginModelPrivate() {
        QList<AbstractDataPluginItem*>::iterator lIt;
        for( lIt = m_itemSet.begin(); lIt != m_itemSet.end(); ++lIt ) {
            (*lIt)->deleteLater();
        }
        
        QHash<QString,AbstractDataPluginItem*>::iterator hIt;
        for( hIt = m_downloadingItems.begin(); hIt != m_downloadingItems.end(); ++hIt ) {
            (*hIt)->deleteLater();
        }
        
        m_downloadManager->storagePolicy()->clearCache();
        delete m_downloadManager;
    }
    
    AbstractDataPluginModel *m_parent;
    QString m_name;
    GeoDataLatLonAltBox m_lastBox;
    GeoDataLatLonAltBox m_downloadedBox;
    qint32 m_lastNumber;
    qint32 m_downloadedNumber;
    MarbleDataFacade *m_lastDataFacade;
    QString m_downloadedTarget;
    QList<AbstractDataPluginItem*> m_itemSet;
    QHash<QString, AbstractDataPluginItem*> m_downloadingItems;
    QList<AbstractDataPluginItem*> m_displayedItems;
    QTimer *m_downloadTimer;
    quint32 m_descriptionFileNumber;
    QHash<QString, QVariant> m_itemSettings;

    HttpDownloadManager *m_downloadManager;
};

AbstractDataPluginModel::AbstractDataPluginModel( const QString& name, QObject *parent )
    : QObject(  parent ),
      d( new AbstractDataPluginModelPrivate( name, this ) )
{
    // Initializing file and download System
    CacheStoragePolicy *storagePolicy = new CacheStoragePolicy( MarbleDirs::localPath()
                                                                + "/cache/" + d->m_name + '/' );
    d->m_downloadManager = new HttpDownloadManager( QUrl(),
                                                    storagePolicy );
    connect( d->m_downloadManager, SIGNAL( downloadComplete( QString, QString ) ),
             this,                 SLOT( processFinishedJob( QString , QString ) ) );
    
    // We want to download a new description file every timeBetweenDownloads ms
    connect( d->m_downloadTimer, SIGNAL( timeout() ),
             this,               SLOT( handleChangedViewport() ),
             Qt::QueuedConnection );
    d->m_downloadTimer->start( timeBetweenDownloads );
}

AbstractDataPluginModel::~AbstractDataPluginModel() {
    delete d;
}

QList<AbstractDataPluginItem*> AbstractDataPluginModel::items( ViewportParams *viewport,
                                                               MarbleDataFacade *facade,
                                                               qint32 number )
{
    GeoDataLatLonAltBox currentBox = viewport->viewLatLonAltBox();
    QString target = facade->target();
    QList<AbstractDataPluginItem*> list;
    
    QList<AbstractDataPluginItem*>::iterator i;
    
    d->m_displayedItems.removeAll( 0 );
    
    // Items that are already shown have the highest priority
    for ( i = d->m_displayedItems.begin();
          i != d->m_displayedItems.end() && list.size() < number;
          ++i )
    {
        // Don't try to access an object that doesn't exist
        if( !*i ) {
            continue;
        }
    
        // Only show items that are initialized
        if( !(*i)->initialized() ) {
            continue;
        }
        
        // Only show items that are on the current planet
        if( (*i)->target() != target ) {
            continue;
        }
        
        if( !currentBox.contains( (*i)->coordinate() ) ) {
            continue;
        }
        
        // If the item was added initially at a nearer position, they don't have priority,
        // because we zoomed out since then.
        if( (*i)->addedAngularResolution() >= viewport->angularResolution() ) {
            list.append( *i );
            (*i)->setSettings( d->m_itemSettings );
        }
    }
        
    d->m_itemSet.removeAll( 0 );
    
    for ( i = d->m_itemSet.begin(); i != d->m_itemSet.end() && list.size() < number; ++i ) {
        // Don't try to access an object that doesn't exist
        if( !*i ) {
            qDebug() << "Warning: Null pointer in m_itemSet";
            continue;
        }
        
        // Only show items that are initialized
        if( !(*i)->initialized() ) {
            continue;
        }
        
        // Only show items that are on the current planet
        if( (*i)->target() != target ) {
            continue;
        }
        
        // If the item is on the viewport, we want to return it
        if( currentBox.contains( (*i)->coordinate() )
            && !list.contains( *i ) )
        {
            list.append( *i );
            (*i)->setSettings( d->m_itemSettings );
            
            // We want to save the angular resolution of the first time the item got added.
            // If it is in the list of displayedItems, it was added before
            if( !d->m_displayedItems.contains( *i ) ) {
                (*i)->setAddedAngularResolution( viewport->angularResolution() );
            }
        }
        // FIXME: We have to do something if the item that is not on the viewport.
    }
    
    d->m_lastDataFacade = facade;
    
    if( (!(currentBox == d->m_lastBox)
          || number != d->m_lastNumber ) )
    {
        d->m_lastBox = currentBox;
        d->m_lastNumber = number;
        d->m_lastDataFacade = facade;
    }
    else {
    }
    
    d->m_displayedItems = list;
    return list;
}

QList<AbstractDataPluginItem *> AbstractDataPluginModel::whichItemAt( const QPoint& curpos ) {
    QList<AbstractDataPluginItem *> itemsAt;
    
    foreach( AbstractDataPluginItem* item, d->m_displayedItems ) {
        if( item && item->contains( QPointF( curpos ) ) )
            itemsAt.append( item );
    }
    
    return itemsAt;
}

void AbstractDataPluginModel::downloadItemData( const QUrl& url,
                                                const QString& type,
                                                AbstractDataPluginItem *item )
{
    if( !item ) {
        return;
    }

    QString id = generateFilename( item->id(), type );
    
    d->m_downloadManager->addJob( url, id, id );
    d->m_downloadingItems.insert( id, item );
    
    connect( item, SIGNAL( destroyed( QObject* ) ), this, SLOT( removeItem( QObject* ) ) );

    addItemToList( item );
}

void AbstractDataPluginModel::downloadDescriptionFile( const QUrl& url ) {    
    if( !url.isEmpty() ) {  
        QString name( descriptionPrefix );
        name += QString::number( d->m_descriptionFileNumber );
        
        d->m_downloadManager->addJob( url, name, name );
        d->m_descriptionFileNumber++;
    }
}

static bool lessThanByPointer( const AbstractDataPluginItem *item1,
                               const AbstractDataPluginItem *item2 )
{
    if( item1 != 0 && item2 != 0 ) {
        return item1->operator<( item2 );
    }
    else {
        return false;
    }
}

void AbstractDataPluginModel::addItemToList( AbstractDataPluginItem *item ) {
    if( !item ) {
        return;
    }

    // If the item is already in our list, don't add it.
    if( AbstractDataPluginItem *oldItem = findItem( item->id() ) ) {
        if ( oldItem == item ) {
            return;
        }
        else {
            item->deleteLater();
            return;
        }
    }
    
    qDebug() << "New item " << item->id();
    
    // This find the right position in the sorted to insert the new item 
    QList<AbstractDataPluginItem*>::iterator i = qLowerBound( d->m_itemSet.begin(),
                                                                d->m_itemSet.end(),
                                                                item,
                                                                lessThanByPointer );
    // Insert the item on the right position in the list
    d->m_itemSet.insert( i, item );
    
    connect( item, SIGNAL( destroyed( QObject* ) ), this, SLOT( removeItem( QObject* ) ) );
}

QString AbstractDataPluginModel::name() const {
    return d->m_name;
}

void AbstractDataPluginModel::setName( const QString& name ) {
    d->m_name = name;
}

QString AbstractDataPluginModel::generateFilename( const QString& id, const QString& type ) const {
    QString name;
    name += id;
    name += fileIdSeparator;
    name += type;
    
    return name;
}

QString AbstractDataPluginModel::generateFilepath( const QString& id, const QString& type ) const {
    return MarbleDirs::localPath() + "/cache/" + d->m_name + '/' + generateFilename( id, type );
}
    
bool AbstractDataPluginModel::fileExists( const QString& fileName ) const {
    return d->m_downloadManager->storagePolicy()->fileExists( fileName );
}

bool AbstractDataPluginModel::fileExists( const QString& id, const QString& type ) const {
    return fileExists( generateFilename( id, type ) );
}

AbstractDataPluginItem *AbstractDataPluginModel::findItem( const QString& id ) const {
    QList<AbstractDataPluginItem*>::iterator listIt;
    
    for( listIt = d->m_itemSet.begin();
         listIt != d->m_itemSet.end();
         ++listIt )
    {
        if( (*listIt)->id() == id ) {
            return (*listIt);
        }
    }
    
    return 0;
}

bool AbstractDataPluginModel::itemExists( const QString& id ) const {
    if ( findItem( id ) ) {
        return true;
    }
    else {
        return false;
    }
}

void AbstractDataPluginModel::setItemSettings( QHash<QString,QVariant> itemSettings ) {
    d->m_itemSettings = itemSettings;
}

void AbstractDataPluginModel::handleChangedViewport() {
    if( !d->m_lastDataFacade ) {
        return;
    }
    
    // All this is to prevent to often downloads
    if( d->m_lastNumber != 0
        // We don't need to download if nothing changed
        && ( !( d->m_downloadedBox == d->m_lastBox )
             || d->m_downloadedNumber != d->m_lastNumber
             || d->m_downloadedTarget != d->m_lastDataFacade->target() )
        // We try to filter little changes of the bounding box
        && ( fabs( d->m_downloadedBox.east() - d->m_lastBox.east() ) * boxComparisonFactor
                                > d->m_lastBox.width()
             || fabs( d->m_downloadedBox.south() - d->m_lastBox.south() ) * boxComparisonFactor
                                > d->m_lastBox.height()
             || fabs( d->m_downloadedBox.north() - d->m_lastBox.north() ) * boxComparisonFactor
                                > d->m_lastBox.height()
             || fabs( d->m_downloadedBox.west() - d->m_lastBox.west() ) * boxComparisonFactor
                                > d->m_lastBox.width() ) )
    {
        // We will wait a littlebit longer to start the the
        // next download as we will really download something now.
        d->m_downloadTimer->setInterval( timeBetweenDownloads );
        
        // Save the download parameter
        d->m_downloadedBox = d->m_lastBox;
        d->m_downloadedNumber = d->m_lastNumber;
        d->m_downloadedTarget = d->m_lastDataFacade->target();
        
        // Get items
        getAdditionalItems( d->m_lastBox, d->m_lastDataFacade, d->m_lastNumber );
    }
    else {
        // Don't wait to long to start the next download as we decided not to download anything.
        // This will enhance response.
        d->m_downloadTimer->setInterval( timeBetweenTriedDownloads );
    }
}

void AbstractDataPluginModel::processFinishedJob( const QString& relativeUrlString,
                                                  const QString& id ) {
    Q_UNUSED( relativeUrlString );
    
    if( id.startsWith( descriptionPrefix ) ) {
        CacheStoragePolicy *storagePolicy
                = qobject_cast<CacheStoragePolicy*>( d->m_downloadManager->storagePolicy() );
        if ( storagePolicy ) {
            parseFile( storagePolicy->data( id ) );
        }
    }
    else {
        // The downloaded file contains item data.
        
        // Splitting the id in itemId and fileType
        QStringList fileInformation = id.split( fileIdSeparator );
        
        if( fileInformation.size() < 2) {
            qDebug() << "Strange file information " << id;
            return;
        }
        QString itemId = fileInformation.at( 0 );
        fileInformation.removeAt( 0 );
        QString fileType = fileInformation.join( QString( fileIdSeparator ) );
        
        // Searching for the right item in m_downloadingItems
        QHash<QString, AbstractDataPluginItem *>::iterator i = d->m_downloadingItems.find( id );
        if( i != d->m_downloadingItems.end() ) {
            if( itemId != (*i)->id() ) {
                return;
            }
            
            (*i)->addDownloadedFile( generateFilepath( itemId, fileType ), 
                                     fileType );

            d->m_downloadingItems.erase( i );
        }
    }
}

void AbstractDataPluginModel::removeItem( QObject *item ) {
    d->m_itemSet.removeAll( (AbstractDataPluginItem *) item );
    QHash<QString, AbstractDataPluginItem *>::iterator i;
    for( i = d->m_downloadingItems.begin(); i != d->m_downloadingItems.end(); ++i ) {
        if( (*i) == (AbstractDataPluginItem *) item ) {
            d->m_downloadingItems.erase( i );
        }
    }
}
    

} // namespace Marble

#include "AbstractDataPluginModel.moc"
