/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include <functional>
#include <vector>

/**
 * An event functor that can be subscribed to with a
 * a std::function to call for simple pub/sub
 */
template<typename... ArgTypes>
class Event {
  public:
    typedef std::function<void(ArgTypes...)> functype;

    void attach(functype f) {
      observers_.push_back(f);
    }

    void operator()(ArgTypes... args) const {
      for(auto &observer : observers_) {
        observer(args...);
      }
    }

  private:
    std::vector<functype> observers_;
};

/**
 * An event functor that sequentially calls a chain of boolean returning filter
 * functions until one signals it handled the event by returning true.
 *
 * Returns true if a filter in the chain handled the event, false otherwise.
 */
template<typename... ArgTypes>
class FilterChain {
  public:
    typedef std::function<bool(ArgTypes...)> functype;

    void attach(functype f) {
      observers_.push_back(f);
    }

    bool operator()(ArgTypes... args) const {
      for(auto &observer : observers_) {
        if(observer(args...)) {
          return true;
        }
      }
      return false;
    }

  private:
    std::vector<functype> observers_;
};


