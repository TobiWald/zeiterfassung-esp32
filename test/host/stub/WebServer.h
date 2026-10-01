#pragma once
#include <Arduino.h>
#include <map>
class WebServer {
 public:
  std::string body; size_t declared = 0; int code = 0; std::string type;
  std::map<std::string, std::string> hdr, args;
  void sendHeader(const String &k, const String &v, bool = false) { hdr[k.s] = v.s; }
  void setContentLength(size_t n) { declared = n; }
  void send(int c, const char *t, const String &b) { code = c; type = t; body += b.s; }
  void sendContent(const char *p, size_t n) { body.append(p, n); }
  bool hasArg(const String &k) { return args.count(k.s); }
  String arg(const String &k) { return String(args[k.s]); }
};
