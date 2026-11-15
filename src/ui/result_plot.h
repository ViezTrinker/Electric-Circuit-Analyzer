/*!
 * \file result_plot.h
 * \brief Diagrams for DC bars, Bode, time, spectrum, and timing results.
 */

#ifndef RESULT_PLOT_H
#define RESULT_PLOT_H

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWidget>

#include <string>
#include <vector>

#include "solver/analysis_result.h"

namespace Analyzer
{

/*!
 * \brief Which diagram the results dock is showing.
 */
enum class PlotKind : int8_t
{
   Empty = 0,
   DcBars,
   Bode,
   Time,
   Spectrum,
   Timing
};

/*!
 * \brief Horizontal scale of a diagram axis.
 */
enum class AxisScale : int8_t
{
   Linear = 0,
   Logarithmic
};

/*!
 * \brief One sample on a curve.
 */
struct PlotSample
{
   double x;
   double y;
};

using PlotSampleList = std::vector<PlotSample>;

/*!
 * \brief One named curve. Bode stores phase samples in the extra list.
 */
struct PlotSeries
{
   std::string name;
   PlotSampleList samples;
   PlotSampleList extra;
};

using PlotSeriesList = std::vector<PlotSeries>;

/*!
 * \brief Paints analysis diagrams without a chart library.
 */
class ResultPlot : public QWidget
{
   Q_OBJECT

public:
   /*!
    * \brief Creates an empty diagram.
    *
    * \param[in] pParent Parent widget. May be null.
    */
   explicit ResultPlot(QWidget* pParent);

   /*!
    * \brief Draws one bar for each DC node voltage.
    *
    * \param[in] nodes Node voltages to draw.
    */
   void ShowDc(const NodeValueList& nodes);

   /*!
    * \brief Draws Bode, time, or spectrum curves.
    *
    * \param[in] kind Diagram to draw.
    * \param[in] title Caption above the axes.
    * \param[in] series Curves. Bode phase is series.extra.
    */
   void ShowSeries(PlotKind kind, std::string_view title, const PlotSeriesList& series);

   /*!
    * \brief Draws the time curves above the spectrum.
    *
    * \param[in] time Voltage or logic curves.
    * \param[in] spectrum Magnitude curve. May be empty.
    */
   void ShowTime(const PlotSeriesList& time, const PlotSeriesList& spectrum);

   /*!
    * \brief Draws one stacked logic trace per net, with analog curves underneath.
    *
    * \param[in] logic Logic levels encoded as 0, 1, 2, and 3.
    * \param[in] analog Analog voltage curves. May be empty.
    */
   void ShowTiming(const PlotSeriesList& logic, const PlotSeriesList& analog);

   /*!
    * \brief Removes every bar and curve.
    */
   void Clear(void);

   /*!
    * \brief Places the vertical cursor and redraws it.
    *
    * \param[in] position X value in the same units as the curves.
    */
   void SetCursor(double position);

signals:
   /*!
    * \brief The user clicked the diagram to move the cursor.
    *
    * \param[in] position X value in the same units as the curves.
    */
   void CursorMoved(double position);

protected:
   /*!
    * \brief Paints the current diagram.
    *
    * \param[in] pEvent Paint event.
    */
   void paintEvent(QPaintEvent* pEvent) override;

   /*!
    * \brief Moves the cursor from a click in the plot area.
    *
    * \param[in] pEvent Mouse event.
    */
   void mousePressEvent(QMouseEvent* pEvent) override;

private:
   /*!
    * \brief Remembers the axis a click should map onto.
    *
    * \param[in] area Plot rectangle in widget pixels.
    * \param[in] minimum Left edge of the data range.
    * \param[in] maximum Right edge of the data range.
    * \param[in] scale Linear or logarithmic mapping.
    */
   void RememberAxis(const QRect& area, double minimum, double maximum, AxisScale scale);

   /*!
    * \brief Draws the vertical cursor on the remembered axis.
    *
    * \param[in] pPainter Painter for the widget. Required.
    */
   void DrawCursor(QPainter* pPainter);

   PlotKind _kind;
   std::string _title;
   NodeValueList _nodes;
   PlotSeriesList _series;
   PlotSeriesList _lower;
   PlotSeriesList _spectrum;
   bool _hasCursor;
   double _cursorX;
   QRect _axisArea;
   double _axisMinimum;
   double _axisMaximum;
   AxisScale _axisScale;
};

} // namespace Analyzer

#endif // RESULT_PLOT_H
