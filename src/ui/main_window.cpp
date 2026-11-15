/*!
 * \file main_window.cpp
 * \brief Builds the analyzer window, the property table, and the result diagrams.
 */

#include "main_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>
#include <variant>

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/circuit_file.h"
#include "core/example_circuits.h"
#include "core/netlist_builder.h"
#include "core/si_parser.h"
#include "result_plot.h"

namespace Analyzer
{

namespace
{

constexpr qsizetype DocumentHistoryDepth = 100;
constexpr int32_t MaxCurves = 6;
constexpr size_t SpectrumLength = 256;
using SpectrumWave = std::array<double, SpectrumLength>;
constexpr double LogicStep = 1.0;
constexpr double MagnitudeFloor = 1.0e-12;
constexpr double AnalogSwingFloor = 1.0e-4;

struct PaletteRow
{
   const char* group;
   ComponentKind kind;
};

constexpr PaletteRow PaletteRows[] = {
   {"Sources", ComponentKind::VoltageSource},
   {"Sources", ComponentKind::CurrentSource},
   {"Sources", ComponentKind::Ground},
   {"Passive", ComponentKind::Resistor},
   {"Passive", ComponentKind::Capacitor},
   {"Passive", ComponentKind::Inductor},
   {"Passive", ComponentKind::Potentiometer},
   {"Semiconductors", ComponentKind::Diode},
   {"Semiconductors", ComponentKind::Npn},
   {"Semiconductors", ComponentKind::Pnp},
   {"Semiconductors", ComponentKind::Nmos},
   {"Semiconductors", ComponentKind::Pmos},
   {"Semiconductors", ComponentKind::Timer555},
   {"Semiconductors", ComponentKind::Comparator},
   {"Switches and Displays", ComponentKind::SpstSwitch},
   {"Switches and Displays", ComponentKind::SpdtSwitch},
   {"Switches and Displays", ComponentKind::Pushbutton},
   {"Switches and Displays", ComponentKind::DipSwitch},
   {"Switches and Displays", ComponentKind::Led},
   {"Switches and Displays", ComponentKind::SevenSegment},
   {"Switches and Displays", ComponentKind::Relay},
   {"Digital", ComponentKind::Buffer},
   {"Digital", ComponentKind::Inverter},
   {"Digital", ComponentKind::SchmittInverter},
   {"Digital", ComponentKind::AndGate},
   {"Digital", ComponentKind::OrGate},
   {"Digital", ComponentKind::NandGate},
   {"Digital", ComponentKind::NorGate},
   {"Digital", ComponentKind::XorGate},
   {"Digital", ComponentKind::XnorGate},
   {"Digital", ComponentKind::TriStateBuffer},
   {"Digital", ComponentKind::SrLatch},
   {"Digital", ComponentKind::DLatch},
   {"Digital", ComponentKind::DFlipFlop},
   {"Digital", ComponentKind::JkFlipFlop},
   {"Digital", ComponentKind::TFlipFlop},
   {"Digital", ComponentKind::Clock},
   {"Digital", ComponentKind::Multiplexer},
   {"Digital", ComponentKind::Decoder},
   {"Digital", ComponentKind::BcdToSevenSegment},
   {"Digital", ComponentKind::FullAdder},
   {"Digital", ComponentKind::Counter},
   {"Digital", ComponentKind::ShiftRegister},
   {"Digital", ComponentKind::PullUp},
   {"Digital", ComponentKind::PullDown},
   {"Instruments", ComponentKind::Ammeter},
   {"Instruments", ComponentKind::Voltmeter},
   {"Instruments", ComponentKind::LogicProbe},
   {"Instruments", ComponentKind::Adc},
   {"Instruments", ComponentKind::Dac}
};

QString LevelText(LogicLevel level)
{
   switch (level)
   {
      case LogicLevel::Low: return "Low";
      case LogicLevel::High: return "High";
      case LogicLevel::HighZ: return "High-Z";
      case LogicLevel::Conflict: return "Conflict";
      default: break;
   }
   return "Unknown";
}

bool HasPrimaryValue(ComponentKind kind, NumericProperty* pProperty, QString* pCaption)
{
   if (pProperty == nullptr || pCaption == nullptr)
   {
      return false;
   }
   switch (kind)
   {
      case ComponentKind::Resistor:
      case ComponentKind::Potentiometer:
         *pProperty = NumericProperty::ResistanceOhm;
         *pCaption = "Resistance";
         return true;
      case ComponentKind::Capacitor:
         *pProperty = NumericProperty::CapacitanceFarad;
         *pCaption = "Capacitance";
         return true;
      case ComponentKind::Inductor:
         *pProperty = NumericProperty::InductanceHenry;
         *pCaption = "Inductance";
         return true;
      case ComponentKind::VoltageSource:
         *pProperty = NumericProperty::DcVoltageVolt;
         *pCaption = "DC voltage";
         return true;
      case ComponentKind::CurrentSource:
         *pProperty = NumericProperty::DcCurrentAmpere;
         *pCaption = "DC current";
         return true;
      default:
         break;
   }
   return false;
}

struct TraceChoice
{
   uint32_t netIndex;
   double swing;
   bool logicTrace;
   std::string name;
};

using TraceChoiceList = std::vector<TraceChoice>;

void KeepLargest(TraceChoiceList* pChoices, const TraceChoice& choice)
{
   if (pChoices == nullptr || choice.swing <= 0.0)
   {
      return;
   }
   TraceChoiceList::iterator place = pChoices->begin();
   while (place != pChoices->end() && place->swing >= choice.swing)
   {
      ++place;
   }
   pChoices->insert(place, choice);
   if (static_cast<int32_t>(pChoices->size()) > MaxCurves)
   {
      pChoices->pop_back();
   }
}

double LevelHeight(LogicLevel level)
{
   switch (level)
   {
      case LogicLevel::Low: return 0.0;
      case LogicLevel::High: return LogicStep;
      case LogicLevel::HighZ: return LogicStep * 2.0;
      case LogicLevel::Conflict: return LogicStep * 3.0;
      default: break;
   }
   return 0.0;
}

void AppendStep(PlotSeries* pSeries, double timeSec, double value)
{
   if (pSeries == nullptr || std::isfinite(timeSec) == false || std::isfinite(value) == false)
   {
      return;
   }
   if (pSeries->samples.empty() == false && timeSec > pSeries->samples.back().x)
   {
      pSeries->samples.push_back(PlotSample{timeSec, pSeries->samples.back().y});
   }
   pSeries->samples.push_back(PlotSample{timeSec, value});
}

PlotSeriesList TimeSeries(const TransientResult& result)
{
   TraceChoiceList choices;
   for (const NodeValue& node : result.initial.nodes)
   {
      if (node.netIndex == 0)
      {
         continue;
      }
      double minimum = 0.0;
      double maximum = 0.0;
      bool started = false;
      int32_t changes = 0;
      LogicLevel previous = LogicLevel::Low;
      bool haveLevel = false;
      for (const TransientSample& sample : result.samples)
      {
         if (node.netIndex < sample.netVoltage.size())
         {
            const double voltage = sample.netVoltage[node.netIndex];
            if (started == false)
            {
               minimum = voltage;
               maximum = voltage;
               started = true;
            }
            minimum = std::min(minimum, voltage);
            maximum = std::max(maximum, voltage);
         }
         if (node.netIndex < sample.netLevel.size())
         {
            if (haveLevel && sample.netLevel[node.netIndex] != previous)
            {
               changes += 1;
            }
            previous = sample.netLevel[node.netIndex];
            haveLevel = true;
         }
      }
      TraceChoice choice = {};
      choice.netIndex = node.netIndex;
      choice.name = node.name;
      choice.logicTrace = (maximum - minimum) < AnalogSwingFloor && changes > 0;
      if (choice.logicTrace)
      {
         continue;
      }
      choice.swing = maximum - minimum;
      KeepLargest(&choices, choice);
   }
   PlotSeriesList series;
   for (const TraceChoice& choice : choices)
   {
      PlotSeries curve;
      curve.name = choice.name;
      for (const TransientSample& sample : result.samples)
      {
         if (choice.netIndex >= sample.netVoltage.size())
         {
            continue;
         }
         AppendStep(&curve, sample.timeSec, sample.netVoltage[choice.netIndex]);
      }
      if (curve.samples.size() >= 2)
      {
         series.push_back(std::move(curve));
      }
   }
   return series;
}

PlotSeriesList LogicSeries(const TransientResult& result)
{
   constexpr int32_t MaxTimingTraces = 12;
   PlotSeriesList series;
   for (const NodeValue& node : result.initial.nodes)
   {
      if (node.netIndex == 0 || static_cast<int32_t>(series.size()) >= MaxTimingTraces)
      {
         continue;
      }
      int32_t changes = 0;
      LogicLevel previous = LogicLevel::Low;
      bool haveLevel = false;
      double minimum = 0.0;
      double maximum = 0.0;
      bool started = false;
      for (const TransientSample& sample : result.samples)
      {
         if (node.netIndex < sample.netVoltage.size())
         {
            const double voltage = sample.netVoltage[node.netIndex];
            if (started == false)
            {
               minimum = voltage;
               maximum = voltage;
               started = true;
            }
            minimum = std::min(minimum, voltage);
            maximum = std::max(maximum, voltage);
         }
         if (node.netIndex < sample.netLevel.size())
         {
            if (haveLevel && sample.netLevel[node.netIndex] != previous)
            {
               changes += 1;
            }
            previous = sample.netLevel[node.netIndex];
            haveLevel = true;
         }
      }
      if (changes == 0 || (maximum - minimum) >= AnalogSwingFloor)
      {
         continue;
      }
      PlotSeries curve;
      curve.name = node.name;
      for (const TransientSample& sample : result.samples)
      {
         if (node.netIndex >= sample.netLevel.size())
         {
            continue;
         }
         AppendStep(&curve, sample.timeSec, LevelHeight(sample.netLevel[node.netIndex]));
      }
      if (curve.samples.size() >= 2)
      {
         series.push_back(std::move(curve));
      }
   }
   return series;
}

PlotSeriesList BodeSeries(const AcResult& result)
{
   PlotSeriesList series;
   for (const NodeValue& node : result.bias.nodes)
   {
      if (node.netIndex == 0 || static_cast<int32_t>(series.size()) >= MaxCurves)
      {
         continue;
      }
      PlotSeries curve;
      curve.name = node.name;
      for (const AcPoint& point : result.points)
      {
         if (node.netIndex >= point.netVoltageReal.size() || node.netIndex >= point.netVoltageImag.size())
         {
            continue;
         }
         const double real = point.netVoltageReal[node.netIndex];
         const double imag = point.netVoltageImag[node.netIndex];
         double magnitude = std::sqrt((real * real) + (imag * imag));
         if (magnitude < MagnitudeFloor)
         {
            magnitude = MagnitudeFloor;
         }
         const double decibel = 20.0 * std::log10(magnitude);
         const double phase = std::atan2(imag, real) * 180.0 / std::numbers::pi;
         curve.samples.push_back(PlotSample{point.frequencyHz, decibel});
         curve.extra.push_back(PlotSample{point.frequencyHz, phase});
      }
      if (curve.samples.size() >= 2)
      {
         series.push_back(std::move(curve));
      }
   }
   return series;
}

double SampleAt(const TransientSampleList& samples, uint32_t netIndex, double timeSec)
{
   if (samples.empty())
   {
      return 0.0;
   }
   if (timeSec <= samples.front().timeSec)
   {
      if (netIndex < samples.front().netVoltage.size())
      {
         return samples.front().netVoltage[netIndex];
      }
      return 0.0;
   }
   for (size_t index = 1; index < samples.size(); index += 1)
   {
      if (timeSec > samples[index].timeSec)
      {
         continue;
      }
      const double span = samples[index].timeSec - samples[index - 1].timeSec;
      double fraction = 0.0;
      if (span > 0.0)
      {
         fraction = (timeSec - samples[index - 1].timeSec) / span;
      }
      double start = 0.0;
      double end = 0.0;
      if (netIndex < samples[index - 1].netVoltage.size())
      {
         start = samples[index - 1].netVoltage[netIndex];
      }
      if (netIndex < samples[index].netVoltage.size())
      {
         end = samples[index].netVoltage[netIndex];
      }
      return start + (fraction * (end - start));
   }
   if (netIndex < samples.back().netVoltage.size())
   {
      return samples.back().netVoltage[netIndex];
   }
   return 0.0;
}

PlotSeriesList SpectrumSeries(const TransientResult& result)
{
   PlotSeriesList series;
   uint32_t netIndex = 0;
   std::string name;
   double bestSwing = 0.0;
   for (const NodeValue& node : result.initial.nodes)
   {
      if (node.netIndex == 0)
      {
         continue;
      }
      double minimum = 0.0;
      double maximum = 0.0;
      bool started = false;
      for (const TransientSample& sample : result.samples)
      {
         if (node.netIndex >= sample.netVoltage.size())
         {
            continue;
         }
         const double voltage = sample.netVoltage[node.netIndex];
         if (started == false)
         {
            minimum = voltage;
            maximum = voltage;
            started = true;
         }
         minimum = std::min(minimum, voltage);
         maximum = std::max(maximum, voltage);
      }
      if ((maximum - minimum) > bestSwing)
      {
         bestSwing = maximum - minimum;
         netIndex = node.netIndex;
         name = node.name;
      }
   }
   if (bestSwing < AnalogSwingFloor || result.samples.size() < 4)
   {
      return series;
   }
   const double start = result.samples.front().timeSec;
   const double stop = result.samples.back().timeSec;
   if (stop <= start)
   {
      return series;
   }
   const double step = (stop - start) / static_cast<double>(SpectrumLength - 1);
   SpectrumWave wave = {};
   for (size_t index = 0; index < SpectrumLength; index += 1)
   {
      wave[index] = SampleAt(result.samples, netIndex, start + (step * static_cast<double>(index)));
   }
   PlotSeries curve;
   curve.name = name;
   const double scale = 2.0 / static_cast<double>(SpectrumLength);
   for (size_t bin = 1; bin <= SpectrumLength / 2; bin += 1)
   {
      double real = 0.0;
      double imag = 0.0;
      for (size_t index = 0; index < SpectrumLength; index += 1)
      {
         const double angle = 2.0 * std::numbers::pi * static_cast<double>(bin) * static_cast<double>(index) / static_cast<double>(SpectrumLength);
         real += wave[index] * std::cos(angle);
         imag -= wave[index] * std::sin(angle);
      }
      const double frequency = static_cast<double>(bin) / (static_cast<double>(SpectrumLength) * step);
      curve.samples.push_back(PlotSample{frequency, scale * std::sqrt((real * real) + (imag * imag))});
   }
   series.push_back(std::move(curve));
   return series;
}

bool IsSameAxis(GridPoint first, GridPoint second)
{
   return first.positionX == second.positionX || first.positionY == second.positionY;
}

QString CircuitFileFilter(void)
{
   return "Circuit (*.eca.json)";
}

constexpr int PropertyKindRole = Qt::UserRole;
constexpr int PropertyCodeRole = Qt::UserRole + 1;
constexpr int NumericKind = 1;
constexpr int EventKind = 2;

QString PropertyCaption(std::string_view key)
{
   QString text;
   bool wordStart = true;
   for (const char character : key)
   {
      if (character >= 'A' && character <= 'Z' && text.isEmpty() == false)
      {
         text += ' ';
      }
      if (wordStart)
      {
         text += QChar(character).toUpper();
         wordStart = false;
      }
      else
      {
         text += QChar(character);
      }
   }
   return text;
}

std::string_view TrimView(std::string_view text)
{
   while (text.empty() == false && (text.front() == ' ' || text.front() == '\t'))
   {
      text.remove_prefix(1);
   }
   while (text.empty() == false && (text.back() == ' ' || text.back() == '\t'))
   {
      text.remove_suffix(1);
   }
   return text;
}

std::string FormatEvents(const SwitchEventList& events)
{
   std::string text;
   for (const SwitchEvent& event : events)
   {
      if (text.empty() == false)
      {
         text += ", ";
      }
      text += FormatSiNumber(event.timeSec);
      text += event.state == SwitchState::Closed ? " closed" : " open";
   }
   return text;
}

Result ParseEvents(std::string_view text, SwitchEventList* pEvents)
{
   if (pEvents == nullptr)
   {
      return Result::InvalidParameter;
   }
   SwitchEventList parsed;
   std::string_view rest = text;
   while (rest.empty() == false)
   {
      const size_t comma = rest.find(',');
      const std::string_view token = TrimView(comma == std::string_view::npos ? rest : rest.substr(0, comma));
      rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
      if (token.empty())
      {
         continue;
      }
      const size_t gap = token.find(' ');
      if (gap == std::string_view::npos)
      {
         return Result::InvalidParameter;
      }
      double timeSec = 0.0;
      if (IsErr(ParseSiNumber(token.substr(0, gap), &timeSec)))
      {
         return Result::InvalidParameter;
      }
      const std::string_view word = TrimView(token.substr(gap + 1));
      SwitchEvent event = {};
      event.timeSec = timeSec;
      if (word == "closed")
      {
         event.state = SwitchState::Closed;
      }
      else if (word == "open")
      {
         event.state = SwitchState::Open;
      }
      else
      {
         return Result::InvalidParameter;
      }
      parsed.push_back(event);
   }
   *pEvents = std::move(parsed);
   return Result::Ok;
}

SwitchEventList* EventListOf(Component* pComponent)
{
   if (pComponent == nullptr)
   {
      return nullptr;
   }
   if (SpstProperties* pSpst = std::get_if<SpstProperties>(&pComponent->properties))
   {
      return &pSpst->events;
   }
   if (PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&pComponent->properties))
   {
      return &pButton->events;
   }
   return nullptr;
}

const SwitchEventList* EventListOf(const Component& component)
{
   if (const SpstProperties* pSpst = std::get_if<SpstProperties>(&component.properties))
   {
      return &pSpst->events;
   }
   if (const PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&component.properties))
   {
      return &pButton->events;
   }
   return nullptr;
}

