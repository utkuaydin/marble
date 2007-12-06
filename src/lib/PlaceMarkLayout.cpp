//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2006-2007 Torsten Rahn <tackat@kde.org>"
// Copyright 2007      Inge Wallin  <ingwa@kde.org>"
//

#include "PlaceMarkLayout.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QVector>
#include <QtCore/QVectorIterator>
#include <QtGui/QFont>
#include <QtGui/QItemSelectionModel>
#include <QtGui/QPainter>

#include "geodata/data/GeoDataPlacemark.h"
#include "geodata/data/GeoDataStyle.h"

#include "global.h"
#include "PlaceMarkPainter.h"
#include "MarblePlacemarkModel.h"
#include "MarbleDirs.h"
#include "ViewParams.h"
#include "VisiblePlaceMark.h"

PlaceMarkLayout::PlaceMarkLayout( QObject* parent )
    : QObject( parent )
{
    m_maxLabelHeight = 0;
    m_styleResetRequested = true;

//  Old weightfilter array. Still here 
// to be able to compare performance
/*
    m_weightfilter
        << 9999
        << 4200
        << 3900
        << 3600

        << 3300
        << 3000
        << 2700
        << 2400

        << 2100
        << 1800
        << 1500
        << 1200

        << 900
        << 400
        << 200
        << 0;
*/
    m_weightfilter
        << 49300
        << 40300
        << 32300
        << 25300

        << 19300
        << 14300
        << 10300
        << 7300

        << 5300
        << 3300
        << 2400
        << 1800

        << 1200
        << 800
        << 300
        << 200;

    m_placeMarkPainter =  new PlaceMarkPainter( this );
}

PlaceMarkLayout::~PlaceMarkLayout()
{
    styleReset();
}

void PlaceMarkLayout::requestStyleReset()
{
    qDebug() << "Style reset requested.";
    m_styleResetRequested = true;
}

void PlaceMarkLayout::styleReset()
{
    m_paintOrder.clear();

    qDeleteAll( m_visiblePlaceMarks );
    m_visiblePlaceMarks.clear();
}

QVector<QPersistentModelIndex> PlaceMarkLayout::whichPlaceMarkAt( const QPoint& curpos ) const
{
    QVector<QPersistentModelIndex> ret;

    QVector<VisiblePlaceMark*>::const_iterator  it;
    for ( it = m_paintOrder.constBegin();
          it != m_paintOrder.constEnd();
          it++ )
    {
        const VisiblePlaceMark  *mark = *it; // no cast

        if ( mark->labelRect().contains( curpos )
             || QRect( mark->symbolPosition(), mark->symbolPixmap().size() ).contains( curpos ) ) {
            ret.append( mark->modelIndex() );
        }
    }

    return ret;
}

PlaceMarkPainter* PlaceMarkLayout::placeMarkPainter() const
{ 
    return m_placeMarkPainter; 
}

int PlaceMarkLayout::maxLabelHeight( const QAbstractItemModel* model,
                                     const QItemSelectionModel* selectionModel ) const
{
    qDebug() << "Detecting maxLabelHeight ...";

    int maxLabelHeight = 0;

    const QModelIndexList selectedIndexes = selectionModel->selection().indexes();

    for ( int i = 0; i < selectedIndexes.count(); ++i ) {
        const QModelIndex index = selectedIndexes.at( i );
//        GeoDataStyle* style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
        GeoDataStyle* style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );
        QFont labelFont = style->labelStyle()->font();
        int textHeight = QFontMetrics( labelFont ).height();
        if ( textHeight > maxLabelHeight )
            maxLabelHeight = textHeight; 
    }

    for ( int i = 0; i < model->rowCount(); ++i )
    {
        QModelIndex index = model->index( i, 0 );

//        GeoDataStyle* style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
        GeoDataStyle* style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );
        QFont labelFont = style->labelStyle()->font();
        int textHeight = QFontMetrics( labelFont ).height();
        if ( textHeight > maxLabelHeight ) 
            maxLabelHeight = textHeight; 
    }

    return maxLabelHeight;
}

