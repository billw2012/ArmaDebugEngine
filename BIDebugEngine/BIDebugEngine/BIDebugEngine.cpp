//BI Debug Engine Interface


#include "BIDebugEngine.h"
#include <windows.h>
#include "EngineHook.h"
#include <thread>
#include "Script.h"
static DllInterface dllIface;
static EngineInterface* engineIface;
extern "C" EngineHook GlobalEngineHook;

uintptr_t engineAlloc;

#include "NamedPipeServer.h"


void DllInterface::Startup() {
    GlobalEngineHook.onStartup();
}

void DllInterface::Shutdown() {
    GlobalEngineHook.onShutdown();
}

void DllInterface::ScriptLoaded(IDebugScript *script, const char *name) {

}

void DllInterface::ScriptEntered(IDebugScript *script) {

}

void DllInterface::ScriptTerminated(IDebugScript *script) {

}

void DllInterface::FireBreakpoint(IDebugScript *script, unsigned int bp) {

}

void DllInterface::Breaked(IDebugScript *script) {

}

void DllInterface::DebugEngineLog(const char *str) {

}

DllInterface* Connect(EngineInterface *engineInt) {
    WAIT_FOR_DEBUGGER_ATTACHED;
    engineIface = engineInt;
    return &dllIface;
}
char VarBuffer[1024];
void IDebugScope::printAllVariables() {


    auto varC = varCount();
    std::vector<IDebugVariable*> vars;
    vars.resize(varC);
    IDebugVariable** vbase = vars.data();
    getVariables(const_cast<const IDebugVariable**>(vbase), varC);

    for (auto& var : vars) {
        var->getName(VarBuffer, 1023);
        std::string name(VarBuffer);
        auto value = var->getValue();
        if (!value) continue;
        value->getTypeString(VarBuffer, 1023);
        std::string valueType(VarBuffer);
        value->getValue(10, VarBuffer, 1023);
        OutputDebugStringA((name + " " + valueType + ": " + std::string(VarBuffer) + "\n").c_str());
    }
}

std::string IDebugScope::allVariablesToString() {
    std::string output;
    auto varC = varCount();
    std::vector<IDebugVariable*> vars;
    vars.resize(varC);
    IDebugVariable** vbase = vars.data();
    getVariables(const_cast<const IDebugVariable**>(vbase), varC);

    for (auto& var : vars) {
        var->getName(VarBuffer, 1023);
        std::string name(VarBuffer);
        auto value = var->getValue();
        if (!value) continue;
        value->getTypeString(VarBuffer, 1023);
        std::string valueType(VarBuffer);
        if (valueType == "code" || valueType == "Array") {
            output += (name + " : " + valueType + "; ");
            continue;
        }
        value->getValue(10, VarBuffer, 1023);
        output += (name + "=" + std::string(VarBuffer) + " : " + valueType + "; ");
    }
    return output;
}

void RV_ScriptVM::debugPrint(std::string prefix) {
    std::string title = _displayName;
    std::string filename = _context._doc._fileName.isNull() ? _context._lastInstructionPos._sourceFile : _context._doc._fileName;
    std::string data;// = _doc._content.data();
    OutputDebugStringA((prefix + " " + title + "F " + filename + " " + data + "\n").c_str());
}
#include "Serialize.h"

const GameVariable* RV_VMContext::getVariable(std::string varName) {
    const GameVariable* value = nullptr;
    callStack.forEachBackwards([&value, &varName](const Ref<CallStackItem>& item) {
        auto var = item->_varSpace.getVariable(varName);
        if (var) {
            value = var;
            return true;
        }
        return false;
    });
    return value;
}

void RV_VMContext::Serialize(JsonArchive& ar) {


    ar.Serialize("callstack", callStack, [](JsonArchive& ar, const Ref<CallStackItem>& item) {
        auto& type = typeid(*item.get());
        const auto typeName = type.name();
        ar.Serialize("type", typeName);
        auto hash = type.hash_code();
        {
            JsonArchive varArchive;

            item->_varSpace._variables.forEach([&varArchive](const GameVariable& var) {

                JsonArchive variableArchive;

                auto name = var._name;
                if (var._value.isNull()) {
                    variableArchive.Serialize("type", "nil");
                    varArchive.Serialize(name.data(), variableArchive);
                    return;
                }
                auto value = var._value._data;
                const auto type = value->getTypeString();

                variableArchive.Serialize("type", type);
                if (strcmp(type, "array") == 0) {
                    variableArchive.Serialize("value", value->getArray());
                } else {
                    variableArchive.Serialize("value", value->getAsString());
                }
                varArchive.Serialize(name.data(), variableArchive);

            });
            ar.Serialize("variables", varArchive);
        }

        switch (hash) {

            case 0xed08ac32: { //CallStackItemSimple
                auto stackItem = static_cast<const CallStackItemSimple*>(item.get());
                ar.Serialize("fileName", stackItem->_content._fileName);
                ar.Serialize("contentSample", stackItem->_content._content.substr(0, 100));
                ar.Serialize("ip", stackItem->_currentInstruction);
                ar.Serialize("lastInstruction", *(stackItem->_instructions.get(stackItem->_currentInstruction - 1)));
                //ar.Serialize("instructions", stackItem->_instructions);
            }   break;

            case 0x224543d0: { //CallStackItemData
                auto stackItem = static_cast<const CallStackItemData*>(item.get());

                ar.Serialize("ip", stackItem->_ip);
                ar.Serialize("final", stackItem->_code->_final);
                ar.Serialize("compiled", stackItem->_code->_instructions._compiled);
                ar.Serialize("contentSample", stackItem->_code->_instructions._string.substr(0, 100)); //#TODO send whole code over

                ar.Serialize("lastInstruction", *(stackItem->_code->_instructions._code.get(stackItem->_ip - 1)));
                //ar.Serialize("instructions", stackItem->_code->_instructions._code);
            }   break;
            case 0x254c4241: { //CallStackItemArrayForEach

            } break;
            default:
                //__debugbreak(); 
                return;
        }
    });
}