GridPoint FirstPinOfNet(const Circuit& circuit, uint32_t netIndex)
{
   Netlist netlist;
   if (IsErr(BuildNetlist(circuit, &netlist)))
   {
      return GridPoint{0, 0};
   }
   for (const NetlistDevice& device : netlist.devices)
   {
      const Component* pComponent = circuit.FindComponent(device.componentId);
      if (pComponent == nullptr)
      {
         continue;
      }
      for (uint8_t pinIndex = 0; pinIndex < device.pinCount; pinIndex += 1)
      {
         if (device.nets[pinIndex] == netIndex)
         {
            return PinPosition(*pComponent, pinIndex);
         }
      }
   }
   return GridPoint{0, 0};
}

void AddNote(SheetNoteList* pNotes, const Circuit& circuit, uint32_t netIndex, const std::string& text)
{
   if (pNotes == nullptr || netIndex == 0 || text.empty())
   {
      return;
   }
   SheetNote note = {};
   note.point = FirstPinOfNet(circuit, netIndex);
   note.text = text;
   pNotes->push_back(note);
}

} // namespace

MainWindow::MainWindow(QWidget* pParent)
   : QMainWindow(pParent)
   , _pSchematic(new SchematicView(this))
   , _pPalette(new QTreeWidget(this))
   , _pNameLabel(new QLabel(this))
   , _pKindLabel(new QLabel(this))
   , _pProperties(new QTableWidget(0, 2, this))
   , _pResults(new QPlainTextEdit(this))
   , _pPlot(new ResultPlot(this))
   , _pUndoAction(nullptr)
   , _pRedoAction(nullptr)
   , _pRunDcAction(nullptr)
   , _pRunAcAction(nullptr)
   , _pRunTransientAction(nullptr)
   , _pCancelAction(nullptr)
   , _worker(this)
   , _placement(Placement::Idle)
   , _armedKind(ComponentKind::Resistor)
   , _updatingProperties(false)
   , _cursorValid(false)
   , _cursorPosition(0.0)
   , _sheetNotes()
   , _history()
   , _savedPaths()
   , _redoPaths()
   , _documentPath()
   , _wirePoints()
{
   setWindowTitle("Electric Circuit Analyzer");
   resize(1180, 760);
   setCentralWidget(_pSchematic);
   _pResults->setReadOnly(true);
   _pProperties->setHorizontalHeaderLabels({"Property", "Value"});
   _pProperties->horizontalHeader()->setStretchLastSection(true);
   _pProperties->verticalHeader()->setVisible(false);
   _pProperties->setSelectionMode(QAbstractItemView::SingleSelection);
   _pPalette->setHeaderHidden(true);
   _pPalette->setRootIsDecorated(true);

   QTreeWidgetItem* pGroup = nullptr;
   QString currentGroup;
   for (const PaletteRow& row : PaletteRows)
   {
      const QString groupName = QString::fromUtf8(row.group);
      if (pGroup == nullptr || currentGroup != groupName)
      {
         pGroup = new QTreeWidgetItem(_pPalette, QStringList(groupName));
         currentGroup = groupName;
      }
      QTreeWidgetItem* pChild = new QTreeWidgetItem(pGroup, QStringList(QString::fromStdString(KindLabel(row.kind))));
      pChild->setData(0, Qt::UserRole, static_cast<int>(row.kind));
   }
   _pPalette->expandAll();

   QDockWidget* pPaletteDock = new QDockWidget("Parts", this);
   pPaletteDock->setWidget(_pPalette);
   addDockWidget(Qt::LeftDockWidgetArea, pPaletteDock);

   QWidget* pPropertyPanel = new QWidget(this);
   QVBoxLayout* pPropertyLayout = new QVBoxLayout(pPropertyPanel);
   pPropertyLayout->addWidget(_pNameLabel);
   pPropertyLayout->addWidget(_pKindLabel);
   pPropertyLayout->addWidget(_pProperties);
   QDockWidget* pPropertyDock = new QDockWidget("Properties", this);
   pPropertyDock->setWidget(pPropertyPanel);
   addDockWidget(Qt::RightDockWidgetArea, pPropertyDock);

   QSplitter* pResultSplit = new QSplitter(Qt::Horizontal, this);
   pResultSplit->addWidget(_pResults);
   pResultSplit->addWidget(_pPlot);
   pResultSplit->setStretchFactor(0, 1);
   pResultSplit->setStretchFactor(1, 1);
   QDockWidget* pResultDock = new QDockWidget("Results", this);
   pResultDock->setWidget(pResultSplit);
   addDockWidget(Qt::BottomDockWidgetArea, pResultDock);

   QMenu* pFileMenu = menuBar()->addMenu("File");
   QAction* pNewAction = pFileMenu->addAction("New");
   QAction* pOpenAction = pFileMenu->addAction("Open...");
   QAction* pSaveAction = pFileMenu->addAction("Save");
   QAction* pSaveAsAction = pFileMenu->addAction("Save As...");
   pFileMenu->addSeparator();
   QMenu* pExampleMenu = pFileMenu->addMenu("Examples");
   for (int8_t code = 0; code < static_cast<int8_t>(ExampleCircuit::Count); code += 1)
   {
      const auto example = static_cast<ExampleCircuit>(code);
      const std::string_view title = ExampleTitle(example);
      const std::string_view fileName = ExampleFileName(example);
      QAction* pExampleAction = pExampleMenu->addAction(QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size())));
      pExampleAction->setData(QString::fromUtf8(fileName.data(), static_cast<qsizetype>(fileName.size())));
      connect(pExampleAction, &QAction::triggered, this, &MainWindow::OnOpenExample);
   }
   pFileMenu->addSeparator();
   QAction* pExitAction = pFileMenu->addAction("Exit");
   QMenu* pEditMenu = menuBar()->addMenu("Edit");
   _pUndoAction = pEditMenu->addAction("Undo");
   _pRedoAction = pEditMenu->addAction("Redo");
   pEditMenu->addSeparator();
   QAction* pWireAction = pEditMenu->addAction("Draw Wire");
   QMenu* pAnalysisMenu = menuBar()->addMenu("Analysis");
   _pRunDcAction = pAnalysisMenu->addAction("Run DC");
   _pRunAcAction = pAnalysisMenu->addAction("Run AC");
   _pRunTransientAction = pAnalysisMenu->addAction("Run Transient");
   _pCancelAction = pAnalysisMenu->addAction("Cancel");
   _pCancelAction->setEnabled(false);
   QMenu* pHelpMenu = menuBar()->addMenu("Help");
   QAction* pAboutAction = pHelpMenu->addAction("About");
   pNewAction->setShortcut(QKeySequence::New);
   pOpenAction->setShortcut(QKeySequence::Open);
   pSaveAction->setShortcut(QKeySequence::Save);
   pSaveAsAction->setShortcut(QKeySequence::SaveAs);
   _pUndoAction->setShortcut(QKeySequence::Undo);
   _pRedoAction->setShortcut(QKeySequence::Redo);

   connect(pNewAction, &QAction::triggered, this, &MainWindow::OnNewCircuit);
   connect(pOpenAction, &QAction::triggered, this, &MainWindow::OnOpenCircuit);
   connect(pSaveAction, &QAction::triggered, this, &MainWindow::OnSaveCircuit);
   connect(pSaveAsAction, &QAction::triggered, this, &MainWindow::OnSaveCircuitAs);
   connect(pExitAction, &QAction::triggered, this, &QWidget::close);
   connect(_pUndoAction, &QAction::triggered, this, &MainWindow::OnUndo);
   connect(_pRedoAction, &QAction::triggered, this, &MainWindow::OnRedo);
   connect(pWireAction, &QAction::triggered, this, &MainWindow::OnDrawWire);
   connect(_pRunDcAction, &QAction::triggered, this, &MainWindow::OnRunDc);
   connect(_pRunAcAction, &QAction::triggered, this, &MainWindow::OnRunAc);
   connect(_pRunTransientAction, &QAction::triggered, this, &MainWindow::OnRunTransient);
   connect(_pCancelAction, &QAction::triggered, this, &MainWindow::OnCancelAnalysis);
   connect(&_worker, &QThread::finished, this, &MainWindow::OnAnalysisFinished);
   connect(pAboutAction, &QAction::triggered, this, &MainWindow::OnAbout);
   connect(_pPalette, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnPaletteActivated);
   connect(_pSchematic, &SchematicView::PlaceRequested, this, &MainWindow::OnPlaceRequested);
   connect(_pSchematic, &SchematicView::PlacementCancelled, this, &MainWindow::OnPlacementCancelled);
   connect(_pSchematic, &SchematicView::DeleteRequested, this, &MainWindow::OnDeleteRequested);
   connect(_pSchematic, &SchematicView::WirePointRequested, this, &MainWindow::OnWirePoint);
   connect(_pSchematic, &SchematicView::WireFinishRequested, this, &MainWindow::OnWireFinish);
   connect(_pSchematic, &SchematicView::ComponentMoved, this, &MainWindow::OnComponentMoved);
   connect(_pSchematic, &SchematicView::RotateRequested, this, &MainWindow::OnRotateRequested);
   connect(_pSchematic->scene(), &QGraphicsScene::selectionChanged, this, &MainWindow::OnSelectionChanged);
   connect(_pProperties, &QTableWidget::itemChanged, this, &MainWindow::OnPropertyEdited);
   connect(_pPlot, &ResultPlot::CursorMoved, this, &MainWindow::OnPlotCursor);

   statusBar()->showMessage("Drag a part to move it. R rotates it. Edit, Draw Wire adds an orthogonal wire.");
   UpdateHistoryActions();
   RefreshSheet();
   _pSchematic->setFocus();
}

