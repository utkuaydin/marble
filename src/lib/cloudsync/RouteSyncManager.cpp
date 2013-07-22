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
#include "GeoDataFolder.h"
#include "GeoDataDocument.h"
#include "GeoDataPlacemark.h"
#include "KmlElementDictionary.h"
#include "CloudRoutesDialog.h"
#include "OwncloudSyncBackend.h"

#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QBuffer>
#include <QScriptValue>
#include <QScriptEngine>
#include <QTemporaryFile>
#include <QNetworkRequest>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QProgressDialog>
#include <QNetworkReply>

namespace Marble
{

/**
 * Class that overrides necessary methods of GeoParser.
 * @see GeoParser
 */
class RouteParser : public GeoParser {
public:
    explicit RouteParser();
    virtual GeoDataDocument* createDocument() const;
    virtual bool isValidRootElement();
};

RouteParser::RouteParser() : GeoParser( 0 )
{
    // Nothing to do.
}

GeoDataDocument* RouteParser::createDocument() const
{
    return new GeoDataDocument;
}

bool RouteParser::isValidRootElement()
{
    return isValidElement(kml::kmlTag_kml);
}

/**
 * Private class for RouteSyncManager.
 */
class RouteSyncManager::Private {
public:
    Private( CloudSyncManager *cloudSyncManager, RoutingManager *routingManager );

    CloudSyncManager *m_cloudSyncManager;

    QDir m_cacheDir;
    CloudRoutesDialog *m_dialog;
    RoutingManager *m_routingManager;
    QProgressDialog *m_uploadProgressDialog;
    QProgressDialog *m_listDownloadProgressDialog;
};

RouteSyncManager::Private::Private( CloudSyncManager *cloudSyncManager, RoutingManager *routingManager ) :
    m_listDownloadProgressDialog( new QProgressDialog() ),
    m_uploadProgressDialog( new QProgressDialog() ),
    m_cloudSyncManager( cloudSyncManager ),
    m_routingManager( routingManager ),
    m_dialog()
{
    m_cacheDir = QDir( MarbleDirs::localPath() + "/cloudsync/cache/routes/" );
    
    m_uploadProgressDialog->setMinimum( 0 );
    m_uploadProgressDialog->setMaximum( 100 );
    m_uploadProgressDialog->setWindowTitle( tr( "Uploading route..." ) );
    
    m_listDownloadProgressDialog->setMinimum( 0 );
    m_listDownloadProgressDialog->setMaximum( 100 );
    m_listDownloadProgressDialog->setWindowTitle( tr( "Downloading route list..." ) );
}

RouteSyncManager::RouteSyncManager( CloudSyncManager *cloudSyncManager, RoutingManager *routingManager ) :
    d( new Private( cloudSyncManager, routingManager ) )
{
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
    if ( d->m_cloudSyncManager->backend()  == CloudSyncManager::Owncloud ) {
        // TODO: Move most of the code to backend.
        OwncloudSyncBackend *syncBackend = new OwncloudSyncBackend( d->m_cloudSyncManager->apiUrl()  );
        
        QString timestamp = saveDisplayedToCache();
        QString filename = d->m_cacheDir.absolutePath() + "/" + timestamp + ".kml";
        
        // All data from the KML file has to be transferred into a
        // QByteArray for later use because once QFile::readAll()
        // done, all the data gets flushed and QFile::readAll()
        // returns nothing when called later.
        QFile routeFile( filename );
        QByteArray kml;

        if ( routeFile.open( QFile::ReadOnly ) ) {
            kml = routeFile.readAll();
            routeFile.close();
        }

        // A QBuffer which uses the QByteArray "kml" needs to be
        // created for RouteParser::read().
        QBuffer buffer( &kml );
        buffer.open( QBuffer::ReadOnly );

        RouteParser parser;
        parser.read( &buffer );
        buffer.close();
        
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
        
        QUrl parameters;
        parameters.addQueryItem( "timestamp", timestamp );
        parameters.addQueryItem( "kml", QString::fromUtf8( kml ) );
        parameters.addQueryItem( "name", routeName.left( routeName.length() - 3 ) );
        parameters.addQueryItem( "distance", 0 );
        parameters.addQueryItem( "duration", 0 );
        QByteArray encodedQuery = parameters.encodedQuery();
        
        syncBackend->uploadRoute( encodedQuery );
        connect( syncBackend, SIGNAL(routeUploadProgress(qint64,qint64)), this, SLOT(updateUploadProgressbar(qint64,qint64)) );
        //connect( d->m_uploadProgressDialog, SIGNAL(canceled()), syncBackend, SLOT(cancelUpload()) );
        d->m_uploadProgressDialog->exec();
    }
}

void RouteSyncManager::downloadRouteList()
{
    if( d->m_cloudSyncManager->backend()  == CloudSyncManager::Owncloud ) {
        OwncloudSyncBackend *syncBackend = new OwncloudSyncBackend( d->m_cloudSyncManager->apiUrl()  );
        connect( syncBackend, SIGNAL(routeListDownloaded(QVector<RouteItem>)), this, SLOT(openCloudRoutesDialog(QVector<RouteItem>)) );
        syncBackend->downloadRouteList();
    }
}

void RouteSyncManager::openCloudRoutesDialog( QVector<RouteItem> routes )
{
    d->m_dialog = new CloudRoutesDialog( routes );
    connect( d->m_dialog, SIGNAL(downloadButtonClicked(QString)), this, SLOT(downloadRoute(QString)) );
    connect( d->m_dialog, SIGNAL(openButtonClicked(QString)), this, SLOT(openRoute(QString)) );
    connect( d->m_dialog, SIGNAL(deleteButtonClicked(QString)), this, SLOT(deleteRoute(QString)) );
    d->m_dialog->exec();
}

void RouteSyncManager::downloadRoute( QString timestamp )
{
    if( d->m_cloudSyncManager->backend() == CloudSyncManager::Owncloud ) {
        OwncloudSyncBackend *syncBackend = new OwncloudSyncBackend( d->m_cloudSyncManager->apiUrl()  );
        connect( syncBackend, SIGNAL(routeDownloadProgress(qint64,qint64)),
                 d->m_dialog->model(), SLOT(updateProgress(qint64,qint64)) );
        syncBackend->downloadRoute( timestamp );
    }
}

void RouteSyncManager::openRoute( QString timestamp )
{
    d->m_routingManager->loadRoute( QString( "%0/%1.kml" )
                                    .arg( d->m_cacheDir.absolutePath() ).arg( timestamp ) );
}

void RouteSyncManager::deleteRoute( QString timestamp )
{
    if( d->m_cloudSyncManager->backend() == CloudSyncManager::Owncloud ) {
        OwncloudSyncBackend syncBackend( d->m_cloudSyncManager->apiUrl()  );
        syncBackend.deleteRoute( timestamp );
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

void RouteSyncManager::updateListDownloadProgressbar(qint64 received, qint64 total)
{
    qint64 percentage = qRound( 100.0 * qreal( received ) / total );
    d->m_listDownloadProgressDialog->setValue( percentage );
    
    if( received == total ) {
        d->m_listDownloadProgressDialog->accept();
        disconnect( this, SLOT(updateListDownloadProgressbar(qint64,qint64)) );
    }
}

}

#include "RouteSyncManager.moc"