#ifndef MODULE_H
#define MODULE_H
#include <vector>
#include <memory>
#include <concepts>

template<typename T>
concept ModuleConcept = requires(T a) {
    { a.next } -> std::same_as<std::vector<std::shared_ptr<T>>&>;
    { a.exec() } -> std::same_as<void>;
};
#endif