void MainWindow::RefreshSheet(void)
{
   _pSchematic->SetWirePreview(_wirePoints);
   _pSchematic->SetNotes(_sheetNotes);
   _pSchematic->ShowCircuit(_circuit);
   const QString title = QString::fromStdString(_circuit.Name());
   setWindowTitle(title + " - Electric Circuit Analyzer");
}

void MainWindow::ShowSelection(void)
{
   const uint64_t selectedId = _pSchematic->SelectedComponentId();
   const Component* pComponent = _circuit.FindComponent(selectedId);
   _updatingProperties = true;
   _pProperties->blockSignals(true);
   _pProperties->setRowCount(0);
   if (pComponent == nullptr)
   {
      _pNameLabel->setText("None");
      _pKindLabel->setText("");
      _pProperties->blockSignals(false);
      _updatingProperties = false;
      return;
   }
   _pNameLabel->setText(QString::fromStdString(pComponent->name));
   _pKindLabel->setText(QString::fromStdString(KindLabel(pComponent->kind)));
   for (int16_t code = 0; code <= static_cast<int16_t>(NumericProperty::InitialValue); code += 1)
   {
      const auto property = static_cast<NumericProperty>(code);
      double value = 0.0;
      if (IsErr(ReadNumeric(*pComponent, property, &value)))
      {
         continue;
      }
      const int row = _pProperties->rowCount();
      _pProperties->insertRow(row);
      QTableWidgetItem* pName = new QTableWidgetItem(PropertyCaption(NumericPropertyKey(property)));
      pName->setFlags(Qt::ItemIsEnabled);
      QTableWidgetItem* pValue = new QTableWidgetItem(QString::fromStdString(FormatSiNumber(value)));
      pValue->setData(PropertyKindRole, NumericKind);
      pValue->setData(PropertyCodeRole, static_cast<int>(code));
      _pProperties->setItem(row, 0, pName);
      _pProperties->setItem(row, 1, pValue);
   }
   for (int16_t code = 0; code <= static_cast<int16_t>(EnumProperty::MuxWidth); code += 1)
   {
      const auto property = static_cast<EnumProperty>(code);
      int32_t selected = 0;
      if (IsErr(ReadEnumCode(*pComponent, property, &selected)))
      {
         continue;
      }
      const int row = _pProperties->rowCount();
      _pProperties->insertRow(row);
      QTableWidgetItem* pName = new QTableWidgetItem(PropertyCaption(EnumPropertyKey(property)));
      pName->setFlags(Qt::ItemIsEnabled);
      QTableWidgetItem* pValue = new QTableWidgetItem();
      pValue->setFlags(Qt::ItemIsEnabled);
      _pProperties->setItem(row, 0, pName);
      _pProperties->setItem(row, 1, pValue);
      QComboBox* pChoices = new QComboBox(_pProperties);
      const int32_t choiceCount = EnumCodeCount(property);
      for (int32_t choice = 0; choice < choiceCount; choice += 1)
      {
         pChoices->addItem(QString::fromStdString(EnumCodeLabel(property, choice)));
      }
      if (selected >= 0 && selected < choiceCount)
      {
         pChoices->setCurrentIndex(selected);
      }
      pChoices->setProperty("enumProperty", static_cast<int>(code));
      _pProperties->setCellWidget(row, 1, pChoices);
      connect(pChoices, &QComboBox::currentIndexChanged, this, &MainWindow::OnEnumEdited);
   }
   if (const SwitchEventList* pEvents = EventListOf(*pComponent))
   {
      const int row = _pProperties->rowCount();
      _pProperties->insertRow(row);
      QTableWidgetItem* pName = new QTableWidgetItem("Events");
      pName->setFlags(Qt::ItemIsEnabled);
      QTableWidgetItem* pValue = new QTableWidgetItem(QString::fromStdString(FormatEvents(*pEvents)));
      pValue->setData(PropertyKindRole, EventKind);
      _pProperties->setItem(row, 0, pName);
      _pProperties->setItem(row, 1, pValue);
   }
   _pProperties->blockSignals(false);
   _updatingProperties = false;
}

