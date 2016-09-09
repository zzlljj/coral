#include "dsb/slave/logging.hpp"

#include <cassert>
#include <cerrno>
#include <ios>
#include <stdexcept>

#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace slave
{


LoggingInstance::LoggingInstance(
    std::shared_ptr<Instance> instance,
    const std::string& outputFilePrefix)
    : m_instance{instance}
    , m_outputFilePrefix(outputFilePrefix)
{
    if (m_outputFilePrefix.empty()) m_outputFilePrefix = "./";
}


bool LoggingInstance::Setup(
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime,
    const std::string& executionName,
    const std::string& slaveName)
{
    const auto ret =
        m_instance->Setup(startTime, stopTime, executionName, slaveName);

    auto outputFileName = m_outputFilePrefix;
    if (executionName.empty()) {
        outputFileName += dsb::util::Timestamp();
    } else {
        outputFileName += executionName;
    }
    outputFileName += '_';
    if (slaveName.empty()) {
        outputFileName += TypeDescription().Name() + '_'
            + dsb::util::RandomString(6, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    } else {
        outputFileName += slaveName;
    }
    outputFileName += ".csv";

    DSB_LOG_TRACE("LoggingInstance: Opening " + outputFileName);
    m_outputStream.open(
        outputFileName,
        std::ios_base::out | std::ios_base::trunc
#ifdef _MSC_VER
        , _SH_DENYWR // Don't let other processes/threads write to the file
#endif
        );
    if (!m_outputStream.is_open()) {
        const int e = errno;
        throw std::runtime_error(dsb::error::ErrnoMessage(
            "Error opening file \"" + outputFileName + "\" for writing",
            e));
    }

    m_outputStream << "Time";
    for (const auto& var : TypeDescription().Variables()) {
        m_outputStream << "," << var.Name();
    }
    m_outputStream << std::endl;

    return ret;
}


const dsb::model::SlaveTypeDescription& LoggingInstance::TypeDescription() const
{
    return m_instance->TypeDescription();
}


double LoggingInstance::GetRealVariable(dsb::model::VariableID varRef) const
{
    return m_instance->GetRealVariable(varRef);
}


int LoggingInstance::GetIntegerVariable(dsb::model::VariableID varRef) const
{
    return m_instance->GetIntegerVariable(varRef);
}


bool LoggingInstance::GetBooleanVariable(dsb::model::VariableID varRef) const
{
    return m_instance->GetBooleanVariable(varRef);
}


std::string LoggingInstance::GetStringVariable(dsb::model::VariableID varRef) const
{
    return m_instance->GetStringVariable(varRef);
}


void LoggingInstance::SetRealVariable(dsb::model::VariableID varRef, double value)
{
    m_instance->SetRealVariable(varRef, value);
}


void LoggingInstance::SetIntegerVariable(dsb::model::VariableID varRef, int value)
{
    m_instance->SetIntegerVariable(varRef, value);
}


void LoggingInstance::SetBooleanVariable(dsb::model::VariableID varRef, bool value)
{
    m_instance->SetBooleanVariable(varRef, value);
}


void LoggingInstance::SetStringVariable(dsb::model::VariableID varRef, const std::string& value)
{
    m_instance->SetStringVariable(varRef, value);
}


namespace
{
    void PrintVariable(
        std::ostream& out,
        const dsb::model::VariableDescription& varInfo,
        Instance& slaveInstance)
    {
        out << ",";
        switch (varInfo.DataType()) {
            case dsb::model::REAL_DATATYPE:
                out << slaveInstance.GetRealVariable(varInfo.ID());
                break;
            case dsb::model::INTEGER_DATATYPE:
                out << slaveInstance.GetIntegerVariable(varInfo.ID());
                break;
            case dsb::model::BOOLEAN_DATATYPE:
                out << slaveInstance.GetBooleanVariable(varInfo.ID());
                break;
            case dsb::model::STRING_DATATYPE:
                out << slaveInstance.GetStringVariable(varInfo.ID());
                break;
            default:
                assert (false);
        }
    }
}


bool LoggingInstance::DoStep(
    dsb::model::TimePoint currentT,
    dsb::model::TimeDuration deltaT)
{
    const auto ret = m_instance->DoStep(currentT, deltaT);

    m_outputStream << (currentT + deltaT);
    for (const auto& var : TypeDescription().Variables()) {
        PrintVariable(m_outputStream, var, *this);
    }
    m_outputStream << std::endl;

    return ret;
}


}} // namespace