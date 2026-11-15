/*!
 * \file circuit_file.cpp
 * \brief JSON text for one electric-circuit-analyzer document.
 */

#include "circuit_file.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace Analyzer
{

namespace
{

constexpr std::string_view FormatName = "electric-circuit-analyzer";
constexpr int32_t FormatVersion = 1;
constexpr uint8_t SavedDipChannelCount = 4;

using Json = nlohmann::json;

std::string_view NumericKey(NumericProperty property)
{
   switch (property)
   {
      case NumericProperty::ResistanceOhm: return "resistanceOhm";
      case NumericProperty::CapacitanceFarad: return "capacitanceFarad";
      case NumericProperty::EsrOhm: return "esrOhm";
      case NumericProperty::InitialVoltageVolt: return "initialVoltageVolt";
      case NumericProperty::InductanceHenry: return "inductanceHenry";
      case NumericProperty::SeriesResistanceOhm: return "seriesResistanceOhm";
      case NumericProperty::InitialCurrentAmpere: return "initialCurrentAmpere";
      case NumericProperty::WiperPosition: return "wiperPosition";
      case NumericProperty::DcVoltageVolt: return "dcVoltageVolt";
      case NumericProperty::AcAmplitudeVolt: return "acAmplitudeVolt";
      case NumericProperty::AcPhaseDeg: return "acPhaseDeg";
      case NumericProperty::SineOffset: return "sineOffset";
      case NumericProperty::SineAmplitude: return "sineAmplitude";
      case NumericProperty::SineFrequencyHz: return "sineFrequencyHz";
      case NumericProperty::SinePhaseDeg: return "sinePhaseDeg";
      case NumericProperty::SineDelaySec: return "sineDelaySec";
      case NumericProperty::PulseInitial: return "pulseInitial";
      case NumericProperty::PulseLevel: return "pulseLevel";
      case NumericProperty::PulseDelaySec: return "pulseDelaySec";
      case NumericProperty::PulseRiseSec: return "pulseRiseSec";
      case NumericProperty::PulseFallSec: return "pulseFallSec";
      case NumericProperty::PulseWidthSec: return "pulseWidthSec";
      case NumericProperty::PulsePeriodSec: return "pulsePeriodSec";
      case NumericProperty::DcCurrentAmpere: return "dcCurrentAmpere";
      case NumericProperty::AcAmplitudeAmpere: return "acAmplitudeAmpere";
      case NumericProperty::ForwardVoltageVolt: return "forwardVoltageVolt";
      case NumericProperty::ForwardCurrentAmpere: return "forwardCurrentAmpere";
      case NumericProperty::EmissionCoefficient: return "emissionCoefficient";
      case NumericProperty::TemperatureCelsius: return "temperatureCelsius";
      case NumericProperty::JunctionCapacitanceFarad: return "junctionCapacitanceFarad";
      case NumericProperty::JunctionPotentialVolt: return "junctionPotentialVolt";
      case NumericProperty::GradingCoefficient: return "gradingCoefficient";
      case NumericProperty::TransitTimeSec: return "transitTimeSec";
      case NumericProperty::ForwardBeta: return "forwardBeta";
      case NumericProperty::ReverseBeta: return "reverseBeta";
      case NumericProperty::EarlyVoltageVolt: return "earlyVoltageVolt";
      case NumericProperty::BaseResistanceOhm: return "baseResistanceOhm";
      case NumericProperty::CollectorResistanceOhm: return "collectorResistanceOhm";
      case NumericProperty::EmitterResistanceOhm: return "emitterResistanceOhm";
      case NumericProperty::CjeFarad: return "cjeFarad";
      case NumericProperty::CjcFarad: return "cjcFarad";
      case NumericProperty::ForwardTransitTimeSec: return "forwardTransitTimeSec";
      case NumericProperty::ReverseTransitTimeSec: return "reverseTransitTimeSec";
      case NumericProperty::ThresholdVoltageVolt: return "thresholdVoltageVolt";
      case NumericProperty::Kp: return "kp";
      case NumericProperty::ChannelWidthMeter: return "channelWidthMeter";
      case NumericProperty::ChannelLengthMeter: return "channelLengthMeter";
      case NumericProperty::LambdaPerVolt: return "lambdaPerVolt";
      case NumericProperty::Cgso: return "cgso";
      case NumericProperty::Cgdo: return "cgdo";
      case NumericProperty::CoilResistanceOhm: return "coilResistanceOhm";
      case NumericProperty::CoilInductanceHenry: return "coilInductanceHenry";
      case NumericProperty::PullInCurrentAmpere: return "pullInCurrentAmpere";
      case NumericProperty::DropOutCurrentAmpere: return "dropOutCurrentAmpere";
      case NumericProperty::OperateDelaySec: return "operateDelaySec";
      case NumericProperty::OutputResistanceOhm: return "outputResistanceOhm";
      case NumericProperty::PropagationDelaySec: return "propagationDelaySec";
      case NumericProperty::HysteresisVolt: return "hysteresisVolt";
      case NumericProperty::SupplyVoltageVolt: return "supplyVoltageVolt";
      case NumericProperty::VohVolt: return "vohVolt";
      case NumericProperty::VolVolt: return "volVolt";
      case NumericProperty::VihVolt: return "vihVolt";
      case NumericProperty::VilVolt: return "vilVolt";
      case NumericProperty::RiseTimeSec: return "riseTimeSec";
      case NumericProperty::FallTimeSec: return "fallTimeSec";
      case NumericProperty::InputCount: return "inputCount";
      case NumericProperty::LowThresholdVolt: return "lowThresholdVolt";
      case NumericProperty::HighThresholdVolt: return "highThresholdVolt";
      case NumericProperty::SetupTimeSec: return "setupTimeSec";
      case NumericProperty::HoldTimeSec: return "holdTimeSec";
      case NumericProperty::FrequencyHz: return "frequencyHz";
      case NumericProperty::DutyCycle: return "dutyCycle";
      case NumericProperty::DelaySec: return "delaySec";
      case NumericProperty::ClosedResistanceOhm: return "closedResistanceOhm";
      case NumericProperty::OpenResistanceOhm: return "openResistanceOhm";
      case NumericProperty::SegmentResistanceOhm: return "segmentResistanceOhm";
      case NumericProperty::BurdenResistanceOhm: return "burdenResistanceOhm";
      case NumericProperty::BurdenInductanceHenry: return "burdenInductanceHenry";
      case NumericProperty::InputResistanceOhm: return "inputResistanceOhm";
      case NumericProperty::InputCapacitanceFarad: return "inputCapacitanceFarad";
      case NumericProperty::InitialValue: return "initialValue";
      default: break;
   }
   return {};
}

std::string_view EnumKey(EnumProperty property)
{
   switch (property)
   {
      case EnumProperty::TransientWaveform: return "transientWaveform";
      case EnumProperty::LogicFamily: return "logicFamily";
      case EnumProperty::OutputStage: return "outputStage";
      case EnumProperty::InitialState: return "initialState";
      case EnumProperty::ClockEdge: return "clockEdge";
      case EnumProperty::PresetPolarity: return "presetPolarity";
      case EnumProperty::ClearPolarity: return "clearPolarity";
      case EnumProperty::SrPolarity: return "srPolarity";
      case EnumProperty::EnablePolarity: return "enablePolarity";
      case EnumProperty::OutputPolarity: return "outputPolarity";
      case EnumProperty::StartLevel: return "startLevel";
      case EnumProperty::MeterMode: return "meterMode";
      case EnumProperty::ClosedMode: return "closedMode";
      case EnumProperty::OpenMode: return "openMode";
      case EnumProperty::RestState: return "restState";
      case EnumProperty::RestThrow: return "restThrow";
      case EnumProperty::ContactForm: return "contactForm";
      case EnumProperty::MechanicalState: return "mechanicalState";
      case EnumProperty::LedColor: return "ledColor";
      case EnumProperty::SegmentCommon: return "segmentCommon";
      case EnumProperty::AdcConversion: return "adcConversion";
      case EnumProperty::MuxWidth: return "muxWidth";
      default: break;
   }
   return {};
}

Result NumericFromKey(std::string_view key, NumericProperty* pProperty)
{
   if (pProperty == nullptr || key.empty())
   {
      return Result::InvalidParameter;
   }
   for (int16_t code = 0; code <= static_cast<int16_t>(NumericProperty::InitialValue); code += 1)
   {
      auto property = static_cast<NumericProperty>(code);
      if (NumericKey(property) == key)
      {
         *pProperty = property;
         return Result::Ok;
      }
   }
   return Result::InvalidParameter;
}

Result EnumFromKey(std::string_view key, EnumProperty* pProperty)
{
   if (pProperty == nullptr || key.empty())
   {
      return Result::InvalidParameter;
   }
   for (int16_t code = 0; code <= static_cast<int16_t>(EnumProperty::MuxWidth); code += 1)
   {
      auto property = static_cast<EnumProperty>(code);
      if (EnumKey(property) == key)
      {
         *pProperty = property;
         return Result::Ok;
      }
   }
   return Result::InvalidParameter;
}

bool IsDocumentKey(std::string_view key)
{
   return key == "format" || key == "version" || key == "name" || key == "settings" || key == "components" || key == "wires" || key == "labels";
}

bool IsSettingsKey(std::string_view key)
{
   return key == "kind" || key == "gminSiemens" || key == "updateDcOnEdit" || key == "sweepScale"
      || key == "sweepStartHz" || key == "sweepStopHz" || key == "pointsPerDecade" || key == "linearPointCount"
      || key == "stopTimeSec" || key == "maximumStepSec" || key == "integration" || key == "initialConditions";
}

bool IsComponentKey(std::string_view key)
{
   return key == "id" || key == "kind" || key == "name" || key == "x" || key == "y" || key == "rotation" || key == "properties";
}

bool IsWireKey(std::string_view key)
{
   return key == "id" || key == "name" || key == "points";
}

bool IsLabelKey(std::string_view key)
{
   return key == "id" || key == "name" || key == "x" || key == "y";
}

bool IsPropertyKey(std::string_view key)
{
   NumericProperty numeric = NumericProperty::ResistanceOhm;
   if (IsOk(NumericFromKey(key, &numeric)))
   {
      return true;
   }
   EnumProperty enumerated = EnumProperty::TransientWaveform;
   if (IsOk(EnumFromKey(key, &enumerated)))
   {
      return true;
   }
   return key == "events" || key == "channels";
}

bool IsEventKey(std::string_view key)
{
   return key == "timeSec" || key == "state";
}

bool IsPointKey(std::string_view key)
{
   return key == "x" || key == "y";
}

Result RequireObjectKeys(const Json& object, bool (*pAllowed)(std::string_view))
{
   if (object.is_object() == false || pAllowed == nullptr)
   {
      return Result::InvalidParameter;
   }
   for (Json::const_iterator entry = object.begin(); entry != object.end(); ++entry)
   {
      if (pAllowed(entry.key()) == false)
      {
         return Result::InvalidParameter;
      }
   }
   return Result::Ok;
}

Result ReadFinite(const Json& value, double* pNumber)
{
   if (pNumber == nullptr || value.is_number() == false)
   {
      return Result::InvalidParameter;
   }
   double number = value.get<double>();
   if (std::isfinite(number) == false)
   {
      return Result::InvalidParameter;
   }
   *pNumber = number;
   return Result::Ok;
}

Result ReadInt32(const Json& value, int32_t* pNumber)
{
   if (pNumber == nullptr || value.is_number_integer() == false)
   {
      return Result::InvalidParameter;
   }
   int64_t number = value.get<int64_t>();
   if (number < static_cast<int64_t>(INT32_MIN) || number > static_cast<int64_t>(INT32_MAX))
   {
      return Result::InvalidParameter;
   }
   *pNumber = static_cast<int32_t>(number);
   return Result::Ok;
}

Result ReadId(const Json& value, uint64_t* pId)
{
   if (pId == nullptr || value.is_number_integer() == false || value.get<int64_t>() < 0)
   {
      return Result::InvalidParameter;
   }
   *pId = value.get<uint64_t>();
   return Result::Ok;
}

Json SettingsJson(const AnalysisSettings& settings)
{
   Json object = Json::object();
   object["kind"] = static_cast<int32_t>(settings.kind);
   object["gminSiemens"] = settings.gminSiemens;
   object["updateDcOnEdit"] = static_cast<int32_t>(settings.updateDcOnEdit);
   object["sweepScale"] = static_cast<int32_t>(settings.sweepScale);
   object["sweepStartHz"] = settings.sweepStartHz;
   object["sweepStopHz"] = settings.sweepStopHz;
   object["pointsPerDecade"] = settings.pointsPerDecade;
   object["linearPointCount"] = settings.linearPointCount;
   object["stopTimeSec"] = settings.stopTimeSec;
   object["maximumStepSec"] = settings.maximumStepSec;
   object["integration"] = static_cast<int32_t>(settings.integration);
   object["initialConditions"] = static_cast<int32_t>(settings.initialConditions);
   return object;
}

const Json* FindField(const Json& object, std::string_view key)
{
   Json::const_iterator found = object.find(std::string(key));
   if (found == object.end())
   {
      return nullptr;
   }
   return &found.value();
}

Result ReadSettings(const Json& object, AnalysisSettings* pSettings)
{
   if (pSettings == nullptr || IsErr(RequireObjectKeys(object, IsSettingsKey)))
   {
      return Result::InvalidParameter;
   }
   const Json* pKind = FindField(object, "kind");
   const Json* pGmin = FindField(object, "gminSiemens");
   const Json* pUpdate = FindField(object, "updateDcOnEdit");
   const Json* pScale = FindField(object, "sweepScale");
   const Json* pStart = FindField(object, "sweepStartHz");
   const Json* pStop = FindField(object, "sweepStopHz");
   const Json* pDecade = FindField(object, "pointsPerDecade");
   const Json* pLinear = FindField(object, "linearPointCount");
   const Json* pStopTime = FindField(object, "stopTimeSec");
   const Json* pStep = FindField(object, "maximumStepSec");
   const Json* pIntegration = FindField(object, "integration");
   const Json* pInitial = FindField(object, "initialConditions");
   if (pKind == nullptr || pGmin == nullptr || pUpdate == nullptr || pScale == nullptr || pStart == nullptr || pStop == nullptr
      || pDecade == nullptr || pLinear == nullptr || pStopTime == nullptr || pStep == nullptr || pIntegration == nullptr || pInitial == nullptr)
   {
      return Result::InvalidParameter;
   }
   AnalysisSettings settings = DefaultAnalysisSettings();
   int32_t kind = 0;
   int32_t update = 0;
   int32_t scale = 0;
   int32_t integration = 0;
   int32_t initial = 0;
   if (IsErr(ReadInt32(*pKind, &kind)) || kind < 0 || kind > static_cast<int32_t>(AnalysisKind::Transient))
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadFinite(*pGmin, &settings.gminSiemens)) || settings.gminSiemens <= 0.0)
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadInt32(*pUpdate, &update)) || (update != 0 && update != 1))
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadInt32(*pScale, &scale)) || (scale != 0 && scale != 1))
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadFinite(*pStart, &settings.sweepStartHz)) || IsErr(ReadFinite(*pStop, &settings.sweepStopHz)))
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadInt32(*pDecade, &settings.pointsPerDecade)) || settings.pointsPerDecade < 1)
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadInt32(*pLinear, &settings.linearPointCount)) || settings.linearPointCount < 2)
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadFinite(*pStopTime, &settings.stopTimeSec)) || IsErr(ReadFinite(*pStep, &settings.maximumStepSec)))
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadInt32(*pIntegration, &integration)) || (integration != 0 && integration != 1))
   {
      return Result::InvalidParameter;
   }
   if (IsErr(ReadInt32(*pInitial, &initial)) || (initial != 0 && initial != 1))
   {
      return Result::InvalidParameter;
   }
   settings.kind = static_cast<AnalysisKind>(kind);
   settings.updateDcOnEdit = static_cast<UpdateDcOnEdit>(update);
   settings.sweepScale = static_cast<SweepScale>(scale);
   settings.integration = static_cast<IntegrationMethod>(integration);
   settings.initialConditions = static_cast<InitialConditionMode>(initial);
   *pSettings = settings;
   return Result::Ok;
}

