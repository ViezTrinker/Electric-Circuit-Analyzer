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
   Armed
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
    * \brief Arms or cancels click-to-place.
    *
    * \param[in] placement Idle ignores empty clicks. Armed emits PlaceRequested.
    */
   void SetPlacement(Placement placement);

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
    * \brief Handles Escape and Delete.
    *
    * \param[in] pEvent Key event.
    */
   void keyPressEvent(QKeyEvent* pEvent) override;

   /*!
    * \brief Zooms the sheet with the mouse wheel.
    *
    * \param[in] pEvent Wheel event.
    */
   void wheelEvent(QWheelEvent* pEvent) override;

private:
   QGraphicsScene* _pScene;
   Placement _placement;
};

} // namespace Analyzer

#endif // SCHEMATIC_VIEW_H
