/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */




#include <QtGui>
#include "UBGraphicsGroupContainerItem.h"
#include "UBGraphicsTextItem.h"
#include "UBGraphicsTextItemDelegate.h"
#include "UBGraphicsScene.h"
#include "UBGraphicsDelegateFrame.h"

#include "core/UBApplication.h"
#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "core/UBSettings.h"
#include "spellcheck/UBSpellChecker.h"
#include "spellcheck/UBSpellHighlighter.h"

#include <QMenu>
#include <QClipboard>
#include <QAbstractTextDocumentLayout>

#include "core/memcheck.h"
QColor UBGraphicsTextItem::lastUsedTextColor = QColor(Qt::black);

UBGraphicsTextItem::UBGraphicsTextItem(QGraphicsItem * parent)
    : QGraphicsTextItem(parent)
    , UBGraphicsItem()
    , mTypeTextHereLabel(tr("<Type Text Here>"))
    , mMultiClickState(0)
    , mLastMousePressTime(QTime::currentTime())
    , isActivatedTextEditor(true)
{
    setDelegate(new UBGraphicsTextItemDelegate(this, 0));

    mSpellHighlighter = new UBSpellHighlighter(document());

    // TODO claudio remove this because in contrast with the fact the frame should be created on demand.
    Delegate()->createControls();
    Delegate()->frame()->setOperationMode(UBGraphicsDelegateFrame::Resizing);
    Delegate()->setUBFlag(GF_FLIPPABLE_ALL_AXIS, false);
    Delegate()->setUBFlag(GF_REVOLVABLE, true);

    setData(UBGraphicsItemData::ItemLayerType, UBItemLayerType::Object);
    setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::ObjectItem)); //Necessary to set if we want z value to be assigned correctly

    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

    setTextInteractionFlags(Qt::TextEditorInteraction);

    setUuid(QUuid::createUuid());

    connect(document(), SIGNAL(contentsChanged()), Delegate(), SLOT(contentsChanged()));
    connect(document(), SIGNAL(undoCommandAdded()), this, SLOT(undoCommandAdded()));

    connect(document()->documentLayout(), SIGNAL(documentSizeChanged(const QSizeF &)),
            this, SLOT(documentSizeChanged(const QSizeF &)));

}

UBGraphicsTextItem::~UBGraphicsTextItem()
{
}

void UBGraphicsTextItem::initFontProperties()
{
    QTextCursor curCursor = textCursor();
    QTextCharFormat format;
    QFont font(createDefaultFont());

    font.setPointSize(UBSettings::settings()->fontPointSize());
    format.setFont(font);
    // Every new text zone starts with the default text color (same as the default pen color)
    if (UBSettings::settings()->isDarkBackground())
        UBGraphicsTextItem::lastUsedTextColor = Qt::white;
    else
        UBGraphicsTextItem::lastUsedTextColor = UBSettings::settings()->textDefaultColor();

    setDefaultTextColor(UBGraphicsTextItem::lastUsedTextColor);
    format.setForeground(QBrush(UBGraphicsTextItem::lastUsedTextColor));
    curCursor.mergeCharFormat(format);
    setTextCursor(curCursor);
    setFont(font);

    adjustSize();
    contentsChanged();
}

QFont UBGraphicsTextItem::createDefaultFont()
{
    QFont font;

    QString fFamily = UBSettings::settings()->fontFamily();
    if (!fFamily.isEmpty())
        font.setFamily(fFamily);

    QString fStyleName = UBSettings::settings()->fontStyleName();
    if (!fStyleName .isEmpty())
        font.setStyleName(fStyleName);

    bool bold = UBSettings::settings()->isBoldFont();
    if (bold)
        font.setWeight(QFont::Bold);

    bool italic = UBSettings::settings()->isItalicFont();
    if (italic)
        font.setItalic(true);


    int pointSize = UBSettings::settings()->fontPointSize();
    if (pointSize > 0) {
        font.setPointSize(pointSize);
    }

    return font;
}

