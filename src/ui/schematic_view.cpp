/*!
 * \file schematic_view.cpp
 * \brief Draws components and wires on the schematic grid.
 */

#include "schematic_view.h"

#include <cmath>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>

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
      uint8_t pinCount = PinCount(component);
      if (pinCount >= 2)
      {
         _pScene->addLine(QLineF(PinScene(component, 0), PinScene(component, static_cast<uint8_t>(pinCount - 1))), bodyPen);
      }
      for (uint8_t pinIndex = 0; pinIndex < pinCount; pinIndex += 1)
      {
         QPointF pin = PinScene(component, pinIndex);
         _pScene->addEllipse(pin.x() - PinRadius, pin.y() - PinRadius, PinRadius * 2.0, PinRadius * 2.0, pinPen);
      }
      QPointF anchor = ToScene(component.position);
      QGraphicsRectItem* pLabel = _pScene->addRect(anchor.x() + 6.0, anchor.y() - (LabelHeight * 0.5), LabelWidth, LabelHeight, bodyPen, QColor(255, 255, 255));
      pLabel->setFlag(QGraphicsItem::ItemIsSelectable, true);
      pLabel->setData(ComponentIdKey, QVariant(static_cast<qulonglong>(component.id)));
      QGraphicsSimpleTextItem* pText = _pScene->addSimpleText(QString::fromStdString(component.name + "\n" + KindLabel(component.kind)));
      pText->setPos(anchor.x() + 10.0, anchor.y() - (LabelHeight * 0.5) + 2.0);
      pText->setParentItem(pLabel);
   }
}

void SchematicView::SetPlacement(Placement placement)
{
   _placement = placement;
}

uint64_t SchematicView::SelectedComponentId(void) const
{
   const QList<QGraphicsItem*> selected = _pScene->selectedItems();
   if (selected.isEmpty())
   {
      return 0;
   }
   const QVariant stored = selected.front()->data(ComponentIdKey);
   if (stored.isValid() == false)
   {
      return 0;
   }
   return stored.toULongLong();
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
   if (_placement == Placement::Armed && pEvent->button() == Qt::LeftButton)
   {
      const QPointF scenePoint = mapToScene(pEvent->position().toPoint());
      const int positionX = static_cast<int>(std::lround(scenePoint.x() / CellPixels));
      const int positionY = static_cast<int>(std::lround(scenePoint.y() / CellPixels));
      emit PlaceRequested(positionX, positionY);
      pEvent->accept();
      return;
   }
   QGraphicsView::mousePressEvent(pEvent);
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
   QGraphicsView::keyPressEvent(pEvent);
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
