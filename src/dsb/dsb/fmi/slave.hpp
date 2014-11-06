#ifndef DSB_FMI_SLAVE_HPP
#define DSB_FMI_SLAVE_HPP

#include <memory>
#include <ostream>
#include <string>

#include "fmilibcpp/fmi1/Fmu.hpp"
#include "dsb/execution/slave.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace fmi
{

    
class FmiSlaveInstance : public dsb::execution::ISlaveInstance
{
public:
    FmiSlaveInstance(
        const std::string& fmuPath,
        std::ostream* outputStream);

    ~FmiSlaveInstance();

    bool Setup(dsb::model::TimePoint startTime, dsb::model::TimePoint stopTime) override;
    size_t VariableCount() const override;
    dsb::model::Variable Variable(size_t index) const override;
    double GetRealVariable(dsb::model::VariableID variable) const override;
    int GetIntegerVariable(dsb::model::VariableID variable) const override;
    bool GetBooleanVariable(dsb::model::VariableID variable) const override;
    std::string GetStringVariable(dsb::model::VariableID variable) const override;
    void SetRealVariable(dsb::model::VariableID variable, double value) override;
    void SetIntegerVariable(dsb::model::VariableID variable, int value) override;
    void SetBooleanVariable(dsb::model::VariableID variable, bool value) override;
    void SetStringVariable(dsb::model::VariableID variable, const std::string& value) override;
    bool DoStep(dsb::model::TimePoint currentT, dsb::model::TimeDuration deltaT) override;

private:
    dsb::util::TempDir m_fmuDir;
    std::shared_ptr<fmilib::fmi1::Fmu> m_fmu;
    bool m_initialized;
    dsb::model::TimePoint m_startTime, m_stopTime;

    std::vector<fmi1_value_reference_t> m_fmiValueRefs;
    std::vector<dsb::model::Variable> m_variables;

    std::ostream* m_outputStream;
};


}}      // namespace
#endif  // header guard
