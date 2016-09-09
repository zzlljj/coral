/**
\file
\brief Defines the dsb::provider::SlaveCreator interface and related functionality.
*/
#ifndef DSB_PROVIDER_SLAVE_CREATOR_HPP_INCLUDED
#define DSB_PROVIDER_SLAVE_CREATOR_HPP_INCLUDED

#include <chrono>
#include <string>

#include "dsb/model.hpp"
#include "dsb/net.hpp"


namespace dsb
{
namespace provider
{


/// An interface for classes that create slaves of a specific type.
class SlaveCreator
{
public:
    /// A description of this slave type.
    virtual const dsb::model::SlaveTypeDescription& Description() const = 0;

    /**
    \brief  Creates a new instance of this slave type.

    This function must report whether a slave was successfully instantiated.
    For example, the slave may represent a particular piece of hardware (e.g.
    a human interface device), of which there is only one.  The function would
    then return `false` if multiple instantiations are attempted.

    If the function returns `true`, it must also update `slaveLocator` with
    information about the new slave.  `slaveLocator.Endpoint()` may then have
    one of three forms:

      1. "Normal", i.e. `transport://address`
      2. Empty, which means that the slave is accessible through the same
         endpoint as the slave provider (typically a proxy), except of course
         with a different identity.
      3. Only a port specification starting with a colon, e.g. `:12345`.
         This may be used if the slave provider is bound to a TCP endpoint,
         and the slave is accessible on the same hostname but with a different
         port number.

    If the function returns `false`, InstantiationFailureDescription() must
    return a textual description of the reasons for this.  `slaveLocator` must
    then be left untouched.

    \param [in] timeout
        How long the master will wait for the slave to start up.  If possible,
        instantiation should be aborted and considered "failed" after this
        time has passed.
    \param [out] slaveLocator
        An object that describes how to connect to the slave.  See the list
        above for different endpoint formats.

    \returns `true` if a slave was successfully instantiated, `false` otherwise.
    */
    virtual bool Instantiate(
        std::chrono::milliseconds timeout,
        dsb::net::SlaveLocator& slaveLocator) = 0;

    /**
    \brief  A textual description of why a previous Instantiate() call failed.

    This function is only called if Instantiate() has returned `false`.
    */
    virtual std::string InstantiationFailureDescription() const = 0;

    // Virtual destructor to allow deletion through base class reference.
    virtual ~SlaveCreator() { }
};


}}      // namespace
#endif  // header guard
