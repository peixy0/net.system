#include "parser.hpp"

namespace network {

std::optional<HttpRequest> ConcreteHttpParser::Parse(std::string& payload) {
  if (not method) {
    method = ParseToken(payload);
    if (not method) {
      return std::nullopt;
    }
    ToLower(*method);
  }
  if (not uri) {
    uri = ParseToken(payload);
    if (not uri) {
      return std::nullopt;
    }
  }
  if (not version) {
    version = ParseToken(payload);
    if (not version) {
      return std::nullopt;
    }
  }
  if (not requestLineEndingParsed) {
    requestLineEndingParsed = Consume(payload, "\r\n");
    if (not requestLineEndingParsed) {
      return std::nullopt;
    }
  }
  if (not headersParsed) {
    headersParsed = ParseHeaders(payload, headers);
    if (not headersParsed) {
      return std::nullopt;
    }
  }
  if (not headersEndingParsed) {
    headersEndingParsed = Consume(payload, "\r\n");
    if (not headersEndingParsed) {
      return std::nullopt;
    }
  }
  bodyRemaining = ParseContentLength(headers);
  if (payload.length() < bodyRemaining) {
    return std::nullopt;
  }
  std::string body = payload.substr(0, bodyRemaining);
  payload.erase(0, bodyRemaining);
  HttpRequest request{std::move(*method), std::move(*uri), std::move(*version), std::move(headers), std::move(body)};
  Reset();
  return request;
}

void ConcreteHttpParser::Reset() {
  method.reset();
  uri.reset();
  version.reset();
  requestLineEndingParsed = false;
  headers.clear();
  headersParsed = false;
  headersEndingParsed = false;
  bodyRemaining = 0;
}

void ConcreteHttpParser::ToLower(std::string& token) const {
  for (auto& c : token) {
    c = std::tolower(c);
  }
}

void ConcreteHttpParser::SkipWhiteSpaces(std::string& payload) const {
  while (not payload.empty() and payload[0] == ' ') {
    payload.erase(0, 1);
  }
}

bool ConcreteHttpParser::Consume(std::string& payload, std::string_view value) const {
  int payloadLength = payload.length();
  int valueLength = value.length();
  for (int i = 0; i < valueLength; i++) {
    if (i >= payloadLength or payload[i] != value[i]) {
      return false;
    }
  }
  payload.erase(0, valueLength);
  return true;
}

std::optional<std::string> ConcreteHttpParser::ParseToken(std::string& payload) const {
  SkipWhiteSpaces(payload);
  if (payload.empty()) {
    return std::nullopt;
  }
  int n = 0;
  int length = payload.length();
  while (n < length and payload[n] != ' ' and payload[n] != '\r' and payload[n] != '\n') {
    n++;
  }
  if (n == length) {
    return std::nullopt;
  }
  std::string token = payload.substr(0, n);
  payload.erase(0, n);
  return token;
}

bool ConcreteHttpParser::ParseHeaders(std::string& payload, HttpHeaders& headers) const {
  while (not payload.empty()) {
    if (payload.starts_with("\r\n")) {
      return true;
    }
    auto header = ParseHeader(payload);
    if (not header) {
      return false;
    }
    headers.emplace(std::move(header->field), std::move(header->value));
  }
  return false;
}

std::optional<HttpHeader> ConcreteHttpParser::ParseHeader(std::string& payload) const {
  auto line = ParseLine(payload);
  if (not line) {
    return std::nullopt;
  }
  ToLower(*line);
  int length = line->length();
  int n = 0;
  while (n < length and (*line)[n] != ':') {
    n++;
  }
  if (n == length) {
    return HttpHeader{std::move(*line), ""};
  }
  auto field = line->substr(0, n);
  line->erase(0, n + 1);
  SkipWhiteSpaces(*line);
  return HttpHeader{std::move(field), std::move(*line)};
}

std::optional<std::string> ConcreteHttpParser::ParseLine(std::string& payload) const {
  int length = payload.length();
  int n = 0;
  while (n + 1 < length and (payload[n] != '\r' or payload[n + 1] != '\n')) {
    n++;
  }
  n += 2;
  if (n >= length) {
    return std::nullopt;
  }
  std::string s = payload.substr(0, n - 2);
  payload.erase(0, n);
  return s;
}

std::uint32_t ConcreteHttpParser::ParseContentLength(const HttpHeaders& headers) const {
  auto it = headers.find("content-length");
  if (it != headers.end()) {
    std::uint32_t r = stoul(it->second);
    return r > 0 ? r : 0;
  }
  return 0;
}

}  // namespace network
