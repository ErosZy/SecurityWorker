#ifndef JPROTECTOR_MAP_HPP
#define JPROTECTOR_MAP_HPP

#include <memory>
#include <memory.h>
#include <iostream>

#define PAIR_PTR_SIZE(LEN) ((LEN) * sizeof(pair *))

template<typename T, typename U>
class map {
  struct pair {
  public:
    pair() : key(nullptr), value(nullptr) {};

    pair(const pair &p) {
      if (this != &p) {
        key = new T(*p.key);
        value = new U(*p.value);
      }
    }

    pair &operator=(const pair &p) {
      if (this != &p) {
        key = new T(*p.key);
        value = new U(*p.value);
      }
      return *this;
    }

    ~pair() {
      delete key;
      delete value;
      key = nullptr;
      value = nullptr;
    }

    T *key;
    U *value;
  };

  typedef void (*foreach_func)(T, U, void *);

public:
  explicit map(size_t size = 10) : capacity(0), max_capacity(size) {
    items = new pair *[size];
    for (int i = 0; i < size; i++) {
      items[i] = NULL;
    }
  }

  map(const map &m) {
    if (this != &m) {
      capacity = m.capacity;
      max_capacity = m.max_capacity;
      items = new pair *[capacity];
      for (uint32_t i = 0; i < capacity; i++) {
        items[i] = new pair(*m.items[i]);
      }
    }
  }

  map &operator=(const map &m) {
    if (this != &m) {
      capacity = m.capacity;
      max_capacity = m.max_capacity;
      items = new pair *[capacity];
      for (uint32_t i = 0; i < capacity; i++) {
        items[i] = new pair(*m.items[i]);
      }
    }
    return *this;
  }

  ~map() {
    for (uint32_t i = 0; i < capacity; i++) {
      delete items[i];
      items[i] = nullptr;
    }
    delete items;
    items = nullptr;
  }

  uint32_t add(const T &key, const U &value);

  int32_t remove(const T &key);

  U *get(const T &key);

  int32_t find(const T &key);

  void foreach(foreach_func, void *);

  size_t size() {
    return capacity;
  }

  size_t max_size() {
    return max_capacity;
  }

private:
  pair **items;
  size_t capacity;
  size_t max_capacity;
};

template<typename T, typename U>
uint32_t map<T, U>::add(const T &key, const U &value) {
  int32_t index = this->find(key);
  pair *p = new pair();
  p->key = new T(key);
  p->value = new U(value);
  if (index == -1) {
    if (max_capacity <= capacity) {
      max_capacity *= 2;
      auto s = new pair *[max_capacity];
      memset(s, NULL, PAIR_PTR_SIZE(max_capacity));
      memcpy(s, items, PAIR_PTR_SIZE(capacity));
      delete items;
      items = s;
    }
    items[capacity++] = p;
    return (uint32_t) (capacity - 1);
  }

  return (uint32_t) index;
}

template<typename T, typename U>
inline int32_t map<T, U>::remove(const T &key) {
  int32_t index = this->find(key);
  if (index == -1) {
    return -1;
  }

  delete items[index];
  items[index] = nullptr;

  memcpy(items + index, items + index + 1, PAIR_PTR_SIZE(capacity - (index + 1)));
  items[capacity--] = NULL;
  return index;
}

template<typename T, typename U>
inline U *map<T, U>::get(const T &key) {
  int32_t index = this->find(key);
  if (index > -1) {
    return items[index]->value;
  }

  return nullptr;
}

template<typename T, typename U>
inline int32_t map<T, U>::find(const T &key) {
  for (uint32_t i = 0; i < capacity; i++) {
    pair *item = items[i];
    if (item == NULL) {
      break;
    }

    if (*item->key == key) {
      return i;
    }
  }

  return -1;
}

template<typename T, typename U>
inline void map<T, U>::foreach(foreach_func func, void *userData) {
  for (uint32_t i = 0; i < capacity; i++) {
    pair *item = items[i];
    if (item != NULL) {
      func(*(item->key), *(item->value), userData);
    }
  }
}


#endif //JPROTECTOR_MAP_HPP
