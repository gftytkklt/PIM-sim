#ifndef PIM_ERRORS_H
#define PIM_ERRORS_H

#include <stdexcept>
#include <string>

namespace pim {

class PIMException : public std::runtime_error {
public:
    explicit PIMException(const std::string& msg) : std::runtime_error(msg) {}
};

class GraphError : public PIMException {
public:
    explicit GraphError(const std::string& msg) : PIMException("GraphError: " + msg) {}
};

class MappingError : public PIMException {
public:
    explicit MappingError(const std::string& msg) : PIMException("MappingError: " + msg) {}
};

class SchedulingError : public PIMException {
public:
    explicit SchedulingError(const std::string& msg) : PIMException("SchedulingError: " + msg) {}
};

class ConfigError : public PIMException {
public:
    explicit ConfigError(const std::string& msg) : PIMException("ConfigError: " + msg) {}
};

} // namespace pim

#endif