void MainWindow::OnComponentMoved(qulonglong componentId, int positionX, int positionY)
{
   const uint64_t id = static_cast<uint64_t>(componentId);
   const Component* pComponent = _circuit.FindComponent(id);
   if (pComponent == nullptr)
   {
      return;
   }
   if (pComponent->position.positionX == positionX && pComponent->position.positionY == positionY)
   {
      return;
   }
   const Circuit before = _circuit;
   const Result moved = _circuit.MoveComponent(id, GridPoint{positionX, positionY});
   if (IsErr(moved))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(moved)));
      RefreshSheet();
      return;
   }
   ClearResults();
   CommitEdit(before, _documentPath);
   RefreshSheet();
   statusBar()->showMessage("Moved " + QString::fromStdString(pComponent->name) + ".");
}

void MainWindow::OnRotateRequested(void)
{
   const uint64_t id = _pSchematic->SelectedComponentId();
   if (id == 0)
   {
      return;
   }
   const Circuit before = _circuit;
   const Result turned = _circuit.RotateComponent(id);
   if (IsErr(turned))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(turned)));
      return;
   }
   ClearResults();
   CommitEdit(before, _documentPath);
   RefreshSheet();
   statusBar()->showMessage("Rotated the selected part.");
}

void MainWindow::OnOpenExample(void)
{
   const QAction* pAction = qobject_cast<const QAction*>(sender());
   if (pAction == nullptr)
   {
      return;
   }
   const QString fileName = pAction->data().toString();
   Circuit loaded;
   const QString path = QCoreApplication::applicationDirPath() + "/examples/" + fileName;
   bool loadedFile = false;
   if (QFile::exists(path))
   {
      QFile file(path);
      if (file.open(QIODevice::ReadOnly))
      {
         const QByteArray bytes = file.readAll();
         const std::string text(bytes.constData(), static_cast<size_t>(bytes.size()));
         if (IsOk(ReadCircuitText(text, &loaded)))
         {
            loadedFile = true;
         }
      }
   }
   if (loadedFile == false)
   {
      ExampleCircuit example = ExampleCircuit::VoltageDivider;
      bool known = false;
      for (int8_t code = 0; code < static_cast<int8_t>(ExampleCircuit::Count); code += 1)
      {
         const auto candidate = static_cast<ExampleCircuit>(code);
         if (fileName == QString::fromUtf8(ExampleFileName(candidate).data(), static_cast<qsizetype>(ExampleFileName(candidate).size())))
         {
            example = candidate;
            known = true;
         }
      }
      if (known == false || IsErr(BuildExample(example, &loaded)))
      {
         QMessageBox::warning(this, "Example", "The example could not be loaded.");
         return;
      }
   }
   const Circuit before = _circuit;
   const QString pathBefore = _documentPath;
   _circuit = std::move(loaded);
   _documentPath.clear();
   _wirePoints.clear();
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   _pResults->clear();
   ClearResults();
   CommitEdit(before, pathBefore);
   RefreshSheet();
   ShowSelection();
   _pSchematic->fitInView(_pSchematic->scene()->itemsBoundingRect().adjusted(-80.0, -80.0, 80.0, 80.0), Qt::KeepAspectRatio);
   statusBar()->showMessage("Loaded " + QString::fromStdString(_circuit.Name()) + ".");
}

