#include <algorithm>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/system/detail/error_category.hpp>
#include <boost/throw_exception.hpp>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

class Bayan
{
public:
  Bayan(std::vector<fs::path>& paths,
        std::vector<fs::path>& excludes,
        std::vector<boost::regex>& masks,
        boost::uintmax_t fileMinSize = 1,
        boost::uintmax_t blockSize = 1024,
        bool scanningLevel = 0)
    : paths_(paths)
    , excludes_(excludes)
    , scanningLevel_(scanningLevel)
    , fileMinSize_(fileMinSize)
    , masks_(masks)
    , blockSize_(blockSize)
  {
  }

  void process()
  {
    boost::smatch what;

    if (scanningLevel_) {
      for (auto directory : paths_) {
        for (fs::recursive_directory_iterator dir(directory), end; dir != end;
             ++dir) {
          if (std::find(excludes_.begin(), excludes_.end(), dir->path()) !=
              excludes_.end()) {
            dir.disable_recursion_pending();
            continue;
          }
          if (fs::is_regular_file(*dir)) {
            for (auto mask : masks_) {
              if (!boost::regex_match(dir->path().string(), what, mask) ||
                  fs::file_size(*dir) < fileMinSize_)
                continue;
              // std::cout << dir->path().string() << '\n';
              files_.push_back(dir->path().string());

              if (files_.size() > 1) {
                for (size_t i = 0; i < files_.size()-1; ++i) {
                  if (hash(files_[i], dir->path()))
                    duplicates_.insert({files_[i], dir->path().string()});
                }
              }
            }
          }
        }
      }
    } else {
      for (auto directory : paths_) {
        for (fs::directory_iterator dir(directory), end; dir != end; ++dir) {
          if (dir->is_regular_file()) {
            for (auto mask : masks_) {
              if (!boost::regex_match(dir->path().string(), what, mask) ||
                  fs::file_size(*dir) < fileMinSize_)
                continue;
              // std::cout << dir->path().string() << '\n';
              files_.push_back(dir->path().string());

              if (files_.size() > 1) {
                for (size_t i = 0; i < files_.size()-1; ++i) {
                  if (hash(files_[i], dir->path()))
                    duplicates_.insert({files_[i], dir->path().string()});
                }
              }
            }
          }
        }
      }
    }
  }

  std::string result()
  {
    if (duplicates_.empty())
      return "There's nothing duplicates!";
    else {
      std::vector<std::set<fs::path>> sets;
      std::string answer = "Duplicates:\n\n";
      for (auto it = duplicates_.begin(); it != duplicates_.end(); ++it) {
        if (sets.empty()) {
          sets.push_back({it->first, it->second});
          continue;
        }
        for (auto& s : sets) {
          if (std::find(s.begin(), s.end(), it->first) != s.end() ||
              std::find(s.begin(), s.end(), it->second) != s.end()) {
            s.insert(it->first);
            s.insert(it->second);
            break;
          } else {
            sets.push_back({it->first, it->second});
            break;
          }
        }
      }
      for (auto& s : sets) {
        for (const fs::path& p : s) {
          answer += p.string() + '\n';
        }
        answer += "==========\n";
      }
      return answer;
    }
  }

private:
  std::uint32_t crc32(const std::string& s, size_t len)
  {
    boost::crc_32_type result;
    result.process_bytes(s.data(), len);
    return result.checksum();
  }

  bool hash(const fs::path& a, const fs::path& b)
  {
    if (fs::file_size(a) != fs::file_size(b))
      return false;

    std::ifstream first(a.string(), std::ios::binary);
    std::ifstream second(b.string(), std::ios::binary);


    std::vector<char> fBuffer(blockSize_);
    std::vector<char> sBuffer(blockSize_);

    while (first && second) {
      first.read(fBuffer.data(), blockSize_);
      second.read(sBuffer.data(), blockSize_);

      std::streamsize fSize = first.gcount();
      std::streamsize sSize = second.gcount();

      if (fSize != sSize)
        return false;
      
      if (crc32(fBuffer.data(), fSize) != crc32(sBuffer.data(), sSize))
        return false;
    }
    return true;
  }