void PlaceMarkLayout::paintPlaceFolder(QPainter* painter,
                                        int imgwidth, int imgheight,
                                        ViewParams *viewParams,
                                        const QAbstractItemModel* model,
                                        const QItemSelectionModel* selectionModel,
                                        Quaternion planetAxis,
                                        bool firstTime )
{
    if ( m_styleResetRequested == true )
    {
        m_styleResetRequested = false;
        styleReset();
        m_maxLabelHeight = maxLabelHeight( model, selectionModel );
    }
    const int secnumber = imgheight / m_maxLabelHeight + 1;
    Quaternion inversePlanetAxis = planetAxis.inverse();

    QVector< QVector< VisiblePlaceMark* > >  rowsection;
    for ( int i = 0; i < secnumber; i++)
        rowsection.append( QVector<VisiblePlaceMark*>( ) );

    m_paintOrder.clear();

    int labelnum = 0;
    int x = 0;
    int y = 0;

    /**
     * First handle the selected place marks, as they have the highest priority.
     */

    const QModelIndexList selectedIndexes = selectionModel->selection().indexes();
    MarblePlacemarkModel* selectionPlacemarkModel = (MarblePlacemarkModel*) selectionModel;

    for ( int i = 0; i < selectedIndexes.count(); ++i ) {
        const QModelIndex index = selectedIndexes.at( i );

        if ( !locatedOnScreen ( index, x, y, imgwidth, imgheight, 
                                inversePlanetAxis, viewParams ) )
        {
            delete m_visiblePlaceMarks.take( index );
            continue;
        }

        // ----------------------------------------------------------------
        // End of checks. Here the actual layouting starts.
        QFont labelFont;
        int textWidth = 0;

        // Find the corresponding visible place mark
        VisiblePlaceMark *mark = m_visiblePlaceMarks.value( index );

        // Specify font properties
        if ( mark ) {
            textWidth = mark->labelRect().width();
        }
        else {
            GeoDataStyle* style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );
//            GeoDataStyle* style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
            labelFont = style->labelStyle()->font();
            labelFont.setWeight( 75 ); // Needed to calculate the correct pixmap size; 

            textWidth = ( QFontMetrics( labelFont ).width( index.data( Qt::DisplayRole ).toString() )
                  + (int)( 2 * s_labelOutlineWidth ) );
        }

        // Choose Section
        const QVector<VisiblePlaceMark*> currentsec = rowsection.at( y / m_maxLabelHeight );

        // Find out whether the area around the placemark is covered already.
        // If there's not enough space free don't add a VisiblePlaceMark here.

        QRect labelRect = roomForLabel( index, currentsec, x, y, textWidth );
        if ( labelRect.isNull() ) continue;

        // Make sure not to draw more placemarks on the screen than 
        // specified by placeMarksOnScreenLimit().

        ++labelnum;
        if ( labelnum >= placeMarksOnScreenLimit() )
            break;
        if ( !mark )
        {
            // If there is no visible place mark yet for this index, create a new one...
            mark = new VisiblePlaceMark;
            mark->setModelIndex( QPersistentModelIndex( index ) );

            m_visiblePlaceMarks.insert( index, mark );
        }

        // Finally save the label position on the map.
//        GeoDataStyle* style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
        GeoDataStyle* style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );
        QPointF hotSpot = style->iconStyle()->hotSpot();

        mark->setSymbolPosition( QPoint( x - (int)( hotSpot.x() ),
                                         y - (int)( hotSpot.y() ) ) );
        mark->setLabelRect( labelRect );

        // Add the current placemark to the matching row and it's
        // direct neighbors.
        int idx = y / m_maxLabelHeight;
        if ( idx - 1 >= 0 )
            rowsection[ idx - 1 ].append( mark );
        rowsection[ idx ].append( mark );
        if ( idx + 1 < secnumber )
            rowsection[ idx + 1 ].append( mark );

        m_paintOrder.append( mark );
    }

    /**
     * Now handle all other place marks...
     */
    const QModelIndex firstIndex = model->index( 0, 0 );
    const int firstPopularity = firstIndex.data( MarblePlacemarkModel::PopularityRole ).toInt();
    const bool  noFilter = ( firstPopularity == 0 
                             || ( firstPopularity != 0
                             && firstIndex.data( MarblePlacemarkModel::GeoTypeRole ).toChar().isNull() ) ) 
                           ? true : false;
    const QItemSelection selection = selectionModel->selection();
    const MarblePlacemarkModel* placemarkModel = (MarblePlacemarkModel*) model;

    for ( int i = 0; i < model->rowCount(); ++i )
    {
        QModelIndex index = model->index( i, 0 );

        int popularityIndex = index.data( MarblePlacemarkModel::PopularityIndexRole ).toInt();

        // Skip the places that are too small.
        if ( noFilter == false ) {
            if ( m_weightfilter.at( popularityIndex ) > viewParams->m_radius )
            {
//            qDebug() << QString("Filter: %1 Radius: %2")
//            .arg(m_weightfilter.at( popularityIndex )).arg(viewParams->m_radius);
//                qDebug() << "BREAK at " << i;
                break;
            }
        }

        if ( !locatedOnScreen ( index, x, y, imgwidth, imgheight, 
                                inversePlanetAxis, viewParams ) )
        {
//           qDebug("Deleting"); 
            delete m_visiblePlaceMarks.take( index );
            continue;
        }

        const int visualCategory  = index.data( MarblePlacemarkModel::VisualCategoryRole ).toInt();

        // Skip city marks if we're not showing cities.
        if ( !viewParams->m_showCities
             && ( visualCategory > 3 && visualCategory < 20 ) )
            continue;

        // Skip terrain marks if we're not showing terrain.
        if ( !viewParams->m_showTerrain
             && ( visualCategory >= 20 && visualCategory <= 21 ) )
            continue;

        if ( !viewParams->m_showOtherPlaces
             && ( visualCategory >= 22 && visualCategory <= 25 ) )
            continue;


        const bool isSelected = selection.contains( index );

        /**
         * We handled selected place marks already, so skip here...
         * Given that we assume that only a small amount of places is selected
         * we check for the selected state after all other filters
         */
        if ( isSelected )
            continue;

        // ----------------------------------------------------------------
        // End of checks. Here the actual layouting starts.
        QFont labelFont;
        int textWidth = 0;

        // Find the corresponding visible place mark
        VisiblePlaceMark *mark = m_visiblePlaceMarks.value( index );
        GeoDataStyle* style = 0;

        // Specify font properties
        if ( mark ) {
            textWidth = mark->labelRect().width();
        }
        else {
            style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );
//            style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
            labelFont = style->labelStyle()->font();

            textWidth = ( QFontMetrics( labelFont ).width( index.data( Qt::DisplayRole ).toString() ) );
        }

        // Choose Section
        const QVector<VisiblePlaceMark*> currentsec = rowsection.at( y / m_maxLabelHeight );

         // Find out whether the area around the placemark is covered already.
        // If there's not enough space free don't add a VisiblePlaceMark here.

        QRect labelRect = roomForLabel( index, currentsec, x, y, textWidth );
        if ( labelRect.isNull() ) continue;

        // Make sure not to draw more placemarks on the screen than 
        // specified by placeMarksOnScreenLimit().

        ++labelnum;
        if ( labelnum >= placeMarksOnScreenLimit() )
            break;

        if ( !mark )
        {
            // If there is no visible place mark yet for this index, create a new one...
            mark = new VisiblePlaceMark;

            mark->setModelIndex( QPersistentModelIndex( index ) );
            m_visiblePlaceMarks.insert( index, mark );
        }

        // Finally save the label position on the map.
        if ( style == 0 )
            style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );
