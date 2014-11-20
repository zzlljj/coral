#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/program_options.hpp"

#include "dsb/domain/slave_provider.hpp"
#include "dsb/fmi.hpp"
#include "dsb/util.hpp"


struct StartSlave
{
public:
    StartSlave(const std::string& slaveExe) : m_slaveExe(slaveExe) { }

    void operator()(
        dsb::model::SlaveID slaveID,
        const dsb::execution::Locator& executionLocator,
        const std::string& fmuPath)
    {
        const auto slaveIdString = boost::lexical_cast<std::string>(slaveID);
        const auto outputFile = "slave_" + slaveIdString + ".csv";
        std::vector<std::string> args;
        args.push_back(slaveIdString);
        args.push_back(executionLocator.SlaveEndpoint());
        args.push_back(executionLocator.VariablePubEndpoint());
        args.push_back(executionLocator.VariableSubEndpoint());
        args.push_back(fmuPath);
        args.push_back(outputFile);

        std::cout << "\nStarting slave " << slaveID << '\n'
            << "  Execution: " << executionLocator.SlaveEndpoint() << '\n'
            << "  FMU      : " << fmuPath << '\n'
            << "  Output   : " << outputFile << '\n'
            << std::flush;
        dsb::util::SpawnProcess(m_slaveExe, args);
    }

private:
    std::string m_slaveExe;
};


void ScanDirectoryForFMUs(const std::string& directory, std::vector<std::string>& fmuPaths)
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
    namespace po = boost::program_options;
    po::options_description optDesc("Options");
    optDesc.add_options()
        ("help",      "Display help message")
        ("slave-exe", po::value<std::string>(), "The path to the DSB slave executable");
    po::options_description argDesc;
    argDesc.add(optDesc);
    argDesc.add_options()
        ("domain",    po::value<std::string>(),                    "The address of the domain")
        ("fmu",       po::value<std::vector<std::string>>(),       "The FMU files and directories");
    po::positional_options_description posArgDesc;
    posArgDesc.add("domain", 1)
              .add("fmu", -1);
    po::variables_map optMap;
    po::store(po::command_line_parser(argc, argv).options(argDesc)
                                                 .positional(posArgDesc)
                                                 .run(),
              optMap);

    if (argc < 2 || optMap.count("help")) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Usage: " << self << " <domain> <fmus...> [options...]\n"
                  << "Arguments:\n"
                  << "  domain   The domain address\n"
                  << "  fmus     FMU files and directories\n"
                  << optDesc << std::endl;
        return 0;
    }

    if (!optMap.count("domain")) throw std::runtime_error("Domain address not specified");
    if (!optMap.count("fmu")) throw std::runtime_error("No FMUs specified");

    const auto domainAddress = optMap["domain"].as<std::string>();
    const auto reportEndpoint = domainAddress + ":51381";
    const auto infoEndpoint = domainAddress + ":51383";

    std::string slaveExe;
    if (optMap.count("slave-exe")) {
        slaveExe = optMap["slave-exe"].as<std::string>();
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
    BOOST_FOREACH (const auto& fmuSpec, optMap["fmu"].as<std::vector<std::string>>()) {
        if (boost::filesystem::is_directory(fmuSpec)) {
            ScanDirectoryForFMUs(fmuSpec, fmuPaths);
        } else {
            fmuPaths.push_back(fmuSpec);
        }
    }

    std::vector<std::unique_ptr<dsb::domain::ISlaveType>> fmus;
    std::vector<dsb::domain::ISlaveType*> fmuPtrs;
    for (auto it = fmuPaths.begin(); it != fmuPaths.end(); ++it) {
        fmus.push_back(dsb::fmi::MakeSlaveType(*it, StartSlave(slaveExe)));
        fmuPtrs.push_back(fmus.back().get());
        std::clog << "FMU loaded: " << *it << std::endl;
    }
    std::clog << fmus.size() << " FMUs loaded" << std::endl;
    dsb::domain::SlaveProvider(reportEndpoint, infoEndpoint, fmuPtrs);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
return 0;
}
