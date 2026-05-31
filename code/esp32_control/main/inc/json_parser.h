#ifndef JSON_PARSER_H_
#define JSON_PARSER_H_

#include <string>
#include <cstdlib>
#include <cctype>

struct JsonNode {
    std::string key;
    int value_int;
    bool is_number;
    JsonNode* next;
};

class JsonParser {
public:
    JsonParser() : root(nullptr) {}
    ~JsonParser() { clear(); }

    bool parse(const std::string& json) {
        clear();
        size_t pos = 0;
        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != '{') return false;
        pos++;
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            if (pos >= json.size()) return false;
            if (json[pos] == '}') { pos++; return true; }
            if (json[pos] == ',') { pos++; continue; }

            if (json[pos] != '"') return false;
            pos++;
            size_t key_start = pos;
            while (pos < json.size() && json[pos] != '"') pos++;
            if (pos >= json.size()) return false;
            std::string key = json.substr(key_start, pos - key_start);
            pos++;
            skipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') return false;
            pos++;
            skipWhitespace(json, pos);
            if (pos >= json.size()) return false;

            if (json[pos] == '"') {
                pos++;
                while (pos < json.size() && json[pos] != '"') pos++;
                if (pos >= json.size()) return false;
                pos++;
            } else if (json[pos] == '-' || isdigit((unsigned char)json[pos])) {
                int sign = 1;
                if (json[pos] == '-') { sign = -1; pos++; }
                int val = 0;
                bool found = false;
                while (pos < json.size() && isdigit((unsigned char)json[pos])) {
                    val = val * 10 + (json[pos] - '0');
                    found = true;
                    pos++;
                }
                if (json[pos] == '.') {
                    while (pos < json.size() && isdigit((unsigned char)json[pos])) pos++;
                }
                if (found) {
                    JsonNode* node = new JsonNode{key, val * sign, true, root};
                    root = node;
                }
            } else if (json[pos] == 't' || json[pos] == 'f') {
                if (json.substr(pos, 4) == "true") pos += 4;
                else if (json.substr(pos, 5) == "false") pos += 5;
                else return false;
            } else if (json[pos] == 'n') {
                if (json.substr(pos, 4) != "null") return false;
                pos += 4;
            } else {
                return false;
            }
        }
        return false;
    }

    int getInt(const char* key, int default_val = -1) const {
        JsonNode* cur = root;
        while (cur) {
            if (cur->key == key && cur->is_number) return cur->value_int;
            cur = cur->next;
        }
        return default_val;
    }

private:
    JsonNode* root;

    void clear() {
        JsonNode* cur = root;
        while (cur) {
            JsonNode* next = cur->next;
            delete cur;
            cur = next;
        }
        root = nullptr;
    }

    void skipWhitespace(const std::string& s, size_t& pos) const {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++;
    }
};

#endif
