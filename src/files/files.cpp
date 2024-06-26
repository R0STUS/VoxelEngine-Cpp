#include "files.hpp"

#include "../coders/commons.hpp"
#include "../coders/json.hpp"
#include "../coders/toml.hpp"
#include "../coders/gzip.hpp"
#include "../util/stringutil.hpp"
#include "../data/dynamic.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <stdexcept>

namespace fs = std::filesystem;

files::rafile::rafile(fs::path filename)
    : file(filename, std::ios::binary | std::ios::ate) {
    if (!file) {
        throw std::runtime_error("could not to open file "+filename.string());
    }
    filelength = file.tellg();
    file.seekg(0);
}

size_t files::rafile::length() const {
    return filelength;
}

void files::rafile::seekg(std::streampos pos) {
    file.seekg(pos);
}

void files::rafile::read(char* buffer, std::streamsize size) {
    file.read(buffer, size);
}

bool files::write_bytes(fs::path filename, const ubyte* data, size_t size) {
    std::ofstream output(filename, std::ios::binary);
    if (!output.is_open())
        return false;
    output.write((const char*)data, size);
    output.close();
    return true;
}

uint files::append_bytes(fs::path filename, const ubyte* data, size_t size) {
    std::ofstream output(filename, std::ios::binary | std::ios::app);
    if (!output.is_open())
        return 0;
    uint position = output.tellp();
    output.write((const char*)data, size);
    output.close();
    return position;
}

bool files::read(fs::path filename, char* data, size_t size) {
    std::ifstream output(filename, std::ios::binary);
    if (!output.is_open())
        return false;
    output.read(data, size);
    output.close();
    return true;
}

std::unique_ptr<ubyte[]> files::read_bytes(fs::path filename, size_t& length) {
    std::ifstream input(filename, std::ios::binary);
    if (!input.is_open())
        return nullptr;
    input.seekg(0, std::ios_base::end);
    length = input.tellg();
    input.seekg(0, std::ios_base::beg);

    auto data = std::make_unique<ubyte[]>(length);
    input.read((char*)data.get(), length);
    input.close();
    return data;
}

std::string files::read_string(fs::path filename) {
    size_t size;
    std::unique_ptr<ubyte[]> bytes (read_bytes(filename, size));
    if (bytes == nullptr) {
        throw std::runtime_error("could not to load file '"+
                                 filename.string()+"'");
    }
    return std::string((const char*)bytes.get(), size);
}

bool files::write_string(fs::path filename, const std::string content) {
    std::ofstream file(filename);
    if (!file) {
        return false;
    }
    file << content;
    return true;
}

bool files::write_json(fs::path filename, const dynamic::Map* obj, bool nice) {
    return files::write_string(filename, json::stringify(obj, nice, "  "));
}

bool files::write_binary_json(fs::path filename, const dynamic::Map* obj, bool compression) {
    auto bytes = json::to_binary(obj, compression);
    return files::write_bytes(filename, bytes.data(), bytes.size());
}

std::shared_ptr<dynamic::Map> files::read_json(fs::path filename) {
    std::string text = files::read_string(filename);
    try {
        return json::parse(filename.string(), text);;
    } catch (const parsing_error& error) {
        std::cerr << error.errorLog() << std::endl;
        throw std::runtime_error("could not to parse "+filename.string());
    }
}

std::shared_ptr<dynamic::Map> files::read_binary_json(fs::path file) {
    size_t size;
    std::unique_ptr<ubyte[]> bytes (files::read_bytes(file, size));
    return json::from_binary(bytes.get(), size);
}

std::shared_ptr<dynamic::Map> files::read_toml(fs::path file) {
    return toml::parse(file.u8string(), files::read_string(file));
}

std::vector<std::string> files::read_list(fs::path filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("could not to open file "+filename.u8string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        util::trim(line);
        if (line.length() == 0)
            continue;
        if (line[0] == '#')
            continue;
        lines.push_back(line);
    }
    return lines;
}