//            style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
        QPointF hotSpot = style->iconStyle()->hotSpot();

        mark->setSymbolPosition( QPoint( x - (int)( hotSpot.x() ),
                                         y - (int)( hotSpot.y() ) ) );
        mark->setLabelRect( labelRect );

        // Add the current placemark to the matching row and it's
        // direct neighbors.
        int idx = y / m_maxLabelHeight;
        if ( idx - 1 >= 0 )
            rowsection[ idx - 1 ].append( mark );
        rowsection[ idx ].append( mark );
        if ( idx + 1 < secnumber )
            rowsection[ idx + 1 ].append( mark );

        m_paintOrder.append( mark );
    }
    m_placeMarkPainter->drawPlaceMarks( painter, m_paintOrder, selection, viewParams );
}

inline bool PlaceMarkLayout::locatedOnScreen ( const QPersistentModelIndex &index, 
                                               int &x, int &y, 
                                               const int &imgwidth, const int &imgheight,
                                               const Quaternion &inversePlanetAxis,
                                               ViewParams * viewParams )
{
    MarblePlacemarkModel* placemarkModel = (MarblePlacemarkModel*) index.model();
    Quaternion qpos = ( placemarkModel->coordinateData( index ) ).quaternion();
//    Quaternion qpos = ( index.data().value<GeoPoint>() ).quaternion();

    if( viewParams->m_projection == Spherical ) {

        qpos.rotateAroundAxis( inversePlanetAxis );

        // Skip placemarks at the other side of the earth.
        if ( qpos.v[Q_Z] < 0 ) {
            return false;
        }

        // Let (x, y) be the position on the screen of the placemark..
        x = (int)(imgwidth  / 2 + viewParams->m_radius * qpos.v[Q_X]);
        y = (int)(imgheight / 2 + viewParams->m_radius * qpos.v[Q_Y]);

        // Skip placemarks that are outside the screen area
        if ( x < 0 || x >= imgwidth || y < 0 || y >= imgheight ) {
            return false;
        }

        return true;
    }

    if( viewParams->m_projection == Equirectangular ) {

            double degX;
            double degY;
            double xyFactor = 2 * viewParams->m_radius / M_PI;

            double centerLon;
            double centerLat;

            // Let (x, y) be the position on the screen of the placemark..
            qpos.getSpherical( degX, degY );
            viewParams->centerCoordinates( centerLon, centerLat );

            x = (int)(imgwidth  / 2 + xyFactor * (degX + centerLon));
            y = (int)(imgheight / 2 + xyFactor * (degY + centerLat));

            // Skip placemarks that are outside the screen area
            //
            // FIXME: I have the feeling that this is wrong, because there
            //        are always insanely few placemarks on the flat map 
            //        compared to the globe.
            if ( ( ( x < 0 || x >= imgwidth )
                   // FIXME: Carlos: check this:
                   && x - 4 * viewParams->m_radius < 0
                   && x + 4 * viewParams->m_radius >= imgwidth )
                   || y < 0 || y >= imgheight )
            {
                return false;
            }

            return true;
    }

    return true;
}

