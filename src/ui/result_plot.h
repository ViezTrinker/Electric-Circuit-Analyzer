/*!
 * \file result_plot.h
 * \brief Diagrams for DC bars, Bode, time, and spectrum results.
 */

#ifndef RESULT_PLOT_H
#define RESULT_PLOT_H

#include <QPaintEvent>
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
   Spectrum
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
    * \brief Removes every bar and curve.
    */
   void Clear(void);

protected:
   /*!
    * \brief Paints the current diagram.
    *
    * \param[in] pEvent Paint event.
    */
   void paintEvent(QPaintEvent* pEvent) override;

private:
   PlotKind _kind;
   std::string _title;
   NodeValueList _nodes;
   PlotSeriesList _series;
   PlotSeriesList _spectrum;
};

} // namespace Analyzer

#endif // RESULT_PLOT_H
