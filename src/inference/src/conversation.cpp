#include "conversation.hpp"
#include <sstream>

void Conversation::setSystemPrompt(const std::string& prompt) {
    system_prompt = prompt;
}

void Conversation::addMessage(const Message& msg) {
    messages.push_back(msg);
}

void Conversation::clear() {
    messages.clear();
    // System prompt persists across clears
}

void Conversation::removeLastMessage() {
    if (!messages.empty()) {
        messages.pop_back();
    }
}

const std::vector<Message>& Conversation::getHistory() const {
    return messages;
}

std::vector<Message> Conversation::getLastExchange() const {
    std::vector<Message> exchange;

    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == Message::User) {
            for (auto fwd_it = std::prev(it.base()); fwd_it != messages.end(); ++fwd_it) {
                exchange.push_back(*fwd_it);
            }
            break;
        }
    }
    
    return exchange;
}

std::string Conversation::formatForModel() const {
    std::stringstream formatted;

    if (!system_prompt.empty()) {
        formatted << "<|system|>\n" << system_prompt << "<|end|>\n";
    }

    // Phi-3 chat template format
    for (const auto& msg : messages) {
        switch (msg.role) {
            case Message::System:
                formatted << "<|system|>\n" << msg.content << "<|end|>\n";
                break;
            case Message::User:
                formatted << "<|user|>\n" << msg.content << "<|end|>\n";
                break;
            case Message::Assistant:
                formatted << "<|assistant|>\n" << msg.content << "<|end|>\n";
                break;
            case Message::Tool:
                formatted << "<|tool|>\n" << msg.content << "<|end|>\n";
                break;
        }
    }

    formatted << "<|assistant|>\n";
    
    return formatted.str();
}

std::string Conversation::formatLastUserMessage() const {
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == Message::User) {
            return it->content;
        }
    }
    return "";
}