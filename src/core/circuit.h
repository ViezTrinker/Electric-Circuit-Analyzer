/*!
 * \file circuit.h
 * \brief Editable circuit document: components, wires, net labels, and undo.
 */

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include "analysis_settings.h"
#include "component.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Analyzer
{

/*!
 * \brief Orthogonal wire stored as grid points.
 */
struct Wire
{
   uint64_t id;
   std::string name;
   GridPointList points;
};

using WireList = std::vector<Wire>;

/*!
 * \brief User name attached to the net under a grid point.
 */
struct NetLabel
{
   uint64_t id;
   std::string name;
   GridPoint position;
};

using NetLabelList = std::vector<NetLabel>;

/*!
 * \brief One saved schematic and its analysis settings.
 */
class Circuit
{
public:
   /*!
    * \brief Creates an empty circuit with default analysis settings.
    */
   Circuit(void);

   /*!
    * \brief Removes every component, wire, and label.
    */
   void Clear(void);

   /*!
    * \brief Sets the display name.
    *
    * \param[in] name New name.
    */
   void SetName(std::string_view name);

   /*!
    * \brief Display name.
    */
   const std::string& Name(void) const;

   /*!
    * \brief Analysis settings stored with the circuit.
    */
   AnalysisSettings& Settings(void);

   /*!
    * \brief Analysis settings stored with the circuit.
    */
   const AnalysisSettings& Settings(void) const;

   /*!
    * \brief Appends a component and assigns a unique id and name when needed.
    *
    * \param[in] component Component to copy into the document.
    * \param[out] pId Assigned id. May be null.
    */
   Result AddComponent(const Component& component, uint64_t* pId);

   /*!
    * \brief Removes a component and returns Ok when it existed.
    *
    * \param[in] componentId Id to remove.
    */
   Result RemoveComponent(uint64_t componentId);

   /*!
    * \brief Mutable component, or null.
    *
    * \param[in] componentId Id to find.
    */
   Component* FindComponent(uint64_t componentId);

   /*!
    * \brief Component, or null.
    *
    * \param[in] componentId Id to find.
    */
   const Component* FindComponent(uint64_t componentId) const;

   /*!
    * \brief All components.
    */
   const ComponentList& Components(void) const;

   /*!
    * \brief Appends a wire.
    *
    * \param[in] wire Wire to copy.
    * \param[out] pId Assigned id. May be null.
    */
   Result AddWire(const Wire& wire, uint64_t* pId);

   /*!
    * \brief Removes a wire.
    *
    * \param[in] wireId Id to remove.
    */
   Result RemoveWire(uint64_t wireId);

   /*!
    * \brief Mutable wire, or null.
    *
    * \param[in] wireId Id to find.
    */
   Wire* FindWire(uint64_t wireId);

   /*!
    * \brief All wires.
    */
   const WireList& Wires(void) const;

   /*!
    * \brief Appends a net label.
    *
    * \param[in] label Label to copy.
    * \param[out] pId Assigned id. May be null.
    */
   Result AddNetLabel(const NetLabel& label, uint64_t* pId);

   /*!
    * \brief Removes a net label.
    *
    * \param[in] labelId Id to remove.
    */
   Result RemoveNetLabel(uint64_t labelId);

   /*!
    * \brief All net labels.
    */
   const NetLabelList& NetLabels(void) const;

   /*!
    * \brief Deletes every wire that has an endpoint on a pin of the component.
    *
    * \param[in] componentId Component whose attached wires are removed.
    */
   void RemoveWiresTouching(uint64_t componentId);

   /*!
    * \brief Moves a part and the wire vertices that sit on its pins.
    *
    * A segment that would become diagonal gains a corner so the wire stays orthogonal.
    *
    * \param[in] componentId Part to move.
    * \param[in] position New anchor on the grid.
    */
   Result MoveComponent(uint64_t componentId, GridPoint position);

   /*!
    * \brief Turns a part 90 degrees clockwise and follows its pins with the wires.
    *
    * \param[in] componentId Part to turn.
    */
   Result RotateComponent(uint64_t componentId);

private:
   uint64_t AllocateId(void);
   bool NameExists(std::string_view name) const;

   std::string _name;
   uint64_t _nextId;
   ComponentList _components;
   WireList _wires;
   NetLabelList _netLabels;
   AnalysisSettings _settings;
};

/*!
 * \brief Snapshot undo and redo for a circuit document.
 */
class UndoHistory
{
public:
   /*!
    * \brief Creates an empty history.
    */
   UndoHistory(void);

   /*!
    * \brief Drops the history.
    */
   void Clear(void);

   /*!
    * \brief Remembers a document state before a new edit.
    *
    * \param[in] circuit State to copy.
    */
   void Push(const Circuit& circuit);

   /*!
    * \brief Restores the previous state and keeps the current one for redo.
    *
    * \param[in,out] pCircuit Document to replace.
    */
   Result Undo(Circuit* pCircuit);

   /*!
    * \brief Restores a state that was undone.
    *
    * \param[in,out] pCircuit Document to replace.
    */
   Result Redo(Circuit* pCircuit);

   /*!
    * \brief True when Undo can restore a state.
    */
   bool CanUndo(void) const;

   /*!
    * \brief True when Redo can restore a state.
    */
   bool CanRedo(void) const;

private:
   std::vector<Circuit> _undo;
   std::vector<Circuit> _redo;
};

} // namespace Analyzer

#endif // CIRCUIT_H
