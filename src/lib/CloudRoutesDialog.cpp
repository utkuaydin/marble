//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2013   Utku Aydın <utkuaydin34@gmail.com>
//

#include "CloudRoutesDialog.h"
#include "ui_CloudRoutesDialog.h"

#include <QDebug>
#include <QPainter>
#include <QApplication>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

namespace Marble {

RouteItemDelegate::RouteItemDelegate( QListView *view, CloudRouteModel *model, MarbleWidget *marbleWidget ) :
    m_view( view ),
    m_model( model ),
    m_buttonWidth( 0 ),
    m_iconSize( 16 )
{
}

void RouteItemDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    
    QStyleOptionViewItemV4 styleOption = option;
    styleOption.text = QString();
    QApplication::style()->drawControl( QStyle::CE_ItemViewItem, &styleOption, painter );
    
    QAbstractTextDocumentLayout::PaintContext paintContext;
    if ( styleOption.state & QStyle::State_Selected)  {
        paintContext.palette.setColor( QPalette::Text, styleOption.palette.color( QPalette::Active, QPalette::HighlightedText ) );
    }
    
    QTextDocument document;
    QRect const textRect = position( Text, option );
    document.setTextWidth( textRect.width() );
    document.setDefaultFont( option.font );
    document.setHtml( text( index ) );

    painter->save();
    painter->translate( textRect.topLeft() );
    painter->setClipRect( 0, 0, textRect.width(), textRect.height() );
    document.documentLayout()->draw( painter, paintContext );
    painter->restore();
    
    bool cached = index.data( CloudRouteModel::IsCached ).toBool();
    
    if ( cached ) {
        QStyleOptionButton openButton = button( OpenButton, option );
        QRect openRect = position( OpenButton, option );
        openButton.rect = openRect;
        QApplication::style()->drawControl( QStyle::CE_PushButton, &openButton, painter );
    
        QStyleOptionButton cacheRemoveButton = button( RemoveFromCacheButton, option );
        QRect cacheRemoveRect = position( RemoveFromCacheButton, option );
        cacheRemoveButton.rect = cacheRemoveRect;
        QApplication::style()->drawControl( QStyle::CE_PushButton, &cacheRemoveButton, painter );
    } else {
        QStyleOptionButton downloadButton = button( DownloadButton, option );
        QRect downloadRect = position( DownloadButton, option );
        downloadButton.rect = downloadRect;
        QApplication::style()->drawControl( QStyle::CE_PushButton, &downloadButton, painter );
    
        QStyleOptionButton cloudRemoveButton = button( RemoveFromCloudButton, option );
        QRect cloudRemoveRect = position( RemoveFromCloudButton, option );
        cloudRemoveButton.rect = cloudRemoveRect;
        QApplication::style()->drawControl( QStyle::CE_PushButton, &cloudRemoveButton, painter );
    }
}

QSize RouteItemDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    if ( index.column() == 0 ) {
        QTextDocument doc;
        doc.setDefaultFont( option.font );
        doc.setHtml( text( index ) );
        return QSize( 0, doc.size().height() );
    }

    return QSize();
}

bool RouteItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if ( ( event->type() == QEvent::MouseButtonRelease ) ) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>( event );
        CloudRouteModel *routeModel = static_cast<CloudRouteModel*>( model );
        QPoint pos = mouseEvent->pos();
        
        bool cached = index.data( CloudRouteModel::IsCached ).toBool();
    
        if ( cached ) {
            QRect openRect = position( OpenButton, option );
            QRect cacheRemoveRect = position( RemoveFromCacheButton, option );
            
            if ( openRect.contains( pos ) ) {
                QString timestamp = index.data( CloudRouteModel::Timestamp ).toString();
                emit openButtonClicked( timestamp );
                return true;
            } else if ( cacheRemoveRect.contains( pos ) ) {
                m_model->removeFromCache( index );
                return true;
            }
        } else {
            QRect downloadRect = position( DownloadButton, option );
            QRect cloudRemoveRect = position( RemoveFromCloudButton, option );
            if ( downloadRect.contains( pos ) ) {
                QString timestamp = index.data( CloudRouteModel::Timestamp ).toString();
                emit downloadButtonClicked( timestamp );
                return true;
            }
        }
    }
    
    return false;
}

int RouteItemDelegate::buttonWidth( const QStyleOptionViewItem &option ) const
{
    if ( m_buttonWidth <= 0 ) {
        int const openWidth = option.fontMetrics.size( 0, tr( "Open" ) ).width();
        int const downloadWidth = option.fontMetrics.size( 0, tr( "Download" ) ).width();
        int const cacheWidth = option.fontMetrics.size( 0, tr( "Remove from cache" ) ).width();
        int const cloudWidth = option.fontMetrics.size( 0, tr( "Remove from cloud" ) ).width();
        m_buttonWidth = 2 * m_iconSize + qMax( qMax( openWidth, downloadWidth ), qMax( cacheWidth, cloudWidth ) );
    }

    return m_buttonWidth;
}

