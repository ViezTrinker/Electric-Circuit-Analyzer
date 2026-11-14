/*!
 * \file main_window.h
 * \brief Main window of the circuit analyzer.
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QKeyEvent>
#include <QMainWindow>

#include "core/circuit.h"
#include "schematic_view.h"

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;

namespace Analyzer
{

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
    * \brief Loads the voltage divider used by the DC test.
    */
   void LoadVoltageDivider(void);

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
    * \brief Writes the edited value back to the part.
    */
   void OnValueEdited(void);

   /*!
    * \brief Clears the document.
    */
   void OnNewCircuit(void);

   /*!
    * \brief Solves the DC operating point and lists it.
    */
   void OnRunDc(void);

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
   QLabel* _pValueCaption;
   QLineEdit* _pValueEdit;
   QPlainTextEdit* _pResults;
   Placement _placement;
   ComponentKind _armedKind;
   NumericProperty _editedProperty;
   bool _updatingProperties;
};

} // namespace Analyzer

#endif // MAIN_WINDOW_H
