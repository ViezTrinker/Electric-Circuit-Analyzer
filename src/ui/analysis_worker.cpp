/*!
 * \file analysis_worker.cpp
 * \brief Runs one analysis away from the window thread.
 */

#include "analysis_worker.h"

#include "solver/simulator.h"

namespace Analyzer
{

AnalysisWorker::AnalysisWorker(QObject* pParent)
   : QThread(pParent)
   , _kind(AnalysisKind::DcOperatingPoint)
{
}

Result AnalysisWorker::Start(const Circuit& circuit, AnalysisKind kind)
{
   if (isRunning())
   {
      return Result::Error;
   }
   _circuit = circuit;
   _kind = kind;
   _cancel.Reset();
   start();
   return Result::Ok;
}

void AnalysisWorker::RequestCancel(void)
{
   _cancel.Request();
}

AnalysisKind AnalysisWorker::Kind(void) const
{
   return _kind;
}

const DcResult& AnalysisWorker::Dc(void) const
{
   return _dc;
}

const AcResult& AnalysisWorker::Ac(void) const
{
   return _ac;
}

const TransientResult& AnalysisWorker::Transient(void) const
{
   return _transient;
}

void AnalysisWorker::run(void)
{
   if (_kind == AnalysisKind::AcSweep)
   {
      _ac = SolveAc(_circuit, &_cancel);
      return;
   }
   if (_kind == AnalysisKind::Transient)
   {
      _transient = SolveTransient(_circuit, &_cancel);
      return;
   }
   _dc = SolveDc(_circuit, &_cancel);
}

} // namespace Analyzer
