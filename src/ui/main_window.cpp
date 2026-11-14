/*!
 * \file main_window.cpp
 * \brief Builds the analyzer window and runs a DC operating point.
 */

#include "main_window.h"

#include <QDockWidget>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTreeWidget>

#include "core/si_parser.h"
#include "solver/simulator.h"

namespace Analyzer
{

namespace
{

constexpr double DividerSupplyVolt = 10.0;
constexpr double DividerResistanceOhm = 1000.0;

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

void AddPart(Circuit* pCircuit, ComponentKind kind, int32_t positionX, int32_t positionY, int32_t rotationDegrees, uint64_t* pId)
{
   Component component = CreateComponent(kind, GridPoint{positionX, positionY});
   component.rotationDegrees = rotationDegrees;
   pCircuit->AddComponent(component, pId);
}

void SetNumber(Circuit* pCircuit, uint64_t id, NumericProperty property, double value)
{
   Component* pComponent = pCircuit->FindComponent(id);
   if (pComponent == nullptr)
   {
      return;
   }
   WriteNumeric(pComponent, property, value);
}

} // namespace

MainWindow::MainWindow(QWidget* pParent)
   : QMainWindow(pParent)
   , _pSchematic(new SchematicView(this))
   , _pPalette(new QTreeWidget(this))
   , _pNameLabel(new QLabel(this))
   , _pKindLabel(new QLabel(this))
   , _pValueCaption(new QLabel("Value", this))
   , _pValueEdit(new QLineEdit(this))
   , _pResults(new QPlainTextEdit(this))
   , _placement(Placement::Idle)
   , _armedKind(ComponentKind::Resistor)
   , _editedProperty(NumericProperty::ResistanceOhm)
   , _updatingProperties(false)
{
   setWindowTitle("Electric Circuit Analyzer");
   resize(1180, 760);
   setCentralWidget(_pSchematic);
   _pResults->setReadOnly(true);
   _pValueEdit->setEnabled(false);
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
   QFormLayout* pForm = new QFormLayout(pPropertyPanel);
   pForm->addRow("Name", _pNameLabel);
   pForm->addRow("Kind", _pKindLabel);
   pForm->addRow(_pValueCaption, _pValueEdit);
   QDockWidget* pPropertyDock = new QDockWidget("Properties", this);
   pPropertyDock->setWidget(pPropertyPanel);
   addDockWidget(Qt::RightDockWidgetArea, pPropertyDock);

   QDockWidget* pResultDock = new QDockWidget("Results", this);
   pResultDock->setWidget(_pResults);
   addDockWidget(Qt::BottomDockWidgetArea, pResultDock);

   QMenu* pFileMenu = menuBar()->addMenu("File");
   QAction* pNewAction = pFileMenu->addAction("New");
   QAction* pExampleAction = pFileMenu->addAction("Example: Voltage Divider");
   pFileMenu->addSeparator();
   QAction* pExitAction = pFileMenu->addAction("Exit");
   QMenu* pAnalysisMenu = menuBar()->addMenu("Analysis");
   QAction* pRunAction = pAnalysisMenu->addAction("Run DC");
   QMenu* pHelpMenu = menuBar()->addMenu("Help");
   QAction* pAboutAction = pHelpMenu->addAction("About");

   connect(pNewAction, &QAction::triggered, this, &MainWindow::OnNewCircuit);
   connect(pExampleAction, &QAction::triggered, this, &MainWindow::LoadVoltageDivider);
   connect(pExitAction, &QAction::triggered, this, &QWidget::close);
   connect(pRunAction, &QAction::triggered, this, &MainWindow::OnRunDc);
   connect(pAboutAction, &QAction::triggered, this, &MainWindow::OnAbout);
   connect(_pPalette, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnPaletteActivated);
   connect(_pSchematic, &SchematicView::PlaceRequested, this, &MainWindow::OnPlaceRequested);
   connect(_pSchematic, &SchematicView::PlacementCancelled, this, &MainWindow::OnPlacementCancelled);
   connect(_pSchematic, &SchematicView::DeleteRequested, this, &MainWindow::OnDeleteRequested);
   connect(_pSchematic->scene(), &QGraphicsScene::selectionChanged, this, &MainWindow::OnSelectionChanged);
   connect(_pValueEdit, &QLineEdit::editingFinished, this, &MainWindow::OnValueEdited);

   statusBar()->showMessage("Double-click a part, then click the sheet. Analysis runs the DC operating point.");
   RefreshSheet();
   _pSchematic->setFocus();
}

void MainWindow::RefreshSheet(void)
{
   _pSchematic->ShowCircuit(_circuit);
   const QString title = QString::fromStdString(_circuit.Name());
   setWindowTitle(title + " - Electric Circuit Analyzer");
}

void MainWindow::ShowSelection(void)
{
   const uint64_t selectedId = _pSchematic->SelectedComponentId();
   const Component* pComponent = _circuit.FindComponent(selectedId);
   _updatingProperties = true;
   if (pComponent == nullptr)
   {
      _pNameLabel->setText("None");
      _pKindLabel->setText("");
      _pValueCaption->setText("Value");
      _pValueEdit->clear();
      _pValueEdit->setEnabled(false);
      _updatingProperties = false;
      return;
   }
   _pNameLabel->setText(QString::fromStdString(pComponent->name));
   _pKindLabel->setText(QString::fromStdString(KindLabel(pComponent->kind)));
   NumericProperty property = NumericProperty::ResistanceOhm;
   QString caption;
   if (HasPrimaryValue(pComponent->kind, &property, &caption))
   {
      double value = 0.0;
      _editedProperty = property;
      _pValueCaption->setText(caption);
      if (IsOk(ReadNumeric(*pComponent, property, &value)))
      {
         _pValueEdit->setText(QString::fromStdString(FormatSiNumber(value)));
      }
      _pValueEdit->setEnabled(true);
   }
   else
   {
      _pValueCaption->setText("Value");
      _pValueEdit->clear();
      _pValueEdit->setEnabled(false);
   }
   _updatingProperties = false;
}

void MainWindow::LoadVoltageDivider(void)
{
   _circuit.Clear();
   _circuit.SetName("Voltage divider");
   uint64_t sourceId = 0;
   uint64_t topId = 0;
   uint64_t bottomId = 0;
   AddPart(&_circuit, ComponentKind::Ground, 0, 0, 0, nullptr);
   AddPart(&_circuit, ComponentKind::VoltageSource, 0, 0, 90, &sourceId);
   AddPart(&_circuit, ComponentKind::Resistor, 0, TwoTerminalSpan, 0, &topId);
   AddPart(&_circuit, ComponentKind::Resistor, TwoTerminalSpan, TwoTerminalSpan, 270, &bottomId);
   SetNumber(&_circuit, sourceId, NumericProperty::DcVoltageVolt, DividerSupplyVolt);
   SetNumber(&_circuit, topId, NumericProperty::ResistanceOhm, DividerResistanceOhm);
   SetNumber(&_circuit, bottomId, NumericProperty::ResistanceOhm, DividerResistanceOhm);
   Wire wire = {};
   wire.points.push_back(GridPoint{TwoTerminalSpan, 0});
   wire.points.push_back(GridPoint{0, 0});
   const Result wired = _circuit.AddWire(wire, nullptr);
   RefreshSheet();
   if (IsErr(wired))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(wired)));
      return;
   }
   _pSchematic->fitInView(_pSchematic->scene()->itemsBoundingRect().adjusted(-80.0, -80.0, 80.0, 80.0), Qt::KeepAspectRatio);
   statusBar()->showMessage("Voltage divider loaded. Choose Analysis, Run DC.");
}

