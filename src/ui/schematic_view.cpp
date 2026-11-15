/*!
 * \file schematic_view.cpp
 * \brief Draws components and wires on the schematic grid.
 */

#include "schematic_view.h"

#include <cmath>

#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>
#include <QScrollBar>

namespace Analyzer
{

namespace
{

constexpr qreal CellPixels = 28.0;
constexpr qreal LabelWidth = 88.0;
constexpr qreal LabelHeight = 36.0;
constexpr qreal PinRadius = 3.5;
constexpr qreal ZoomInFactor = 1.15;
constexpr qreal ZoomOutFactor = 1.0 / ZoomInFactor;
constexpr int ComponentIdKey = 0;
constexpr int AnchorXKey = 1;
constexpr int AnchorYKey = 2;
constexpr int NoteKey = 3;
constexpr qreal NoteOffsetX = 8.0;
constexpr qreal NoteOffsetY = -16.0;

QPointF ToScene(GridPoint point)
{
   return QPointF(static_cast<qreal>(point.positionX) * CellPixels, static_cast<qreal>(point.positionY) * CellPixels);
}

QPointF PinScene(const Component& component, uint8_t pinIndex)
{
   return ToScene(PinPosition(component, pinIndex));
}

} // namespace

SchematicView::SchematicView(QWidget* pParent)
   : QGraphicsView(pParent)
   , _pScene(new QGraphicsScene(this))
   , _placement(Placement::Idle)
   , _wirePreview()
   , _notes()
   , _pDragItem(nullptr)
   , _dragAnchor()
   , _dragGrab()
   , _panStart()
   , _spaceDown(false)
   , _panning(false)
{
   setScene(_pScene);
   setRenderHint(QPainter::Antialiasing, true);
   setDragMode(QGraphicsView::RubberBandDrag);
   setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
   _pScene->setSceneRect(-20.0 * CellPixels, -20.0 * CellPixels, 140.0 * CellPixels, 90.0 * CellPixels);
   setBackgroundBrush(QColor(250, 250, 250));
}

void SchematicView::ShowCircuit(const Circuit& circuit)
{
   _pScene->clear();
   QPen wirePen(QColor(70, 70, 70), 2.0);
   for (const Wire& wire : circuit.Wires())
   {
      for (size_t index = 1; index < wire.points.size(); index += 1)
      {
         _pScene->addLine(QLineF(ToScene(wire.points[index - 1]), ToScene(wire.points[index])), wirePen);
      }
   }
   QPen bodyPen(QColor(20, 70, 140), 1.5);
   QPen pinPen(QColor(180, 40, 40));
   for (const Component& component : circuit.Components())
   {
      QGraphicsItemGroup* pGroup = new QGraphicsItemGroup();
      pGroup->setData(ComponentIdKey, QVariant(static_cast<qulonglong>(component.id)));
      pGroup->setData(AnchorXKey, component.position.positionX);
      pGroup->setData(AnchorYKey, component.position.positionY);
      pGroup->setFlag(QGraphicsItem::ItemIsSelectable, true);
      _pScene->addItem(pGroup);
      uint8_t pinCount = PinCount(component);
      if (pinCount >= 2)
      {
         QGraphicsLineItem* pBody = new QGraphicsLineItem(QLineF(PinScene(component, 0), PinScene(component, static_cast<uint8_t>(pinCount - 1))), pGroup);
         pBody->setPen(bodyPen);
      }
      for (uint8_t pinIndex = 0; pinIndex < pinCount; pinIndex += 1)
      {
         QPointF pin = PinScene(component, pinIndex);
         QGraphicsEllipseItem* pPin = new QGraphicsEllipseItem(pin.x() - PinRadius, pin.y() - PinRadius, PinRadius * 2.0, PinRadius * 2.0, pGroup);
         pPin->setPen(pinPen);
      }
      QPointF anchor = ToScene(component.position);
      QGraphicsRectItem* pLabel = new QGraphicsRectItem(anchor.x() + 6.0, anchor.y() - (LabelHeight * 0.5), LabelWidth, LabelHeight, pGroup);
      pLabel->setPen(bodyPen);
      pLabel->setBrush(QColor(255, 255, 255));
      pLabel->setFlag(QGraphicsItem::ItemIsSelectable, true);
      pLabel->setData(ComponentIdKey, QVariant(static_cast<qulonglong>(component.id)));
      QGraphicsSimpleTextItem* pText = new QGraphicsSimpleTextItem(QString::fromStdString(component.name + "\n" + KindLabel(component.kind)), pLabel);
      pText->setPos(anchor.x() + 10.0, anchor.y() - (LabelHeight * 0.5) + 2.0);
   }
   QPen previewPen(QColor(30, 110, 200), 2.0, Qt::DashLine);
   for (size_t index = 1; index < _wirePreview.size(); index += 1)
   {
      _pScene->addLine(QLineF(ToScene(_wirePreview[index - 1]), ToScene(_wirePreview[index])), previewPen);
   }
   if (_wirePreview.size() == 1)
   {
      const QPointF start = ToScene(_wirePreview.front());
      _pScene->addEllipse(start.x() - PinRadius, start.y() - PinRadius, PinRadius * 2.0, PinRadius * 2.0, previewPen);
   }
   DrawNotes();
}

void SchematicView::SetNotes(const SheetNoteList& notes)
{
   _notes = notes;
}

void SchematicView::RefreshNotes(void)
{
   DrawNotes();
}

void SchematicView::DrawNotes(void)
{
   QList<QGraphicsItem*> existing = _pScene->items();
   QList<QGraphicsItem*> stale;
   for (QGraphicsItem* pItem : existing)
   {
      if (pItem != nullptr && pItem->data(NoteKey).isValid())
      {
         stale.append(pItem);
      }
   }
   for (QGraphicsItem* pItem : stale)
   {
      _pScene->removeItem(pItem);
      delete pItem;
   }
   for (const SheetNote& note : _notes)
   {
      if (note.text.empty())
      {
         continue;
      }
      QGraphicsSimpleTextItem* pText = _pScene->addSimpleText(QString::fromStdString(note.text));
      const QPointF origin = ToScene(note.point);
      pText->setPos(origin.x() + NoteOffsetX, origin.y() + NoteOffsetY);
      pText->setBrush(QColor(20, 110, 60));
      pText->setData(NoteKey, true);
      pText->setFlag(QGraphicsItem::ItemIsSelectable, false);
   }
}

void SchematicView::SetPlacement(Placement placement)
{
   _placement = placement;
}

void SchematicView::SetWirePreview(const GridPointList& points)
{
   _wirePreview = points;
}

uint64_t SchematicView::ComponentIdOf(QGraphicsItem* pItem) const
{
   while (pItem != nullptr)
   {
      const QVariant stored = pItem->data(ComponentIdKey);
      if (stored.isValid())
      {
         return stored.toULongLong();
      }
      pItem = pItem->parentItem();
   }
   return 0;
}

uint64_t SchematicView::SelectedComponentId(void) const
{
   const QList<QGraphicsItem*> selected = _pScene->selectedItems();
   if (selected.isEmpty())
   {
      return 0;
   }
   return ComponentIdOf(selected.front());
}

void SchematicView::drawBackground(QPainter* pPainter, const QRectF& rect)
{
   QGraphicsView::drawBackground(pPainter, rect);
   pPainter->setPen(QPen(QColor(226, 226, 226)));
   const qreal leftEdge = std::floor(rect.left() / CellPixels) * CellPixels;
   const qreal rightEdge = rect.right();
   const qreal topEdge = std::floor(rect.top() / CellPixels) * CellPixels;
   const qreal bottomEdge = rect.bottom();
   for (qreal column = leftEdge; column <= rightEdge; column += CellPixels)
   {
      pPainter->drawLine(QPointF(column, topEdge), QPointF(column, bottomEdge));
   }
   for (qreal row = topEdge; row <= bottomEdge; row += CellPixels)
   {
      pPainter->drawLine(QPointF(leftEdge, row), QPointF(rightEdge, row));
   }
}

void SchematicView::mousePressEvent(QMouseEvent* pEvent)
{
   if ((_placement == Placement::Armed || _placement == Placement::Wire) && pEvent->button() == Qt::LeftButton)
   {
      const QPointF scenePoint = mapToScene(pEvent->position().toPoint());
      const int positionX = static_cast<int>(std::lround(scenePoint.x() / CellPixels));
      const int positionY = static_cast<int>(std::lround(scenePoint.y() / CellPixels));
      if (_placement == Placement::Armed)
      {
         emit PlaceRequested(positionX, positionY);
      }
      else
      {
         emit WirePointRequested(positionX, positionY);
      }
      pEvent->accept();
      return;
   }
   if (pEvent->button() == Qt::MiddleButton || (_spaceDown && pEvent->button() == Qt::LeftButton))
   {
      _panning = true;
      _panStart = pEvent->position().toPoint();
      pEvent->accept();
      return;
   }
   if (_placement == Placement::Idle && pEvent->button() == Qt::LeftButton)
   {
      QGraphicsItem* pHit = itemAt(pEvent->position().toPoint());
      const uint64_t componentId = ComponentIdOf(pHit);
      if (componentId != 0)
      {
         QGraphicsItem* pGroup = pHit;
         while (pGroup != nullptr && pGroup->parentItem() != nullptr)
         {
            pGroup = pGroup->parentItem();
         }
         _pScene->clearSelection();
         if (pGroup != nullptr)
         {
            pGroup->setSelected(true);
            _pDragItem = pGroup;
            _dragAnchor = ToScene(GridPoint{pGroup->data(AnchorXKey).toInt(), pGroup->data(AnchorYKey).toInt()});
            _dragGrab = mapToScene(pEvent->position().toPoint()) - _dragAnchor;
         }
         pEvent->accept();
         return;
      }
   }
   QGraphicsView::mousePressEvent(pEvent);
}

void SchematicView::mouseMoveEvent(QMouseEvent* pEvent)
{
   if (_panning)
   {
      const QPoint current = pEvent->position().toPoint();
      const QPoint delta = current - _panStart;
      _panStart = current;
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
      verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
      pEvent->accept();
      return;
   }
   if (_pDragItem != nullptr && (pEvent->buttons() & Qt::LeftButton) != 0)
   {
      const QPointF scenePoint = mapToScene(pEvent->position().toPoint());
      _pDragItem->setPos(scenePoint - _dragAnchor - _dragGrab);
      pEvent->accept();
      return;
   }
   QGraphicsView::mouseMoveEvent(pEvent);
}

void SchematicView::mouseReleaseEvent(QMouseEvent* pEvent)
{
   if (_panning && (pEvent->button() == Qt::MiddleButton || pEvent->button() == Qt::LeftButton))
   {
      _panning = false;
      pEvent->accept();
      return;
   }
   if (_pDragItem != nullptr && pEvent->button() == Qt::LeftButton)
   {
      const QPointF shifted = _dragAnchor + _pDragItem->pos();
      const qulonglong componentId = static_cast<qulonglong>(ComponentIdOf(_pDragItem));
      const int positionX = static_cast<int>(std::lround(shifted.x() / CellPixels));
      const int positionY = static_cast<int>(std::lround(shifted.y() / CellPixels));
      const int originX = _pDragItem->data(AnchorXKey).toInt();
      const int originY = _pDragItem->data(AnchorYKey).toInt();
      _pDragItem = nullptr;
      if (positionX != originX || positionY != originY)
      {
         emit ComponentMoved(componentId, positionX, positionY);
      }
      pEvent->accept();
      return;
   }
   QGraphicsView::mouseReleaseEvent(pEvent);
}

void SchematicView::keyPressEvent(QKeyEvent* pEvent)
{
   if (pEvent->key() == Qt::Key_Escape)
   {
      emit PlacementCancelled();
      pEvent->accept();
      return;
   }
   if (pEvent->key() == Qt::Key_Delete)
   {
      emit DeleteRequested();
      pEvent->accept();
      return;
   }
   if (_placement == Placement::Wire && (pEvent->key() == Qt::Key_Return || pEvent->key() == Qt::Key_Enter))
   {
      emit WireFinishRequested();
      pEvent->accept();
      return;
   }
   if (pEvent->key() == Qt::Key_R && _placement == Placement::Idle)
   {
      emit RotateRequested();
      pEvent->accept();
      return;
   }
   if (pEvent->key() == Qt::Key_Space)
   {
      _spaceDown = true;
      pEvent->accept();
      return;
   }
   QGraphicsView::keyPressEvent(pEvent);
}

void SchematicView::keyReleaseEvent(QKeyEvent* pEvent)
{
   if (pEvent->key() == Qt::Key_Space)
   {
      _spaceDown = false;
      _panning = false;
      pEvent->accept();
      return;
   }
   QGraphicsView::keyReleaseEvent(pEvent);
}

void SchematicView::wheelEvent(QWheelEvent* pEvent)
{
   if (pEvent->angleDelta().y() > 0)
   {
      scale(ZoomInFactor, ZoomInFactor);
   }
   else if (pEvent->angleDelta().y() < 0)
   {
      scale(ZoomOutFactor, ZoomOutFactor);
   }
   pEvent->accept();
}

} // namespace Analyzer
