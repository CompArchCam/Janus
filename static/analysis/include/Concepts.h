#ifndef _janus_CONCEPTS_
#define _janus_CONCEPTS_

#include <concepts>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <unordered_map>
#include <utility>

#include "BasicBlock.h"

template <typename T>
concept ProvidesDominanceFrontier = requires(T cfg)
{
    typename T;
    // clang-format off
    // Checks for content of the class
    // Note that we can check other properties of all classes
    { cfg.dominanceFrontiers } -> std::convertible_to<std::unordered_map<janus::BasicBlock *, std::set<janus::BasicBlock *>>>;
    // clang-format on
};

template <typename T>
concept ProvidesPostDominanceFrontier = requires(T cfg)
{
    typename T;
    // clang-format off
    // Checks for content of the class
    // Note that we can check other properties of all classes
    { cfg.postDominanceFrontiers } -> std::convertible_to<std::unordered_map<janus::BasicBlock *, std::set<janus::BasicBlock *>>>;
    // clang-format on
};

template <typename T>
concept ProvidesBasicCFG = requires(T cfg)
{
    typename T;

    // clang-format off
    { cfg.blocks } -> std::convertible_to<std::vector<janus::BasicBlock>&>;
    { *(cfg.entry) } -> std::convertible_to<janus::BasicBlock>;
    // clang-format on
};

template <typename T>
concept ProvidesTerminationBlocks = requires(T cfg)
{
    typename T;

    // clang-format off
    { cfg.terminations } -> std::convertible_to<std::set<BlockID> &>;
    // clang-format on
};

template <typename T>
concept ProvidesFunctionReference = requires(T cfg)
{
    typename T;

    // clang-format off
    { cfg.func } -> std::convertible_to<janus::Function&>;
    // clang-format on
};

#endif
