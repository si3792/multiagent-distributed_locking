#include "RicartAgrawalaExtended.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <stdexcept>

namespace fipa {
namespace distributed_locking {

// Set the protocol
const protocol::Protocol RicartAgrawalaExtended::protocol = protocol::RICART_AGRAWALA_EXTENDED;
    
RicartAgrawalaExtended::RicartAgrawalaExtended() 
    : RicartAgrawala()
{
}

RicartAgrawalaExtended::RicartAgrawalaExtended(const fipa::Agent& self, const std::vector< std::string >& resources) 
    : RicartAgrawala(self, resources)
{
}

} // namespace distributed_locking
} // namespace fipa