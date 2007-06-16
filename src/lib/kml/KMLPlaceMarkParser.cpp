//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2007      Murad Tagirov <tmurad@gmail.com>
//


#include "KMLPlaceMarkParser.h"

#include "KMLContainer.h"
#include "KMLPlaceMark.h"
#include "KMLPointParser.h"

namespace
{
    const QString PLACEMARK_TAG = "placemark";
    const QString POINT_TAG = "point";
}

KMLPlaceMarkParser::KMLPlaceMarkParser( KMLContainer& container )
  : KMLFeatureParser( container ),
    m_placemark( new KMLPlaceMark() ),
    m_currentParser( 0 )
{
}

KMLPlaceMarkParser::~KMLPlaceMarkParser()
{
    if ( ! m_parsed ) {
        delete m_placemark;
    }
}

bool KMLPlaceMarkParser::startElement( const QString& namespaceURI,
                            const QString& localName,
                            const QString& name,
                            const QXmlAttributes& atts )
{
    if ( m_parsed ) {
        return false;
    }

    bool result = false;

    if ( m_currentParser != 0 ) {
        result = m_currentParser->startElement( namespaceURI, localName, name, atts );
    }
    else {
        result = KMLFeatureParser::startElement( namespaceURI, localName, name, atts );
    }

    if ( ! result ) {
        QString lowerName = name.toLower();

        /*
         * Iterate over tags which current parser supports
         * TODO: check if this tag appears first time or not
         */
        if ( lowerName == PLACEMARK_TAG ) {
            result = true;
        }
        else if ( lowerName == POINT_TAG ) {
            if ( m_currentParser != 0 ) {
                delete m_currentParser;
                m_currentParser = 0;
            }

            if ( m_placemark != 0 ) {
                m_currentParser = new KMLPointParser( *m_placemark );
                result = m_currentParser->startElement( namespaceURI, localName, name, atts );
            }
        }
    }

    return result;
}

bool KMLPlaceMarkParser::endElement( const QString& namespaceURI,
                            const QString& localName,
                            const QString& qName )
{
    if ( m_parsed ) {
        return false;
    }

    bool result = false;

    if ( m_currentParser != 0 ) {
        result = m_currentParser->endElement( namespaceURI, localName, qName );
    }
    else {
        result = KMLFeatureParser::endElement( namespaceURI, localName, qName );
    }

    if ( ! result ) {
        /*
         * Check this is our end tag
         * TODO: check if this tag appears first time or not
         */
        QString lowerName = qName.toLower();

        if ( lowerName == PLACEMARK_TAG ) {
            KMLContainer&  container = (KMLContainer&) m_object;
            container.addPlaceMark( m_placemark );
            m_parsed = true;

            result = true;
        }
    }

    return result;
}

bool KMLPlaceMarkParser::characters( const QString& ch )
{
    if ( m_parsed ) {
        return false;
    }

    bool result = false;

    if ( m_currentParser != 0 ) {
        result = m_currentParser->characters( ch );
    }
    else
    {
        result = KMLFeatureParser::characters( ch );
    }

    return result;
}
