/*
    This file is part of KXForms.

    Copyright (c) 2006 Cornelius Schumacher <schumacher@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "formcreator.h"

#include "xmlbuilder.h"

#include <QDebug>

using namespace KXForms;

FormCreator::FormCreator()
{
}

QString FormCreator::create( const Schema::Document &schemaDocument )
{
  qDebug() << "---FormCreator::create()";

  mDocument = schemaDocument;

  mCollapsedForms.clear();

  XmlBuilder xml( "kxforms" );

  createForm( &xml, schemaDocument.startElement() );

  foreach ( Schema::Element element, schemaDocument.usedElements() ) {
    if ( element.identifier() != schemaDocument.startElement().identifier() ) {
      createForm( &xml, element );
    }
  }

  qDebug() << "---FormCreator::create() done";

  return xml.print();
}

void FormCreator::createForm( XmlBuilder *xml, const Schema::Element &element )
{
  if ( mCollapsedForms.contains( element.name() ) ) return;

  qDebug() << "ELEMENT" << element.name(); 
  XmlBuilder *form = xml->tag( "form" )->attribute( "ref", element.name() );

  form->tag( "xf:label", humanizeString( element.name() ) );

  foreach( Schema::Relation r, element.attributeRelations() ) {
    Schema::Attribute a = mDocument.attribute( r );

    qDebug() << "  ATTRIBUTE: " << a.identifier();

    if ( a.type() == Schema::Attribute::String ) {
      XmlBuilder *input = form->tag( "xf:input" );
      input->attribute( "ref", a.ref() );
      createLabel( input, a );
    } else if ( a.type() == Schema::Attribute::Enumeration ) {
      XmlBuilder *select1 = form->tag( "xf:select1" );
      select1->attribute( "ref", a.ref() );
      createLabel( select1, a );
      foreach( QString value, a.enumerationValues() ) {
        XmlBuilder *item = select1->tag( "xf:item" );
        QString itemLabel;
        Hint hint = mHints.hint( element.identifier() + "/" + a.ref() );
        if ( hint.isValid() ) itemLabel = hint.enumValue( value );
        if ( itemLabel.isEmpty() ) itemLabel = humanizeString( value );
        item->tag( "xf:label", itemLabel );
        item->tag( "xf:value", value );  
      }
    } else {
      qDebug() << "Unsupported type: " << a.type();
    }
  }


  if ( element.type() == Schema::Node::String ) {
    XmlBuilder *textArea = form->tag( "xf:textarea" );
    textArea->attribute( "ref", "." );
    createLabel( textArea, element );
  } else if ( element.type() == Schema::Node::NormalizedString ||
              element.type() == Schema::Node::Token ) {
    XmlBuilder *input = form->tag( "xf:input" );
    input->attribute( "ref", "." );
    createLabel( input, element );
  } else {
    QString currentChoice;

    XmlBuilder *list = 0;

    foreach( Schema::Relation r, element.elementRelations() ) {
      qDebug() << "  CHILD ELEMENT" << r.target();
      qDebug() << "    CHOICE" << r.choice();
      if ( r.isList() ) {
        bool isMixedList = r.choice().contains( "+" );

        if ( !list || r.choice().isEmpty() || currentChoice != r.choice() ) {
          list = form->tag( "list" );
          QString label;
          if ( isMixedList ) {
            label = "Item";
          } else {
            label = getLabel( element.identifier() + "[" + r.target() + "]" );
            if ( label.isEmpty() ) {
              label = humanizeString( r.target(), true );
            }
          }
          list->tag( "xf:label", label );
        }
        XmlBuilder *item = list->tag( "itemclass" );
        item->attribute( "ref", r.target() );

        QString itemLabel;

        itemLabel = getLabel( element.identifier() + "/" + r.target() );

        Schema::Element itemElement = mDocument.element( r );

        if ( itemLabel.isEmpty() ) {
          // Try to guess a suitable item label.
          foreach( Schema::Relation r2, itemElement.attributeRelations() ) {
            if ( r2.target() == "name" ) {
              if ( isMixedList ) {
                itemLabel = humanizeString( itemElement.name() ) + ": ";
              }
              itemLabel += "<arg ref=\"@name\"/>";
              break;
            }
          }
        }

        if ( itemLabel.isEmpty() ) {
          if ( itemElement.type() == Schema::Node::String ) {
            itemLabel += "<arg ref=\".\" truncate=\"40\"/>";
          } else if ( itemElement.type() == Schema::Node::NormalizedString ||
                      itemElement.type() == Schema::Node::Token ) {
            itemLabel += "<arg ref=\".\"/>";
          }
        }

        if ( itemLabel.isEmpty() ) itemLabel = humanizeString( r.target() );
        item->tag( "itemlabel", itemLabel );

        currentChoice = r.choice();
      } else {
        Schema::Element textElement = mDocument.element( r.target() );
        XmlBuilder *textInput = 0;
        if ( textElement.type() == Schema::Node::NormalizedString ) {
          textInput = form->tag( "xf:input" );
        } else {
          textInput = form->tag( "xf:textarea" );
        }
        textInput->attribute( "ref", textElement.name() );
        createLabel( textInput, textElement );
      
        mCollapsedForms.append( r.target() );
      }
    }
  }
}

QString FormCreator::humanizeString( const QString &str, bool pluralize )
{
  if ( str.isEmpty() ) return str;
  QString result = str[0].toUpper() + str.mid( 1 );
  if ( pluralize ) {
    if ( result.endsWith( "y" ) ) {
      result = result.left( str.length() - 1 ) + "ies";
    } else {
      result += "s";
    }
  }

  return result;
}

void FormCreator::setHints( const Hints &hints )
{
  mHints = hints;
}

void FormCreator::createLabel( XmlBuilder *parent, const Schema::Node &node )
{
  parent->tag( "xf:label", getLabel( node.identifier(), node.name() ) );
}

QString FormCreator::getLabel( const QString &ref, const QString &fallback,
  bool pluralize )
{
//  qDebug() << "GETLABEL: " << ref;

  QString label;

  Hint hint = mHints.hint( ref );
  if ( hint.isValid() ) label = hint.label();
  
  if ( label.isEmpty() ) label = humanizeString( fallback, pluralize );
  
  return label;
}