void MainWindow::OnPlaceRequested(int positionX, int positionY)
{
   if (_placement != Placement::Armed)
   {
      return;
   }
   const Circuit before = _circuit;
   Component component = CreateComponent(_armedKind, GridPoint{positionX, positionY});
   const Result added = _circuit.AddComponent(component, nullptr);
   if (IsErr(added))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(added)));
      return;
   }
   ClearResults();
   CommitEdit(before, _documentPath);
   RefreshSheet();
   statusBar()->showMessage("Placed " + QString::fromStdString(KindLabel(_armedKind)) + ". Press Escape to stop placing.");
}

void MainWindow::OnPlacementCancelled(void)
{
   const bool drawing = _placement == Placement::Wire || _wirePoints.empty() == false;
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   if (_wirePoints.empty() == false)
   {
      _wirePoints.clear();
      RefreshSheet();
   }
   if (drawing)
   {
      statusBar()->showMessage("Wire cancelled.");
      return;
   }
   statusBar()->showMessage("Placement cancelled.");
}

void MainWindow::OnDeleteRequested(void)
{
   const uint64_t selectedId = _pSchematic->SelectedComponentId();
   if (selectedId == 0)
   {
      return;
   }
   const Circuit before = _circuit;
   _circuit.RemoveWiresTouching(selectedId);
   _circuit.RemoveComponent(selectedId);
   ClearResults();
   CommitEdit(before, _documentPath);
   RefreshSheet();
   ShowSelection();
}

