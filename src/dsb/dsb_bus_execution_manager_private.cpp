#define NOMINMAX
#include "dsb/bus/execution_manager_private.hpp"

#include <cassert>
#include <typeinfo>
#include <utility>

#include "boost/numeric/conversion/cast.hpp"

#include "dsb/bus/execution_state.hpp"
#include "dsb/log.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace bus
{


ExecutionManagerPrivate::ExecutionManagerPrivate(
        dsb::net::Reactor& reactor_,
        const std::string& executionName,
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint maxTime)
    : reactor(reactor_),
      slaveSetup(startTime, maxTime, executionName),
      lastSlaveID(0),
      slaves(),
      m_state(), // created below
      m_operationCount(0),
      m_allSlaveOpsCompleteHandler(),
      m_currentStepID(-1)
{
    SwapState(std::make_unique<ReadyExecutionState>());
}


ExecutionManagerPrivate::~ExecutionManagerPrivate()
{
    // For the moment, the destructor does nothing.  We just need it to be able
    // to use std::unique_ptr (for m_state) with an undefined type (i.e.,
    // ExecutionState) in the header.
}


void ExecutionManagerPrivate::Reconstitute(
    const std::vector<AddedSlave>& slavesToAdd,
    std::chrono::milliseconds commTimeout,
    ExecutionManager::ReconstituteHandler onComplete,
    ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
{
    if (std::numeric_limits<dsb::model::SlaveID>::max() - lastSlaveID
            < (long long) slavesToAdd.size()) {
        throw std::length_error{"Maximum number of slaves reached"};
    }
    DSB_INPUT_CHECK(onComplete);
    DSB_INPUT_CHECK(onSlaveComplete);
    m_state->Reconstitute(
        *this, slavesToAdd, commTimeout,
        std::move(onComplete), std::move(onSlaveComplete));
}


void ExecutionManagerPrivate::Reconfigure(
    const std::vector<SlaveConfig>& slaveConfigs,
    std::chrono::milliseconds commTimeout,
    ExecutionManager::ReconfigureHandler onComplete,
    ExecutionManager::SlaveReconfigureHandler onSlaveComplete)
{
    m_state->Reconfigure(
        *this, slaveConfigs, commTimeout,
        std::move(onComplete), std::move(onSlaveComplete));
}


void ExecutionManagerPrivate::Step(
    dsb::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    ExecutionManager::StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
{
    m_state->Step(
        *this,
        stepSize,
        timeout,
        std::move(onComplete),
        std::move(onSlaveStepComplete));
}


void ExecutionManagerPrivate::AcceptStep(
    std::chrono::milliseconds timeout,
    ExecutionManager::AcceptStepHandler onComplete,
    ExecutionManager::SlaveAcceptStepHandler onSlaveStepComplete)
{
    m_state->AcceptStep(
        *this,
        timeout,
        std::move(onComplete),
        std::move(onSlaveStepComplete));
}


void ExecutionManagerPrivate::Terminate()
{
    m_state->Terminate(*this);
}


void ExecutionManagerPrivate::DoTerminate()
{
    for (auto it = begin(slaves); it != end(slaves); ++it) {
        if (it->second.slave->State() != SLAVE_NOT_CONNECTED) {
            it->second.slave->Terminate();
        }
    }
    SwapState(std::make_unique<TerminatedExecutionState>());
    assert(m_operationCount == 0);
    assert(!m_allSlaveOpsCompleteHandler);
}


dsb::model::StepID ExecutionManagerPrivate::NextStepID()
{
    return ++m_currentStepID;
}


dsb::model::TimePoint ExecutionManagerPrivate::CurrentSimTime() const
{
    return slaveSetup.startTime;
}


void ExecutionManagerPrivate::AdvanceSimTime(dsb::model::TimeDuration delta)
{
    assert(delta > 0.0);
    slaveSetup.startTime += delta;
}


void ExecutionManagerPrivate::SlaveOpStarted() DSB_NOEXCEPT
{
    assert(m_operationCount >= 0);
    ++m_operationCount;
}


void ExecutionManagerPrivate::SlaveOpComplete()
{
    assert(m_operationCount > 0);
    --m_operationCount;
    if (m_operationCount == 0 && m_allSlaveOpsCompleteHandler) {
        dsb::util::LastCall(m_allSlaveOpsCompleteHandler, std::error_code());
    }
}


void ExecutionManagerPrivate::WhenAllSlaveOpsComplete(
    AllSlaveOpsCompleteHandler handler)
{
    assert(!!handler);
    assert(m_operationCount >= 0);
    assert(!m_allSlaveOpsCompleteHandler);
    if (m_operationCount == 0) {
        handler(std::error_code());
    } else {
        m_allSlaveOpsCompleteHandler = std::move(handler);
    }
}


std::unique_ptr<ExecutionState> ExecutionManagerPrivate::SwapState(
    std::unique_ptr<ExecutionState> next)
{
    AbortSlaveOpWaiting();
    DSB_LOG_TRACE(boost::format("ExecutionManager state change: %s -> %s")
        % (m_state ? typeid(*m_state).name() : "none")
        % (next    ? typeid(*next).name()    : "none"));
    std::swap(m_state, next);
    m_state->StateEntered(*this);
    return next;
}


void ExecutionManagerPrivate::AbortSlaveOpWaiting() DSB_NOEXCEPT
{
    if (m_allSlaveOpsCompleteHandler) {
        dsb::util::LastCall(m_allSlaveOpsCompleteHandler,
            make_error_code(dsb::error::generic_error::aborted));
    }
}


// =============================================================================
// ExecutionManagerPrivate::Slave
// =============================================================================

ExecutionManagerPrivate::Slave::Slave(
    std::unique_ptr<dsb::bus::SlaveController> slave_,
    dsb::net::SlaveLocator locator_,
    const dsb::model::SlaveDescription& description_)
    : slave(std::move(slave_))
    , locator(std::move(locator_))
    , description(description_)
{ }


}} // namespace