void UBGraphicsTextItem::recolor()
{
    UBGraphicsTextItemDelegate * del = dynamic_cast<UBGraphicsTextItemDelegate*>(Delegate());
    if (del)
        del->recolor();
}

void UBGraphicsTextItem::setSelected(bool selected)
{
    if(selected){
        Delegate()->createControls();
        Delegate()->frame()->setOperationMode(UBGraphicsDelegateFrame::Resizing);
    }
    QGraphicsTextItem::setSelected(selected);
}

QVariant UBGraphicsTextItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    QVariant newValue = value;

    if(Delegate())
        newValue = Delegate()->itemChange(change, value);

    return QGraphicsTextItem::itemChange(change, newValue);
}

void UBGraphicsTextItem::focusInEvent(QFocusEvent *event)
{
    QGraphicsTextItem::focusInEvent(event);

    // show the spelling mistakes only while the text zone is being edited
    if (mSpellHighlighter && (textInteractionFlags() & Qt::TextEditable))
        mSpellHighlighter->setActive(true);
}

void UBGraphicsTextItem::focusOutEvent(QFocusEvent *event)
{
    QGraphicsTextItem::focusOutEvent(event);

    // a context menu or another window taking the focus does not end the editing
    if (event->reason() == Qt::PopupFocusReason || event->reason() == Qt::ActiveWindowFocusReason)
        return;

    if (mSpellHighlighter)
        mSpellHighlighter->setActive(false);
}

void UBGraphicsTextItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!(textInteractionFlags() & Qt::TextEditable) || !UBSpellChecker::instance()->isAvailable())
    {
        QGraphicsTextItem::contextMenuEvent(event);
        return;
    }

    // find the word under the mouse
    const int position = document()->documentLayout()->hitTest(event->pos(), Qt::FuzzyHit);
    QTextBlock block = document()->findBlock(qMax(position, 0));
    QTextCursor wordCursor;
    QString word;

    if (position >= 0 && block.isValid())
    {
        const int positionInBlock = position - block.position();

        for (const auto& w : UBSpellChecker::words(block.text()))
        {
            if (positionInBlock >= w.first && positionInBlock <= w.first + w.second)
            {
                word = block.text().mid(w.first, w.second);
                wordCursor = QTextCursor(document());
                wordCursor.setPosition(block.position() + w.first);
                wordCursor.setPosition(block.position() + w.first + w.second, QTextCursor::KeepAnchor);
                break;
            }
        }
    }

    QMenu menu;
    const bool misspelled = !word.isEmpty() && !UBSpellChecker::instance()->isCorrect(word);

    if (misspelled)
    {
        const QStringList suggestions = UBSpellChecker::instance()->suggestions(word).mid(0, 8);

        if (suggestions.isEmpty())
            menu.addAction(QString::fromUtf8("(aucune suggestion)"))->setEnabled(false);

        for (const QString& suggestion : suggestions)
        {
            QAction* action = menu.addAction(suggestion);
            QFont font = action->font();
            font.setBold(true);
            action->setFont(font);

            connect(action, &QAction::triggered, this, [this, wordCursor, suggestion]() {
                QTextCursor cursor(wordCursor);
                cursor.insertText(suggestion);
                setTextCursor(cursor);
                contentsChanged();
            });
        }

        menu.addSeparator();
        connect(menu.addAction(QString::fromUtf8("Ajouter « %1 » au dictionnaire").arg(word)), &QAction::triggered, this, [this, word]() {
            UBSpellChecker::instance()->addToPersonalDictionary(word);
            if (mSpellHighlighter)
                mSpellHighlighter->rehighlight();
        });
        menu.addSeparator();
    }

    // usual editing actions
    QTextCursor cursor = textCursor();
    const bool hasSelection = cursor.hasSelection();

    QAction* cut = menu.addAction(QString::fromUtf8("Couper"));
    cut->setEnabled(hasSelection);
    connect(cut, &QAction::triggered, this, [this]() {
        QTextCursor c = textCursor();
        QApplication::clipboard()->setText(c.selectedText().replace(QChar::ParagraphSeparator, '\n'));
        c.removeSelectedText();
        setTextCursor(c);
    });

    QAction* copy = menu.addAction(QString::fromUtf8("Copier"));
    copy->setEnabled(hasSelection);
    connect(copy, &QAction::triggered, this, [this]() {
        QApplication::clipboard()->setText(textCursor().selectedText().replace(QChar::ParagraphSeparator, '\n'));
    });

    QAction* paste = menu.addAction(QString::fromUtf8("Coller"));
    paste->setEnabled(!QApplication::clipboard()->text().isEmpty());
    connect(paste, &QAction::triggered, this, [this]() {
        QTextCursor c = textCursor();
        c.insertText(QApplication::clipboard()->text());
        setTextCursor(c);
    });

    menu.addSeparator();
    connect(menu.addAction(QString::fromUtf8("Tout sélectionner")), &QAction::triggered, this, [this]() {
        QTextCursor c(document());
        c.select(QTextCursor::Document);
        setTextCursor(c);
    });

    menu.exec(event->screenPos());
    event->accept();
}

void UBGraphicsTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{

    // scene()->itemAt(pos) returns 0 if pos is not over text, but over text item, but mouse press comes.
    // It is a cludge...
    if (UBStylusTool::Play == UBDrawingController::drawingController()->stylusTool())
    {
        QGraphicsTextItem::mousePressEvent(event);
        event->accept();
        clearFocus();
        return;
    }

    if (Delegate())
    {
        Delegate()->mousePressEvent(event);
        if (Delegate() && parentItem() && UBGraphicsGroupContainerItem::Type == parentItem()->type())
        {
            UBGraphicsGroupContainerItem *group = qgraphicsitem_cast<UBGraphicsGroupContainerItem*>(parentItem());
            if (group)
            {
                QGraphicsItem *curItem = group->getCurrentItem();
                if (curItem && this != curItem)
                {
                    group->deselectCurrentItem();
                }
                group->setCurrentItem(this);
                this->setSelected(true);
                Delegate()->positionHandles();
            }

        }
    }

    if (!data(UBGraphicsItemData::ItemEditable).toBool()) {
        setTextInteractionFlags(Qt::NoTextInteraction);
        return;
    }

    setTextInteractionFlags(Qt::TextEditorInteraction);

    int elapsed = mLastMousePressTime.msecsTo(QTime::currentTime());

    if (elapsed < UBApplication::app()->doubleClickInterval())
    {
        mMultiClickState++;
        if (mMultiClickState > 3)
            mMultiClickState = 1;
    }
    else
    {
        mMultiClickState = 1;
    }

    mLastMousePressTime = QTime::currentTime();

    if (mMultiClickState == 1)
    {
        activateTextEditor(true);
        QGraphicsTextItem::mousePressEvent(event);
        setFocus();
    }
    else if (mMultiClickState == 2)
    {
        QTextCursor tc= textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        setTextCursor(tc);
    }
    else if (mMultiClickState == 3)
    {
        QTextCursor tc= textCursor();
        tc.select(QTextCursor::Document);
        setTextCursor(tc);
    }
    else
    {
        mMultiClickState = 0;
    }
}

void UBGraphicsTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!Delegate() || !Delegate()->mouseMoveEvent(event))
    {
        QGraphicsTextItem::mouseMoveEvent(event);
    }
}

void UBGraphicsTextItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{

}

void UBGraphicsTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // scene()->itemAt(pos) returns 0 if pos is not over text, but over text item, but mouse press comes.
    // It is a cludge...
    if (UBStylusTool::Play == UBDrawingController::drawingController()->stylusTool())
    {
        event->accept();
        clearFocus();
        return;
    }

    if (mMultiClickState == 1)
    {
        QGraphicsTextItem::mouseReleaseEvent(event);

        if (Delegate())
            Delegate()->mouseReleaseEvent(event);
    }
    else
    {
        event->accept();
    }
}

