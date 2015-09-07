#include "dsb/protocol/glue.hpp"

#include <cassert>


// TODO: Use a lookup table or something in the two following functions?
// (A job for the "X macro"?  http://www.drdobbs.com/cpp/the-x-macro/228700289)

dsbproto::model::VariableDefinition dsb::protocol::ToProto(
    const dsb::model::VariableDescription& dsbVariable)
{
    dsbproto::model::VariableDefinition protoVariable;
    protoVariable.set_id(dsbVariable.ID());
    protoVariable.set_name(dsbVariable.Name());
    switch (dsbVariable.DataType()) {
        case dsb::model::REAL_DATATYPE:
            protoVariable.set_data_type(dsbproto::model::REAL);
            break;
        case dsb::model::INTEGER_DATATYPE:
            protoVariable.set_data_type(dsbproto::model::INTEGER);
            break;
        case dsb::model::BOOLEAN_DATATYPE:
            protoVariable.set_data_type(dsbproto::model::BOOLEAN);
            break;
        case dsb::model::STRING_DATATYPE:
            protoVariable.set_data_type(dsbproto::model::STRING);
            break;
        default:
            assert (!"Unknown data type");
    }
    switch (dsbVariable.Causality()) {
        case dsb::model::PARAMETER_CAUSALITY:
            protoVariable.set_causality(dsbproto::model::PARAMETER);
            break;
        case dsb::model::CALCULATED_PARAMETER_CAUSALITY:
            protoVariable.set_causality(dsbproto::model::CALCULATED_PARAMETER);
            break;
        case dsb::model::INPUT_CAUSALITY:
            protoVariable.set_causality(dsbproto::model::INPUT);
            break;
        case dsb::model::OUTPUT_CAUSALITY:
            protoVariable.set_causality(dsbproto::model::OUTPUT);
            break;
        case dsb::model::LOCAL_CAUSALITY:
            protoVariable.set_causality(dsbproto::model::LOCAL);
            break;
        default:
            assert (!"Unknown causality");
    }
    switch (dsbVariable.Variability()) {
        case dsb::model::CONSTANT_VARIABILITY:
            protoVariable.set_variability(dsbproto::model::CONSTANT);
            break;
        case dsb::model::FIXED_VARIABILITY:
            protoVariable.set_variability(dsbproto::model::FIXED);
            break;
        case dsb::model::TUNABLE_VARIABILITY:
            protoVariable.set_variability(dsbproto::model::TUNABLE);
            break;
        case dsb::model::DISCRETE_VARIABILITY:
            protoVariable.set_variability(dsbproto::model::DISCRETE);
            break;
        case dsb::model::CONTINUOUS_VARIABILITY:
            protoVariable.set_variability(dsbproto::model::CONTINUOUS);
            break;
        default:
            assert (!"Unknown variability");
    }
    return protoVariable;
}


dsb::model::VariableDescription dsb::protocol::FromProto(
    const dsbproto::model::VariableDefinition& protoVariable)
{
    dsb::model::DataType dataType;
    switch (protoVariable.data_type()) {
        case dsbproto::model::REAL:
            dataType = dsb::model::REAL_DATATYPE;
            break;
        case dsbproto::model::INTEGER:
            dataType = dsb::model::INTEGER_DATATYPE;
            break;
        case dsbproto::model::BOOLEAN:
            dataType = dsb::model::BOOLEAN_DATATYPE;
            break;
        case dsbproto::model::STRING:
            dataType = dsb::model::STRING_DATATYPE;
            break;
        default:
            assert (!"Unknown data type");
    }
    dsb::model::Causality causality;
    switch (protoVariable.causality()) {
        case dsbproto::model::PARAMETER:
            causality = dsb::model::PARAMETER_CAUSALITY;
            break;
        case dsbproto::model::CALCULATED_PARAMETER:
            causality = dsb::model::CALCULATED_PARAMETER_CAUSALITY;
            break;
        case dsbproto::model::INPUT:
            causality = dsb::model::INPUT_CAUSALITY;
            break;
        case dsbproto::model::OUTPUT:
            causality = dsb::model::OUTPUT_CAUSALITY;
            break;
        case dsbproto::model::LOCAL:
            causality = dsb::model::LOCAL_CAUSALITY;
            break;
        default:
            assert (!"Unknown causality");
    }
    dsb::model::Variability variability;
    switch (protoVariable.variability()) {
        case dsbproto::model::CONSTANT:
            variability = dsb::model::CONSTANT_VARIABILITY;
            break;
        case dsbproto::model::FIXED:
            variability = dsb::model::FIXED_VARIABILITY;
            break;
        case dsbproto::model::TUNABLE:
            variability = dsb::model::TUNABLE_VARIABILITY;
            break;
        case dsbproto::model::DISCRETE:
            variability = dsb::model::DISCRETE_VARIABILITY;
            break;
        case dsbproto::model::CONTINUOUS:
            variability = dsb::model::CONTINUOUS_VARIABILITY;
            break;
        default:
            assert (!"Unknown variability");
    }
    return dsb::model::VariableDescription(
        protoVariable.id(),
        protoVariable.name(),
        dataType,
        causality,
        variability);
}



