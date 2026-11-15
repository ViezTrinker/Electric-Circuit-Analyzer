/*!
 * \file circuit.cpp
 * \brief Circuit document editing and undo snapshots.
 */

#include "circuit.h"

#include <algorithm>

namespace Analyzer
{

namespace
{

constexpr uint32_t MaxUndoDepth = 100;
constexpr int32_t QuarterTurnDegrees = 90;
constexpr int32_t FullCircleDegrees = 360;

using PinPointList = std::vector<GridPoint>;

PinPointList CollectPins(const Component& component)
{
   PinPointList pins;
   uint8_t pinCount = PinCount(component);
   for (uint8_t pinIndex = 0; pinIndex < pinCount; pinIndex += 1)
   {
      pins.push_back(PinPosition(component, pinIndex));
   }
   return pins;
}

void RepairOrthogonal(Wire* pWire)
{
   if (pWire == nullptr || pWire->points.empty())
   {
      return;
   }
   GridPointList repaired;
   repaired.push_back(pWire->points.front());
   for (size_t index = 1; index < pWire->points.size(); index += 1)
   {
      GridPoint previous = repaired.back();
      GridPoint next = pWire->points[index];
      if (previous == next)
      {
         continue;
      }
      if (previous.positionX != next.positionX && previous.positionY != next.positionY)
      {
         repaired.push_back(GridPoint{previous.positionX, next.positionY});
      }
      repaired.push_back(next);
   }
   pWire->points = std::move(repaired);
}

void RetargetWires(Circuit* pCircuit, const PinPointList& oldPins, const PinPointList& newPins)
{
   if (pCircuit == nullptr || oldPins.size() != newPins.size())
   {
      return;
   }
   std::vector<uint64_t> wireIds;
   for (const Wire& wire : pCircuit->Wires())
   {
      wireIds.push_back(wire.id);
   }
   for (uint64_t wireId : wireIds)
   {
      Wire* pWire = pCircuit->FindWire(wireId);
      if (pWire == nullptr)
      {
         continue;
      }
      for (GridPoint& point : pWire->points)
      {
         for (size_t index = 0; index < oldPins.size(); index += 1)
         {
            if (point == oldPins[index])
            {
               point = newPins[index];
               break;
            }
         }
      }
      RepairOrthogonal(pWire);
   }
}

} // namespace

Circuit::Circuit(void)
   : _name("Untitled")
   , _nextId(1)
   , _settings(DefaultAnalysisSettings())
{
}

void Circuit::Clear(void)
{
   _components.clear();
   _wires.clear();
   _netLabels.clear();
   _nextId = 1;
}

void Circuit::SetName(std::string_view name)
{
   _name = std::string(name);
}

const std::string& Circuit::Name(void) const
{
   return _name;
}

AnalysisSettings& Circuit::Settings(void)
{
   return _settings;
}

const AnalysisSettings& Circuit::Settings(void) const
{
   return _settings;
}

uint64_t Circuit::AllocateId(void)
{
   uint64_t id = _nextId;
   _nextId += 1;
   return id;
}

bool Circuit::NameExists(std::string_view name) const
{
   for (const Component& component : _components)
   {
      if (component.name == name)
      {
         return true;
      }
   }
   for (const Wire& wire : _wires)
   {
      if (wire.name == name)
      {
         return true;
      }
   }
   return false;
}

Result Circuit::AddComponent(const Component& component, uint64_t* pId)
{
   if (IsQuarterTurn(component.rotationDegrees) == false)
   {
      return Result::InvalidParameter;
   }
   Component stored = component;
   if (stored.id == 0)
   {
      stored.id = AllocateId();
   }
   else if (stored.id >= _nextId)
   {
      _nextId = stored.id + 1;
   }
   if (stored.name.empty() || stored.name == KindPrefix(stored.kind))
   {
      uint32_t number = 1;
      std::string candidate = KindPrefix(stored.kind) + std::to_string(number);
      while (NameExists(candidate))
      {
         number += 1;
         candidate = KindPrefix(stored.kind) + std::to_string(number);
      }
      stored.name = candidate;
   }
   if (pId != nullptr)
   {
      *pId = stored.id;
   }
   _components.push_back(std::move(stored));
   return Result::Ok;
}

Result Circuit::RemoveComponent(uint64_t componentId)
{
   for (ComponentList::iterator iterator = _components.begin(); iterator != _components.end(); ++iterator)
   {
      if (iterator->id == componentId)
      {
         _components.erase(iterator);
         return Result::Ok;
      }
   }
   return Result::Error;
}

Component* Circuit::FindComponent(uint64_t componentId)
{
   for (Component& component : _components)
   {
      if (component.id == componentId)
      {
         return &component;
      }
   }
   return nullptr;
}

const Component* Circuit::FindComponent(uint64_t componentId) const
{
   for (const Component& component : _components)
   {
      if (component.id == componentId)
      {
         return &component;
      }
   }
   return nullptr;
}

const ComponentList& Circuit::Components(void) const
{
   return _components;
}

Result Circuit::AddWire(const Wire& wire, uint64_t* pId)
{
   if (wire.points.size() < 2)
   {
      return Result::InvalidParameter;
   }
   Wire stored = wire;
   if (stored.id == 0)
   {
      stored.id = AllocateId();
   }
   else if (stored.id >= _nextId)
   {
      _nextId = stored.id + 1;
   }
   if (pId != nullptr)
   {
      *pId = stored.id;
   }
   _wires.push_back(std::move(stored));
   return Result::Ok;
}

Result Circuit::RemoveWire(uint64_t wireId)
{
   for (WireList::iterator iterator = _wires.begin(); iterator != _wires.end(); ++iterator)
   {
      if (iterator->id == wireId)
      {
         _wires.erase(iterator);
         return Result::Ok;
      }
   }
   return Result::Error;
}

Wire* Circuit::FindWire(uint64_t wireId)
{
   for (Wire& wire : _wires)
   {
      if (wire.id == wireId)
      {
         return &wire;
      }
   }
   return nullptr;
}

const WireList& Circuit::Wires(void) const
{
   return _wires;
}

Result Circuit::AddNetLabel(const NetLabel& label, uint64_t* pId)
{
   if (label.name.empty())
   {
      return Result::InvalidParameter;
   }
   NetLabel stored = label;
   if (stored.id == 0)
   {
      stored.id = AllocateId();
   }
   else if (stored.id >= _nextId)
   {
      _nextId = stored.id + 1;
   }
   if (pId != nullptr)
   {
      *pId = stored.id;
   }
   _netLabels.push_back(std::move(stored));
   return Result::Ok;
}

Result Circuit::RemoveNetLabel(uint64_t labelId)
{
   for (NetLabelList::iterator iterator = _netLabels.begin(); iterator != _netLabels.end(); ++iterator)
   {
      if (iterator->id == labelId)
      {
         _netLabels.erase(iterator);
         return Result::Ok;
      }
   }
   return Result::Error;
}

const NetLabelList& Circuit::NetLabels(void) const
{
   return _netLabels;
}

void Circuit::RemoveWiresTouching(uint64_t componentId)
{
   const Component* pComponent = FindComponent(componentId);
   if (pComponent == nullptr)
   {
      return;
   }
   GridPointList pins;
   uint8_t pinCount = PinCount(*pComponent);
   for (uint8_t pinIndex = 0; pinIndex < pinCount; pinIndex += 1)
   {
      pins.push_back(PinPosition(*pComponent, pinIndex));
   }
   WireList kept;
   for (const Wire& wire : _wires)
   {
      bool touching = false;
      if (wire.points.empty() == false)
      {
         for (const GridPoint& pin : pins)
         {
            if (wire.points.front() == pin || wire.points.back() == pin)
            {
               touching = true;
            }
         }
      }
      if (touching == false)
      {
         kept.push_back(wire);
      }
   }
   _wires = std::move(kept);
}

Result Circuit::MoveComponent(uint64_t componentId, GridPoint position)
{
   Component* pComponent = FindComponent(componentId);
   if (pComponent == nullptr)
   {
      return Result::Error;
   }
   if (pComponent->position == position)
   {
      return Result::Ok;
   }
   PinPointList oldPins = CollectPins(*pComponent);
   pComponent->position = position;
   RetargetWires(this, oldPins, CollectPins(*pComponent));
   return Result::Ok;
}

Result Circuit::RotateComponent(uint64_t componentId)
{
   Component* pComponent = FindComponent(componentId);
   if (pComponent == nullptr)
   {
      return Result::Error;
   }
   int32_t turned = pComponent->rotationDegrees + QuarterTurnDegrees;
   if (turned >= FullCircleDegrees)
   {
      turned -= FullCircleDegrees;
   }
   if (IsQuarterTurn(turned) == false)
   {
      return Result::InvalidParameter;
   }
   PinPointList oldPins = CollectPins(*pComponent);
   pComponent->rotationDegrees = turned;
   RetargetWires(this, oldPins, CollectPins(*pComponent));
   return Result::Ok;
}

UndoHistory::UndoHistory(void)
{
}

void UndoHistory::Clear(void)
{
   _undo.clear();
   _redo.clear();
}

void UndoHistory::Push(const Circuit& circuit)
{
   _undo.push_back(circuit);
   if (_undo.size() > MaxUndoDepth)
   {
      _undo.erase(_undo.begin());
   }
   _redo.clear();
}

Result UndoHistory::Undo(Circuit* pCircuit)
{
   if (pCircuit == nullptr || _undo.empty())
   {
      return Result::Error;
   }
   _redo.push_back(*pCircuit);
   *pCircuit = _undo.back();
   _undo.pop_back();
   return Result::Ok;
}

Result UndoHistory::Redo(Circuit* pCircuit)
{
   if (pCircuit == nullptr || _redo.empty())
   {
      return Result::Error;
   }
   _undo.push_back(*pCircuit);
   *pCircuit = _redo.back();
   _redo.pop_back();
   return Result::Ok;
}

bool UndoHistory::CanUndo(void) const
{
   return _undo.empty() == false;
}

bool UndoHistory::CanRedo(void) const
{
   return _redo.empty() == false;
}

} // namespace Analyzer
