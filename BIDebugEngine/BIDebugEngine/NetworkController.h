#pragma once
#include "NamedPipeServer.h"
#include <thread>

enum class NC_CommandType {
    invalid,
    addBreakpoint,
    delBreakpoint,
    BPContinue,//Tell's breakpoint to leave breakState
    MonitorDump, //Dump's all Monitors
    setHookEnable,
    getVariable,
    getCurrentCode //While in breakState returns full preproced code of last Instructions script file
};

enum class NC_OutgoingCommandType {
    invalid,
    BreakpointHalt,
    ContinueExecution,
    VariableReturn //returning from getVariable
};

class NetworkController {
public:
    NetworkController();
    ~NetworkController();
    void init();
    void incomingMessage(const std::string& message);
    void sendMessage(const std::string& message);
private:
    NamedPipeServer server;
    std::thread* pipeThread{ nullptr };
};