dsbproto::net::ExecutionLocator dsb::protocol::ToProto(
    const dsb::net::ExecutionLocator& executionLocator)
{
    dsbproto::net::ExecutionLocator el;
    el.set_master_endpoint(executionLocator.MasterEndpoint());
    el.set_slave_endpoint(executionLocator.SlaveEndpoint());
    el.set_variable_pub_endpoint(executionLocator.VariablePubEndpoint());
    el.set_variable_sub_endpoint(executionLocator.VariableSubEndpoint());
    el.set_execution_name(executionLocator.ExecName());
    el.set_comm_timeout_seconds(executionLocator.CommTimeout().count());
    return el;
}


dsb::net::ExecutionLocator dsb::protocol::FromProto(
    const dsbproto::net::ExecutionLocator& executionLocator)
{
    return dsb::net::ExecutionLocator(
        executionLocator.master_endpoint(),
        executionLocator.slave_endpoint(),
        executionLocator.variable_pub_endpoint(),
        executionLocator.variable_sub_endpoint(),
        "",
        executionLocator.execution_name(),
        boost::chrono::seconds(executionLocator.comm_timeout_seconds()));
}


namespace
{
    class ScalarValueConverterVisitor : public boost::static_visitor<>
    {
    public:
        explicit ScalarValueConverterVisitor(dsbproto::model::ScalarValue& value)
            : m_value(&value) { }
        void operator()(const double& value)      const { m_value->set_real_value(value); }
        void operator()(const int& value)         const { m_value->set_integer_value(value); }
        void operator()(const bool& value)        const { m_value->set_boolean_value(value); }
        void operator()(const std::string& value) const { m_value->set_string_value(value); }
    private:
        dsbproto::model::ScalarValue* m_value;
    };
}

void dsb::protocol::ConvertToProto(
    const dsb::model::ScalarValue& source,
    dsbproto::model::ScalarValue& target)
{
    target.Clear();
    boost::apply_visitor(ScalarValueConverterVisitor(target), source);
}


dsb::model::ScalarValue dsb::protocol::FromProto(
    const dsbproto::model::ScalarValue& source)
{
    if (source.has_real_value())            return source.real_value();
    else if (source.has_integer_value())    return source.integer_value();
    else if (source.has_boolean_value())    return source.boolean_value();
    else if (source.has_string_value())     return source.string_value();
    else {
        assert (!"Corrupt or empty ScalarValue protocol buffer");
        return dsb::model::ScalarValue();
    }
}


void dsb::protocol::ConvertToProto(
    const dsb::model::Variable& source,
    dsbproto::model::Variable& target)
{
    target.Clear();
    target.set_slave_id(source.Slave());
    target.set_variable_id(source.ID());
}


dsb::model::Variable dsb::protocol::FromProto(const dsbproto::model::Variable& source)
{
    return dsb::model::Variable(source.slave_id(), source.variable_id());
}


void dsb::protocol::ConvertToProto(
    const dsb::net::SlaveLocator& source,
    dsbproto::net::SlaveLocator& target)
{
    target.Clear();
    target.set_endpoint(source.Endpoint());
    if (source.HasIdentity()) target.set_identity(source.Identity());
}


dsb::net::SlaveLocator dsb::protocol::FromProto(
    const dsbproto::net::SlaveLocator& source)
{
    return dsb::net::SlaveLocator(source.endpoint(), source.identity());
}
