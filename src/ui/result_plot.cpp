/*!
 * \file result_plot.cpp
 * \brief Paints DC bars, Bode curves, time curves, a spectrum, and a timing diagram.
 */

#include "result_plot.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "core/si_parser.h"

namespace Analyzer
{

namespace
{

constexpr int PlotMarginLeftPx = 64;
constexpr int PlotMarginOtherPx = 16;
constexpr int TitleHeightPx = 22;
constexpr int MinimumDiagramWidthPx = 280;
constexpr int MinimumDiagramHeightPx = 180;
constexpr int SeriesColorCount = 6;
constexpr double MinimumSpan = 1.0e-9;
constexpr double LogFloor = 1.0e-15;

const QColor SeriesColors[SeriesColorCount] = {
   QColor(46, 110, 190),
   QColor(190, 70, 50),
   QColor(40, 140, 80),
   QColor(140, 90, 20),
   QColor(110, 60, 160),
   QColor(20, 130, 150)
};

struct AxisRange
{
   double minimum;
   double maximum;
};

AxisRange RangeOf(const PlotSeriesList& series, bool useExtra)
{
   AxisRange range{0.0, 1.0};
   bool found = false;
   for (const PlotSeries& seriesItem : series)
   {
      const PlotSampleList& samples = useExtra ? seriesItem.extra : seriesItem.samples;
      for (const PlotSample& sample : samples)
      {
         if (std::isfinite(sample.x) == false || std::isfinite(sample.y) == false)
         {
            continue;
         }
         if (found == false)
         {
            range.minimum = sample.x;
            range.maximum = sample.x;
            found = true;
         }
         range.minimum = std::min(range.minimum, sample.x);
         range.maximum = std::max(range.maximum, sample.x);
      }
   }
   return range;
}

AxisRange ValueRange(const PlotSeriesList& series, bool useExtra)
{
   AxisRange range{0.0, 1.0};
   bool found = false;
   for (const PlotSeries& seriesItem : series)
   {
      const PlotSampleList& samples = useExtra ? seriesItem.extra : seriesItem.samples;
      for (const PlotSample& sample : samples)
      {
         if (std::isfinite(sample.y) == false)
         {
            continue;
         }
         if (found == false)
         {
            range.minimum = sample.y;
            range.maximum = sample.y;
            found = true;
         }
         range.minimum = std::min(range.minimum, sample.y);
         range.maximum = std::max(range.maximum, sample.y);
      }
   }
   double span = range.maximum - range.minimum;
   if (span < MinimumSpan)
   {
      range.minimum -= 1.0;
      range.maximum += 1.0;
      return range;
   }
   range.minimum -= span * 0.05;
   range.maximum += span * 0.05;
   return range;
}

bool UseLogAxis(const AxisRange& range)
{
   return range.minimum > 0.0 && range.maximum / range.minimum >= 10.0;
}

double MapAlong(double value, const AxisRange& range, bool logarithmic, double start, double length)
{
   double fraction = 0.0;
   if (logarithmic && value > 0.0 && range.minimum > 0.0 && range.maximum > range.minimum)
   {
      fraction = (std::log10(value) - std::log10(range.minimum)) / (std::log10(range.maximum) - std::log10(range.minimum));
   }
   else if (range.maximum > range.minimum)
   {
      fraction = (value - range.minimum) / (range.maximum - range.minimum);
   }
   if (fraction < 0.0)
   {
      fraction = 0.0;
   }
   if (fraction > 1.0)
   {
      fraction = 1.0;
   }
   return start + (fraction * length);
}

double UnmapAlong(double pixel, const AxisRange& range, bool logarithmic, double start, double length)
{
   double fraction = 0.0;
   if (length > 0.0)
   {
      fraction = (pixel - start) / length;
   }
   if (fraction < 0.0)
   {
      fraction = 0.0;
   }
   if (fraction > 1.0)
   {
      fraction = 1.0;
   }
   if (logarithmic && range.minimum > 0.0 && range.maximum > range.minimum)
   {
      const double logStart = std::log10(range.minimum);
      const double logStop = std::log10(range.maximum);
      return std::pow(10.0, logStart + (fraction * (logStop - logStart)));
   }
   return range.minimum + (fraction * (range.maximum - range.minimum));
}

void DrawCurve(QPainter* pPainter, const PlotSampleList& samples, const AxisRange& xRange, const AxisRange& yRange, bool logarithmic, const QRect& area, const QColor& color);

void DrawTiming(QPainter* pPainter, const QRect& area, const PlotSeriesList& series)
{
   if (pPainter == nullptr || series.empty() || area.height() <= 0)
   {
      return;
   }
   AxisRange xRange = RangeOf(series, false);
   if (xRange.maximum <= xRange.minimum)
   {
      xRange.maximum = xRange.minimum + 1.0;
   }
   const int bandCount = static_cast<int>(series.size());
   const int bandHeight = std::max(18, area.height() / bandCount);
   constexpr double LogicTop = 3.0;
   AxisRange yRange{-0.25, LogicTop + 0.25};
   int row = 0;
   for (const PlotSeries& seriesItem : series)
   {
      QRect band(area.left(), area.top() + (row * bandHeight), area.width(), bandHeight);
      pPainter->setPen(SeriesColors[row % SeriesColorCount]);
      pPainter->drawText(QRect(band.left(), band.top(), 72, band.height()), Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(seriesItem.name));
      QRect trace(band.left() + 76, band.top() + 2, std::max(8, band.width() - 80), std::max(8, band.height() - 4));
      DrawCurve(pPainter, seriesItem.samples, xRange, yRange, false, trace, SeriesColors[row % SeriesColorCount]);
      row += 1;
   }
}

QString AxisText(double value)
{
   return QString::fromStdString(FormatSiNumber(value));
}

void DrawCurve(QPainter* pPainter, const PlotSampleList& samples, const AxisRange& xRange, const AxisRange& yRange, bool logarithmic, const QRect& area, const QColor& color)
{
   if (pPainter == nullptr || samples.size() < 2)
   {
      return;
   }
   QPainterPath path;
   bool started = false;
   for (const PlotSample& sample : samples)
   {
      if (std::isfinite(sample.x) == false || std::isfinite(sample.y) == false)
      {
         continue;
      }
      double x = MapAlong(sample.x, xRange, logarithmic, area.left(), area.width());
      double y = area.bottom() - MapAlong(sample.y, yRange, false, 0.0, area.height());
      if (started == false)
      {
         path.moveTo(x, y);
         started = true;
      }
      else
      {
         path.lineTo(x, y);
      }
   }
   pPainter->setPen(QPen(color, 1.6));
   pPainter->drawPath(path);
}

void DrawFrame(QPainter* pPainter, const QRect& area, const AxisRange& xRange, const AxisRange& yRange, bool logarithmic, std::string_view caption)
{
   if (pPainter == nullptr)
   {
      return;
   }
   pPainter->setPen(QColor(80, 80, 80));
   pPainter->drawRect(area);
   pPainter->drawText(QRect(area.left(), area.top() - TitleHeightPx, area.width(), TitleHeightPx - 4), Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(caption.data(), static_cast<int>(caption.size())));
   const std::array<double, 3> xMarks = {xRange.minimum, (logarithmic && xRange.minimum > 0.0) ? std::sqrt(xRange.minimum * xRange.maximum) : (xRange.minimum + xRange.maximum) * 0.5, xRange.maximum};
   const std::array<double, 3> yMarks = {yRange.minimum, (yRange.minimum + yRange.maximum) * 0.5, yRange.maximum};
   pPainter->setPen(QColor(120, 120, 120));
   for (double mark : xMarks)
   {
      int x = static_cast<int>(std::lround(MapAlong(mark, xRange, logarithmic, area.left(), area.width())));
      pPainter->drawLine(x, area.bottom(), x, area.bottom() + 4);
      pPainter->drawText(QRect(x - 36, area.bottom() + 4, 72, 16), Qt::AlignHCenter | Qt::AlignTop, AxisText(mark));
   }
   for (double mark : yMarks)
   {
      int y = static_cast<int>(std::lround(area.bottom() - MapAlong(mark, yRange, false, 0.0, area.height())));
      pPainter->drawLine(area.left() - 4, y, area.left(), y);
      pPainter->drawText(QRect(0, y - 8, area.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter, AxisText(mark));
   }
}

void DrawLegend(QPainter* pPainter, const PlotSeriesList& series, int left, int top)
{
   if (pPainter == nullptr)
   {
      return;
   }
   int row = 0;
   for (const PlotSeries& seriesItem : series)
   {
      const QColor color = SeriesColors[row % SeriesColorCount];
      int y = top + (row * 14);
      pPainter->setPen(QPen(color, 2.0));
      pPainter->drawLine(left, y + 6, left + 16, y + 6);
      pPainter->setPen(QColor(40, 40, 40));
      pPainter->drawText(left + 20, y + 11, QString::fromStdString(seriesItem.name));
      row += 1;
   }
}

void DrawBars(QPainter* pPainter, const QRect& widgetRect, const NodeValueList& nodes)
{
   if (pPainter == nullptr)
   {
      return;
   }
   if (nodes.empty())
   {
      pPainter->drawText(widgetRect, Qt::AlignCenter, "Run an analysis to plot a result.");
      return;
   }
   double peak = MinimumSpan;
   bool anyNegative = false;
   for (const NodeValue& node : nodes)
   {
      peak = std::max(peak, std::abs(node.value));
      if (node.value < 0.0)
      {
         anyNegative = true;
      }
   }
   constexpr int LabelColumnPx = 96;
   constexpr int BarGapPx = 6;
   const int plotTop = PlotMarginOtherPx;
   const int plotBottom = std::max(plotTop + 1, widgetRect.height() - PlotMarginOtherPx);
   const int slotCount = static_cast<int>(nodes.size());
   const int slotHeight = std::max(1, (plotBottom - plotTop) / slotCount);
   const int barThickness = std::max(1, slotHeight - BarGapPx);
   int span = std::max(1, widgetRect.width() - PlotMarginOtherPx - LabelColumnPx - PlotMarginOtherPx);
   int zeroX = PlotMarginOtherPx + LabelColumnPx;
   if (anyNegative)
   {
      span = std::max(1, span / 2);
      zeroX = PlotMarginOtherPx + LabelColumnPx + span;
   }
   pPainter->setPen(QColor(150, 150, 150));
   pPainter->drawLine(zeroX, plotTop, zeroX, plotBottom);
   const QFontMetrics metrics(pPainter->font());
   int row = 0;
   for (const NodeValue& node : nodes)
   {
      const int barTop = plotTop + (row * slotHeight);
      const QString name = metrics.elidedText(QString::fromStdString(node.name), Qt::ElideRight, LabelColumnPx - BarGapPx);
      pPainter->setPen(QColor(40, 40, 40));
      pPainter->drawText(QRect(PlotMarginOtherPx, barTop, LabelColumnPx - BarGapPx, barThickness), Qt::AlignVCenter | Qt::AlignLeft, name);
      const int signedWidth = static_cast<int>(std::lround((node.value / peak) * static_cast<double>(span)));
      if (signedWidth > 0)
      {
         pPainter->fillRect(QRect(zeroX, barTop, signedWidth, barThickness), SeriesColors[0]);
      }
      else if (signedWidth < 0)
      {
         pPainter->fillRect(QRect(zeroX + signedWidth, barTop, -signedWidth, barThickness), SeriesColors[1]);
      }
      row += 1;
   }
}

void DrawSeries(QPainter* pPainter, const QRect& area, const PlotSeriesList& series, bool useExtra, std::string_view caption, bool logarithmic)
{
   AxisRange xRange = RangeOf(series, useExtra);
   if (xRange.maximum <= xRange.minimum)
   {
      xRange.maximum = xRange.minimum + 1.0;
   }
   if (logarithmic && xRange.minimum <= 0.0)
   {
      xRange.minimum = LogFloor;
   }
   AxisRange yRange = ValueRange(series, useExtra);
   DrawFrame(pPainter, area, xRange, yRange, logarithmic, caption);
   int row = 0;
   for (const PlotSeries& seriesItem : series)
   {
      const PlotSampleList& samples = useExtra ? seriesItem.extra : seriesItem.samples;
      DrawCurve(pPainter, samples, xRange, yRange, logarithmic, area, SeriesColors[row % SeriesColorCount]);
      row += 1;
   }
}

} // namespace

ResultPlot::ResultPlot(QWidget* pParent)
   : QWidget(pParent)
   , _kind(PlotKind::Empty)
   , _hasCursor(false)
   , _cursorX(0.0)
   , _axisArea()
   , _axisMinimum(0.0)
   , _axisMaximum(1.0)
   , _axisScale(AxisScale::Linear)
{
   setMinimumSize(MinimumDiagramWidthPx, MinimumDiagramHeightPx);
   setAutoFillBackground(true);
   setMouseTracking(false);
}

void ResultPlot::ShowDc(const NodeValueList& nodes)
{
   _kind = PlotKind::DcBars;
   _title = "Node voltages";
   _nodes = nodes;
   _series.clear();
   _lower.clear();
   _spectrum.clear();
   _hasCursor = false;
   update();
}

void ResultPlot::ShowSeries(PlotKind kind, std::string_view title, const PlotSeriesList& series)
{
   _kind = kind;
   _title = std::string(title);
   _nodes.clear();
   _series = series;
   _lower.clear();
   _spectrum.clear();
   _hasCursor = false;
   update();
}

void ResultPlot::ShowTime(const PlotSeriesList& time, const PlotSeriesList& spectrum)
{
   _kind = PlotKind::Time;
   _title = "Time";
   _nodes.clear();
   _series = time;
   _lower.clear();
   _spectrum = spectrum;
   _hasCursor = false;
   update();
}

void ResultPlot::ShowTiming(const PlotSeriesList& logic, const PlotSeriesList& analog)
{
   _kind = PlotKind::Timing;
   _title = "Timing";
   _nodes.clear();
   _series = logic;
   _lower = analog;
   _spectrum.clear();
   _hasCursor = false;
   update();
}

void ResultPlot::Clear(void)
{
   _kind = PlotKind::Empty;
   _title.clear();
   _nodes.clear();
   _series.clear();
   _lower.clear();
   _spectrum.clear();
   _hasCursor = false;
   update();
}

void ResultPlot::SetCursor(double position)
{
   _cursorX = position;
   _hasCursor = std::isfinite(position);
   update();
}

void ResultPlot::RememberAxis(const QRect& area, double minimum, double maximum, AxisScale scale)
{
   _axisArea = area;
   _axisMinimum = minimum;
   _axisMaximum = maximum;
   _axisScale = scale;
}

void ResultPlot::paintEvent(QPaintEvent* pEvent)
{
   (void)pEvent;
   QPainter painter(this);
   painter.fillRect(rect(), QColor(255, 255, 255));
   painter.setPen(QColor(40, 40, 40));
   if (_kind == PlotKind::Empty)
   {
      painter.drawText(rect(), Qt::AlignCenter, "Run an analysis to plot a result.");
      return;
   }
   if (_kind == PlotKind::DcBars)
   {
      DrawBars(&painter, rect(), _nodes);
      return;
   }
   if (_series.empty())
   {
      painter.drawText(rect(), Qt::AlignCenter, "The result has no curves to plot.");
      return;
   }
   const bool logarithmic = _kind == PlotKind::Bode || _kind == PlotKind::Spectrum;
   if (_kind == PlotKind::Bode)
   {
      const int half = std::max(40, (height() - 48) / 2);
      QRect magnitude(PlotMarginLeftPx, TitleHeightPx + 8, width() - PlotMarginLeftPx - PlotMarginOtherPx, half - TitleHeightPx);
      QRect phase(PlotMarginLeftPx, magnitude.bottom() + TitleHeightPx + 12, magnitude.width(), half - TitleHeightPx);
      AxisRange frequency = RangeOf(_series, false);
      if (frequency.maximum <= frequency.minimum)
      {
         frequency.maximum = frequency.minimum + 1.0;
      }
      if (frequency.minimum <= 0.0)
      {
         frequency.minimum = LogFloor;
      }
      RememberAxis(magnitude, frequency.minimum, frequency.maximum, AxisScale::Logarithmic);
      DrawSeries(&painter, magnitude, _series, false, "Magnitude (dB)", true);
      DrawSeries(&painter, phase, _series, true, "Phase (deg)", true);
      DrawLegend(&painter, _series, PlotMarginLeftPx, 2);
      DrawCursor(&painter);
      return;
   }
   if (_kind == PlotKind::Timing)
   {
      const bool split = _lower.empty() == false;
      const int timingHeight = split ? std::max(40, (height() - 48) / 2) : (height() - TitleHeightPx - 36);
      QRect timingArea(PlotMarginLeftPx, TitleHeightPx + 8, width() - PlotMarginLeftPx - PlotMarginOtherPx, timingHeight);
      AxisRange timeRange = RangeOf(_series, false);
      if (timeRange.maximum <= timeRange.minimum)
      {
         timeRange.maximum = timeRange.minimum + 1.0;
      }
      RememberAxis(timingArea, timeRange.minimum, timeRange.maximum, AxisScale::Linear);
      DrawTiming(&painter, timingArea, _series);
      if (split)
      {
         QRect analogArea(PlotMarginLeftPx, timingArea.bottom() + TitleHeightPx + 12, timingArea.width(), timingHeight);
         DrawSeries(&painter, analogArea, _lower, false, "Time", false);
      }
      DrawCursor(&painter);
      return;
   }
   if (_kind == PlotKind::Time && _spectrum.empty() == false)
   {
      const int half = std::max(40, (height() - 48) / 2);
      QRect timeArea(PlotMarginLeftPx, TitleHeightPx + 8, width() - PlotMarginLeftPx - PlotMarginOtherPx, half - TitleHeightPx);
      QRect spectrumArea(PlotMarginLeftPx, timeArea.bottom() + TitleHeightPx + 12, timeArea.width(), half - TitleHeightPx);
      AxisRange timeRange = RangeOf(_series, false);
      if (timeRange.maximum <= timeRange.minimum)
      {
         timeRange.maximum = timeRange.minimum + 1.0;
      }
      RememberAxis(timeArea, timeRange.minimum, timeRange.maximum, AxisScale::Linear);
      DrawSeries(&painter, timeArea, _series, false, "Time", false);
      DrawSeries(&painter, spectrumArea, _spectrum, false, "Spectrum", UseLogAxis(RangeOf(_spectrum, false)));
      DrawLegend(&painter, _series, PlotMarginLeftPx, 2);
      DrawCursor(&painter);
      return;
   }
   QRect area(PlotMarginLeftPx, TitleHeightPx + 8, width() - PlotMarginLeftPx - PlotMarginOtherPx, height() - TitleHeightPx - 36);
   std::string_view caption = _kind == PlotKind::Spectrum ? std::string_view("Spectrum") : std::string_view("Time");
   if (_title.empty() == false)
   {
      caption = _title;
   }
   const bool logAxis = logarithmic && UseLogAxis(RangeOf(_series, false));
   RememberAxis(area, RangeOf(_series, false).minimum, RangeOf(_series, false).maximum, logAxis ? AxisScale::Logarithmic : AxisScale::Linear);
   DrawSeries(&painter, area, _series, false, caption, logAxis);
   DrawLegend(&painter, _series, PlotMarginLeftPx, 2);
   DrawCursor(&painter);
}

void ResultPlot::DrawCursor(QPainter* pPainter)
{
   if (pPainter == nullptr || _hasCursor == false || _axisArea.width() <= 0)
   {
      return;
   }
   AxisRange range{_axisMinimum, _axisMaximum};
   const bool logarithmic = _axisScale == AxisScale::Logarithmic;
   const double x = MapAlong(_cursorX, range, logarithmic, _axisArea.left(), _axisArea.width());
   pPainter->setPen(QColor(20, 20, 20));
   pPainter->drawLine(QPointF(x, _axisArea.top()), QPointF(x, _axisArea.bottom()));
}

void ResultPlot::mousePressEvent(QMouseEvent* pEvent)
{
   if (pEvent == nullptr || _kind == PlotKind::Empty || _kind == PlotKind::DcBars || _axisArea.width() <= 0)
   {
      QWidget::mousePressEvent(pEvent);
      return;
   }
   const QPoint position = pEvent->position().toPoint();
   if (_axisArea.contains(position) == false)
   {
      QWidget::mousePressEvent(pEvent);
      return;
   }
   AxisRange range{_axisMinimum, _axisMaximum};
   _cursorX = UnmapAlong(static_cast<double>(position.x()), range, _axisScale == AxisScale::Logarithmic, _axisArea.left(), _axisArea.width());
   _hasCursor = true;
   update();
   emit CursorMoved(_cursorX);
   pEvent->accept();
}

} // namespace Analyzer