void WriteEvents(const SwitchEventList& events, Json* pObject)
{
   if (pObject == nullptr || events.empty())
   {
      return;
   }
   Json list = Json::array();
   for (const SwitchEvent& event : events)
   {
      Json item = Json::object();
      item["timeSec"] = event.timeSec;
      item["state"] = static_cast<int32_t>(event.state);
      list.push_back(item);
   }
   (*pObject)["events"] = list;
}

void WriteChannels(const DipStateArray& channels, Json* pObject)
{
   if (pObject == nullptr)
   {
      return;
   }
   Json list = Json::array();
   for (uint8_t index = 0; index < SavedDipChannelCount; index += 1)
   {
      list.push_back(static_cast<int32_t>(channels[index]));
   }
   (*pObject)["channels"] = list;
}

Json PropertiesJson(const Component& component)
{
   Json object = Json::object();
   for (int16_t code = 0; code <= static_cast<int16_t>(NumericProperty::InitialValue); code += 1)
   {
      auto property = static_cast<NumericProperty>(code);
      const std::string_view key = NumericKey(property);
      double value = 0.0;
      if (key.empty() || IsErr(ReadNumeric(component, property, &value)))
      {
         continue;
      }
      Component probe = component;
      if (IsOk(WriteNumeric(&probe, property, value)))
      {
         object[std::string(key)] = value;
      }
   }
   for (int16_t code = 0; code <= static_cast<int16_t>(EnumProperty::MuxWidth); code += 1)
   {
      auto property = static_cast<EnumProperty>(code);
      const std::string_view key = EnumKey(property);
      int32_t value = 0;
      if (key.empty() || IsErr(ReadEnumCode(component, property, &value)))
      {
         continue;
      }
      Component probe = component;
      if (IsOk(WriteEnumCode(&probe, property, value)))
      {
         object[std::string(key)] = value;
      }
   }
   if (const SpstProperties* pSwitch = std::get_if<SpstProperties>(&component.properties))
   {
      WriteEvents(pSwitch->events, &object);
   }
   if (const PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&component.properties))
   {
      WriteEvents(pButton->events, &object);
   }
   if (const DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&component.properties))
   {
      WriteChannels(pDip->channelState, &object);
   }
   return object;
}