QRect PlaceMarkLayout::roomForLabel( const QPersistentModelIndex& index,
                                      const QVector<VisiblePlaceMark*> &currentsec,
                                      const int x, const int y,
                                      const int textWidth )
{
    bool  isRoom      = false;

//    GeoDataStyle* style = index.data( MarblePlacemarkModel::StyleRole ).value<GeoDataStyle*>();
    GeoDataStyle* style = ( ( MarblePlacemarkModel* )index.model() )->styleData( index );

    int symbolwidth = style->iconStyle()->icon().width();

    QFont labelFont = style->labelStyle()->font();
    int textHeight = QFontMetrics( labelFont ).height();

    if( style->labelStyle()->alignment() == GeoDataLabelStyle::Corner )
    {
        int  xpos = symbolwidth / 2 + x + 1;
        int  ypos = 0;

        // Check the four possible positions by going through all of them
 
        QRect  labelRect( xpos, ypos, textWidth, textHeight );
    
        while ( xpos >= x - textWidth - symbolwidth - 1 ) {
            ypos = y;

            while ( ypos >= y - textHeight ) {

                isRoom = true;
                labelRect.moveTo( xpos, ypos );

                // Check if there is another label or symbol that overlaps.
                for ( QVector<VisiblePlaceMark*>::const_iterator beforeit = currentsec.constBegin();
                      beforeit != currentsec.constEnd();
                      ++beforeit )
                {
                    if ( labelRect.intersects( (*beforeit)->labelRect()) ) {
                        isRoom = false;
                        break;
                    }
                }

                if ( isRoom ) {
                    // claim the place immediately if it hasn't been used yet 
                    return labelRect;
                }

                ypos -= textHeight;
            }

            xpos -= ( symbolwidth + textWidth + 2 );
        }
    }
    else if( style->labelStyle()->alignment() == GeoDataLabelStyle::Center )
    {
        isRoom = true;
        QRect  labelRect( x - textWidth / 2, y - textHeight / 2, textWidth, textHeight );

        // Check if there is another label or symbol that overlaps.
        for ( QVector<VisiblePlaceMark*>::const_iterator beforeit = currentsec.constBegin();
              beforeit != currentsec.constEnd();
              ++beforeit )
        {
            if ( labelRect.intersects( (*beforeit)->labelRect()) ) {
                isRoom = false;
                break;
            }
        }

        if ( isRoom ) {
            // claim the place immediately if it hasn't been used yet 
            return labelRect;
        }
    }

    return QRect(); // At this point there is no space left 
                    // for the rectangle anymore.
}

int PlaceMarkLayout::placeMarksOnScreenLimit() const
{
    // For now we just return 100.
    // Later on once we focus on decent high dpi print quality
    // we should replace this static value by a dynamic value
    // that takes the area that gets displayed into account.
    return 100;
}


#include "PlaceMarkLayout.moc"
