#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/lexical_cast.hpp"

#include "zmq.hpp"

#include "dsb/fmi/fmu.hpp"
#include "dsb/fmi/importer.hpp"
#include "dsb/log.hpp"
#include "dsb/net/zmqx.hpp"
#include "dsb/provider.hpp"
#include "dsb/util.hpp"
#include "dsb/util/console.hpp"


struct DSBSlaveCreator : public dsb::provider::SlaveCreator
{
public:
    DSBSlaveCreator(
        dsb::fmi::Importer& importer,
        const boost::filesystem::path& fmuPath,
        const std::string& networkInterface,
        const std::string& slaveExe,
        std::chrono::seconds commTimeout,
        const std::string& outputDir)
        : m_fmuPath{fmuPath}
        , m_fmu{importer.Import(fmuPath)}
        , m_networkInterface(networkInterface)
        , m_slaveExe(slaveExe)
        , m_commTimeout{commTimeout}
        , m_outputDir(outputDir.empty() ? "." : outputDir)
    {
    }

    const dsb::model::SlaveTypeDescription& Description() const override
    {
        return m_fmu->Description();
    }

    bool Instantiate(
        std::chrono::milliseconds timeout,
        dsb::net::SlaveLocator& slaveLocator) override
    {
        m_instantiationFailureDescription.clear();
        try {
            auto slaveStatusSocket = zmq::socket_t(dsb::net::zmqx::GlobalContext(), ZMQ_PULL);
            const auto slaveStatusPort = dsb::net::zmqx::BindToEphemeralPort(slaveStatusSocket);
            const auto slaveStatusEp = "tcp://localhost:" + boost::lexical_cast<std::string>(slaveStatusPort);

            std::vector<std::string> args;
            args.push_back(slaveStatusEp);
            args.push_back(m_fmuPath.string());
            args.push_back(m_networkInterface);
            args.push_back(std::to_string(m_commTimeout.count()));
            args.push_back(m_outputDir);

            std::cout << "\nStarting slave...\n"
                << "  FMU       : " << m_fmuPath << '\n'
                << std::flush;
            dsb::util::SpawnProcess(m_slaveExe, args);

            std::clog << "Waiting for verification..." << std::flush;
            std::vector<zmq::message_t> slaveStatus;
            const auto feedbackTimedOut = !dsb::net::zmqx::WaitForIncoming(
                slaveStatusSocket,
                timeout);
            if (feedbackTimedOut) {
                throw std::runtime_error(
                    "Slave took more than "
                    + boost::lexical_cast<std::string>(timeout.count())
                    + " milliseconds to start; presumably it has failed altogether");
            }
            dsb::net::zmqx::Receive(slaveStatusSocket, slaveStatus);
            if (dsb::net::zmqx::ToString(slaveStatus[0]) == "ERROR" &&
                    slaveStatus.size() == 2) {
                throw std::runtime_error(dsb::net::zmqx::ToString(slaveStatus[1]));
            } else if (dsb::net::zmqx::ToString(slaveStatus[0]) != "OK" ||
                    slaveStatus.size() < 3 ||
                    slaveStatus[1].size() == 0 ||
                    slaveStatus[2].size() == 0) {
                throw std::runtime_error("Invalid data received from slave executable");
            }
            // At this point, we know that slaveStatus contains three frames, where
            // the first one is "OK", signifying that the slave seems to be up and
            // running.  The following two contains the endpoints to which the slave
            // is bound.
            slaveLocator = dsb::net::SlaveLocator{
                dsb::net::ip::Endpoint{dsb::net::zmqx::ToString(slaveStatus[1])}
                    .ToEndpoint("tcp"),
                dsb::net::ip::Endpoint{dsb::net::zmqx::ToString(slaveStatus[2])}
                    .ToEndpoint("tcp")
            };

            std::clog << "OK" << std::endl;
            return true;
        } catch (const std::exception& e) {
            m_instantiationFailureDescription = e.what();
            return false;
        }
    }

    std::string InstantiationFailureDescription() const override
    {
        return m_instantiationFailureDescription;
    }

private:
    boost::filesystem::path m_fmuPath;
    std::shared_ptr<dsb::fmi::FMU> m_fmu;
    std::string m_networkInterface;
    std::string m_slaveExe;
    std::chrono::seconds m_commTimeout;
    std::string m_outputDir;
    std::string m_instantiationFailureDescription;
};