Result ReadEvents(const Json& list, SwitchEventList* pEvents)
{
   if (pEvents == nullptr || list.is_array() == false)
   {
      return Result::InvalidParameter;
   }
   SwitchEventList events;
   for (const Json& item : list)
   {
      if (IsErr(RequireObjectKeys(item, IsEventKey)))
      {
         return Result::InvalidParameter;
      }
      const Json* pTime = FindField(item, "timeSec");
      const Json* pState = FindField(item, "state");
      SwitchEvent event = {};
      int32_t state = 0;
      if (pTime == nullptr || pState == nullptr || IsErr(ReadFinite(*pTime, &event.timeSec)) || event.timeSec < 0.0)
      {
         return Result::InvalidParameter;
      }
      if (IsErr(ReadInt32(*pState, &state)) || (state != 0 && state != 1))
      {
         return Result::InvalidParameter;
      }
      event.state = static_cast<SwitchState>(state);
      events.push_back(event);
   }
   *pEvents = events;
   return Result::Ok;
}

Result ReadChannels(const Json& list, DipStateArray* pChannels)
{
   if (pChannels == nullptr || list.is_array() == false || list.size() != SavedDipChannelCount)
   {
      return Result::InvalidParameter;
   }
   DipStateArray channels = {};
   for (uint8_t index = 0; index < SavedDipChannelCount; index += 1)
   {
      int32_t state = 0;
      if (IsErr(ReadInt32(list.at(index), &state)) || (state != 0 && state != 1))
      {
         return Result::InvalidParameter;
      }
      channels[index] = static_cast<SwitchState>(state);
   }
   *pChannels = channels;
   return Result::Ok;
}

