#include <string>
#include <istream>

// Due to performance reasons it is not optimal to reread the whole displayed
// chunk into buffer. This class maintains a buffer while abstracting away the underlying file stream.
// The derived classes may implement different extracting strategies.

class ContentExtractor {
public:
  ContentExtractor() = default;

  ContentExtractor(const ContentExtractor&) = delete;
  ContentExtractor(ContentExtractor&&) = delete;
  ContentExtractor& operator=(const ContentExtractor&) = delete;
  ContentExtractor& operator=(ContentExtractor&&) = delete;

  virtual ~ContentExtractor() = default;

  virtual void setStartPos(std::streampos pos) = 0;

  virtual std::streampos getStartPos() = 0;

  virtual void setContentLength(size_t) = 0;

  virtual size_t getContentLength() = 0;
  // Returns the content window based on the starting position and the length
  virtual std::string extract() = 0;
};