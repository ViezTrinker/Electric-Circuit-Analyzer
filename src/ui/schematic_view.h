/*!
 * \file schematic_view.h
 * \brief Grid view that draws the current circuit.
 */

#ifndef SCHEMATIC_VIEW_H
#define SCHEMATIC_VIEW_H

#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "core/circuit.h"

namespace Analyzer
{

/*!
 * \brief Whether the next sheet click places a part.
 */
enum class Placement : int8_t
{
   Idle = 0,
   Armed,
   Wire
};

/*!
 * \brief Schematic sheet. Parts are drawn from pin positions on the grid.
 */
class SchematicView : public QGraphicsView
{
   Q_OBJECT

public:
   /*!
    * \brief Creates an empty sheet.
    *
    * \param[in] pParent Parent widget. May be null.
    */
   explicit SchematicView(QWidget* pParent);

   /*!
    * \brief Redraws every component and wire.
    *
    * \param[in] circuit Document to display.
    */
   void ShowCircuit(const Circuit& circuit);

   /*!
    * \brief Arms click-to-place or orthogonal wire drawing.
    *
    * \param[in] placement Idle selects parts. Armed emits PlaceRequested. Wire emits WirePointRequested.
    */
   void SetPlacement(Placement placement);

   /*!
    * \brief Stores the wire that is still being drawn.
    *
    * The next ShowCircuit paints these points as a dashed preview.
    *
    * \param[in] points Grid points in click order.
    */
   void SetWirePreview(const GridPointList& points);

   /*!
    * \brief Id of the selected part, or 0.
    */
   uint64_t SelectedComponentId(void) const;

signals:
   /*!
    * \brief The user clicked an empty grid cell while placement is armed.
    *
    * \param[in] positionX Grid column.
    * \param[in] positionY Grid row.
    */
   void PlaceRequested(int positionX, int positionY);

   /*!
    * \brief The user pressed Escape.
    */
   void PlacementCancelled(void);

   /*!
    * \brief The user pressed Delete.
    */
   void DeleteRequested(void);

   /*!
    * \brief The user clicked a grid point while drawing a wire.
    *
    * \param[in] positionX Grid column.
    * \param[in] positionY Grid row.
    */
   void WirePointRequested(int positionX, int positionY);

   /*!
    * \brief The user pressed Enter to keep the wire being drawn.
    */
   void WireFinishRequested(void);

   /*!
    * \brief The user dropped a part on a new grid cell.
    *
    * \param[in] componentId Part that moved.
    * \param[in] positionX New grid column.
    * \param[in] positionY New grid row.
    */
   void ComponentMoved(qulonglong componentId, int positionX, int positionY);

   /*!
    * \brief The user pressed R to turn the selection clockwise.
    */
   void RotateRequested(void);

protected:
   /*!
    * \brief Draws the grid.
    *
    * \param[in] pPainter Painter for the background.
    * \param[in] rect Exposed scene rectangle.
    */
   void drawBackground(QPainter* pPainter, const QRectF& rect) override;

   /*!
    * \brief Places a part or selects an existing one.
    *
    * \param[in] pEvent Mouse event.
    */
   void mousePressEvent(QMouseEvent* pEvent) override;

   /*!
    * \brief Handles Escape, Delete, rotate, and pan.
    *
    * \param[in] pEvent Key event.
    */
   void keyPressEvent(QKeyEvent* pEvent) override;

   /*!
    * \brief Ends temporary panning.
    *
    * \param[in] pEvent Key event.
    */
   void keyReleaseEvent(QKeyEvent* pEvent) override;

   /*!
    * \brief Drags a part or pans the sheet.
    *
    * \param[in] pEvent Mouse event.
    */
   void mouseMoveEvent(QMouseEvent* pEvent) override;

   /*!
    * \brief Commits a dragged part.
    *
    * \param[in] pEvent Mouse event.
    */
   void mouseReleaseEvent(QMouseEvent* pEvent) override;

   /*!
    * \brief Zooms the sheet with the mouse wheel.
    *
    * \param[in] pEvent Wheel event.
    */
   void wheelEvent(QWheelEvent* pEvent) override;

private:
   /*!
    * \brief Component id stored on an item or one of its parents.
    *
    * \param[in] pItem Hit item. May be null.
    */
   uint64_t ComponentIdOf(QGraphicsItem* pItem) const;

   QGraphicsScene* _pScene;
   Placement _placement;
   GridPointList _wirePreview;
   QGraphicsItem* _pDragItem;
   QPointF _dragAnchor;
   QPointF _dragGrab;
   QPoint _panStart;
   bool _spaceDown;
   bool _panning;
};

} // namespace Analyzer

#endif // SCHEMATIC_VIEW_H