Result ApplyProperty(Component* pComponent, const std::string& key, const Json& value)
{
   if (pComponent == nullptr)
   {
      return Result::InvalidParameter;
   }
   if (key == "events")
   {
      SwitchEventList events;
      Result read = ReadEvents(value, &events);
      if (IsErr(read))
      {
         return read;
      }
      if (SpstProperties* pSwitch = std::get_if<SpstProperties>(&pComponent->properties))
      {
         pSwitch->events = events;
         return Result::Ok;
      }
      if (PushbuttonProperties* pButton = std::get_if<PushbuttonProperties>(&pComponent->properties))
      {
         pButton->events = events;
         return Result::Ok;
      }
      return Result::InvalidParameter;
   }
   if (key == "channels")
   {
      DipSwitchProperties* pDip = std::get_if<DipSwitchProperties>(&pComponent->properties);
      if (pDip == nullptr)
      {
         return Result::InvalidParameter;
      }
      return ReadChannels(value, &pDip->channelState);
   }
   NumericProperty numeric = NumericProperty::ResistanceOhm;
   if (IsOk(NumericFromKey(key, &numeric)))
   {
      double number = 0.0;
      if (IsErr(ReadFinite(value, &number)))
      {
         return Result::InvalidParameter;
      }
      return WriteNumeric(pComponent, numeric, number);
   }
   EnumProperty enumerated = EnumProperty::TransientWaveform;
   if (IsOk(EnumFromKey(key, &enumerated)))
   {
      int32_t code = 0;
      if (IsErr(ReadInt32(value, &code)))
      {
         return Result::InvalidParameter;
      }
      return WriteEnumCode(pComponent, enumerated, code);
   }
   return Result::InvalidParameter;
}

