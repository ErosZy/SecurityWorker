#ifndef JPROTECTOR_STRING_HPP
#define JPROTECTOR_STRING_HPP

#include <memory.h>
#include <memory>
#include <string.h>
#include <iostream>

#define CHAR_SIZE(LEN) ((LEN) * sizeof(char))

class string {
public:
  string();

  explicit string(const char *c_str);

  string(const string &str);

  string &operator=(const string &str);

  ~string();

  string &operator<<(string &str);

  string &operator<<(const char *c_str);

  bool operator==(const string &str);

  const char *c_str();

  size_t size();

private:
  char *ptr;
  size_t len;
};

inline string::string() {
  len = 0;
  ptr = (char *) malloc(CHAR_SIZE(1));
  ptr[0] = '\0';
}

inline string::string(const char *c_str) {
  if (c_str == nullptr) {
    len = 0;
    ptr = (char *) malloc(CHAR_SIZE(1));
    ptr[0] = '\0';
  } else {
    len = strlen(c_str);
    ptr = (char *) malloc(CHAR_SIZE(len + 1));
    memset(ptr, '\0', CHAR_SIZE(len + 1));
    memcpy(ptr, c_str, CHAR_SIZE(len));
  }
}

inline string::string(const string &str) {
  this->len = str.len;
  this->ptr = (char *) malloc(CHAR_SIZE(str.len + 1));
  memset(this->ptr, '\0', CHAR_SIZE(this->len + 1));
  memcpy(this->ptr, str.ptr, CHAR_SIZE(this->len));
}

inline string &string::operator=(const string &str) {
  if (this == &str) {
    return *this;
  }

  this->len = str.len;
  this->ptr = (char *) malloc(CHAR_SIZE(str.len + 1));
  memset(this->ptr, '\0', CHAR_SIZE(this->len + 1));
  memcpy(this->ptr, str.ptr, CHAR_SIZE(this->len));
  return *this;
}

inline string::~string() {
  delete ptr;
  ptr = nullptr;
}

inline string &string::operator<<(string &str) {
  size_t totalLen = this->len + str.len;
  char *constr = (char *) malloc(CHAR_SIZE(totalLen + 1));
  memset(constr, '\0', CHAR_SIZE(totalLen + 1));
  memcpy(constr, this->ptr, CHAR_SIZE(this->len));
  memcpy(constr + this->len, str.ptr, CHAR_SIZE(str.len));
  delete this->ptr;
  this->ptr = constr;
  this->len = totalLen;
  return *this;
}

inline string &string::operator<<(const char *c_str) {
  if (c_str == nullptr) {
    return *this;
  }

  size_t totalLen = strlen(c_str) + this->len;
  char *constr = (char *) malloc(CHAR_SIZE(totalLen + 1));
  memset(constr, '\0', CHAR_SIZE(totalLen + 1));
  memcpy(constr, this->ptr, CHAR_SIZE(this->len));
  memcpy(constr + this->len, c_str, CHAR_SIZE(strlen(c_str)));
  delete this->ptr;
  this->ptr = constr;
  this->len = totalLen;
  return *this;
}

inline bool string::operator==(const string &str) {
  return this == &str ||
         (this->len == str.len &&
          memcmp(this->ptr, str.ptr, str.len) == 0);
}

inline const char *string::c_str() {
  return ptr;
}

inline size_t string::size() {
  return len;
}


#endif //JPROTECTOR_STRING_HPP
