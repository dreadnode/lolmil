#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <chrono>

struct Message {
    enum Role { 
        System, 
        User, 
        Assistant,
        Tool 
    };
    
    Role role;
    std::string content;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;
    
    Message(Role r, const std::string& c) 
        : role(r), content(c), timestamp(std::chrono::steady_clock::now()) {}
    
    static Message system(const std::string& content) {
        return Message(System, content);
    }
    
    static Message user(const std::string& content) {
        return Message(User, content);
    }
    
    static Message assistant(const std::string& content) {
        return Message(Assistant, content);
    }
    
    static Message tool(const std::string& content) {
        return Message(Tool, content);
    }
    
    std::string roleToString() const {
        switch (role) {
            case System: return "system";
            case User: return "user";
            case Assistant: return "assistant";
            case Tool: return "tool";
            default: return "unknown";
        }
    }
};

#endif // MESSAGE_HPP