Result ReadProperties(const Json& object, Component* pComponent)
{
   if (pComponent == nullptr || IsErr(RequireObjectKeys(object, IsPropertyKey)))
   {
      return Result::InvalidParameter;
   }
   Json::const_iterator family = object.find("logicFamily");
   if (family != object.end())
   {
      Result applied = ApplyProperty(pComponent, "logicFamily", family.value());
      if (IsErr(applied))
      {
         return applied;
      }
   }
   for (Json::const_iterator entry = object.begin(); entry != object.end(); ++entry)
   {
      if (entry.key() == "logicFamily")
      {
         continue;
      }
      Result applied = ApplyProperty(pComponent, entry.key(), entry.value());
      if (IsErr(applied))
      {
         return applied;
      }
   }
   return Result::Ok;
}

Result ReadPoint(const Json& object, GridPoint* pPoint)
{
   if (pPoint == nullptr || IsErr(RequireObjectKeys(object, IsPointKey)))
   {
      return Result::InvalidParameter;
   }
   const Json* pPositionX = FindField(object, "x");
   const Json* pPositionY = FindField(object, "y");
   int32_t positionX = 0;
   int32_t positionY = 0;
   if (pPositionX == nullptr || pPositionY == nullptr || IsErr(ReadInt32(*pPositionX, &positionX)) || IsErr(ReadInt32(*pPositionY, &positionY)))
   {
      return Result::InvalidParameter;
   }
   *pPoint = GridPoint{positionX, positionY};
   return Result::Ok;
}

