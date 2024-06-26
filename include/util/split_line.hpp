#include <string>
#include <vector>

std::vector<std::string> splitLine(const std::string& line, int max_width, char sep = ' ') {
    std::vector<std::string> ret;

    for (size_t index = 0; index < line.size();) {
      // Take the rest of the line if possible
      size_t cut_width = line.size() - index;

      if (index + max_width < line.size()) {
        // If cannot take the rest of the line, take until the last separator if there is one
        size_t next_space = line.find_last_of(sep, index + max_width - 1);

        if (next_space == -1 || (next_space >= 0 && next_space < index)) {
          // No space before next cut
          // We just simply cut at that point.
          // In extreme cases a word will be seperated
          cut_width = max_width;
        } else {
          // Cut until the space (inclusively)
          cut_width = next_space + 1 - index;
        }
      }

      ret.push_back(line.substr(index, cut_width));

      index += cut_width;
    }

    return ret;
}