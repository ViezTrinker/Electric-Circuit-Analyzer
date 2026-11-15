/*!
 * \file analysis_worker.h
 * \brief Background thread for DC, AC, and transient runs.
 */

#ifndef ANALYSIS_WORKER_H
#define ANALYSIS_WORKER_H

#include <QThread>

#include "core/cancel_token.h"
#include "core/circuit.h"
#include "solver/analysis_result.h"

namespace Analyzer
{

/*!
 * \brief Copies a circuit and solves it off the window thread.
 */
class AnalysisWorker : public QThread
{
public:
   /*!
    * \brief Creates an idle worker.
    *
    * \param[in] pParent Parent object. May be null.
    */
   explicit AnalysisWorker(QObject* pParent);

   /*!
    * \brief Starts one analysis. Fails when a run is already active.
    *
    * \param[in] circuit Schematic copied for the run.
    * \param[in] kind DC, AC, or transient.
    */
   Result Start(const Circuit& circuit, AnalysisKind kind);

   /*!
    * \brief Asks the active solver to stop.
    */
   void RequestCancel(void);

   /*!
    * \brief Kind passed to the latest Start.
    */
   AnalysisKind Kind(void) const;

   /*!
    * \brief DC result from the latest DC run.
    */
   const DcResult& Dc(void) const;

   /*!
    * \brief AC result from the latest AC run.
    */
   const AcResult& Ac(void) const;

   /*!
    * \brief Transient result from the latest transient run.
    */
   const TransientResult& Transient(void) const;

protected:
   /*!
    * \brief Solves the copied circuit.
    */
   void run(void) override;

private:
   Circuit _circuit;
   AnalysisKind _kind;
   CancelToken _cancel;
   DcResult _dc;
   AcResult _ac;
   TransientResult _transient;
};

} // namespace Analyzer

#endif // ANALYSIS_WORKER_H