bool IdExists(const Circuit& circuit, uint64_t id)
{
   if (circuit.FindComponent(id) != nullptr)
   {
      return true;
   }
   for (const Wire& wire : circuit.Wires())
   {
      if (wire.id == id)
      {
         return true;
      }
   }
   for (const NetLabel& label : circuit.NetLabels())
   {
      if (label.id == id)
      {
         return true;
      }
   }
   return false;
}

Result ReadComponent(const Json& object, Circuit* pCircuit)
{
   if (pCircuit == nullptr || IsErr(RequireObjectKeys(object, IsComponentKey)) || object.contains("kind") == false || object.at("kind").is_string() == false)
   {
      return Result::InvalidParameter;
   }
   ComponentKind kind = ComponentKind::Resistor;
   Result known = KindFromKey(object.at("kind").get<std::string>(), &kind);
   if (IsErr(known))
   {
      return known;
   }
   int32_t positionX = 0;
   int32_t positionY = 0;
   int32_t rotation = 0;
   uint64_t id = 0;
   if (object.contains("x") && IsErr(ReadInt32(object.at("x"), &positionX)))
   {
      return Result::InvalidParameter;
   }
   if (object.contains("y") && IsErr(ReadInt32(object.at("y"), &positionY)))
   {
      return Result::InvalidParameter;
   }
   if (object.contains("rotation") && IsErr(ReadInt32(object.at("rotation"), &rotation)))
   {
      return Result::InvalidParameter;
   }
   if (object.contains("id"))
   {
      if (IsErr(ReadId(object.at("id"), &id)) || id == 0 || IdExists(*pCircuit, id))
      {
         return Result::InvalidParameter;
      }
   }
   Component component = CreateComponent(kind, GridPoint{positionX, positionY});
   component.id = id;
   component.rotationDegrees = rotation;
   if (object.contains("name"))
   {
      if (object.at("name").is_string() == false)
      {
         return Result::InvalidParameter;
      }
      component.name = object.at("name").get<std::string>();
   }
   if (object.contains("properties"))
   {
      Result properties = ReadProperties(object.at("properties"), &component);
      if (IsErr(properties))
      {
         return properties;
      }
   }
   return pCircuit->AddComponent(component, nullptr);
}