void MainWindow::OnPaletteActivated(QTreeWidgetItem* pItem, int column)
{
   (void)column;
   if (pItem == nullptr || pItem->data(0, Qt::UserRole).isValid() == false)
   {
      return;
   }
   _armedKind = static_cast<ComponentKind>(pItem->data(0, Qt::UserRole).toInt());
   _placement = Placement::Armed;
   _wirePoints.clear();
   _pSchematic->SetPlacement(Placement::Armed);
   RefreshSheet();
   statusBar()->showMessage("Click the sheet to place " + QString::fromStdString(KindLabel(_armedKind)) + ".");
}

void MainWindow::OnSelectionChanged(void)
{
   ShowSelection();
}

void MainWindow::OnPropertyEdited(QTableWidgetItem* pItem)
{
   if (_updatingProperties || pItem == nullptr || pItem->column() != 1)
   {
      return;
   }
   Component* pComponent = _circuit.FindComponent(_pSchematic->SelectedComponentId());
   if (pComponent == nullptr)
   {
      return;
   }
   const int kind = pItem->data(PropertyKindRole).toInt();
   if (kind == EventKind)
   {
      SwitchEventList* pEvents = EventListOf(pComponent);
      if (pEvents == nullptr)
      {
         return;
      }
      const std::string edited = pItem->text().toStdString();
      if (edited == FormatEvents(*pEvents))
      {
         return;
      }
      SwitchEventList parsed;
      if (IsErr(ParseEvents(edited, &parsed)))
      {
         statusBar()->showMessage("Enter events as 1m closed, 2m open.");
         ShowSelection();
         return;
      }
      const Circuit before = _circuit;
      *pEvents = std::move(parsed);
      ClearResults();
      CommitEdit(before, _documentPath);
      statusBar()->showMessage("Updated " + QString::fromStdString(pComponent->name) + ".");
      return;
   }
   if (kind != NumericKind)
   {
      return;
   }
   const auto property = static_cast<NumericProperty>(pItem->data(PropertyCodeRole).toInt());
   double value = 0.0;
   if (IsErr(ParseSiNumber(pItem->text().toStdString(), &value)))
   {
      statusBar()->showMessage("Enter a number such as 4.7k, 10u, or 1Meg.");
      ShowSelection();
      return;
   }
   double current = 0.0;
   if (IsOk(ReadNumeric(*pComponent, property, &current)) && current == value)
   {
      return;
   }
   const Circuit before = _circuit;
   const Result written = WriteNumeric(pComponent, property, value);
   if (IsErr(written))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(written)));
      ShowSelection();
      return;
   }
   ClearResults();
   CommitEdit(before, _documentPath);
   statusBar()->showMessage("Updated " + QString::fromStdString(pComponent->name) + ".");
}

void MainWindow::OnEnumEdited(int index)
{
   if (_updatingProperties || index < 0)
   {
      return;
   }
   QComboBox* pChoices = qobject_cast<QComboBox*>(sender());
   if (pChoices == nullptr)
   {
      return;
   }
   Component* pComponent = _circuit.FindComponent(_pSchematic->SelectedComponentId());
   if (pComponent == nullptr)
   {
      return;
   }
   const auto property = static_cast<EnumProperty>(pChoices->property("enumProperty").toInt());
   int32_t current = 0;
   if (IsOk(ReadEnumCode(*pComponent, property, &current)) && current == index)
   {
      return;
   }
   const Circuit before = _circuit;
   const Result written = WriteEnumCode(pComponent, property, index);
   if (IsErr(written))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(written)));
      ShowSelection();
      return;
   }
   ClearResults();
   CommitEdit(before, _documentPath);
   statusBar()->showMessage("Updated " + QString::fromStdString(pComponent->name) + ".");
}

void MainWindow::OnNewCircuit(void)
{
   const Circuit before = _circuit;
   const QString pathBefore = _documentPath;
   _circuit.Clear();
   _circuit.SetName("Untitled");
   _documentPath.clear();
   _wirePoints.clear();
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   _pResults->clear();
   ClearResults();
   CommitEdit(before, pathBefore);
   RefreshSheet();
   ShowSelection();
   statusBar()->showMessage("New circuit.");
}

void MainWindow::OnOpenCircuit(void)
{
   const QString path = QFileDialog::getOpenFileName(this, "Open Circuit", _documentPath, CircuitFileFilter());
   if (path.isEmpty())
   {
      return;
   }
   QFile file(path);
   if (file.open(QIODevice::ReadOnly) == false)
   {
      QMessageBox::warning(this, "Open Circuit", "The file could not be read.");
      return;
   }
   const QByteArray bytes = file.readAll();
   const std::string text(bytes.constData(), static_cast<size_t>(bytes.size()));
   Circuit loaded;
   const Result read = ReadCircuitText(text, &loaded);
   if (IsErr(read))
   {
      QMessageBox::warning(this, "Open Circuit", QString::fromStdString(Describe(read)));
      return;
   }
   const Circuit before = _circuit;
   const QString pathBefore = _documentPath;
   _circuit = std::move(loaded);
   _documentPath = path;
   _wirePoints.clear();
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   _pResults->clear();
   ClearResults();
   CommitEdit(before, pathBefore);
   RefreshSheet();
   ShowSelection();
   statusBar()->showMessage("Opened " + path + ".");
}