void GameData::Serialize(JsonArchive& ar) const {
    const auto type = getTypeString();
    ar.Serialize("type", type);
    if (strcmp(type, "array") == 0) {
        ar.Serialize("value", getArray());
    } else if (strcmp(type, "code") == 0) {
		SourceDocPos x;
		x._content = getAsString();
        ar.Serialize("value", Script::getScriptFromFirstLine(x,true));
	} else {
		ar.Serialize("value", getAsString());
	}
}

void GameValue::Serialize(JsonArchive& ar) const {
    _data->Serialize(ar);
}

const GameVariable* GameVarSpace::getVariable(const std::string& varName) const {
    auto & var = _variables.get(varName.c_str());
    if (!_variables.isNull(var)) {
        return &var;
    }
    if (_parent) {
        return _parent->getVariable(varName);
    }
    return nullptr;
}

void GameEvaluator::SerializeError(JsonArchive& ar) {
	ar.Serialize("fileOffset", { _errorPosition._sourceLine, _errorPosition._pos, Script::getScriptLineOffset(_errorPosition) });

	ar.Serialize("type", _errorType);
      //#TODO errorType enum
     /*
     "OK"
.rdata:01853BD0 aGen            db 'GEN',0              ; DATA XREF: sub_D78D0:loc_D79D5o
.rdata:01853BD4 aExpo           db 'EXPO',0
.rdata:01853BDC aNum            db 'NUM',0              ; DATA XREF: sub_D78D0:loc_D7BB9o
.rdata:01853BE0 aVar_0          db 'VAR',0              ; DATA XREF: sub_D78D0:loc_D7CADo
.rdata:01853BE4 aBad_var        db 'BAD_VAR',0          ; DATA XREF: sub_D78D0:loc_D7DA1o
.rdata:01853BEC aDiv_zero       db 'DIV_ZERO',0         ; DATA XREF: sub_D78D0+5C8o
.rdata:01853BF8 aTg90           db 'TG90',0             ; DATA XREF: sub_D78D0+61Eo
.rdata:01853C00 aOpenb          db 'OPENB',0            ; DATA XREF: sub_D78D0+674o
.rdata:01853C08 aCloseb         db 'CLOSEB',0           ; DATA XREF: sub_D78D0+6CAo
.rdata:01853C10 aOpen_brackets  db 'OPEN_BRACKETS',0    ; DATA XREF: sub_D78D0+720o
.rdata:01853C20 aClose_brackets db 'CLOSE_BRACKETS',0   ; DATA XREF: sub_D78D0+776o
.rdata:01853C30 aOpen_braces    db 'OPEN_BRACES',0      ; DATA XREF: sub_D78D0+7CCo
.rdata:01853C3C aClose_braces   db 'CLOSE_BRACES',0     ; DATA XREF: sub_D78D0+822o
.rdata:01853C4C aEqu            db 'EQU',0              ; DATA XREF: sub_D78D0+872o
.rdata:01853C50 aSemicolon      db 'SEMICOLON',0        ; DATA XREF: sub_D78D0+8C8o
.rdata:01853C5C aQuote          db 'QUOTE',0            ; DATA XREF: sub_D78D0+91Eo
.rdata:01853C64 aSingle_quote   db 'SINGLE_QUOTE',0     ; DATA XREF: sub_D78D0+974o
.rdata:01853C74 aLine_long      db 'LINE_LONG',0        ; DATA XREF: sub_D78D0+A20o
.rdata:01853C80 aNamespace_0    db 'NAMESPACE',0        ; DATA XREF: sub_D78D0+AC9o
.rdata:01853C8C aDim            db 'DIM',0              ; DATA XREF: sub_D78D0+B1Fo
.rdata:01853C90 aUnexpected_clo db 'UNEXPECTED_CLOSEB',0 ; DATA XREF: sub_D78D0+B75o
.rdata:01853CA4 aAssertation_fa db 'ASSERTATION_FAILED',0 ; DATA XREF: sub_D78D0+BCBo
.rdata:01853CB8 aHalt_function  db 'HALT_FUNCTION',0    ; DATA XREF: sub_D78D0+C21o
.rdata:01853CC8 aForeign        db 'FOREIGN',0          ; DATA XREF: sub_D78D0+C77o
.rdata:01853CD0 aScope_name_def db 'SCOPE_NAME_DEFINED_TWICE',0 ; DATA XREF: sub_D78D0+CCCo
.rdata:01853CEC aScope_not_foun db 'SCOPE_NOT_FOUND',0  ; DATA XREF: sub_D78D0+D1Fo
.rdata:01853CFC aInvalid_try_bl db 'INVALID_TRY_BLOCK',0 ; DATA XREF: sub_D78D0+D72o
.rdata:01853D10 aUnhandled_exce db 'UNHANDLED_EXCEPTION',0 ; DATA XREF: sub_D78D0+DC5o
.rdata:01853D24 aStack_overflow db 'STACK_OVERFLOW',0   ; DATA XREF: sub_D78D0+E18o
.rdata:01853D34 aHandled        db 'HANDLED',0
     */



	ar.Serialize("message", _errorMessage);
	ar.Serialize("filename", _errorPosition._sourceFile);
	ar.Serialize("content", Script::getScriptFromFirstLine(_errorPosition));
}
