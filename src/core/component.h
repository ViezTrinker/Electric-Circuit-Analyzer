/*!
 * \file component.h
 * \brief Schematic component kinds, parameters, pins, and names.
 */

#ifndef COMPONENT_H
#define COMPONENT_H

#include "grid_point.h"
#include "result.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Analyzer
{

inline constexpr uint8_t MaxPinCount = 16;
inline constexpr int32_t TwoTerminalSpan = 2;
inline constexpr int32_t BoxWidth = 4;
inline constexpr int32_t PinSpacing = 2;

inline constexpr double DefaultResistanceOhm = 1000.0;
inline constexpr double DefaultCapacitanceFarad = 1.0e-6;
inline constexpr double DefaultInductanceHenry = 1.0e-3;
inline constexpr double DefaultSourceVoltageVolt = 5.0;
inline constexpr double DefaultSourceCurrentAmpere = 0.001;
inline constexpr double DefaultForwardVoltageVolt = 0.7;
inline constexpr double DefaultForwardCurrentAmpere = 0.001;
inline constexpr double DefaultEmissionCoefficient = 1.0;
inline constexpr double DefaultTemperatureCelsius = 27.0;
inline constexpr double DefaultJunctionPotentialVolt = 0.7;
inline constexpr double DefaultGradingCoefficient = 0.5;
inline constexpr double DefaultForwardBeta = 100.0;
inline constexpr double DefaultReverseBeta = 1.0;
inline constexpr double DefaultMosThresholdVolt = 1.0;
inline constexpr double DefaultMosKp = 2.0e-4;
inline constexpr double DefaultChannelWidthMeter = 1.0e-4;
inline constexpr double DefaultChannelLengthMeter = 1.0e-4;
inline constexpr double DefaultSupplyVoltageVolt = 5.0;
inline constexpr double CmosHighFraction = 0.7;
inline constexpr double CmosLowFraction = 0.3;
inline constexpr double DefaultPropagationDelaySec = 10.0e-9;
inline constexpr double DefaultEdgeTimeSec = 5.0e-9;
inline constexpr double DefaultOutputResistanceOhm = 100.0;
inline constexpr double DefaultClosedResistanceOhm = 0.01;
inline constexpr double DefaultOpenResistanceOhm = 1.0e9;
inline constexpr double DefaultSegmentResistanceOhm = 330.0;
inline constexpr double TtlVolVolt = 0.2;
inline constexpr double TtlVohVolt = 3.4;
inline constexpr double TtlVilVolt = 0.8;
inline constexpr double TtlVihVolt = 2.0;
inline constexpr double ZeroCelsiusInKelvin = 273.15;
inline constexpr double BoltzmannJoulePerKelvin = 1.380649e-23;
inline constexpr double ElectronChargeCoulomb = 1.602176634e-19;
inline constexpr double MinimumWiperFraction = 1.0e-6;

/*!
 * \brief Kind of a placed component.
 */
enum class ComponentKind : int16_t
{
   Resistor = 0,
   Capacitor,
   Inductor,
   Potentiometer,
   VoltageSource,
   CurrentSource,
   Diode,
   Led,
   Npn,
   Pnp,
   Nmos,
   Pmos,
   Relay,
   Timer555,
   Comparator,
   Ground,
   SpstSwitch,
   SpdtSwitch,
   Pushbutton,
   DipSwitch,
   SevenSegment,
   Ammeter,
   Voltmeter,
   LogicProbe,
   Buffer,
   Inverter,
   SchmittInverter,
   AndGate,
   OrGate,
   NandGate,
   NorGate,
   XorGate,
   XnorGate,
   TriStateBuffer,
   SrLatch,
   DLatch,
   DFlipFlop,
   JkFlipFlop,
   TFlipFlop,
   Clock,
   Multiplexer,
   Decoder,
   BcdToSevenSegment,
   FullAdder,
   Counter,
   ShiftRegister,
   PullUp,
   PullDown,
   Adc,
   Dac
};

enum class SourceWaveform : int8_t
{
   Dc = 0,
   Sine,
   Pulse
};

enum class LogicFamily : int8_t
{
   Ttl = 0,
   Cmos5V,
   Cmos3V3,
   Custom
};

enum class OutputStage : int8_t
{
   PushPull = 0,
   OpenDrain
};

enum class LogicLevel : int8_t
{
   Low = 0,
   High,
   HighZ,
   Conflict
};

enum class ClockEdge : int8_t
{
   Rising = 0,
   Falling
};

enum class Polarity : int8_t
{
   ActiveHigh = 0,
   ActiveLow
};

enum class MeterMode : int8_t
{
   Ideal = 0,
   NonIdeal
};

enum class ResistanceMode : int8_t
{
   Ideal = 0,
   Resistive
};

enum class SwitchState : int8_t
{
   Open = 0,
   Closed
};

enum class ThrowPosition : int8_t
{
   ThrowA = 0,
   ThrowB
};

enum class ContactForm : int8_t
{
   NormallyOpen = 0,
   NormallyClosed
};

enum class LedColor : int8_t
{
   Red = 0,
   Green,
   Yellow,
   Blue,
   White
};

enum class SegmentCommon : int8_t
{
   CommonCathode = 0,
   CommonAnode
};

enum class AdcConversion : int8_t
{
   Clocked = 0,
   Continuous
};

enum class MuxWidth : int8_t
{
   Two = 2,
   Four = 4
};

struct ResistorProperties
{
   double resistanceOhm;
};

struct CapacitorProperties
{
   double capacitanceFarad;
   double esrOhm;
   double initialVoltageVolt;
};

struct InductorProperties
{
   double inductanceHenry;
   double seriesResistanceOhm;
   double initialCurrentAmpere;
};

struct PotentiometerProperties
{
   double resistanceOhm;
   double wiperPosition;
};

struct VoltageSourceProperties
{
   double dcVoltageVolt;
   double acAmplitudeVolt;
   double acPhaseDeg;
   SourceWaveform transientWaveform;
   double sineOffsetVolt;
   double sineAmplitudeVolt;
   double sineFrequencyHz;
   double sinePhaseDeg;
   double sineDelaySec;
   double pulseInitialVolt;
   double pulseLevelVolt;
   double pulseDelaySec;
   double pulseRiseSec;
   double pulseFallSec;
   double pulseWidthSec;
   double pulsePeriodSec;
};

struct CurrentSourceProperties
{
   double dcCurrentAmpere;
   double acAmplitudeAmpere;
   double acPhaseDeg;
   SourceWaveform transientWaveform;
   double sineOffsetAmpere;
   double sineAmplitudeAmpere;
   double sineFrequencyHz;
   double sinePhaseDeg;
   double sineDelaySec;
   double pulseInitialAmpere;
   double pulseLevelAmpere;
   double pulseDelaySec;
   double pulseRiseSec;
   double pulseFallSec;
   double pulseWidthSec;
   double pulsePeriodSec;
};

struct DiodeProperties
{
   double forwardVoltageVolt;
   double forwardCurrentAmpere;
   double emissionCoefficient;
   double seriesResistanceOhm;
   double temperatureCelsius;
   double junctionCapacitanceFarad;
   double junctionPotentialVolt;
   double gradingCoefficient;
   double transitTimeSec;
};

struct LedProperties
{
   DiodeProperties diode;
   LedColor color;
};

struct BjtProperties
{
   double forwardVoltageVolt;
   double forwardCurrentAmpere;
   double emissionCoefficient;
   double temperatureCelsius;
   double forwardBeta;
   double reverseBeta;
   double earlyVoltageVolt;
   double baseResistanceOhm;
   double collectorResistanceOhm;
   double emitterResistanceOhm;
   double cjeFarad;
   double cjcFarad;
   double forwardTransitTimeSec;
   double reverseTransitTimeSec;
};

struct MosfetProperties
{
   double thresholdVoltageVolt;
   double kpAmperePerVoltSquared;
   double channelWidthMeter;
   double channelLengthMeter;
   double lambdaPerVolt;
   double cgsoFaradPerMeter;
   double cgdoFaradPerMeter;
};

struct RelayProperties
{
   double coilResistanceOhm;
   double coilInductanceHenry;
   double pullInCurrentAmpere;
   double dropOutCurrentAmpere;
   double operateDelaySec;
};

struct Timer555Properties
{
   double outputResistanceOhm;
   double propagationDelaySec;
};

struct ComparatorProperties
{
   double hysteresisVolt;
   double outputResistanceOhm;
   double propagationDelaySec;
};

struct EmptyProperties
{
   int32_t unused;
};

struct SwitchEvent
{
   double timeSec;
   SwitchState state;
};

using SwitchEventList = std::vector<SwitchEvent>;

struct SpstProperties
{
   SwitchState restState;
   ResistanceMode closedMode;
   double closedResistanceOhm;
   ResistanceMode openMode;
   double openResistanceOhm;
   SwitchEventList events;
};

struct SpdtProperties
{
   ThrowPosition restPosition;
   ResistanceMode closedMode;
   double closedResistanceOhm;
   ResistanceMode openMode;
   double openResistanceOhm;
};

struct PushbuttonProperties
{
   ContactForm contact;
   SwitchState mechanicalState;
   ResistanceMode closedMode;
   double closedResistanceOhm;
   ResistanceMode openMode;
   double openResistanceOhm;
   SwitchEventList events;
};

constexpr uint8_t DipChannelCount = 4;
using DipStateArray = std::array<SwitchState, DipChannelCount>;

struct DipSwitchProperties
{
   DipStateArray channelState;
   ResistanceMode closedMode;
   double closedResistanceOhm;
   ResistanceMode openMode;
   double openResistanceOhm;
};

struct SevenSegmentProperties
{
   SegmentCommon common;
   LedColor color;
   double forwardVoltageVolt;
   double forwardCurrentAmpere;
   double seriesResistanceOhm;
};

struct MeterProperties
{
   MeterMode mode;
   double burdenResistanceOhm;
   double burdenInductanceHenry;
   double inputResistanceOhm;
   double inputCapacitanceFarad;
};

struct ProbeProperties
{
   LogicFamily family;
   double supplyVoltageVolt;
   double vihVolt;
   double vilVolt;
};

struct LogicProperties
{
   LogicFamily family;
   double supplyVoltageVolt;
   double vohVolt;
   double volVolt;
   double vihVolt;
   double vilVolt;
   double propagationDelaySec;
   double riseTimeSec;
   double fallTimeSec;
   double outputResistanceOhm;
   OutputStage outputStage;
   uint8_t inputCount;
   Polarity enablePolarity;
   double lowThresholdVolt;
   double highThresholdVolt;
};

struct SequentialProperties
{
   LogicProperties logic;
   LogicLevel initialState;
   ClockEdge clockEdge;
   Polarity presetPolarity;
   Polarity clearPolarity;
   Polarity srPolarity;
   Polarity enablePolarity;
   double setupTimeSec;
   double holdTimeSec;
};

struct ClockProperties
{
   LogicProperties logic;
   double frequencyHz;
   double dutyCycle;
   double delaySec;
   LogicLevel startLevel;
};

struct MuxProperties
{
   LogicProperties logic;
   MuxWidth width;
};

struct DecoderProperties
{
   LogicProperties logic;
   Polarity outputPolarity;
   Polarity enablePolarity;
};

struct CounterProperties
{
   LogicProperties logic;
   Polarity clearPolarity;
   ClockEdge clockEdge;
   uint8_t initialValue;
};

struct ShiftRegisterProperties
{
   LogicProperties logic;
   Polarity clearPolarity;
   ClockEdge clockEdge;
   uint8_t initialValue;
};

struct PullProperties
{
   LogicProperties logic;
   double resistanceOhm;
};

struct AdcProperties
{
   LogicProperties logic;
   AdcConversion conversion;
   ClockEdge clockEdge;
};

struct DacProperties
{
   LogicProperties logic;
};

using ComponentProperties = std::variant<
   ResistorProperties,
   CapacitorProperties,
   InductorProperties,
   PotentiometerProperties,
   VoltageSourceProperties,
   CurrentSourceProperties,
   DiodeProperties,
   LedProperties,
   BjtProperties,
   MosfetProperties,
   RelayProperties,
   Timer555Properties,
   ComparatorProperties,
   EmptyProperties,
   SpstProperties,
   SpdtProperties,
   PushbuttonProperties,
   DipSwitchProperties,
   SevenSegmentProperties,
   MeterProperties,
   ProbeProperties,
   LogicProperties,
   SequentialProperties,
   ClockProperties,
   MuxProperties,
   DecoderProperties,
   CounterProperties,
   ShiftRegisterProperties,
   PullProperties,
   AdcProperties,
   DacProperties>;

/*!
 * \brief A placed component. The position is the rotation anchor.
 */
struct Component
{
   uint64_t id;
   ComponentKind kind;
   std::string name;
   GridPoint position;
   int32_t rotationDegrees;
   ComponentProperties properties;
};

using ComponentList = std::vector<Component>;

/*!
 * \brief Numeric fields edited from the property panel and the file.
 */
enum class NumericProperty : int16_t
{
   ResistanceOhm = 0,
   CapacitanceFarad,
   EsrOhm,
   InitialVoltageVolt,
   InductanceHenry,
   SeriesResistanceOhm,
   InitialCurrentAmpere,
   WiperPosition,
   DcVoltageVolt,
   AcAmplitudeVolt,
   AcPhaseDeg,
   SineOffset,
   SineAmplitude,
   SineFrequencyHz,
   SinePhaseDeg,
   SineDelaySec,
   PulseInitial,
   PulseLevel,
   PulseDelaySec,
   PulseRiseSec,
   PulseFallSec,
   PulseWidthSec,
   PulsePeriodSec,
   DcCurrentAmpere,
   AcAmplitudeAmpere,
   ForwardVoltageVolt,
   ForwardCurrentAmpere,
   EmissionCoefficient,
   TemperatureCelsius,
   JunctionCapacitanceFarad,
   JunctionPotentialVolt,
   GradingCoefficient,
   TransitTimeSec,
   ForwardBeta,
   ReverseBeta,
   EarlyVoltageVolt,
   BaseResistanceOhm,
   CollectorResistanceOhm,
   EmitterResistanceOhm,
   CjeFarad,
   CjcFarad,
   ForwardTransitTimeSec,
   ReverseTransitTimeSec,
   ThresholdVoltageVolt,
   Kp,
   ChannelWidthMeter,
   ChannelLengthMeter,
   LambdaPerVolt,
   Cgso,
   Cgdo,
   CoilResistanceOhm,
   CoilInductanceHenry,
   PullInCurrentAmpere,
   DropOutCurrentAmpere,
   OperateDelaySec,
   OutputResistanceOhm,
   PropagationDelaySec,
   HysteresisVolt,
   SupplyVoltageVolt,
   VohVolt,
   VolVolt,
   VihVolt,
   VilVolt,
   RiseTimeSec,
   FallTimeSec,
   InputCount,
   LowThresholdVolt,
   HighThresholdVolt,
   SetupTimeSec,
   HoldTimeSec,
   FrequencyHz,
   DutyCycle,
   DelaySec,
   ClosedResistanceOhm,
   OpenResistanceOhm,
   SegmentResistanceOhm,
   BurdenResistanceOhm,
   BurdenInductanceHenry,
   InputResistanceOhm,
   InputCapacitanceFarad,
   InitialValue
};

/*!
 * \brief Enumeration fields edited from the property panel and the file.
 */
enum class EnumProperty : int16_t
{
   TransientWaveform = 0,
   LogicFamily,
   OutputStage,
   InitialState,
   ClockEdge,
   PresetPolarity,
   ClearPolarity,
   SrPolarity,
   EnablePolarity,
   OutputPolarity,
   StartLevel,
   MeterMode,
   ClosedMode,
   OpenMode,
   RestState,
   RestThrow,
   ContactForm,
   MechanicalState,
   LedColor,
   SegmentCommon,
   AdcConversion,
   MuxWidth
};

/*!
 * \brief English display name of a kind.
 *
 * \param[in] kind Component kind.
 */
std::string KindLabel(ComponentKind kind);

/*!
 * \brief Stable file key of a kind, such as resistor or dFlipFlop.
 *
 * \param[in] kind Component kind.
 */
std::string KindKey(ComponentKind kind);

/*!
 * \brief Parses a file key into a kind.
 *
 * \param[in] key Key from a circuit file.
 * \param[out] pKind Kind when the result is Ok.
 */
Result KindFromKey(std::string_view key, ComponentKind* pKind);

/*!
 * \brief Name prefix such as R, C, or U.
 *
 * \param[in] kind Component kind.
 */
std::string KindPrefix(ComponentKind kind);

/*!
 * \brief True when the kind contributes an analog stamp or needs ground.
 *
 * \param[in] kind Component kind.
 */
bool KindIsAnalog(ComponentKind kind);

/*!
 * \brief Number of pins for the current properties.
 *
 * \param[in] component Component to measure.
 */
uint8_t PinCount(const Component& component);

/*!
 * \brief Short pin name such as A, CLK, or Q.
 *
 * \param[in] component Component that owns the pin.
 * \param[in] pinIndex Zero-based pin index.
 */
std::string PinName(const Component& component, uint8_t pinIndex);

/*!
 * \brief Grid position of one pin after rotation.
 *
 * \param[in] component Component that owns the pin.
 * \param[in] pinIndex Zero-based pin index.
 */
GridPoint PinPosition(const Component& component, uint8_t pinIndex);

/*!
 * \brief Properties and a generated name for a newly placed kind.
 *
 * \param[in] kind Kind to create.
 * \param[in] position Anchor on the grid.
 */
Component CreateComponent(ComponentKind kind, GridPoint position);

/*!
 * \brief Whether applying a logic family also replaces the supply voltage.
 */
enum class FamilySupplyReset : int8_t
{
   Keep = 0,
   FamilyDefault = 1
};

/*!
 * \brief Writes the CMOS or TTL preset into logic properties.
 *
 * \param[in,out] pLogic Logic properties. Custom leaves the numbers unchanged.
 * \param[in] resetSupply FamilyDefault replaces the supply with the family default.
 */
void ApplyLogicFamily(LogicProperties* pLogic, FamilySupplyReset resetSupply);

/*!
 * \brief Reads a numeric property.
 *
 * \param[in] component Component to read.
 * \param[in] property Property id.
 * \param[out] pValue Value when the result is Ok.
 */
Result ReadNumeric(const Component& component, NumericProperty property, double* pValue);

/*!
 * \brief Writes a numeric property when it belongs to the component and is legal.
 *
 * \param[in,out] pComponent Component to edit.
 * \param[in] property Property id.
 * \param[in] value New value.
 */
Result WriteNumeric(Component* pComponent, NumericProperty property, double value);

/*!
 * \brief Reads an enumeration property as an integer code.
 *
 * \param[in] component Component to read.
 * \param[in] property Property id.
 * \param[out] pCode Code when the result is Ok.
 */
Result ReadEnumCode(const Component& component, EnumProperty property, int32_t* pCode);

/*!
 * \brief Writes an enumeration property from an integer code.
 *
 * \param[in,out] pComponent Component to edit.
 * \param[in] property Property id.
 * \param[in] code New code.
 */
Result WriteEnumCode(Component* pComponent, EnumProperty property, int32_t code);

/*!
 * \brief English label for one code of an enumeration property.
 *
 * \param[in] property Property id.
 * \param[in] code Enumeration code.
 */
std::string EnumCodeLabel(EnumProperty property, int32_t code);

/*!
 * \brief How many codes an enumeration property has.
 *
 * \param[in] property Property id.
 */
int32_t EnumCodeCount(EnumProperty property);

/*!
 * \brief Thermal voltage kT/q at a Celsius temperature.
 *
 * \param[in] temperatureCelsius Device temperature.
 */
double ThermalVoltage(double temperatureCelsius);

/*!
 * \brief Shockley saturation current implied by a forward knee.
 *
 * \param[in] forwardVoltageVolt Junction voltage at the stated current.
 * \param[in] forwardCurrentAmpere Current that defines the knee.
 * \param[in] emissionCoefficient Ideality factor.
 * \param[in] temperatureCelsius Device temperature.
 */
double SaturationCurrent(double forwardVoltageVolt, double forwardCurrentAmpere, double emissionCoefficient, double temperatureCelsius);

} // namespace Analyzer

#endif // COMPONENT_H