  std::vector<fs::path> paths_;
  std::vector<fs::path> excludes_;
  bool scanningLevel_;
  boost::uintmax_t fileMinSize_;
  std::vector<boost::regex> masks_;
  const boost::uintmax_t blockSize_;
  std::vector<fs::path> files_;
  std::multimap<fs::path, fs::path> duplicates_;
};

std::string
wildcard_to_regex(const std::string& pattern)
{
  std::string rx;
  rx.reserve(pattern.size() * 2);
  rx += "^";

  for (char c : pattern) {
    switch (c) {
      case '*':
        rx += ".*";
        break;
      case '?':
        rx += ".";
        break;
      case '.':
        rx += "\\.";
        break;
      case '\\':
        rx += "\\\\";
        break;
      default:
        if (std::isalnum(c))
          rx += c;
        else {
          rx += "\\";
          rx += c;
        }
    }
  }

  rx += "$";
  return rx;
}

int
main(int argc, char* argv[])
{
  try {
    // parsing command line
    po::options_description desc("Programm for searching duplicates");
    desc.add_options()("help,h", "Show help")(
      "directories,d",
      po::value<std::vector<std::string>>()->multitoken(),
      "Directories for scan")(
      "excludes,e",
      po::value<std::vector<std::string>>()->multitoken(),
      "Directories for excluding from scanning")(
      "level,l",
      po::value<bool>()->default_value(0),
      "Level of scanning. 1 - scan all directories, 0 - scan without nested "
      "dirs")("min-size,s",
              po::value<boost::uintmax_t>()->default_value(1),
              "Minimum size of file for scanning")(
      "masks,m",
      po::value<std::vector<std::string>>()->multitoken(),
      "Masks for file")("block-size,b",
                        po::value<boost::uintmax_t>()->default_value(1024),
                        "Size of block for reading file");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // vars for class
    std::vector<fs::path> paths;
    std::vector<fs::path> excludes;
    bool scanningLevel = vm["level"].as<bool>();
    boost::uintmax_t fileMinSize = vm["min-size"].as<boost::uintmax_t>();
    std::vector<boost::regex> masks;
    boost::uintmax_t blockSize = vm["block-size"].as<boost::uintmax_t>();

    // processing options
    if (vm.count("help") || vm.count("h")) {
      std::cout << desc;
      return 0;
    }
    if (!vm["directories"].empty()) {
      std::vector<std::string> rawPaths =
        vm["directories"].as<std::vector<std::string>>();
      paths.reserve(rawPaths.size());
      for (size_t i = 0; i < rawPaths.size(); ++i)
        paths.push_back(fs::absolute(rawPaths[i]));
    }
    if (!vm["excludes"].empty()) {
      std::vector<std::string> rawExcludes =
        vm["excludes"].as<std::vector<std::string>>();
      excludes.reserve(rawExcludes.size());
      for (size_t i = 0; i < rawExcludes.size(); ++i)
        excludes.push_back(fs::absolute(rawExcludes[i]));
    }
    if (!vm["masks"].empty()) {
      std::vector<std::string> rawMasks =
        vm["masks"].as<std::vector<std::string>>();
      masks.reserve(rawMasks.size());
      for (const std::string& mask : rawMasks)
        masks.push_back(
          boost::regex(wildcard_to_regex(mask), boost::regex::icase));
    } else {
      masks = { boost::regex("^.*$") };
    }

    Bayan b(paths, excludes, masks, fileMinSize, blockSize, scanningLevel);
    b.process();
    std::cout << b.result();

    return 0;
  } catch (const boost::wrapexcept<po::unknown_option>& e) {
    std::cout << "Unrecognized option. Try «bayan --help» or «bayan -h»";
    return 1;
  } catch (const boost::wrapexcept<po::invalid_command_line_syntax>& e) {
    std::cout << "Arguments not provided for " << e.get_option_name();
    return 1;
  }
}