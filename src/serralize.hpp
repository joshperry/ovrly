/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

/*
  * This module's job is to serialize data structures for IPC transport.
  * This is restricted to the transport of fairly simple classes or structs, sans inheritance (no vtables).
  *
  * We can take advantage of the architecture of chromium, where the root process and each browser
  * render process are instancess of the same executable running on the same machine, to create an
  * incredibly fast blitting serialization method.
  *
  * Based somewhat on the concepts discussed here: https://accu.org/index.php/journals/2317
  */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <sstream>
#include <iterator>

namespace ovrly { namespace serralize { // yes, I can spell even though I'm an sc2 fan

  class OutMemStream {
  public:
    OutMemStream() : buf_{} {}
    inline void write(const void* p, size_t size) {
      //stream_.write(reinterpret_cast<const char*>(p), size);
      auto cp = static_cast<const char*>(p);
      std::copy(cp, cp + size, std::back_inserter(buf_));
    }

    std::vector<char> &getBuf() {
      return buf_;
    }

  private:
    std::vector<char> buf_;
  };

  class InMemStream {
  public:
    InMemStream(void* buf, size_t size) : buf_{ static_cast<char*>(buf) }, size_{ size } {}

    void read(void* p, size_t size) {
      assert(pos_ + size <= size_);
      memcpy(p, buf_ + pos_, size);
      pos_ += size;
    }

    // Get a pointer into the buffer at the current position and increment the position
    void *pos_ptr(size_t size) {
      assert(pos_ + size <= size_);
      auto ptr = buf_ + pos_;
      pos_ += size;
      return ptr;
    }

  private:
    char *buf_;
    size_t size_;
    size_t pos_{ 0 };
  };

  template<typename T>
  void serialize(OutMemStream& o, const std::basic_string<T>& s) {
    size_t l = s.length() * sizeof(T);
    o.write(&l, sizeof(size_t));
    o.write(s.c_str(), l);
  }

  template<typename T>
  void deserialize(InMemStream& i, std::basic_string<T>* s) {
    size_t l;
    i.read(&l, sizeof(size_t));
    new(s) std::basic_string<T>(static_cast<T*>(i.pos_ptr(l)), l / sizeof(T));
  }

  void serialize(OutMemStream& o, const vr::TrackedDevice& dev) {
    o.write(&dev, sizeof(vr::TrackedDevice));
    serialize(o, dev.manufacturer);
    serialize(o, dev.model);
    serialize(o, dev.serial);
  }

  void deserialize(InMemStream &i, vr::TrackedDevice *dev) {
    // Destruct the member non-pods
    dev->manufacturer.~basic_string();
    dev->model.~basic_string();
    dev->serial.~basic_string();

    // Restore the object memory
    i.read(dev, sizeof(vr::TrackedDevice));

    // Restore the member non-pods
    deserialize(i, &dev->manufacturer);
    deserialize(i, &dev->model);
    deserialize(i, &dev->serial);
  }

  template<class T>
  inline void serialize(OutMemStream &o, const std::vector<T>& v) {
    // NB: can be further optimized by writing the
    // whole v.data() at once.
    size_t sz = v.size();
    o.write(&sz, sizeof(size_t));
    for(auto &it : v)
      serialize(o, it);
  }

  template<class T>
  inline void deserialize(InMemStream &i, std::vector<T>* v) {
    size_t sz;
    i.read(&sz, sizeof(size_t));
    new(v) std::vector<T>;

    for(size_t x = 0; x < sz; ++x) {
      T tmp;
      deserialize(i, &tmp);
      v->push_back(std::move(tmp));
    }
  }

  template<typename T>
  inline void serialize(OutMemStream& o, const std::optional<T>& option) {
    char have = 0;
    if(option) {
      have = 1;
      o.write(&have, 1);
      serialize(o, option.value());
    } else {
      o.write(&have, 1);
    }
  }

  template<typename T>
  inline void deserialize(InMemStream& i, std::optional<T>* o) {
    char hasVal;
    i.read(&hasVal, 1);

    if(hasVal) {
      T tmp;
      deserialize(i, &tmp);
      new(o) std::optional<T>{std::move(tmp)};
    } else {
      new(o) std::optional<T>{};
    }
  }

} } // namespace