QStyleOptionButton RouteItemDelegate::button( Element element, const QStyleOptionViewItem &option ) const
{
    QStyleOptionButton result;
    result.state = option.state;
    result.state &= ~QStyle::State_HasFocus;

    result.palette = option.palette;
    result.features = QStyleOptionButton::None;

    switch ( element ) {
        case OpenButton:
            result.text = tr( "Open" );
            result.icon = QIcon( ":/marble/document-open.png" );
            result.iconSize = QSize( m_iconSize, m_iconSize );
            break;
        case DownloadButton:
            result.text = tr( "Download" );
            result.icon = QIcon( ":/marble/dialog-ok.png" );
            result.iconSize = QSize( m_iconSize, m_iconSize );
            break;
        case RemoveFromCacheButton:
            result.text = tr( "Remove from cache" );
            result.icon = QIcon( ":/marble/edit-clear.png" );
            result.iconSize = QSize( m_iconSize, m_iconSize );
            break;
        case RemoveFromCloudButton:
            result.text = tr( "Remove from cloud" );
            result.icon = QIcon( ":/marble/edit-delete.png" );
            result.iconSize = QSize( m_iconSize, m_iconSize );
            break;
        default:
            // Ignored.
            break;
    }

    return result;
}

QString RouteItemDelegate::text( const QModelIndex& index ) const
{
    return QString( "%0<br /><b>Duration:</b> %1<br/><b>Distance:</b> %2" )
            .arg( index.data( CloudRouteModel::Name ).toString() )
            .arg( index.data( CloudRouteModel::Duration ).toString() )
            .arg( index.data( CloudRouteModel::Distance ).toString() );
}

QRect RouteItemDelegate::position( Element element, const QStyleOptionViewItem& option ) const
{   
    int const width = buttonWidth( option );
    QPoint const firstColumn = option.rect.topLeft();
    QPoint const secondColumn = firstColumn + QPoint( option.decorationSize.width(), 0 );
    QPoint const thirdColumn = secondColumn + QPoint( option.rect.width() - width - option.decorationSize.width(), 0 );
    switch (element) {
        case Text:
            return QRect( secondColumn, QSize( thirdColumn.x() - secondColumn.x(), option.rect.height() ) );
        case OpenButton:
        case DownloadButton:
        {
            QStyleOptionButton optionButton = button( element, option );
            QSize size = option.fontMetrics.size( 0, optionButton.text ) + QSize( 4, 4 );
            QSize buttonSize = QApplication::style()->sizeFromContents( QStyle::CT_PushButton, &optionButton, size );
            buttonSize.setWidth( width );
            return QRect( thirdColumn, buttonSize );
        }
        case RemoveFromCacheButton:
        case RemoveFromCloudButton:
        {
            QStyleOptionButton optionButton = button( element, option );
            QSize size = option.fontMetrics.size( 0, optionButton.text ) + QSize( 4, 4 );
            QSize buttonSize = QApplication::style()->sizeFromContents( QStyle::CT_PushButton, &optionButton, size );
            buttonSize.setWidth( width );
            return QRect( thirdColumn + QPoint( 0, option.fontMetrics.height() + 10 ), buttonSize );
        }
    }
    
    return QRect();
}

class CloudRoutesDialog::Private : public Ui::CloudRoutesDialog {
    public:
        explicit Private();
        CloudRouteModel *m_model;
};

CloudRoutesDialog::Private::Private() : Ui::CloudRoutesDialog(),
    m_model( new CloudRouteModel() )
{
}

CloudRoutesDialog::CloudRoutesDialog( QString json, Marble::MarbleWidget* marbleWidget ) : QDialog( marbleWidget ),
    d( new Private )
{
    d->setupUi( this );
    
    RouteItemDelegate *delegate = new RouteItemDelegate( d->listView, d->m_model, marbleWidget );
    connect( delegate, SIGNAL(downloadButtonClicked(QString)), this, SIGNAL(downloadButtonClicked(QString)) );
    connect( delegate, SIGNAL(openButtonClicked(QString)), this, SIGNAL(openButtonClicked(QString)) );
    
    d->m_model->parseRouteList( json );
    d->listView->setItemDelegate( delegate );
    d->listView->setModel( d->m_model );
}

}

#include "CloudRoutesDialog.moc"