void UBGraphicsTextItem::keyPressEvent(QKeyEvent *event)
{
    if (Delegate() && !Delegate()->keyPressEvent(event)) {
        qDebug() << "UBGraphicsTextItem::keyPressEvent(QKeyEvent *event) has been rejected by delegate. Don't call base class method";
        return;
    }

    if (event->matches (QKeySequence::StandardKey::Bold) ||
        event->matches (QKeySequence::StandardKey::Underline) ||
        event->matches (QKeySequence::StandardKey::Italic))
    {
        QTextCursor curCursor = textCursor();
        QTextCharFormat format;
        QFont ft(curCursor.charFormat().font());
        if (event->matches (QKeySequence::StandardKey::Bold))
            ft.setBold(!ft.bold());
        else if (event->matches (QKeySequence::StandardKey::Underline))
            ft.setUnderline(!ft.underline());
        else if (event->matches (QKeySequence::StandardKey::Italic))
            ft.setItalic(!ft.italic());
        format.setFont(ft);
        curCursor.mergeCharFormat(format);
        setTextCursor(curCursor);

        contentsChanged();
        return;
    }

    QGraphicsTextItem::keyPressEvent(event);
}

void UBGraphicsTextItem::keyReleaseEvent(QKeyEvent *event)
{
    if (Delegate() && !Delegate()->keyReleaseEvent(event)) {
        qDebug() << "UBGraphicsTextItem::keyPressEvent(QKeyEvent *event) has been rejected by delegate. Don't call base class method";
        return;
    }

    QGraphicsTextItem::keyReleaseEvent(event);
}

void UBGraphicsTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Never draw the rubber band, we draw our custom selection with the DelegateFrame
    QStyleOptionGraphicsItem styleOption = QStyleOptionGraphicsItem(*option);
    styleOption.state &= ~QStyle::State_Selected;
    styleOption.state &= ~QStyle::State_HasFocus;

    QGraphicsTextItem::paint(painter, &styleOption, widget);

    if (widget == UBApplication::boardController->controlView()->viewport() && !isSelected())
    {
        setTextInteractionFlags(Qt::NoTextInteraction);
        if (toPlainText().isEmpty())
        {
            painter->setFont(font());
            painter->setPen(UBSettings::paletteColor);
            painter->drawText(boundingRect(), Qt::AlignLeft, mTypeTextHereLabel);
        }
    }

    Delegate()->postpaint(painter, option, widget);
}


UBItem* UBGraphicsTextItem::deepCopy() const
{
    UBGraphicsTextItem* copy = new UBGraphicsTextItem();

    copyItemParameters(copy);

   // TODO UB 4.7 ... complete all members ?

   return copy;
}

void UBGraphicsTextItem::copyItemParameters(UBItem *copy) const
{
    UBGraphicsTextItem *cp = dynamic_cast<UBGraphicsTextItem*>(copy);
    if (cp)
    {
        cp->setHtml(toHtml());
        cp->setPos(this->pos());
        cp->setTransform(this->transform());
        cp->setFlag(QGraphicsItem::ItemIsMovable, true);
        cp->setFlag(QGraphicsItem::ItemIsSelectable, true);
        cp->setData(UBGraphicsItemData::ItemLayerType, this->data(UBGraphicsItemData::ItemLayerType));
        cp->setData(UBGraphicsItemData::ItemLocked, this->data(UBGraphicsItemData::ItemLocked));
        cp->setData(UBGraphicsItemData::ItemIsHiddenOnDisplay, this->data(UBGraphicsItemData::ItemIsHiddenOnDisplay));
        cp->setData(UBGraphicsItemData::ItemEditable, data(UBGraphicsItemData::ItemEditable).toBool());
        cp->setData(UBGraphicsItemData::ItemOwnZValue, this->data(UBGraphicsItemData::ItemOwnZValue));
        cp->setTextWidth(this->textWidth());
        cp->setTextHeight(this->textHeight());

        cp->setSourceUrl(this->sourceUrl());
        cp->setZValue(this->zValue());
    }
}