void MainWindow::OnPlaceRequested(int positionX, int positionY)
{
   if (_placement != Placement::Armed)
   {
      return;
   }
   Component component = CreateComponent(_armedKind, GridPoint{positionX, positionY});
   uint64_t id = 0;
   const Result added = _circuit.AddComponent(component, &id);
   if (IsErr(added))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(added)));
      return;
   }
   RefreshSheet();
   statusBar()->showMessage("Placed " + QString::fromStdString(KindLabel(_armedKind)) + ". Press Escape to stop placing.");
}

void MainWindow::OnPlacementCancelled(void)
{
   _placement = Placement::Idle;
   _pSchematic->SetPlacement(Placement::Idle);
   statusBar()->showMessage("Placement cancelled.");
}

void MainWindow::OnDeleteRequested(void)
{
   const uint64_t selectedId = _pSchematic->SelectedComponentId();
   if (selectedId == 0)
   {
      return;
   }
   _circuit.RemoveWiresTouching(selectedId);
   _circuit.RemoveComponent(selectedId);
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
   _pSchematic->SetPlacement(Placement::Armed);
   statusBar()->showMessage("Click the sheet to place " + QString::fromStdString(KindLabel(_armedKind)) + ".");
}

void MainWindow::OnSelectionChanged(void)
{
   ShowSelection();
}

void MainWindow::OnValueEdited(void)
{
   if (_updatingProperties || _pValueEdit->isEnabled() == false)
   {
      return;
   }
   Component* pComponent = _circuit.FindComponent(_pSchematic->SelectedComponentId());
   if (pComponent == nullptr)
   {
      return;
   }
   double value = 0.0;
   const Result parsed = ParseSiNumber(_pValueEdit->text().toStdString(), &value);
   if (IsErr(parsed))
   {
      statusBar()->showMessage("Enter a number such as 4.7k, 10u, or 1Meg.");
      return;
   }
   const Result written = WriteNumeric(pComponent, _editedProperty, value);
   if (IsErr(written))
   {
      statusBar()->showMessage(QString::fromStdString(Describe(written)));
      return;
   }
   statusBar()->showMessage("Updated " + QString::fromStdString(pComponent->name) + ".");
}

void MainWindow::OnNewCircuit(void)
{
   _circuit.Clear();
   _circuit.SetName("Untitled");
   _pResults->clear();
   RefreshSheet();
   ShowSelection();
   statusBar()->showMessage("New circuit.");
}

void MainWindow::OnRunDc(void)
{
   const DcResult result = SolveDc(_circuit, nullptr);
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
   statusBar()->showMessage(QString::fromStdString(result.message));
}

void MainWindow::OnAbout(void)
{
   QMessageBox::about(this, "Electric Circuit Analyzer",
      "Places parts on a grid and solves the DC operating point.\n"
      "SI values such as 4.7k and 10u are accepted in the property field.");
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