Result ReadWire(const Json& object, Circuit* pCircuit)
{
   if (pCircuit == nullptr || IsErr(RequireObjectKeys(object, IsWireKey)) || object.contains("points") == false || object.at("points").is_array() == false)
   {
      return Result::InvalidParameter;
   }
   Wire wire = {};
   if (object.contains("id"))
   {
      if (IsErr(ReadId(object.at("id"), &wire.id)) || wire.id == 0 || IdExists(*pCircuit, wire.id))
      {
         return Result::InvalidParameter;
      }
   }
   if (object.contains("name"))
   {
      if (object.at("name").is_string() == false)
      {
         return Result::InvalidParameter;
      }
      wire.name = object.at("name").get<std::string>();
   }
   for (const Json& point : object.at("points"))
   {
      GridPoint grid = {};
      if (IsErr(ReadPoint(point, &grid)))
      {
         return Result::InvalidParameter;
      }
      wire.points.push_back(grid);
   }
   return pCircuit->AddWire(wire, nullptr);
}

Result ReadLabel(const Json& object, Circuit* pCircuit)
{
   if (pCircuit == nullptr || IsErr(RequireObjectKeys(object, IsLabelKey)) || object.contains("name") == false || object.at("name").is_string() == false)
   {
      return Result::InvalidParameter;
   }
   NetLabel label = {};
   label.name = object.at("name").get<std::string>();
   if (label.name.empty())
   {
      return Result::InvalidParameter;
   }
   if (object.contains("id"))
   {
      if (IsErr(ReadId(object.at("id"), &label.id)) || label.id == 0 || IdExists(*pCircuit, label.id))
      {
         return Result::InvalidParameter;
      }
   }
   if (object.contains("x") && IsErr(ReadInt32(object.at("x"), &label.position.positionX)))
   {
      return Result::InvalidParameter;
   }
   if (object.contains("y") && IsErr(ReadInt32(object.at("y"), &label.position.positionY)))
   {
      return Result::InvalidParameter;
   }
   return pCircuit->AddNetLabel(label, nullptr);
}

} // namespace

