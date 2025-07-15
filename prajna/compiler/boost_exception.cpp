#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace boost {

#ifdef BOOST_NO_EXCEPTIONS

void throw_exception(std::exception const& e) { throw e; }

void throw_exception(std::exception const& e, boost::source_location const& loc) { throw e; }

#endif

}  // namespace boost
