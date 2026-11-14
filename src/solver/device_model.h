/*!
 * \file device_model.h
 * \brief Linearized diode, BJT, and MOSFET equations.
 *
 * Positive diode current flows from anode to cathode.
 * BJT and MOSFET currents are into the device terminals.
 * A voltage source current is the current leaving the positive terminal toward the negative terminal.
 */

#ifndef DEVICE_MODEL_H
#define DEVICE_MODEL_H

#include "core/component.h"

namespace Analyzer
{

struct DiodeLinearization
{
   double conductanceSiemens;
   double currentAmpere;
};

struct BjtLinearization
{
   double baseCurrent;
   double collectorCurrent;
   double baseEmitterConductance;
   double baseCollectorConductance;
   double collectorEmitterTransconductance;
   double collectorCollectorConductance;
};

struct MosfetLinearization
{
   double drainCurrent;
   double gateSourceTransconductance;
   double drainSourceConductance;
};

/*!
 * \brief Shockley diode at one junction voltage.
 *
 * \param[in] properties Diode parameters.
 * \param[in] junctionVoltageVolt Anode minus cathode across the junction.
 */
DiodeLinearization EvaluateDiode(const DiodeProperties& properties, double junctionVoltageVolt);

/*!
 * \brief Ebers-Moll transport model.
 *
 * \param[in] properties BJT parameters.
 * \param[in] baseEmitterVolt Base minus emitter.
 * \param[in] baseCollectorVolt Base minus collector.
 */
BjtLinearization EvaluateBjt(const BjtProperties& properties, double baseEmitterVolt, double baseCollectorVolt);

/*!
 * \brief Level-1 MOSFET. The drain current is into the drain terminal.
 *
 * \param[in] channelKind Nmos or Pmos.
 * \param[in] properties MOSFET parameters.
 * \param[in] gateSourceVolt Gate minus source.
 * \param[in] drainSourceVolt Drain minus source.
 */
MosfetLinearization EvaluateMosfet(ComponentKind channelKind, const MosfetProperties& properties, double gateSourceVolt, double drainSourceVolt);

/*!
 * \brief Limits a junction step to a few thermal voltages.
 *
 * \param[in] proposedVolt Unbounded Newton voltage.
 * \param[in] previousVolt Previous accepted voltage.
 * \param[in] thermalVolt n*Vt.
 */
double LimitJunctionStep(double proposedVolt, double previousVolt, double thermalVolt);

} // namespace Analyzer

#endif // DEVICE_MODEL_H