Result WriteCircuitText(const Circuit& circuit, std::string* pText)
{
   if (pText == nullptr)
   {
      return Result::InvalidParameter;
   }
   Json document = Json::object();
   document["format"] = std::string(FormatName);
   document["version"] = FormatVersion;
   document["name"] = circuit.Name();
   document["settings"] = SettingsJson(circuit.Settings());
   document["components"] = Json::array();
   for (const Component& component : circuit.Components())
   {
      Json item = Json::object();
      item["id"] = component.id;
      item["kind"] = KindKey(component.kind);
      item["name"] = component.name;
      item["x"] = component.position.positionX;
      item["y"] = component.position.positionY;
      item["rotation"] = component.rotationDegrees;
      item["properties"] = PropertiesJson(component);
      document["components"].push_back(item);
   }
   document["wires"] = Json::array();
   for (const Wire& wire : circuit.Wires())
   {
      Json item = Json::object();
      item["id"] = wire.id;
      item["name"] = wire.name;
      item["points"] = Json::array();
      for (const GridPoint& point : wire.points)
      {
         Json grid = Json::object();
         grid["x"] = point.positionX;
         grid["y"] = point.positionY;
         item["points"].push_back(grid);
      }
      document["wires"].push_back(item);
   }
   document["labels"] = Json::array();
   for (const NetLabel& label : circuit.NetLabels())
   {
      Json item = Json::object();
      item["id"] = label.id;
      item["name"] = label.name;
      item["x"] = label.position.positionX;
      item["y"] = label.position.positionY;
      document["labels"].push_back(item);
   }
   *pText = document.dump(3);
   pText->push_back('\n');
   return Result::Ok;
}

Result ReadCircuitText(std::string_view text, Circuit* pCircuit)
{
   if (pCircuit == nullptr)
   {
      return Result::InvalidParameter;
   }
   Json document = Json::parse(text, nullptr, false);
   if (document.is_discarded() || document.is_object() == false || IsErr(RequireObjectKeys(document, IsDocumentKey)))
   {
      return Result::InvalidParameter;
   }
   if (document.contains("format") == false || document.at("format").is_string() == false || document.at("format").get<std::string>() != FormatName)
   {
      return Result::UnknownVersion;
   }
   int32_t version = 0;
   if (document.contains("version") == false || IsErr(ReadInt32(document.at("version"), &version)))
   {
      return Result::UnknownVersion;
   }
   if (version != FormatVersion)
   {
      return Result::UnknownVersion;
   }
   Circuit loaded;
   if (document.contains("name"))
   {
      if (document.at("name").is_string() == false)
      {
         return Result::InvalidParameter;
      }
      loaded.SetName(document.at("name").get<std::string>());
   }
   if (document.contains("settings"))
   {
      AnalysisSettings settings = {};
      Result read = ReadSettings(document.at("settings"), &settings);
      if (IsErr(read))
      {
         return read;
      }
      loaded.Settings() = settings;
   }
   if (document.contains("components") == false || document.at("components").is_array() == false)
   {
      return Result::InvalidParameter;
   }
   for (const Json& item : document.at("components"))
   {
      Result read = ReadComponent(item, &loaded);
      if (IsErr(read))
      {
         return read;
      }
   }
   if (document.contains("wires"))
   {
      if (document.at("wires").is_array() == false)
      {
         return Result::InvalidParameter;
      }
      for (const Json& item : document.at("wires"))
      {
         Result read = ReadWire(item, &loaded);
         if (IsErr(read))
         {
            return read;
         }
      }
   }
   if (document.contains("labels"))
   {
      if (document.at("labels").is_array() == false)
      {
         return Result::InvalidParameter;
      }
      for (const Json& item : document.at("labels"))
      {
         Result read = ReadLabel(item, &loaded);
         if (IsErr(read))
         {
            return read;
         }
      }
   }
   *pCircuit = std::move(loaded);
   return Result::Ok;
}

std::string_view NumericPropertyKey(NumericProperty property)
{
   return NumericKey(property);
}

std::string_view EnumPropertyKey(EnumProperty property)
{
   return EnumKey(property);
}

} // namespace Analyzer