void MainWindow::OnSaveCircuit(void)
{
   if (_documentPath.isEmpty())
   {
      OnSaveCircuitAs();
      return;
   }
   SaveCircuitFile(_documentPath);
}

void MainWindow::OnSaveCircuitAs(void)
{
   QString path = QFileDialog::getSaveFileName(this, "Save Circuit", _documentPath, CircuitFileFilter());
   if (path.isEmpty())
   {
      return;
   }
   if (path.endsWith(".eca.json", Qt::CaseInsensitive) == false)
   {
      path += ".eca.json";
   }
   SaveCircuitFile(path);
}

void MainWindow::OnUndo(void)
{
   if (_savedPaths.isEmpty() || IsErr(_history.Undo(&_circuit)))
   {
      return;
   }
   _redoPaths.append(_documentPath);
   _documentPath = _savedPaths.back();
   _savedPaths.removeLast();
   _wirePoints.clear();
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   _pResults->clear();
   ClearResults();
   UpdateHistoryActions();
   RefreshSheet();
   ShowSelection();
   statusBar()->showMessage("Undid the last edit.");
}

void MainWindow::OnRedo(void)
{
   if (_redoPaths.isEmpty() || IsErr(_history.Redo(&_circuit)))
   {
      return;
   }
   _savedPaths.append(_documentPath);
   _documentPath = _redoPaths.back();
   _redoPaths.removeLast();
   _wirePoints.clear();
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   _pResults->clear();
   ClearResults();
   UpdateHistoryActions();
   RefreshSheet();
   ShowSelection();
   statusBar()->showMessage("Redid the edit.");
}

void MainWindow::OnDrawWire(void)
{
   _placement = Placement::Wire;
   _wirePoints.clear();
   _pSchematic->SetPlacement(Placement::Wire);
   RefreshSheet();
   _pSchematic->setFocus();
   statusBar()->showMessage("Click grid points on one axis at a time. Enter keeps the wire. Escape cancels it.");
}

void MainWindow::OnWirePoint(int positionX, int positionY)
{
   if (_placement != Placement::Wire)
   {
      return;
   }
   const GridPoint point{positionX, positionY};
   if (_wirePoints.empty() == false)
   {
      if (point == _wirePoints.back())
      {
         return;
      }
      if (IsSameAxis(_wirePoints.back(), point) == false)
      {
         statusBar()->showMessage("Each wire segment stays on one grid axis.");
         return;
      }
   }
   _wirePoints.push_back(point);
   RefreshSheet();
   statusBar()->showMessage("Wire point added. Enter keeps the wire. Escape cancels it.");
}

void MainWindow::OnWireFinish(void)
{
   if (_placement != Placement::Wire)
   {
      return;
   }
   if (_wirePoints.size() < 2)
   {
      statusBar()->showMessage("A wire needs two points. Press Escape to cancel.");
      return;
   }
   const Circuit before = _circuit;
   Wire wire = {};
   wire.points = _wirePoints;
   const Result added = _circuit.AddWire(wire, nullptr);
   if (IsErr(added))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(added)));
      return;
   }
   _wirePoints.clear();
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   ClearResults();
   CommitEdit(before, _documentPath);
   RefreshSheet();
   statusBar()->showMessage("Wire added.");
}

void MainWindow::CommitEdit(const Circuit& before, const QString& pathBefore)
{
   _history.Push(before);
   _savedPaths.append(pathBefore);
   if (_savedPaths.size() > DocumentHistoryDepth)
   {
      _savedPaths.removeFirst();
   }
   _redoPaths.clear();
   UpdateHistoryActions();
}

void MainWindow::UpdateHistoryActions(void)
{
   _pUndoAction->setEnabled(_history.CanUndo());
   _pRedoAction->setEnabled(_history.CanRedo());
}

void MainWindow::SaveCircuitFile(const QString& path)
{
   std::string text;
   const Result written = WriteCircuitText(_circuit, &text);
   if (IsErr(written))
   {
      QMessageBox::warning(this, "Save Circuit", QString::fromStdString(Describe(written)));
      return;
   }
   QFile file(path);
   if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
   {
      QMessageBox::warning(this, "Save Circuit", "The file could not be written.");
      return;
   }
   const qint64 stored = file.write(text.data(), static_cast<qint64>(text.size()));
   if (stored != static_cast<qint64>(text.size()))
   {
      QMessageBox::warning(this, "Save Circuit", "The file could not be written.");
      return;
   }
   _documentPath = path;
   statusBar()->showMessage("Saved " + path + ".");
}

void MainWindow::OnRunDc(void)
{
   StartAnalysis(AnalysisKind::DcOperatingPoint);
}

void MainWindow::OnRunAc(void)
{
   StartAnalysis(AnalysisKind::AcSweep);
}

void MainWindow::OnRunTransient(void)
{
   StartAnalysis(AnalysisKind::Transient);
}

void MainWindow::OnCancelAnalysis(void)
{
   _worker.RequestCancel();
   statusBar()->showMessage("Cancelling the analysis.");
}

void MainWindow::StartAnalysis(AnalysisKind kind)
{
   const Result started = _worker.Start(_circuit, kind);
   if (IsErr(started))
   {
      statusBar()->showMessage("An analysis is already running.");
      return;
   }
   SetAnalysisBusy(AnalysisBusy::Yes);
   _pResults->setPlainText("Running...");
   statusBar()->showMessage("Running...");
}

void MainWindow::SetAnalysisBusy(AnalysisBusy busy)
{
   const bool running = busy == AnalysisBusy::Yes;
   _pRunDcAction->setEnabled(running == false);
   _pRunAcAction->setEnabled(running == false);
   _pRunTransientAction->setEnabled(running == false);
   _pCancelAction->setEnabled(running);
}

void MainWindow::ClearResults(void)
{
   _pPlot->Clear();
   _sheetNotes.clear();
   _cursorValid = false;
   _pSchematic->SetNotes(_sheetNotes);
   _pSchematic->RefreshNotes();
}

void MainWindow::OnPlotCursor(double position)
{
   _cursorPosition = position;
   _cursorValid = true;
   statusBar()->showMessage("Cursor " + QString::fromStdString(FormatSiNumber(position)));
   ApplySheetNotes();
}

