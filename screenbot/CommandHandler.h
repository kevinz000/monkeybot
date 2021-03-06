#ifndef COMMAND_HANDLER_H_
#define COMMAND_HANDLER_H_

#include "Common.h"
#include "Messages/Messages.h"

#include <functional>
#include <unordered_map>

class Bot;

typedef std::function<void(const std::string&)> CommandFunction;

class CommandHandler : public MessageHandler<ChatMessage> {
private:
    std::unordered_map<std::string, CommandFunction> m_Commands;
    Bot* m_Bot;
    std::string m_Owner;

    void HandleMessage(ChatMessage* mesg);

    void CommandPause(const std::string& args);
    void CommandTarget(const std::string& args);
    void CommandFreq(const std::string& args);
    void CommandFlag(const std::string& args);
    void CommandTaunt(const std::string& args);
    void CommandConfig(const std::string& args);
public:
    CommandHandler(Bot* bot);
    ~CommandHandler();

    bool Initialize();
};

#endif
