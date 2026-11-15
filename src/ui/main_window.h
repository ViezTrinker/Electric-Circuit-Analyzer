/*!
 * \file main_window.h
 * \brief Main window of the circuit analyzer.
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QKeyEvent>
#include <QList>
#include <QMainWindow>
#include <QString>

#include "analysis_worker.h"
#include "core/circuit.h"
#include "schematic_view.h"

class QAction;
class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;

namespace Analyzer
{

class ResultPlot;

/*!
 * \brief Whether the Run actions are available.
 */
enum class AnalysisBusy : bool
{
   No = false,
   Yes = true
};

/*!
 * \brief Palette, schematic sheet, properties, and DC results.
 */
class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   /*!
    * \brief Creates the analyzer window.
    *
    * \param[in] pParent Parent widget. May be null.
    */
   explicit MainWindow(QWidget* pParent = nullptr);

private:
   /*!
    * \brief Rebuilds the sheet from the document.
    */
   void RefreshSheet(void);

   /*!
    * \brief Fills the property panel for the selected part.
    */
   void ShowSelection(void);

   /*!
    * \brief Places the armed part.
    *
    * \param[in] positionX Grid column.
    * \param[in] positionY Grid row.
    */
   void OnPlaceRequested(int positionX, int positionY);

   /*!
    * \brief Leaves placement mode.
    */
   void OnPlacementCancelled(void);

   /*!
    * \brief Removes the selected part.
    */
   void OnDeleteRequested(void);

   /*!
    * \brief Arms the part that was double-clicked in the palette.
    *
    * \param[in] pItem Palette row.
    * \param[in] column Unused column index.
    */
   void OnPaletteActivated(QTreeWidgetItem* pItem, int column);

   /*!
    * \brief Updates the property panel after a selection change.
    */
   void OnSelectionChanged(void);

   /*!
    * \brief Writes an edited numeric value or switch event list back to the part.
    *
    * \param[in] pItem Edited table cell.
    */
   void OnPropertyEdited(QTableWidgetItem* pItem);

   /*!
    * \brief Writes the selected enumeration code back to the part.
    *
    * \param[in] index Selected combo index.
    */
   void OnEnumEdited(int index);

   /*!
    * \brief Moves the schematic readout to the plot cursor.
    *
    * \param[in] position Frequency or time selected on the diagram.
    */
   void OnPlotCursor(double position);

   /*!
    * \brief Draws the latest measured values on the sheet.
    */
   void ApplySheetNotes(void);

   /*!
    * \brief Clears the plot, the cursor, and the schematic readouts.
    */
   void ClearResults(void);

   /*!
    * \brief Clears the document.
    */
   void OnNewCircuit(void);

   /*!
    * \brief Replaces the document with a .eca.json file.
    */
   void OnOpenCircuit(void);

   /*!
    * \brief Writes the document to the current path, or asks for one.
    */
   void OnSaveCircuit(void);

   /*!
    * \brief Asks for a path and writes the document.
    */
   void OnSaveCircuitAs(void);

   /*!
    * \brief Restores the document from before the last edit.
    */
   void OnUndo(void);

   /*!
    * \brief Restores a document that was undone.
    */
   void OnRedo(void);

   /*!
    * \brief Starts an orthogonal wire. Enter keeps it and Escape cancels it.
    */
   void OnDrawWire(void);

   /*!
    * \brief Adds one grid point to the wire being drawn.
    *
    * \param[in] positionX Grid column.
    * \param[in] positionY Grid row.
    */
   void OnWirePoint(int positionX, int positionY);

   /*!
    * \brief Stores the wire when it has at least two points.
    */
   void OnWireFinish(void);

   /*!
    * \brief Moves the dropped part and the wires on its pins.
    *
    * \param[in] componentId Part that moved.
    * \param[in] positionX New grid column.
    * \param[in] positionY New grid row.
    */
   void OnComponentMoved(qulonglong componentId, int positionX, int positionY);

   /*!
    * \brief Turns the selected part 90 degrees clockwise.
    */
   void OnRotateRequested(void);

   /*!
    * \brief Opens one example file from the examples directory.
    */
   void OnOpenExample(void);

   /*!
    * \brief Solves the DC operating point and lists it.
    */
   void OnRunDc(void);

   /*!
    * \brief Runs the AC sweep and draws the Bode diagram.
    */
   void OnRunAc(void);

   /*!
    * \brief Runs the transient analysis and draws time and spectrum diagrams.
    */
   void OnRunTransient(void);

   /*!
    * \brief Asks the active analysis to stop.
    */
   void OnCancelAnalysis(void);

   /*!
    * \brief Shows the result after the worker finishes.
    */
   void OnAnalysisFinished(void);

   /*!
    * \brief Starts one analysis on the worker thread.
    *
    * \param[in] kind DC, AC, or transient.
    */
   void StartAnalysis(AnalysisKind kind);

   /*!
    * \brief Enables or disables the Run actions.
    *
    * \param[in] busy Yes while a worker is running.
    */
   void SetAnalysisBusy(AnalysisBusy busy);

   /*!
    * \brief Remembers the document and its file path from before a successful edit.
    *
    * \param[in] before Document state to restore with Undo.
    * \param[in] pathBefore File path to restore with Undo.
    */
   void CommitEdit(const Circuit& before, const QString& pathBefore);

   /*!
    * \brief Enables Undo and Redo to match the history.
    */
   void UpdateHistoryActions(void);

   /*!
    * \brief Writes JSON to a path and remembers that path.
    *
    * \param[in] path Destination file path.
    */
   void SaveCircuitFile(const QString& path);

   /*!
    * \brief Shows a short description of the window.
    */
   void OnAbout(void);

   /*!
    * \brief Deletes the selection or cancels placement when the sheet is not focused.
    *
    * \param[in] pEvent Key event.
    */
   void keyPressEvent(QKeyEvent* pEvent) override;

   Circuit _circuit;
   SchematicView* _pSchematic;
   QTreeWidget* _pPalette;
   QLabel* _pNameLabel;
   QLabel* _pKindLabel;
   QTableWidget* _pProperties;
   QPlainTextEdit* _pResults;
   ResultPlot* _pPlot;
   QAction* _pUndoAction;
   QAction* _pRedoAction;
   QAction* _pRunDcAction;
   QAction* _pRunAcAction;
   QAction* _pRunTransientAction;
   QAction* _pCancelAction;
   AnalysisWorker _worker;
   Placement _placement;
   ComponentKind _armedKind;
   bool _updatingProperties;
   bool _cursorValid;
   double _cursorPosition;
   SheetNoteList _sheetNotes;
   UndoHistory _history;
   QList<QString> _savedPaths;
   QList<QString> _redoPaths;
   QString _documentPath;
   GridPointList _wirePoints;
};

} // namespace Analyzer

#endif // MAIN_WINDOW_H
