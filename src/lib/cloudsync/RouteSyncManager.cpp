//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2013   Utku Aydın <utkuaydin34@gmail.com>
//

#include "RouteSyncManager.h"

#include "GeoParser.h"
#include "MarbleDirs.h"
#include "MarbleDebug.h"
#include "RouteParser.h"
#include "GeoDataFolder.h"
#include "GeoDataDocument.h"
#include "GeoDataPlacemark.h"
#include "CloudRoutesDialog.h"
#include "OwncloudSyncBackend.h"

#include <QDir>
#include <QUrl>
#include <QFile>
#include <QTimer>
#include <QBuffer>
#include <QPointer>
#include <QScriptValue>
#include <QScriptEngine>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QNetworkAccessManager>

namespace Marble
{

/**
 * Private class for RouteSyncManager.
 */
class RouteSyncManager::Private {
public:
    Private( CloudSyncManager *cloudSyncManager, RoutingManager *routingManager );

    QProgressDialog *m_uploadProgressDialog;
    CloudSyncManager *m_cloudSyncManager;
    RoutingManager *m_routingManager;
    CloudRouteModel *m_model;

    QDir m_cacheDir;
    OwncloudSyncBackend *m_owncloudBackend;
};

RouteSyncManager::Private::Private( CloudSyncManager *cloudSyncManager, RoutingManager *routingManager ) :
    m_uploadProgressDialog( new QProgressDialog() ),
    m_cloudSyncManager( cloudSyncManager ),
    m_routingManager( routingManager ),
    m_model( new CloudRouteModel() )
{
    m_cacheDir = QDir( MarbleDirs::localPath() + "/cloudsync/cache/routes/" );
    m_owncloudBackend = new OwncloudSyncBackend( m_cloudSyncManager->apiUrl() );
    
    m_uploadProgressDialog->setMinimum( 0 );
    m_uploadProgressDialog->setMaximum( 100 );
    m_uploadProgressDialog->setWindowTitle( tr( "Uploading route..." ) );
}

RouteSyncManager::RouteSyncManager( CloudSyncManager *cloudSyncManager, RoutingManager *routingManager ) :
    d( new Private( cloudSyncManager, routingManager ) )
{
}

RouteSyncManager::~RouteSyncManager()
{
    delete d;
}

CloudRouteModel* RouteSyncManager::model()
{
    return d->m_model;
}

QString RouteSyncManager::generateTimestamp() const
{
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    return QString::number( timestamp );
}

QString RouteSyncManager::saveDisplayedToCache() const
{
    d->m_cacheDir.mkpath( d->m_cacheDir.absolutePath() );
    
    const QString timestamp = generateTimestamp();
    const QString filename = d->m_cacheDir.absolutePath() + "/" + timestamp + ".kml";
    d->m_routingManager->saveRoute( filename );
    return timestamp;
}

void RouteSyncManager::uploadRoute()
{
    if( !d->m_cloudSyncManager->offlineMode() ) {
        d->m_owncloudBackend->uploadRoute( saveDisplayedToCache() );
        connect( d->m_owncloudBackend, SIGNAL(routeUploadProgress(qint64,qint64)),
                 this, SLOT(updateUploadProgressbar(qint64,qint64)) );
        // FIXME connect( d->m_uploadProgressDialog, SIGNAL(canceled()), syncBackend, SLOT(cancelUpload()) );
        d->m_uploadProgressDialog->exec();
    }
}

QVector<RouteItem> RouteSyncManager::cachedRouteList()
{
    QVector<RouteItem> routeList;
    QStringList cachedRoutes = d->m_cacheDir.entryList( QStringList() << "*.kml", QDir::Files );
    foreach ( QString routeFilename, cachedRoutes ) {
        QFile file( d->m_cacheDir.absolutePath() + "/" + routeFilename );
        file.open( QFile::ReadOnly );

        RouteParser parser;
        if( !parser.read( &file ) ) {
            mDebug() << "Could not read " + routeFilename;
        }

        file.close();

        QString routeName;
        GeoDocument *geoDoc = parser.releaseDocument();
        GeoDataDocument *container = dynamic_cast<GeoDataDocument*>( geoDoc );
        if ( container && container->size() > 0 ) {
            GeoDataFolder *folder = container->folderList().at( 0 );
            foreach ( GeoDataPlacemark *placemark, folder->placemarkList() ) {
                routeName.append( placemark->name() );
                routeName.append( " - " );
            }
        }

        routeName = routeName.left( routeName.length() - 3 );
        QString timestamp = routeFilename.left( routeFilename.length() - 4 );
        QString distance = "0";
        QString duration = "0";

        QString previewPath = QString( "%0/preview/%1.jpg" ).arg( d->m_cacheDir.absolutePath(), timestamp );
        QIcon preview;

        if( QFile( previewPath ).exists() ) {
            preview = QIcon( previewPath );
        }

        RouteItem item;
        item.setTimestamp( timestamp );
        item.setName( routeName );
        item.setDistance( distance );
        item.setDistance( duration );
        item.setPreview( preview );
        routeList.append( item );
    }

    return routeList;
}

void RouteSyncManager::downloadRouteList()
{
    if( !d->m_cloudSyncManager->offlineMode() ) {
        if( d->m_cloudSyncManager->backend()  == CloudSyncManager::Owncloud ) {
            connect( d->m_owncloudBackend, SIGNAL(routeListDownloaded(QVector<RouteItem>)),
                     this, SLOT(processRouteList(QVector<RouteItem>)) );
            connect( d->m_owncloudBackend, SIGNAL(routeListDownloadProgress(qint64,qint64)),
                     this, SIGNAL(routeListDownloadProgress(qint64,qint64)) );
            d->m_owncloudBackend->downloadRouteList();
        }
    } else {
        d->m_model->setItems( cachedRouteList() );
    }
}

void RouteSyncManager::processRouteList( QVector<RouteItem> routeList )
{
    d->m_model->setItems( routeList );
}

void RouteSyncManager::downloadRoute( QString timestamp )
{
    if( d->m_cloudSyncManager->backend() == CloudSyncManager::Owncloud ) {
        connect( d->m_owncloudBackend, SIGNAL(routeDownloadProgress(qint64,qint64)),
                 this, SIGNAL(routeDownloadProgress(qint64,qint64)) );
        d->m_owncloudBackend->downloadRoute( timestamp );
    }
}

void RouteSyncManager::openRoute( QString timestamp )
{
    d->m_routingManager->loadRoute( QString( "%0/%1.kml" )
                                    .arg( d->m_cacheDir.absolutePath() )
                                    .arg( timestamp ) );
}

void RouteSyncManager::deleteRoute( QString timestamp )
{
    if( d->m_cloudSyncManager->backend() == CloudSyncManager::Owncloud ) {
        connect( d->m_owncloudBackend, SIGNAL(routeDeleted()), this, SLOT(downloadRouteList()) );
        d->m_owncloudBackend->deleteRoute( timestamp );
    }
}

void RouteSyncManager::updateUploadProgressbar( qint64 sent, qint64 total )
{
    qint64 percentage = 100.0 * sent / total;
    d->m_uploadProgressDialog->setValue( percentage );
    
    if( sent == total ) {
        d->m_uploadProgressDialog->accept();
        disconnect( this, SLOT(updateUploadProgressbar(qint64,qint64)) );
    }
}

}

#include "RouteSyncManager.moc"