void ScanDirectoryForFMUs(
    const std::string& directory,
    std::vector<std::string>& fmuPaths)
{
    namespace fs = boost::filesystem;
    for (auto it = fs::recursive_directory_iterator(directory);
         it != fs::recursive_directory_iterator();
         ++it)
    {
        if (it->path().extension() == ".fmu") {
            fmuPaths.push_back(it->path().string());
        }
    }
}


int main(int argc, const char** argv)
{
try {
#ifdef DSB_LOG_TRACE_ENABLED
    dsb::log::SetLevel(dsb::log::trace);
#elif defined(DSB_LOG_DEBUG_ENABLED)
    dsb::log::SetLevel(dsb::log::debug);
#endif

    const auto fmuCacheDir = boost::filesystem::temp_directory_path() / "dsb" / "cache";
    auto importer = dsb::fmi::Importer::Create(fmuCacheDir);

    namespace po = boost::program_options;
    po::options_description options("Options");
    options.add_options()
        ("clean-cache",
            "Clear the cache which contains previously unpacked FMU contents."
            "The program will exit immediately after performing this action.")
        ("domain,d", po::value<std::string>()->default_value("localhost"),
            "The domain address, of the form \"hostname:port\". (\":port\" is "
            "optional, and only required if a nonstandard port is used.)")
        ("slave-exe", po::value<std::string>(),
            "The path to the DSB slave executable")
        ("output-dir,o", po::value<std::string>()->default_value("."),
            "The directory where output files should be written")
        ("timeout", po::value<unsigned int>()->default_value(3600),
            "The number of seconds of inactivity before a slave shuts itself down");
    po::options_description positionalOptions("Arguments");
    positionalOptions.add_options()
        ("fmu",       po::value<std::vector<std::string>>(), "The FMU files and directories");
    po::positional_options_description positions;
    positions.add("fmu", -1);

    const auto args = dsb::util::CommandLine(argc-1, argv+1);
    const auto optionValues = dsb::util::ParseArguments(
        args, options, positionalOptions, positions,
        std::cerr,
        "slave_provider",
        "Slave provider (DSB v" DSB_VERSION_STRING ")\n\n"
        "This program loads one or more FMUs and makes them available as\n"
        "slaves on a domain.");
    if (!optionValues) return 0;
    if (optionValues->count("clean-cache")) {
        importer->CleanCache();
        return 0;
    }
    if (!optionValues->count("fmu")) throw std::runtime_error("No FMUs specified");

    const auto domainAddress = (*optionValues)["domain"].as<std::string>();
    const auto networkInterface = std::string("*"); // TODO: Make cmdline switch
    const auto outputDir = (*optionValues)["output-dir"].as<std::string>();
    const auto timeout = std::chrono::seconds((*optionValues)["timeout"].as<unsigned int>());

    std::string slaveExe;
    if (optionValues->count("slave-exe")) {
        slaveExe = (*optionValues)["slave-exe"].as<std::string>();
    } else if (const auto slaveExeEnv = std::getenv("DSB_SLAVE_EXE")) {
        slaveExe = slaveExeEnv;
    } else {
#ifdef _WIN32
        const auto exeName = "slave.exe";
#else
        const auto exeName = "slave";
#endif
        auto tryPath = dsb::util::ThisExePath().parent_path() / exeName;
        if (boost::filesystem::exists(tryPath)) {
            slaveExe = tryPath.string();
        } else {
            throw std::runtime_error("Slave executable not specified or found");
        }
    }
    assert (!slaveExe.empty());

    std::vector<std::string> fmuPaths;
    for (const auto& fmuSpec :
            (*optionValues)["fmu"].as<std::vector<std::string>>()) {
        if (boost::filesystem::is_directory(fmuSpec)) {
            ScanDirectoryForFMUs(fmuSpec, fmuPaths);
        } else {
            fmuPaths.push_back(fmuSpec);
        }
    }

    std::vector<std::unique_ptr<dsb::provider::SlaveCreator>> fmus;
    for (const auto& p : fmuPaths) {
        fmus.push_back(std::make_unique<DSBSlaveCreator>(
            *importer,
            p,
            networkInterface,
            slaveExe,
            timeout,
            outputDir));
        std::cout << "FMU loaded: " << p << std::endl;
    }
    std::cout << fmus.size() << " FMUs loaded" << std::endl;

    dsb::provider::SlaveProvider slaveProvider{
        dsb::util::RandomUUID(),
        std::move(fmus),
        networkInterface,
        10272,
        [](std::exception_ptr e) {
            try { std::rethrow_exception(e); }
            catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                std::exit(1);
            }
        }
    };
    std::cout << "Press ENTER to quit" << std::flush;
    std::cin.ignore();
    slaveProvider.Stop();
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
}
return 0;
}