QRectF UBGraphicsTextItem::boundingRect() const
{
    qreal width = textWidth();
    qreal height = textHeight();
    return QRectF(QPointF(), QSizeF(width, height));
}


QPainterPath UBGraphicsTextItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void UBGraphicsTextItem::setTextWidth(qreal width)
{
    qreal titleBarWidth = 0;
    UBGraphicsTextItemDelegate * del = dynamic_cast<UBGraphicsTextItemDelegate*>(Delegate());
    if (del)
        titleBarWidth = del->titleBarWidth();

    qreal newWidth = qMax(titleBarWidth, width);

    QGraphicsTextItem::setTextWidth(newWidth);
}


void UBGraphicsTextItem::setTextHeight(qreal height)
{
    QFontMetrics fm(font());
    qreal minHeight = fm.height() + document()->documentMargin() * 2;
    mTextHeight = qMax(minHeight, height);
    update();
    setFocus();
}


qreal UBGraphicsTextItem::textHeight() const
{
    return mTextHeight;
}

/**
 * @brief Get the ratio between font size in pixels and points.
 * @return The ratio of pixel size to point size of the first character, or 0 if the text item is empty.
 *
 * Qt may display fonts differently on different platforms -- on the same display,
 * the same point size may be displayed at different pixel sizes. This function returns the
 * ratio of pixel size to point size, based on the first character in the text item.
 */
qreal UBGraphicsTextItem::pixelsPerPoint() const
{
    QTextCursor cursor = textCursor();
    if (cursor.isNull())
        return 0;

    cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

    QFont f = cursor.charFormat().font();
    qDebug() << "ppp. Font: " << f;
    QFontInfo fi(cursor.charFormat().font());

    qreal pixelSize = fi.pixelSize();
    qreal pointSize = fi.pointSizeF();

    //qDebug() << "Pixel size: " << pixelSize;
    //qDebug() << "Point size: " << pointSize;

    if (pointSize == 0)
        return 0;

    return pixelSize/pointSize;
}


void UBGraphicsTextItem::contentsChanged()
{
    if (scene())
    {
        scene()->setModified(true);
    }
}


std::shared_ptr<UBGraphicsScene> UBGraphicsTextItem::scene()
{
    auto scenePtr = dynamic_cast<UBGraphicsScene*>(QGraphicsItem::scene());
    return scenePtr ? scenePtr->shared_from_this() : nullptr;
}


void UBGraphicsTextItem::resize(qreal w, qreal h)
{
    setTextWidth(w);
    setTextHeight(document()->size().height());
    if (Delegate())
        Delegate()->positionHandles();
}


QSizeF UBGraphicsTextItem::size() const
{
    return QSizeF(textWidth(), textHeight());
}

void UBGraphicsTextItem::setUuid(const QUuid &pUuid)
{
    UBItem::setUuid(pUuid);
    setData(UBGraphicsItemData::ItemUuid, QVariant(pUuid)); //store item uuid inside the QGraphicsItem to fast operations with Items on the scene
}


void UBGraphicsTextItem::undoCommandAdded()
{
    emit textUndoCommandAdded(this);
}


void UBGraphicsTextItem::documentSizeChanged(const QSizeF & newSize)
{
    resize(newSize.width(), newSize.height());
}

void UBGraphicsTextItem::activateTextEditor(bool activate)
{
    qDebug() << textInteractionFlags();

    this->isActivatedTextEditor = activate;

    if(!activate){
        setTextInteractionFlags(Qt::TextSelectableByMouse);
    }else{
        setTextInteractionFlags(Qt::TextEditorInteraction);
    }

    qDebug() <<  textInteractionFlags();
}
