#ifndef PIM_ERRORS_H
#define PIM_ERRORS_H

#include <stdexcept>
#include <string>

namespace pim {

/// Base exception class for all PIM-sim errors.
class PIMException : public std::runtime_error {
public:
    explicit PIMException(const std::string& msg) : std::runtime_error(msg) {}
};

/// Thrown on graph construction or traversal errors.
class GraphError : public PIMException {
public:
    explicit GraphError(const std::string& msg) : PIMException("GraphError: " + msg) {}
};

/// Thrown on physical tile mapping failures (e.g., grid full, no valid placement).
class MappingError : public PIMException {
public:
    explicit MappingError(const std::string& msg) : PIMException("MappingError: " + msg) {}
};

/// Thrown on path scheduling or congestion routing failures.
class SchedulingError : public PIMException {
public:
    explicit SchedulingError(const std::string& msg) : PIMException("SchedulingError: " + msg) {}
};

/// Thrown on invalid hardware configuration or SimConfig.ini parsing errors.
class ConfigError : public PIMException {
public:
    explicit ConfigError(const std::string& msg) : PIMException("ConfigError: " + msg) {}
};

} // namespace pim

#endif