void MainWindow::ApplySheetNotes(void)
{
   _sheetNotes.clear();
   if (_worker.Kind() == AnalysisKind::AcSweep)
   {
      const AcResult& result = _worker.Ac();
      if (IsOk(result.code) && result.points.empty() == false)
      {
         const AcPoint* pPoint = &result.points.front();
         if (_cursorValid)
         {
            double bestDistance = std::abs(pPoint->frequencyHz - _cursorPosition);
            for (const AcPoint& point : result.points)
            {
               const double distance = std::abs(point.frequencyHz - _cursorPosition);
               if (distance < bestDistance)
               {
                  bestDistance = distance;
                  pPoint = &point;
               }
            }
         }
         for (const NodeValue& node : result.bias.nodes)
         {
            if (node.netIndex == 0 || node.netIndex >= pPoint->netVoltageReal.size() || node.netIndex >= pPoint->netVoltageImag.size())
            {
               continue;
            }
            const double realPart = pPoint->netVoltageReal[node.netIndex];
            const double imagPart = pPoint->netVoltageImag[node.netIndex];
            const double magnitude = std::sqrt((realPart * realPart) + (imagPart * imagPart));
            const double phase = std::atan2(imagPart, realPart) * 180.0 / std::numbers::pi;
            AddNote(&_sheetNotes, _circuit, node.netIndex, FormatSiNumber(magnitude) + " V  " + FormatSiNumber(phase) + " deg");
         }
      }
   }
   else if (_worker.Kind() == AnalysisKind::Transient)
   {
      const TransientResult& result = _worker.Transient();
      if (IsOk(result.code) && result.samples.empty() == false)
      {
         const TransientSample* pSample = &result.samples.front();
         if (_cursorValid)
         {
            double bestDistance = std::abs(pSample->timeSec - _cursorPosition);
            for (const TransientSample& sample : result.samples)
            {
               const double distance = std::abs(sample.timeSec - _cursorPosition);
               if (distance < bestDistance)
               {
                  bestDistance = distance;
                  pSample = &sample;
               }
            }
         }
         for (const NodeValue& node : result.initial.nodes)
         {
            if (node.netIndex == 0 || node.netIndex >= pSample->netVoltage.size())
            {
               continue;
            }
            std::string text = FormatSiNumber(pSample->netVoltage[node.netIndex]) + " V";
            if (node.netIndex < pSample->netLevel.size() && pSample->netLevel[node.netIndex] != LogicLevel::HighZ)
            {
               text += " ";
               text += LevelText(pSample->netLevel[node.netIndex]).toStdString();
            }
            AddNote(&_sheetNotes, _circuit, node.netIndex, text);
         }
      }
   }
   else
   {
      const DcResult& result = _worker.Dc();
      if (IsOk(result.code))
      {
         for (const NodeValue& node : result.nodes)
         {
            if (node.netIndex == 0)
            {
               continue;
            }
            std::string text = FormatSiNumber(node.value) + " V";
            if (node.level != LogicLevel::HighZ)
            {
               text += " ";
               text += LevelText(node.level).toStdString();
            }
            AddNote(&_sheetNotes, _circuit, node.netIndex, text);
         }
      }
   }
   _pSchematic->SetNotes(_sheetNotes);
   _pSchematic->RefreshNotes();
}

void MainWindow::OnAnalysisFinished(void)
{
   SetAnalysisBusy(AnalysisBusy::No);
   if (_worker.Kind() == AnalysisKind::AcSweep)
   {
      const AcResult& result = _worker.Ac();
      QString report = QString::fromStdString(result.message) + "\n";
      report += QString::number(result.points.size()) + " frequency points\n";
      _pResults->setPlainText(report);
      _pPlot->ShowSeries(PlotKind::Bode, "Bode", BodeSeries(result));
      if (result.points.empty() == false)
      {
         _cursorPosition = result.points.front().frequencyHz;
         _cursorValid = true;
         _pPlot->SetCursor(_cursorPosition);
      }
      ApplySheetNotes();
      statusBar()->showMessage(QString::fromStdString(result.message));
      return;
   }
   if (_worker.Kind() == AnalysisKind::Transient)
   {
      const TransientResult& result = _worker.Transient();
      QString report = QString::fromStdString(result.message) + "\n";
      for (const std::string& warning : result.warnings)
      {
         report += "Warning: " + QString::fromStdString(warning) + "\n";
      }
      report += QString::number(result.samples.size()) + " samples\n";
      _pResults->setPlainText(report);
      const PlotSeriesList logic = LogicSeries(result);
      if (logic.empty())
      {
         _pPlot->ShowTime(TimeSeries(result), SpectrumSeries(result));
      }
      else
      {
         _pPlot->ShowTiming(logic, TimeSeries(result));
      }
      if (result.samples.empty() == false)
      {
         _cursorPosition = result.samples.front().timeSec;
         _cursorValid = true;
         _pPlot->SetCursor(_cursorPosition);
      }
      ApplySheetNotes();
      statusBar()->showMessage(QString::fromStdString(result.message));
      return;
   }
   const DcResult& result = _worker.Dc();
   QString report = QString::fromStdString(result.message) + "\n";
   for (const std::string& warning : result.warnings)
   {
      report += "Warning: " + QString::fromStdString(warning) + "\n";
   }
   report += "\nNodes\n";
   for (const NodeValue& node : result.nodes)
   {
      report += QString::fromStdString(node.name) + "    " + QString::fromStdString(FormatSiNumber(node.value)) + " V";
      if (node.level != LogicLevel::HighZ)
      {
         report += "    " + LevelText(node.level);
      }
      report += "\n";
   }
   report += "\nBranches\n";
   for (const BranchValue& branch : result.branches)
   {
      report += QString::fromStdString(branch.name);
      report += "    " + QString::fromStdString(FormatSiNumber(branch.voltageVolt)) + " V";
      report += "    " + QString::fromStdString(FormatSiNumber(branch.currentAmpere)) + " A";
      if (branch.level != LogicLevel::HighZ)
      {
         report += "    " + LevelText(branch.level);
      }
      report += "\n";
   }
   _pResults->setPlainText(report);
   _pPlot->ShowDc(result.nodes);
   ApplySheetNotes();
   statusBar()->showMessage(QString::fromStdString(result.message));
}

void MainWindow::OnAbout(void)
{
   QMessageBox::about(this, "Electric Circuit Analyzer",
      "Drag a part to move it, and press R to rotate it. Wires on its pins follow.\n"
      "The property table edits every numeric and enumerated field. Switch events use lines such as 1m closed, 2m open.\n"
      "Analysis runs DC, AC, or transient. Click a diagram to move the cursor. The sheet shows the value at that cursor.\n"
      "A transient with logic levels draws a timing diagram. File, Examples opens the circuits in the examples folder.");
}

void MainWindow::keyPressEvent(QKeyEvent* pEvent)
{
   if (pEvent->key() == Qt::Key_Escape)
   {
      OnPlacementCancelled();
      pEvent->accept();
      return;
   }
   if (pEvent->key() == Qt::Key_Delete)
   {
      OnDeleteRequested();
      pEvent->accept();
      return;
   }
   QMainWindow::keyPressEvent(pEvent);
}

} // namespace Analyzer
