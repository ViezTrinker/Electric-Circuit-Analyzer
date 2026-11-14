/*!
 * \file netlist_builder.cpp
 * \brief Union of pins, orthogonal wires, grounds, and net labels.
 */

#include "netlist_builder.h"

#include <map>
#include <string>

namespace Analyzer
{

namespace
{

class UnionFind
{
public:
   explicit UnionFind(uint32_t count)
      : _parent(count)
   {
      for (uint32_t index = 0; index < count; index += 1)
      {
         _parent[index] = index;
      }
   }

   uint32_t Find(uint32_t index)
   {
      uint32_t root = index;
      while (_parent[root] != root)
      {
         root = _parent[root];
      }
      while (_parent[index] != root)
      {
         uint32_t next = _parent[index];
         _parent[index] = root;
         index = next;
      }
      return root;
   }

   void Unite(uint32_t left, uint32_t right)
   {
      uint32_t leftRoot = Find(left);
      uint32_t rightRoot = Find(right);
      if (leftRoot != rightRoot)
      {
         _parent[rightRoot] = leftRoot;
      }
   }

private:
   std::vector<uint32_t> _parent;
};

using PointIndexMap = std::map<GridPoint, uint32_t>;

void AppendPoint(PointIndexMap* pPoints, GridPointList* pOrder, GridPoint point)
{
   if (pPoints->find(point) != pPoints->end())
   {
      return;
   }
   uint32_t index = static_cast<uint32_t>(pOrder->size());
   pPoints->emplace(point, index);
   pOrder->push_back(point);
}

Result AppendSegment(PointIndexMap* pPoints, GridPointList* pOrder, std::vector<uint32_t>* pSegmentIndices, GridPoint start, GridPoint end)
{
   int32_t deltaX = end.positionX - start.positionX;
   int32_t deltaY = end.positionY - start.positionY;
   if (deltaX != 0 && deltaY != 0)
   {
      return Result::InvalidParameter;
   }
   int32_t stepX = 0;
   int32_t stepY = 0;
   int32_t length = 0;
   if (deltaX != 0)
   {
      stepX = deltaX > 0 ? 1 : -1;
      length = deltaX > 0 ? deltaX : -deltaX;
   }
   else
   {
      stepY = deltaY > 0 ? 1 : -1;
      length = deltaY > 0 ? deltaY : -deltaY;
   }
   for (int32_t step = 0; step <= length; step += 1)
   {
      GridPoint point{start.positionX + (stepX * step), start.positionY + (stepY * step)};
      AppendPoint(pPoints, pOrder, point);
      pSegmentIndices->push_back(pPoints->find(point)->second);
   }
   return Result::Ok;
}

} // namespace

Result BuildNetlist(const Circuit& circuit, Netlist* pNetlist)
{
   if (pNetlist == nullptr)
   {
      return Result::InvalidParameter;
   }
   *pNetlist = Netlist{};
   PointIndexMap pointIndex;
   GridPointList pointOrder;
   for (const Component& component : circuit.Components())
   {
      uint8_t pinCount = PinCount(component);
      for (uint8_t pin = 0; pin < pinCount; pin += 1)
      {
         AppendPoint(&pointIndex, &pointOrder, PinPosition(component, pin));
      }
   }
   for (const NetLabel& label : circuit.NetLabels())
   {
      AppendPoint(&pointIndex, &pointOrder, label.position);
   }
   std::vector<std::vector<uint32_t>> wireChains;
   for (const Wire& wire : circuit.Wires())
   {
      if (wire.points.size() < 2)
      {
         return Result::InvalidParameter;
      }
      std::vector<uint32_t> chain;
      for (size_t segment = 0; segment + 1 < wire.points.size(); segment += 1)
      {
         Result segmentResult = AppendSegment(&pointIndex, &pointOrder, &chain, wire.points[segment], wire.points[segment + 1]);
         if (IsErr(segmentResult))
         {
            return segmentResult;
         }
      }
      wireChains.push_back(std::move(chain));
   }
   UnionFind groups(static_cast<uint32_t>(pointOrder.size()));
   for (const std::vector<uint32_t>& chain : wireChains)
   {
      for (size_t index = 1; index < chain.size(); index += 1)
      {
         groups.Unite(chain[index - 1], chain[index]);
      }
   }
   bool foundGround = false;
   uint32_t groundPoint = 0;
   for (const Component& component : circuit.Components())
   {
      if (component.kind != ComponentKind::Ground)
      {
         continue;
      }
      uint32_t point = pointIndex.find(PinPosition(component, 0))->second;
      if (foundGround == false)
      {
         groundPoint = point;
         foundGround = true;
      }
      else
      {
         groups.Unite(groundPoint, point);
      }
   }
   std::map<uint32_t, uint32_t> rootToNet;
   uint32_t nextNet = 1;
   if (foundGround)
   {
      rootToNet[groups.Find(groundPoint)] = GroundNetIndex;
   }
   struct NetAssigner
   {
      UnionFind* pGroups;
      std::map<uint32_t, uint32_t>* pRootToNet;
      uint32_t* pNextNet;

      uint32_t NetOfPoint(uint32_t point)
      {
         uint32_t root = pGroups->Find(point);
         std::map<uint32_t, uint32_t>::iterator found = pRootToNet->find(root);
         if (found != pRootToNet->end())
         {
            return found->second;
         }
         uint32_t assigned = *pNextNet;
         *pNextNet += 1;
         pRootToNet->emplace(root, assigned);
         return assigned;
      }
   };
   NetAssigner assigner{&groups, &rootToNet, &nextNet};
   for (uint32_t point = 0; point < pointOrder.size(); point += 1)
   {
      (void)assigner.NetOfPoint(point);
   }
   pNetlist->netCount = nextNet;
   pNetlist->hasGround = foundGround;
   pNetlist->netNames.assign(pNetlist->netCount, std::string());
   if (foundGround)
   {
      pNetlist->netNames[GroundNetIndex] = "0";
   }
   for (const NetLabel& label : circuit.NetLabels())
   {
      uint32_t net = assigner.NetOfPoint(pointIndex.find(label.position)->second);
      if (pNetlist->netNames[net].empty())
      {
         pNetlist->netNames[net] = label.name;
      }
      else if (pNetlist->netNames[net] != label.name)
      {
         pNetlist->warnings.push_back("Net " + pNetlist->netNames[net] + " also has the label " + label.name + ".");
      }
   }
   pNetlist->hasAnalog = false;
   for (const Component& component : circuit.Components())
   {
      NetlistDevice device = {};
      device.componentId = component.id;
      device.name = component.name;
      device.kind = component.kind;
      device.pinCount = PinCount(component);
      device.nets.fill(GroundNetIndex);
      device.properties = component.properties;
      for (uint8_t pin = 0; pin < device.pinCount; pin += 1)
      {
         uint32_t point = pointIndex.find(PinPosition(component, pin))->second;
         device.nets[pin] = assigner.NetOfPoint(point);
      }
      if (KindIsAnalog(component.kind))
      {
         pNetlist->hasAnalog = true;
      }
      pNetlist->devices.push_back(std::move(device));
   }
   if (pNetlist->hasAnalog && pNetlist->hasGround == false)
   {
      return Result::MissingGround;
   }
   for (uint32_t net = 0; net < pNetlist->netCount; net += 1)
   {
      if (pNetlist->netNames[net].empty())
      {
         pNetlist->netNames[net] = "N" + std::to_string(net);
      }
   }
   return Result::Ok;
}

} // namespace Analyzer
