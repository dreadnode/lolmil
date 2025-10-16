#ifndef CONVERSATION_HPP
#define CONVERSATION_HPP

#include "message.hpp"
#include <vector>
#include <string>

class Conversation {
private:
    std::vector<Message> messages;
    std::string system_prompt;
    
public:
    Conversation() = default;
    
    void setSystemPrompt(const std::string& prompt);
    void addMessage(const Message& msg);
    void clear();
    void removeLastMessage();
    
    [[nodiscard]] const std::vector<Message>& getHistory() const;
    [[nodiscard]] std::vector<Message> getLastExchange() const;
    
    [[nodiscard]] std::string formatForModel() const;
    [[nodiscard]] std::string formatLastUserMessage() const;
    
    [[nodiscard]] bool hasSystemPrompt() const { return !system_prompt.empty(); }
    [[nodiscard]] const std::string& getSystemPrompt() const { return system_prompt; }
    [[nodiscard]] size_t size() const { return messages.size(); }
    [[nodiscard]] bool empty() const { return messages.empty(); }
};

#endif // CONVERSATION_HPP