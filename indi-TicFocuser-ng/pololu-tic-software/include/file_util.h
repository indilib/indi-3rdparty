// Helpers for opening input and output files.  If the file cannot be opened, an
// exception is thrown.  For functions with "pipe" in the name, if the filename
// is "-", the standard input or output is used instead of actually opening a
// file.

#pragma once

#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace
{
  // We would like to just return a std::ifstream here, but that does not
  // work in GCC 4.9.2 for Raspbian/Debian Jessie.
  void open_file_input(const std::string & filename, std::ifstream & file)
  {
    file.open(filename);
    if (!file)
    {
      int error_code = errno;
      throw std::runtime_error(filename + ": " + strerror(error_code) + ".");
    }
  }

  std::shared_ptr<std::istream> open_file_or_pipe_input(const std::string & filename)
  {
    std::shared_ptr<std::istream> file;
    if (filename == "-")
    {
      file.reset(&std::cin, [](std::istream *){});
    }
    else
    {
      std::ifstream * concrete_file = new std::ifstream();
      open_file_input(filename, *concrete_file);
      file.reset(concrete_file);
    }
    return file;
  }

  // We would like to just return a std::ifstream here, but that does not
  // work in GCC 4.9.2 for Raspbian/Debian Jessie.
  void open_file_output(const std::string & filename, std::ofstream & file)
  {
    file.open(filename);
    if (!file)
    {
      int error_code = errno;
      throw std::runtime_error(filename + ": " + strerror(error_code) + ".");
    }
  }

  std::shared_ptr<std::ostream> open_file_or_pipe_output(const std::string & filename)
  {
    std::shared_ptr<std::ostream> file;
    if (filename == "-")
    {
      file.reset(&std::cout, [](std::ostream *){});
    }
    else
    {
      std::ofstream * concrete_file = new std::ofstream();
      open_file_output(filename, *concrete_file);
      file.reset(concrete_file);
    }
    return file;
  }

  inline void write_string_to_file(const std::string & filename,
    const std::string & contents)
  {
    std::ofstream file;
    open_file_output(filename, file);
    file << contents;
    if (file.fail())
    {
      throw std::runtime_error("Failed to write to file.");
    }
  }

  inline void write_string_to_file_or_pipe(const std::string & filename,
    const std::string & contents)
  {
    auto stream = open_file_or_pipe_output(filename);
    *stream << contents;
    if (stream->fail())
    {
      throw std::runtime_error("Failed to write to file or pipe.");
    }
  }

  inline std::string read_string_from_file(const std::string & filename)
  {
    std::ifstream file;
    open_file_input(filename, file);
    std::string contents =
      std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
      );
    if (file.fail())
    {
      throw std::runtime_error("Failed to read from file.");
    }
    return contents;
  }

  inline std::string read_string_from_file_or_pipe(const std::string & filename)
  {
    auto stream = open_file_or_pipe_input(filename);
    std::string contents =
      std::string(
        std::istreambuf_iterator<char>(*stream),
        std::istreambuf_iterator<char>()
      );
    if (stream->fail())
    {
      throw std::runtime_error("Failed to read from file or pipe.");
    }
    return contents;
  }
}
